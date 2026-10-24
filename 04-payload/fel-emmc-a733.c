/*
 * fel-emmc.c -- Bare-metal eMMC read/write driver for Allwinner A523/T527
 *
 * Copyright (c) 2026 Contributors
 * SPDX-License-Identifier: MIT
 *
 * Runs from SRAM via FEL mode. Uses PIO/FIFO mode (no DMA, no DRAM).
 * Fits entirely within the A523's 40KB SRAM -- no DRAM initialization needed,
 * bypassing TrustZone restrictions that block DRAM writes from Non-Secure code.
 *
 * Memory map:
 *   0x20000          Code (loaded by xfel)
 *   0x2FFF0          Command block (16 bytes, written by host before exec)
 *   0x30000-0x4FFFF  Data buffer (128KB = 256 sectors)
 *   0x58000          Stack (grows down)
 *
 * Command block at 0x2FFF0:
 *   +0x00  cmd     (uint32)  0x01=read, 0x02=write, 0x03=info
 *   +0x04  sector  (uint32)  starting eMMC sector (LBA)
 *   +0x08  count   (uint32)  number of sectors (max 256)
 *   +0x0C  flags   (uint32)  reserved (0)
 *
 * Status via RTC_GP registers (0x07090100 + n*4):
 *   GP0  state     0x01=init, 0x02=card_ready, 0x10=reading,
 *                  0x20=writing, 0xAA=done, 0xEE=error
 *   GP1  progress  sectors completed, or error code if GP0=0xEE
 *   GP2  diag0     first data word (read) / CID[0] (info) / error detail
 *   GP3  diag1     CID[1] (info)
 *
 * Build:
 *   arm-none-eabi-gcc -nostdlib -nostartfiles -ffreestanding -O2 \
 *     -DSPEED_1BIT_FAST \
 *     -march=armv7-a -Wl,-Ttext=0x20000 -o fel-emmc.elf fel-emmc.c
 *   arm-none-eabi-objcopy -O binary fel-emmc.elf fel-emmc.bin
 *
 * Usage:
 *   See emmc-tool.sh for orchestrated operations.
 *   Manual single-sector read example:
 *     printf '\x01\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00' > /tmp/cmd.bin
 *     xfel write 0x2FFF0 /tmp/cmd.bin
 *     xfel write 0x20000 fel-emmc.bin && xfel exec 0x20000
 *     xfel read 0x30000 512 sector0.bin
 */

/* --- Register helpers --- */

/*
 * Memory barriers: required for MMIO with MMU off on ARMv7.
 * Without barriers, CPU can reorder/speculate FIFO reads, causing
 * byte-level corruption (~0.003% error rate observed without these).
 */
#define dmb()           __asm__ volatile("dmb" ::: "memory")
#define dsb()           __asm__ volatile("dsb" ::: "memory")

#define writel(v, a)    do { dsb(); (*(volatile unsigned int *)(a) = (v)); } while(0)
#define readl(a)        ({ unsigned int __v = (*(volatile unsigned int *)(a)); dmb(); __v; })
#define setbits(a, v)   writel(readl(a) | (v), (a))
#define clrbits(a, v)   writel(readl(a) & ~(v), (a))
#define BIT(x)          (1u << (x))

/* --- Hardware addresses --- */

#define SMHC2_BASE      0x04022000      /* eMMC controller (SMHC2) */
#define S(off)          (SMHC2_BASE + (off))

/* SMHC2 register offsets */
#define SMHC_GCTRL      S(0x00)         /* Global control */
#define SMHC_CLKCR      S(0x04)         /* Clock control */
#define SMHC_TMOUT      S(0x08)         /* Timeout */
#define SMHC_WIDTH      S(0x0C)         /* Bus width */
#define SMHC_BLKSIZ     S(0x10)         /* Block size */
#define SMHC_BYTCNT     S(0x14)         /* Byte count */
#define SMHC_CMD        S(0x18)         /* Command register */
#define SMHC_CMDARG     S(0x1C)         /* Command argument */
#define SMHC_RESP0      S(0x20)         /* Response 0 */
#define SMHC_RESP1      S(0x24)         /* Response 1 */
#define SMHC_RESP2      S(0x28)         /* Response 2 */
#define SMHC_RESP3      S(0x2C)         /* Response 3 */
#define SMHC_IMASK      S(0x30)         /* Interrupt mask */
#define SMHC_RINT       S(0x38)         /* Raw interrupt status */
#define SMHC_STATUS     S(0x3C)         /* Status */
#define SMHC_HWRST      S(0x78)         /* Hardware reset */
#define SMHC_FIFO       S(0x200)        /* FIFO data port */

/* CCU (Clock Control Unit) registers -- A733, CCU base 0x02002000 */
#define CCU_SMHC2_GATE  0x02002d2c      /* bit0 = gate, bit16 = reset */

/* GPIO (PC port for eMMC data lines) -- A733 GPIO V3 ("NCAT3") layout */
#define PC_CFG0         0x02000180      /* PC0-PC7   */
#define PC_CFG1         0x02000184      /* PC8-PC15  */
#define PC_CFG2         0x02000188      /* PC16-PC23 */
#define PC_DRV0         0x020001a0
#define PC_DRV1         0x020001a4
#define PC_DRV2         0x020001a8
#define PC_PULL         0x020001b0

