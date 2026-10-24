

#ifndef A733_CLOCK_CONFIG_H
#define A733_CLOCK_CONFIG_H

#define BIT(x)          (1u << (x))


#define writel(v, a)    do { __asm__ volatile("dsb sy" ::: "memory"); \
                             (*(volatile unsigned int *)(a) = (v)); } while (0)
#define readl(a)        ({ unsigned int __v = (*(volatile unsigned int *)(a)); \
                            __asm__ volatile("dmb sy" ::: "memory"); __v; })
#define setbits(a, v)   writel(readl(a) | (v), (a))
#define clrbits(a, v)   writel(readl(a) & ~(v), (a))


#define SMHC2_BASE      0x04022000
#define S(off)          (SMHC2_BASE + (off))

#define SMHC_GCTRL      S(0x00)     
#define SMHC_CLKCR      S(0x04)     
#define SMHC_TMOUT      S(0x08)
#define SMHC_WIDTH      S(0x0C)     
#define SMHC_BLKSIZ     S(0x10)
#define SMHC_BYTCNT     S(0x14)
#define SMHC_CMD        S(0x18)     
#define SMHC_CMDARG     S(0x1C)
#define SMHC_RESP0      S(0x20)
#define SMHC_RESP1      S(0x24)
#define SMHC_RESP2      S(0x28)
#define SMHC_RESP3      S(0x2C)
#define SMHC_IMASK      S(0x30)
#define SMHC_RINT       S(0x38)
#define SMHC_STATUS     S(0x3C)
#define SMHC_HWRST      S(0x78)
#define SMHC_FIFO       S(0x200)


#define SMHC_CSDC       S(0x54)    
#define SMHC_DBGC       S(0x50)     
#define SMHC_THLDC      S(0x100)    
#define SMHC_SAMP_DL    S(0x144)   


#define PC_CFG0         0x02000180  /* PC0-PC7   */
#define PC_CFG1         0x02000184  /* PC8-PC15  */
#define PC_CFG2         0x02000188  /* PC16-PC23 */
#define PC_DRV0         0x020001A0
#define PC_DRV1         0x020001A4
#define PC_DRV2         0x020001A8
#define PC_PULL         0x020001B0

/* ---- CCU (Clock Control Unit), CCU base 0x02002000 ---- */
#define CCU_SMHC2_CLK   0x02002D20  /* bit31 gate, mux[26:24], M/P divider */
#define CCU_SMHC2_GATE  0x02002D2C  /* bit0 gate, bit16 reset */

/* ---- Watchdog (disabled before long eMMC operations) ---- */
#define WDT_MODE0       0x02050018
#define WDT_MODE1       0x020500B8

/* ---- RTC general-purpose registers: survive across FEL exec calls ---- */
#define RTC_GP(n)       (0x07090100 + (n) * 4)

/* ---- Clock / timing ---------------------------------------------- */

/* Busy-wait delay; calibration is approximate and only used for resets. */
void a733_udelay(unsigned int us);

/* Commit a pending CLKCR change through START | UPCLK_ONLY. */
int a733_smhc_update_clk(void);

/* Route PC0..PC16 to sdc2 and set drive strength / pull resistors. */
void a733_emmc_pio_init(void);

/* SMHC2 module clock (mux = 24MHz HOSC, divider) + gate/reset pulse. */
void a733_emmc_ccu_init(void);

/*
 * Program CLKCR and commit it.
 *   div    : divider field (24MHz / (2 * (div + 1)) for the HOSC path)
 *   enable : 1 = CLK_EN set, 0 = clock stopped
 * Returns 0 on success, -1 if the controller never acknowledges.
 */
int a733_smhc_clock_set(unsigned int div, int enable);

/*
 * Switch to the fast 12MHz/1-bit configuration (divider 0) and refresh
 * the sample delay.  Returns 0 on success, -1 on failure (caller falls
 * back to 400kHz).
 */
int a733_smhc_clock_fast(void);

#endif /* A733_CLOCK_CONFIG_H */
