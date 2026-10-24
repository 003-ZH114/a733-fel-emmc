/*
 * A733 (sun60iw2p1) -- eMMC / SMHC2 initialization
 *
 * Standalone extraction of emmc_init() from 04-payload/fel-emmc-a733.c.
 * Pin mux and clock programming live in 02-clock/clock_config.c.
 *
 * Initialization order:
 *   1. PC0..PC16 -> sdc2, drive strength, pulls
 *   2. CCU module clock + gate/reset
 *   3. Controller soft reset, force 1-bit, vendor timing registers
 *   4. 400kHz clock
 *   5. Hardware reset + 80 initialization clocks
 *   6. CMD0, CMD1, CMD2, CMD3, CMD7, CMD6 (1-bit)
 *   7. Optional 12MHz (and 4/8-bit) speed upgrade with fallback
 */

#include "clock_config.h"
#include "emmc_init.h"

/* Raw interrupt flags */
#define RINT_RESP_ERR   BIT(1)
#define RINT_CMD_DONE   BIT(2)
#define RINT_DATA_OVER  BIT(3)
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

#define STATUS_CARD_BUSY BIT(9)

/*
 * Send a command to the card.
 *   ci   : command index (0-63)
 *   arg  : command argument
 *   fl   : extra CMD register flags (RESP_EXPIRE, CHK_CRC, LONG_RESPONSE...)
 *   resp : receives RESP0 when non-NULL
 * Returns 0 on success, -1 on error, -2 on timeout.
 */
static int smhc_cmd(unsigned int ci, unsigned int arg, unsigned int fl,
                    unsigned int *resp)
{
    unsigned int i, ri;

    writel(0xFFFFFFFF, SMHC_RINT);
    writel(arg, SMHC_CMDARG);
    writel(BIT(31) | fl | (ci & 0x3F), SMHC_CMD);

    for (i = 0; i < 500000; i++) {
        ri = readl(SMHC_RINT);
        if (ri & (RINT_CMD_DONE | RINT_RESP_TMO | RINT_FIFO_ERR | RINT_HW_LOCKED)) {
            if (resp)
                *resp = readl(SMHC_RESP0);
            if (ri & RINT_ERR_MASK) {
                /* Reset the controller to clear a stuck state. */
                writel(BIT(0) | BIT(1) | BIT(2), SMHC_GCTRL);
                for (i = 0; i < 100000; i++)
                    if (!(readl(SMHC_GCTRL) & (BIT(0) | BIT(1) | BIT(2))))
                        break;
                writel(0, SMHC_WIDTH);      /* re-force 1-bit after reset */
                return -1;
            }
            return 0;
        }
    }
    return -2;
}

int emmc_init(unsigned int *cid)
{
    unsigned int resp, i;

    /* 1. Pin mux + 2. CCU module clock / gate / reset */
    a733_emmc_pio_init();
    a733_emmc_ccu_init();

    /* 3. Controller reset */
    writel(BIT(0) | BIT(1) | BIT(2), SMHC_GCTRL);
    for (i = 0; i < 100000; i++)
        if (!(readl(SMHC_GCTRL) & (BIT(0) | BIT(1) | BIT(2))))
            break;

    /* Software reset does not clear WIDTH: force 1-bit because the BROM
     * may have left the controller in 8-bit mode. */
    writel(0, SMHC_WIDTH);

    /* Vendor timing registers */
    writel(0x00000003, SMHC_CSDC);
    writel(0x00000deb, SMHC_DBGC);
    writel((512 << 16) | BIT(2) | BIT(0), SMHC_THLDC);
    writel(BIT(7), SMHC_SAMP_DL);

    /* 4. Initial clock: 24MHz / (2 * (29 + 1)) = 400kHz */
    if (a733_smhc_clock_set(29, 1))
        return -1;

    writel(0xFFFFFFFF, SMHC_TMOUT);
    writel(0, SMHC_IMASK);                  /* polling mode */

    /* 5. Hardware reset pulse + 80 initialization clocks */
    writel(0, SMHC_HWRST);
    a733_udelay(1000);
    writel(1, SMHC_HWRST);
    a733_udelay(1000);

    writel(BIT(31) | BIT(15), SMHC_CMD);    /* START | SEND_INIT_SEQ */
    for (i = 0; i < 100000; i++)
        if (!(readl(SMHC_CMD) & BIT(31)))
            break;
    a733_udelay(1000);

    /* 6. Card initialization */
    smhc_cmd(0, 0, BIT(15), 0);             /* CMD0 GO_IDLE_STATE */

    for (i = 0; i < 1000; i++) {            /* CMD1 SEND_OP_COND */
        if (!smhc_cmd(1, 0x40FF8080, BIT(6), &resp) && (resp & BIT(31)))
            break;
        a733_udelay(1000);
    }
    if (i >= 1000)
        return -2;

    smhc_cmd(2, 0, BIT(8) | BIT(7) | BIT(6), 0);   /* CMD2 ALL_SEND_CID */
    if (cid) {
        cid[0] = readl(SMHC_RESP0);
        cid[1] = readl(SMHC_RESP1);
        cid[2] = readl(SMHC_RESP2);
        cid[3] = readl(SMHC_RESP3);
    }

    smhc_cmd(3, 1 << 16, BIT(8) | BIT(6), 0);      /* CMD3 SET_RELATIVE_ADDR */
    smhc_cmd(7, 1 << 16, BIT(8) | BIT(6), 0);      /* CMD7 SELECT_CARD */

    /* Always return the card to 1-bit first: the BROM may have left it
     * in 4/8-bit mode while our controller reads DAT0 only. */
    smhc_cmd(6, 0x03B70000, BIT(8) | BIT(6), 0);   /* CMD6 1-bit SDR */
    for (i = 0; i < 500000; i++)
        if (!(readl(SMHC_STATUS) & STATUS_CARD_BUSY))
            break;
    a733_udelay(1000);

#ifndef SPEED_1BIT_FAST
    /* Optional wider bus.  Build with -DSPEED_4BIT_FAST (default) or
     * -DSPEED_8BIT_FAST. */
#ifdef SPEED_8BIT_FAST
    if (smhc_cmd(6, 0x03B70200, BIT(8) | BIT(6), 0) < 0)
#else
    if (smhc_cmd(6, 0x03B70100, BIT(8) | BIT(6), 0) < 0)
#endif
        return -3;

    for (i = 0; i < 500000; i++)            /* R1b busy wait */
        if (!(readl(SMHC_STATUS) & STATUS_CARD_BUSY))
            break;
    a733_udelay(1000);
#endif

    /* 7. Speed upgrade to 12MHz, fall back to 400kHz/1-bit on failure. */
    {
        int speed_ok = 0;

        writel(BIT(31), SMHC_CLKCR);        /* MASK_D0 */
        if (a733_smhc_update_clk() == 0) {
#ifndef SPEED_1BIT_FAST
#ifdef SPEED_8BIT_FAST
            writel(2, SMHC_WIDTH);          /* 8-bit */
#else
            writel(1, SMHC_WIDTH);          /* 4-bit */
#endif
#endif
            if (a733_smhc_clock_fast() == 0)
                speed_ok = 1;
        }

        if (!speed_ok) {
            writel(0, SMHC_WIDTH);
            writel(29, SMHC_CLKCR);
            a733_smhc_update_clk();
            setbits(SMHC_CLKCR, BIT(16));
            a733_smhc_update_clk();
        }
    }

    return 0;
}