/* RTC general-purpose registers (survive across FEL exec calls) */
#define RTC_GP(n)       (0x07090100 + (n) * 4)

/* Command block location in SRAM (A733: writable area starts ~0x51000) */
#define CMD_ADDR        0x5AFF0
#define DATA_BUF        0x5B000
#define MAX_SECTORS     512     /* 64KB buffer */

/* Command codes */
#define CMD_READ        0x01
#define CMD_WRITE       0x02
#define CMD_INFO        0x03
#define CMD_BOOT_READ   0x04    /* Read from eMMC Boot Partition 1 */
#define CMD_RPMB_READ   0x05    /* Read RPMB data blocks */
#define CMD_FIX_EXTCSD  0x06    /* Write a value to EXT_CSD -- sector=index, count=value */

/* RINT (Raw Interrupt) bits */
#define RINT_RESP_ERR   BIT(1)
#define RINT_CMD_DONE   BIT(2)
#define RINT_DATA_OVER  BIT(3)
#define RINT_TX_REQ     BIT(4)
#define RINT_RX_REQ     BIT(5)
#define RINT_RESP_CRC   BIT(6)
#define RINT_DATA_CRC   BIT(7)
#define RINT_RESP_TMO   BIT(8)
#define RINT_DATA_TMO   BIT(9)
#define RINT_FIFO_ERR   BIT(11)
#define RINT_HW_LOCKED  BIT(12)
#define RINT_START_ERR  BIT(13)
#define RINT_END_ERR    BIT(15)

#define RINT_ERR_MASK   (RINT_RESP_ERR | RINT_RESP_CRC | RINT_DATA_CRC | \
                         RINT_RESP_TMO | RINT_DATA_TMO | RINT_FIFO_ERR | \
                         RINT_HW_LOCKED | RINT_START_ERR | RINT_END_ERR)

/* STATUS register bits */
#define STATUS_FIFO_FULL  BIT(3)
#define STATUS_CARD_BUSY  BIT(9)

/* --- Timing --- */

static void udelay(unsigned int us)
{
    volatile unsigned int i;
    for (i = 0; i < us * 10; i++)
        ;
}

/* --- Status reporting --- */

static void set_state(unsigned int st)  { writel(st, RTC_GP(0)); }
static void set_progress(unsigned int v) { writel(v, RTC_GP(1)); }
static void set_diag0(unsigned int v)   { writel(v, RTC_GP(2)); }
static void set_diag1(unsigned int v)   { writel(v, RTC_GP(3)); }

static void report_error(unsigned int code)
{
    set_progress(code);
    set_state(0xEE);
}

/* --- SMHC command engine --- */

/*
 * Send a command to the eMMC card.
 * ci:   command index (0-63)
 * arg:  command argument
 * fl:   extra CMD register flags (RESP_EXPIRE, CHK_CRC, LONG_RESPONSE, etc.)
 * resp: if non-NULL, receives RESP0 value
 *
 * Returns 0 on success, -1 on error, -2 on timeout.
 */
static int smhc_cmd(unsigned int ci, unsigned int arg, unsigned int fl,
                    unsigned int *resp)
{
    unsigned int i, ri;

    writel(0xFFFFFFFF, SMHC_RINT);                  /* clear all interrupts */
    writel(arg, SMHC_CMDARG);
    writel(BIT(31) | fl | (ci & 0x3F), SMHC_CMD);   /* START + flags + index */

    for (i = 0; i < 500000; i++) {
        ri = readl(SMHC_RINT);
        if (ri & (RINT_CMD_DONE | RINT_RESP_TMO | RINT_FIFO_ERR | RINT_HW_LOCKED)) {
            if (resp)
                *resp = readl(SMHC_RESP0);
            if (ri & RINT_ERR_MASK) {
                /* Hardware reset on error (SyterKit pattern) -- clears
                 * stuck controller state that would corrupt future cmds */
                writel(BIT(0) | BIT(1) | BIT(2), SMHC_GCTRL);
                for (i = 0; i < 100000; i++)
                    if (!(readl(SMHC_GCTRL) & (BIT(0) | BIT(1) | BIT(2))))
                        break;
                writel(0, SMHC_WIDTH);          /* re-force 1-bit after reset */
                return -1;
            }
            return 0;
        }
    }
    return -2;
}

/* Update clock configuration (must be called after changing CLKCR) */
static int smhc_update_clk(void)
{
    unsigned int i;
    writel(BIT(31) | BIT(21), SMHC_CMD);    /* START + UPCLK_ONLY */
    for (i = 0; i < 100000; i++)
        if (!(readl(SMHC_CMD) & BIT(31)))
            return 0;
    return -1;
}

/* --- Sector I/O (PIO/FIFO) --- */

/*
 * Read one 512-byte sector from eMMC to buf.
 * Uses CMD17 + wait-for-DATA_OVER + FIFO drain.
 *
 * IMPORTANT: Uses raw volatile access (no macro barriers) -- this matches
 * the proven implementation that read thousands of sectors without errors.
 * The dmb/dsb barriers in our macros appear to break multi-sector reads.
 */
static int read_sector(unsigned int sector, unsigned int *buf)
{
    volatile unsigned int *rint = (volatile unsigned int *)0x04022038;
    volatile unsigned int *fifo = (volatile unsigned int *)0x04022200;
    unsigned int i;

    *(volatile unsigned int *)0x04022038 = 0xFFFFFFFF;  /* clear RINT */
    *(volatile unsigned int *)0x04022010 = 512;          /* BLKSIZ */
    *(volatile unsigned int *)0x04022014 = 512;          /* BYTCNT */
    *(volatile unsigned int *)0x0402201C = sector;       /* CMDARG */
    *(volatile unsigned int *)0x04022018 =               /* CMD17 */
        (1u << 31) | (1u << 9) | (1u << 8) | (1u << 6) | 17;

    /* Wait for DATA_OVER or error */
    for (i = 0; i < 2000000; i++)
        if (*rint & ((1u << 3) | (1u << 9)))
            break;
    /* Accept data if DATA_OVER is set, even with END_ERR -- the BROM's
     * prior high-speed eMMC init can leave residual signal state that
     * causes spurious END_ERR on our 400kHz reads. The data is valid. */
    if (*rint & (1u << 3))
        goto drain_fifo;
    if (*rint & (1u << 9))
        return -1;     /* DATA_TMO -- no data received at all */
    return -2;         /* loop exhausted -- neither DATA_OVER nor timeout */

drain_fifo:

    /* Set ACCESS_BY_AHB before FIFO drain -- required for read pointer to advance.
     * Without this bit, FIFO reads return the same first word infinitely. */
    *(volatile unsigned int *)0x04022000 |= (1u << 31);

    /* Drain FIFO -- all 128 words ready after DATA_OVER */
    for (i = 0; i < 128; i++)
        buf[i] = *fifo;

    /* Reset FIFO for next read, then re-assert ACCESS_BY_AHB.
     * FIFO_RESET (GCTRL BIT(1)) clears ACCESS_BY_AHB as a hardware side effect. */
    *(volatile unsigned int *)0x04022000 |= (1u << 1);     /* GCTRL FIFO_RESET */
    for (i = 0; i < 100000; i++)
        if (!(*(volatile unsigned int *)0x04022000 & (1u << 1)))
            break;
    *(volatile unsigned int *)0x04022000 |= (1u << 31);    /* ACCESS_BY_AHB */

    return 0;
}

/*
 * Write one 512-byte sector from buf to eMMC.
 * Uses CMD24 (WRITE_BLOCK) + FIFO push.
 *
 * Flow:
 *   1. Set ACCESS_BY_AHB
 *   2. Issue CMD24
 *   3. Push data to FIFO (checking not full)
 *   4. Wait for COMMAND_DONE + DATA_OVER
 *   5. Wait for card busy clear
 *   6. Clear RINT
 */
static int write_sector(unsigned int sector, unsigned int *buf)
{
    unsigned int i, status;

    writel(0x30, RTC_GP(4));    /* checkpoint: starting write */

    writel(0xFFFFFFFF, SMHC_RINT);
    writel(512, SMHC_BLKSIZ);
    writel(512, SMHC_BYTCNT);

    /* Set AHB access mode for CPU FIFO access */
    setbits(SMHC_GCTRL, BIT(31));

    writel(sector, SMHC_CMDARG);
    /* CMD24: START | WRITE | DATA_EXPIRE | CHK_CRC | RESP_EXPIRE */
    writel(BIT(31) | BIT(10) | BIT(9) | BIT(8) | BIT(6) | 24, SMHC_CMD);

    writel(0x31, RTC_GP(4));    /* checkpoint: CMD24 issued */

    /* Push 128 words to FIFO, checking FIFO_FULL before each word */
    for (i = 0; i < 128; i++) {
        unsigned int timeout = 500000;
        while ((readl(SMHC_STATUS) & STATUS_FIFO_FULL) && --timeout)
            ;
        if (!timeout)
            return -2;
        writel(buf[i], SMHC_FIFO);
    }

    writel(0x32, RTC_GP(4));    /* checkpoint: FIFO push complete */

    /* Wait for COMMAND_DONE */
    for (i = 0; i < 500000; i++)
        if (readl(SMHC_RINT) & RINT_CMD_DONE)
            break;

    /* Wait for DATA_OVER */
    for (i = 0; i < 2000000; i++)
        if (readl(SMHC_RINT) & RINT_DATA_OVER)
            break;

    writel(0x33, RTC_GP(4));    /* checkpoint: completion flags received */

    /* Wait for card to finish programming (busy on DAT0) */
    for (i = 0; i < 5000000; i++)
        if (!(readl(SMHC_STATUS) & STATUS_CARD_BUSY))
            break;

    /* Clear all interrupt flags */
    writel(readl(SMHC_RINT), SMHC_RINT);

    writel(0x34, RTC_GP(4));    /* checkpoint: write complete */

    return 0;
}

/* --- eMMC initialization --- */

/*
 * Initialize SMHC2 controller and eMMC card.
 * On success: card is selected, bus configured, clock set.
 * On failure: returns negative error code.
 *
 * cid[4] receives the Card Identification register if non-NULL.
 */
static int emmc_init(unsigned int *cid)
{
    unsigned int resp, i;

    /* === Pin mux: PC port for eMMC (function 3), A733 GPIO V3 === */
    writel(0xf33fff33, PC_CFG0);        /* PC0,1,5,6 = sdc2 */
    writel(0x333f3333, PC_CFG1);        /* PC8-11,13-15 = sdc2 */
    writel(0x00000003, PC_CFG2);        /* PC16 = sdc2 */
    writel(0x11111111, PC_DRV0);
    writel(0x11111111, PC_DRV1);
    writel(0x11111111, PC_DRV2);
    writel(0x54555946, PC_PULL);

    /* === CCU: SMHC2 module clock: mux=sys24M(0), div=1, gate on === */
    writel(0x80000000, 0x02002d20);

    /* === CCU: SMHC2 gate/reset (from A733 BROM sequence) === */
    clrbits(CCU_SMHC2_GATE, BIT(16));                  /* assert reset */
    clrbits(CCU_SMHC2_GATE, BIT(0));                   /* gate off */
    udelay(10);
    setbits(CCU_SMHC2_GATE, BIT(0));                   /* gate on */
    setbits(CCU_SMHC2_GATE, BIT(16));                  /* deassert reset */
    udelay(100);

    /* === Controller reset === */
    writel(BIT(0) | BIT(1) | BIT(2), SMHC_GCTRL);     /* soft + FIFO + DMA reset */
    for (i = 0; i < 100000; i++)
        if (!(readl(SMHC_GCTRL) & (BIT(0) | BIT(1) | BIT(2))))
            break;

    /* Force 1-bit bus width -- BROM may leave controller in 8-bit mode
     * from its own eMMC boot attempt. Software reset does NOT clear WIDTH.
     * Without this, reads fail with START_ERR (card sends on DAT0 only,
     * controller reads all 8 lines and sees garbage on DAT1-7). */
    writel(0, SMHC_WIDTH);

    /* === SyterKit-recommended init registers === */
    writel(0x00000003, S(0x54));                        /* csdc: CRC status detect */
    writel(0x00000deb, S(0x50));                        /* dbgc: debug enable */
    writel((512 << 16) | BIT(2) | BIT(0), S(0x100));   /* thldc: card threshold = 512B */
    /* Sample delay: enable delay chain, delay=0 (timing mode 4 for SMHC2) */
    writel(BIT(7), S(0x144));                           /* samp_dl: enable, delay=0 */

    /* === Clock: init speed ~400kHz (24MHz / 60) === */
    /* Mask D0 during clock changes to prevent card bus contention */
    writel(29 | BIT(31), SMHC_CLKCR);                  /* divider=29 + MASK_D0 */
    if (smhc_update_clk())
        return -1;
    writel(29 | BIT(16) | BIT(31), SMHC_CLKCR);        /* + enable clock + MASK_D0 */
    if (smhc_update_clk())
        return -1;
    clrbits(SMHC_CLKCR, BIT(31));                       /* clear MASK_D0 */
    if (smhc_update_clk())
        return -1;

    /* Timeout and interrupt config */
    writel(0xFFFFFFFF, SMHC_TMOUT);
    writel(0, SMHC_IMASK);                              /* no interrupts (polling) */

    /* === Hardware reset pulse === */
    writel(0, SMHC_HWRST);
    udelay(1000);
    writel(1, SMHC_HWRST);
    udelay(1000);

    /* Send 80 init clocks */
    writel(BIT(31) | BIT(15), SMHC_CMD);                /* START + SEND_INIT_SEQ */
    for (i = 0; i < 100000; i++)
        if (!(readl(SMHC_CMD) & BIT(31)))
            break;
    udelay(1000);

    set_state(0x01);    /* init started */

    /* === Card initialization sequence === */

    /* CMD0: GO_IDLE_STATE */
    smhc_cmd(0, 0, BIT(15), 0);

    /* CMD1: SEND_OP_COND -- poll until card is ready.
     * Limited to 1000 retries to stay within USB timeout for
     * synchronous FEL exec. If eMMC has no power, fails fast instead
     * of hanging and killing the FEL session. */
    for (i = 0; i < 1000; i++) {
        if (!smhc_cmd(1, 0x40FF8080, BIT(6), &resp) && (resp & BIT(31)))
            break;
        udelay(1000);
    }
    if (i >= 1000)
        return -2;  /* CMD1 timeout */

    /* CMD2: ALL_SEND_CID */
    smhc_cmd(2, 0, BIT(8) | BIT(7) | BIT(6), 0);
    if (cid) {
        cid[0] = readl(SMHC_RESP0);
        cid[1] = readl(SMHC_RESP1);
        cid[2] = readl(SMHC_RESP2);
        cid[3] = readl(SMHC_RESP3);
    }

    /* CMD3: SET_RELATIVE_ADDR (RCA=1) */
    smhc_cmd(3, 1 << 16, BIT(8) | BIT(6), 0);

    /* CMD7: SELECT_CARD */
    smhc_cmd(7, 1 << 16, BIT(8) | BIT(6), 0);

    set_state(0x02);    /* card ready */

    /* === Speed upgrade: faster clock, then optionally wider bus ===
     *
     * SyterKit reference: sunxi_mmc_probe() in sys-mmc.c
     * Step-by-step with D0 masking and R1B busy wait.
     *
     * Build with -DSPEED_1BIT_FAST for 12MHz/1-bit (~30x vs 400kHz)
     *         or -DSPEED_4BIT_FAST for 12MHz/4-bit (~120x)
     *         or -DSPEED_8BIT_FAST for 12MHz/8-bit (~240x)
     * Default (no define): 12MHz/4-bit as safe middle ground.
     */

    /* ALWAYS reset card bus width to 1-bit first -- BROM boot attempt may
     * have left the card in 4-bit or 8-bit mode. Without this, the controller
     * (1-bit after our SMHC_WIDTH reset) reads DAT0 only, but the card sends
     * data split across multiple lines -> garbled FIFO data.
     * CMD line is always 1-bit, so CMD6 itself always works. */
    smhc_cmd(6, 0x03B70000, BIT(8) | BIT(6), 0);   /* 1-bit SDR */
    for (i = 0; i < 500000; i++)
        if (!(readl(SMHC_STATUS) & STATUS_CARD_BUSY))
            break;
    udelay(1000);

#ifndef SPEED_1BIT_FAST
    /* CMD6 SWITCH: set card bus width (EXT_CSD[183]) */
#ifdef SPEED_8BIT_FAST
    if (smhc_cmd(6, 0x03B70200, BIT(8) | BIT(6), 0) < 0)   /* 8-bit SDR */
#else
    if (smhc_cmd(6, 0x03B70100, BIT(8) | BIT(6), 0) < 0)   /* 4-bit SDR */
#endif
        return -3;

    /* Wait for card busy (R1B) -- CMD6 has R1b response */
    for (i = 0; i < 500000; i++)
        if (!(readl(SMHC_STATUS) & STATUS_CARD_BUSY))
            break;
    udelay(1000);
#endif /* !SPEED_1BIT_FAST */

    /* Speed upgrade: try 12MHz, fall back to 400kHz if it fails.
     * BROM may leave SMHC2 in a state where clock changes fail
     * (observed on some boards where BROM booted before FEL entry). */
    {
        int speed_ok = 0;

        /* Disable card clock with D0 masking before reconfiguring */
        writel(BIT(31), SMHC_CLKCR);                         /* MASK_D0 only */
        if (smhc_update_clk() == 0) {
#ifndef SPEED_1BIT_FAST
            /* Set controller bus width to match card */
#ifdef SPEED_8BIT_FAST
            writel(2, SMHC_WIDTH);                            /* 8-bit */
#else
            writel(1, SMHC_WIDTH);                            /* 4-bit */
#endif
#endif
            /* Switch clock to ~12MHz (24MHz HOSC, divider=0 -> HOSC/2=12MHz) */
            writel(0 | BIT(31), SMHC_CLKCR);                 /* div=0 + MASK_D0 */
            if (smhc_update_clk() == 0) {
                writel(0 | BIT(16) | BIT(31), SMHC_CLKCR);   /* + CLK_EN + MASK_D0 */
                if (smhc_update_clk() == 0) {
                    /* Reconfigure sample delay for higher speed */
                    unsigned int samp = readl(S(0x144));
                    samp &= ~0x3F;
                    samp |= BIT(7);
                    writel(samp, S(0x144));

                    /* Clear D0 mask */
                    clrbits(SMHC_CLKCR, BIT(31));
                    if (smhc_update_clk() == 0)
                        speed_ok = 1;
                }
            }
        }

        if (!speed_ok) {
            /* Speed upgrade failed -- fall back to 400kHz/1-bit.
             * Re-do the basic clock setup that worked during init. */
            writel(0, SMHC_WIDTH);
            writel(29, SMHC_CLKCR);
            smhc_update_clk();
            setbits(SMHC_CLKCR, BIT(16));
            smhc_update_clk();
        }
    }

    return 0;
}

/* --- Main work function --- */

void __attribute__((noinline)) work(void);

/*
 * Entry point called by BROM FEL exec.
 * Must save and restore BROM's stack pointer -- overwriting it kills
 * the FEL USB handler and requires a full power cycle to recover.
 */
void __attribute__((naked, section(".start"))) main(void)
{
    __asm__ volatile(
        "mov r4, sp\n"              /* save BROM's SP */
        "ldr sp, =0x000A0000\n"     /* set our work stack */
        "push {r4, lr}\n"           /* save BROM SP + return addr */
        "bl work\n"
        "pop {r4, lr}\n"
        "mov sp, r4\n"             /* restore BROM's SP */
        "bx lr\n"
    );
}

void __attribute__((noinline)) work(void)
{
    unsigned int cmd, sector, count, i;
    unsigned int cid[4];

    /* Disable hardware watchdog -- BROM may set one before jumping to user code.
     * Without this, long operations (multi-sector writes) trigger a reset. */
    writel(0, 0x02050018);              /* WDT_MODE: disable watchdog timer 0 */
    writel(0, 0x020500B8);              /* WDT1_MODE: disable watchdog timer 1 (if present) */
    int rc;

    /* Preserve the "card already initialized" marker across execs */
    unsigned int already = (readl(RTC_GP(5)) == 0xCAFEBABEu);

    /* Clear status registers GP0-GP3 (keep GP5 marker) */
    for (i = 0; i < 4; i++)
        writel(0, RTC_GP(0) + i * 4);

    /* Read command block from SRAM */
    cmd    = readl(CMD_ADDR + 0x00);
    sector = readl(CMD_ADDR + 0x04);
    count  = readl(CMD_ADDR + 0x08);

    /* Validate */
    if (cmd < CMD_READ || cmd > CMD_FIX_EXTCSD) {
        report_error(0xE0);     /* bad command */
        return;
    }
    if (cmd != CMD_INFO && cmd != CMD_FIX_EXTCSD && count == 0) {
        report_error(0xE1);     /* zero count */
        return;
    }
    if (cmd != CMD_INFO && cmd != CMD_FIX_EXTCSD && count > MAX_SECTORS) {
        report_error(0xE2);     /* count too large */
        return;
    }

    /* Initialize eMMC once; reuse the initialized controller/card after that */
    if (cmd == CMD_INFO || !already) {
        rc = emmc_init(cid);
        if (rc < 0) {
            set_diag0((unsigned int)(-rc));
            report_error(0xF0);     /* init failed, diag0 has sub-code */
            return;
        }
        writel(0xCAFEBABEu, RTC_GP(5));
    }

    /* === Execute command === */

    if (cmd == CMD_INFO) {
        /* Report CID and return */
        set_diag0(cid[0]);
        set_diag1(cid[1]);
        /* Store full CID + EXT_CSD capacity in data buffer */
        writel(cid[0], DATA_BUF + 0x00);
        writel(cid[1], DATA_BUF + 0x04);
        writel(cid[2], DATA_BUF + 0x08);
        writel(cid[3], DATA_BUF + 0x0C);
        /* Read EXT_CSD (CMD8) for capacity -- 512 bytes to DATA_BUF+0x10 */
        writel(0xFFFFFFFF, SMHC_RINT);
        writel(512, SMHC_BLKSIZ);
        writel(512, SMHC_BYTCNT);
        writel(0, SMHC_CMDARG);
        writel(BIT(31) | BIT(13) | BIT(9) | BIT(8) | BIT(6) | 8, SMHC_CMD);
        for (i = 0; i < 2000000; i++)
            if (readl(SMHC_RINT) & (RINT_DATA_OVER | RINT_DATA_TMO))
                break;
        if (readl(SMHC_RINT) & RINT_DATA_OVER) {
            /* Set ACCESS_BY_AHB before FIFO drain */
            setbits(SMHC_GCTRL, BIT(31));
            unsigned int *ext = (unsigned int *)(DATA_BUF + 0x10);
            for (i = 0; i < 128; i++)
                ext[i] = readl(SMHC_FIFO);
            /* Reset FIFO and re-set ACCESS_BY_AHB */
            setbits(SMHC_GCTRL, BIT(1));
            udelay(100);
            setbits(SMHC_GCTRL, BIT(31));
        }
        set_progress(1);
        set_state(0xAA);
        return;
    }

    if (cmd == CMD_READ) {
        set_state(0x10);        /* reading */
        for (i = 0; i < count; i++) {
            unsigned int *buf = (unsigned int *)(DATA_BUF + i * 512);
            rc = read_sector(sector + i, buf);
            if (rc < 0) {
                set_diag0(sector + i);          /* which sector failed */
                set_diag1((unsigned int)(-rc));  /* error sub-code */
                report_error(0xD0 + (i & 0x0F));
                return;
            }
            set_progress(i + 1);
        }
        /* Success */
        set_diag0(readl(DATA_BUF));     /* first word of data */
        set_state(0xAA);
        return;
    }

    if (cmd == CMD_WRITE) {
        set_state(0x20);        /* writing */
        for (i = 0; i < count; i++) {
            unsigned int *buf = (unsigned int *)(DATA_BUF + i * 512);
            rc = write_sector(sector + i, buf);
            if (rc < 0) {
                set_diag0(sector + i);
                set_diag1((unsigned int)(-rc));
                report_error(0xC0 + (i & 0x0F));
                return;
            }
            set_progress(i + 1);
        }
        /* Verify: read back first sector and compare first word */
        {
            unsigned int verify_buf[128];
            if (read_sector(sector, verify_buf) == 0) {
                set_diag0(verify_buf[0]);
                set_diag1(readl(DATA_BUF));     /* original first word */
            }
        }
        set_state(0xAA);
        return;
    }

    if (cmd == CMD_BOOT_READ) {
        /* Switch to eMMC Boot Partition 1 via EXT_CSD[179] PARTITION_CONFIG.
         * CRITICAL: only modify PARTITION_ACCESS bits [2:0], preserving
         * BOOT_PARTITION_ENABLE [5:3] and BOOT_ACK [6] which are non-volatile.
         * Writing 0 to those bits permanently disables eMMC boot! */
        unsigned int caller_cfg = readl(CMD_ADDR + 0x0C);  /* flags field */
        unsigned int cur_cfg = (caller_cfg != 0) ? (caller_cfg & 0xF8) : 0x48;
        unsigned int part_cfg_switch = cur_cfg | 0x01;  /* set ACCESS=boot partition 1 */
        unsigned int part_cfg_restore = cur_cfg | 0x00;  /* set ACCESS=user area */
        if (smhc_cmd(6, 0x03B30000 | (part_cfg_switch << 8), BIT(8) | BIT(6), 0) < 0) {
            report_error(0xB0);     /* partition switch failed */
            return;
        }
        udelay(1000);

        set_state(0x10);    /* reading */
        for (i = 0; i < count; i++) {
            unsigned int *buf = (unsigned int *)(DATA_BUF + i * 512);
            rc = read_sector(sector + i, buf);
            if (rc < 0) {
                /* Switch back to User Data before returning */
                smhc_cmd(6, 0x03B30000 | (part_cfg_restore << 8), BIT(8) | BIT(6), 0);
                set_diag0(sector + i);
                set_diag1((unsigned int)(-rc));
                report_error(0xD0 + (i & 0x0F));
                return;
            }
            set_progress(i + 1);
        }

        /* Switch back to User Data Area */
        smhc_cmd(6, 0x03B30000 | (part_cfg_restore << 8), BIT(8) | BIT(6), 0);
        udelay(1000);

        set_diag0(readl(DATA_BUF));
        set_state(0xAA);
        return;
    }

    if (cmd == CMD_RPMB_READ) {
        /*
         * RPMB read using proper JEDEC protocol.
         * CMD17 is ILLEGAL in RPMB partition -- must use:
         *   CMD23 (set block count) + CMD25 (write request frame)
         *   CMD23 (set block count) + CMD18 (read response frame)
         *
         * 'sector' field: RPMB block address (or 0 for write counter)
         * 'count' field:  request type:
         *   2 = GET_WRITE_COUNTER (no auth needed)
         *   4 = READ_DATA (no auth needed for read, just no MAC verify)
         */
        unsigned int req_type = (count == 0) ? 2 : count;
        unsigned int rpmb_addr = sector;

        /* Switch to RPMB partition */
        if (smhc_cmd(6, 0x03B30300, BIT(8) | BIT(6), 0) < 0) {
            report_error(0xB3);     /* RPMB partition switch failed */
            return;
        }
        udelay(10000);

        /* Build request frame (512 bytes, mostly zero) */
        unsigned char *frame = (unsigned char *)DATA_BUF;
        for (i = 0; i < 512; i++) frame[i] = 0;
        frame[504] = (rpmb_addr >> 8) & 0xFF;
        frame[505] = rpmb_addr & 0xFF;
        frame[510] = (req_type >> 8) & 0xFF;
        frame[511] = req_type & 0xFF;

        /* Step 1: CMD23 (SET_BLOCK_COUNT) = 1, reliable write for request */
        if (smhc_cmd(23, (1u << 31) | 1, BIT(8) | BIT(6), 0) < 0) {
            smhc_cmd(6, 0x03B30000, BIT(8) | BIT(6), 0);
            report_error(0xA1);
            return;
        }

        /* Step 2: CMD25 (WRITE_MULTIPLE_BLOCK) -- send request frame */
        writel(0xFFFFFFFF, SMHC_RINT);
        setbits(SMHC_GCTRL, BIT(31));
        writel(512, SMHC_BLKSIZ);
        writel(512, SMHC_BYTCNT);
        writel(0, SMHC_CMDARG);
        writel(BIT(31) | BIT(13) | BIT(10) | BIT(9) | BIT(8) | BIT(6) | 25, SMHC_CMD);

        /* Wait for CMD_DONE */
        for (i = 0; i < 500000; i++)
            if (readl(SMHC_RINT) & RINT_CMD_DONE) break;

        /* Fill FIFO with request frame */
        {
            unsigned int *fbuf = (unsigned int *)DATA_BUF;
            for (i = 0; i < 128; i++)
                writel(fbuf[i], SMHC_FIFO);
        }

        /* Wait for DATA_OVER */
        for (i = 0; i < 2000000; i++)
            if (readl(SMHC_RINT) & (RINT_DATA_OVER | RINT_DATA_TMO)) break;

        if (readl(SMHC_RINT) & RINT_DATA_TMO) {
            smhc_cmd(6, 0x03B30000, BIT(8) | BIT(6), 0);
            report_error(0xA2);
            return;
        }

        /* Reset FIFO between write and read */
        setbits(SMHC_GCTRL, BIT(1));
        udelay(1000);
        setbits(SMHC_GCTRL, BIT(31));

        /* Step 3: CMD23 = 1 for read (no reliable write flag) */
        if (smhc_cmd(23, 1, BIT(8) | BIT(6), 0) < 0) {
            smhc_cmd(6, 0x03B30000, BIT(8) | BIT(6), 0);
            report_error(0xA3);
            return;
        }

        /* Step 4: CMD18 (READ_MULTIPLE_BLOCK) -- read response frame */
        writel(0xFFFFFFFF, SMHC_RINT);
        setbits(SMHC_GCTRL, BIT(31));
        writel(512, SMHC_BLKSIZ);
        writel(512, SMHC_BYTCNT);
        writel(0, SMHC_CMDARG);
        writel(BIT(31) | BIT(13) | BIT(9) | BIT(8) | BIT(6) | 18, SMHC_CMD);

        /* Wait for DATA_OVER */
        for (i = 0; i < 2000000; i++)
            if (readl(SMHC_RINT) & (RINT_DATA_OVER | RINT_DATA_TMO)) break;

        if (readl(SMHC_RINT) & RINT_DATA_OVER) {
            /* Drain response frame */
            setbits(SMHC_GCTRL, BIT(31));
            unsigned int *rbuf = (unsigned int *)DATA_BUF;
            for (i = 0; i < 128; i++)
                rbuf[i] = readl(SMHC_FIFO);

            /* Extract write counter (big-endian at bytes 500-503) */
            unsigned int wc = (frame[500] << 24) | (frame[501] << 16) |
                              (frame[502] << 8) | frame[503];
            set_diag0(wc);
            /* Result code at bytes 508-509 */
            set_diag1((frame[508] << 8) | frame[509]);
        } else {
            smhc_cmd(6, 0x03B30000, BIT(8) | BIT(6), 0);
            report_error(0xA4);
            return;
        }

        /* Switch back to User Data */
        smhc_cmd(6, 0x03B30000, BIT(8) | BIT(6), 0);
        udelay(10000);

        /* Reset FIFO */
        setbits(SMHC_GCTRL, BIT(1));
        udelay(100);
        setbits(SMHC_GCTRL, BIT(31));

        set_state(0xAA);
        return;
    }

    if (cmd == CMD_FIX_EXTCSD) {
        /* Write a byte to EXT_CSD.
         * 'sector' field = EXT_CSD index (e.g. 179 for PARTITION_CONFIG)
         * 'count' field = value to write
         *
         * CMD6 SWITCH returns R1b (busy on DAT0). Must wait for busy clear
         * before returning, otherwise the write may not commit to non-volatile
         * storage before BROM regains control.
         */
        unsigned int index = sector;
        unsigned int value = count;
        unsigned int arg = (0x03 << 24) | (index << 16) | (value << 8);
        unsigned int resp;

        if (smhc_cmd(6, arg, BIT(8) | BIT(6), &resp) < 0) {
            report_error(0xE6);     /* CMD6 failed */
            return;
        }

        /* Wait for card busy (R1b) */
        for (i = 0; i < 5000000; i++)
            if (!(readl(SMHC_STATUS) & STATUS_CARD_BUSY))
                break;

        /* CMD13: SEND_STATUS -- verify no SWITCH error (bit 7 of R1) */
        if (smhc_cmd(13, 1 << 16, BIT(8) | BIT(6), &resp) == 0) {
            if (resp & BIT(7)) {
                set_diag0(0xBAD6);      /* SWITCH_ERROR in card status */
                set_diag1(resp);
                report_error(0xE7);
                return;
            }
        }

        /* Read back EXT_CSD via CMD8 to verify the write took effect */
        {
            writel(0xFFFFFFFF, SMHC_RINT);
            writel(512, SMHC_BLKSIZ);
            writel(512, SMHC_BYTCNT);
            writel(0, SMHC_CMDARG);
            writel(BIT(31) | BIT(9) | BIT(8) | BIT(6) | 8, SMHC_CMD);
            for (i = 0; i < 2000000; i++)
                if (readl(SMHC_RINT) & (RINT_DATA_OVER | RINT_DATA_TMO))
                    break;
            if (readl(SMHC_RINT) & RINT_DATA_OVER) {
                setbits(SMHC_GCTRL, BIT(31));
                unsigned int *ext = (unsigned int *)(DATA_BUF);
                for (i = 0; i < 128; i++)
                    ext[i] = readl(SMHC_FIFO);
                setbits(SMHC_GCTRL, BIT(1));
                udelay(100);
                setbits(SMHC_GCTRL, BIT(31));

                /* Check the byte we just wrote */
                unsigned char *extb = (unsigned char *)(DATA_BUF);
                unsigned int readback = extb[index];
                set_diag0(readback);        /* actual value read back */
                set_diag1(value);           /* expected value */
                if (readback != value) {
                    report_error(0xE8);     /* verify failed */
                    return;
                }
            } else {
                /* EXT_CSD readback failed, but write might have worked */
                set_diag0(index);
                set_diag1(value);
            }
        }

        set_state(0xAA);
        return;
    }
}
