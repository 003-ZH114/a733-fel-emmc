/*
 * A733 FEL init-only payload entry point.
 *
 * Called by main() in 01-startup/start.S.  Initializes the eMMC through
 * SMHC2 and leaves the result in the RTC general-purpose registers, which
 * survive after the payload returns to the BROM:
 *
 *   GP0  state: 0x01 = init started, 0xAA = success, 0xEE = error
 *   GP1  error sub-code (negated emmc_init return value) on failure
 *   GP2  CID[0] (or 0 when the CID was not read)
 *   GP3  CID[1]
 *   GP5  0xCAFEBABE = card initialized
 */

#include "clock_config.h"
#include "emmc_init.h"

void __attribute__((noinline)) work(void);

void __attribute__((noinline)) work(void)
{
    unsigned int cid[4];
    unsigned int i, rc;

    /* The BROM may leave a watchdog armed; long init would reset the SoC. */
    writel(0, WDT_MODE0);
    writel(0, WDT_MODE1);

    for (i = 0; i < 4; i++) {
        cid[i] = 0;
        writel(0, RTC_GP(i));
    }

    writel(0x01, RTC_GP(0));            /* init started */

    rc = emmc_init(cid);
    if (rc < 0) {
        writel((unsigned int)(-rc), RTC_GP(1));
        writel(cid[0], RTC_GP(2));
        writel(cid[1], RTC_GP(3));
        writel(0xEE, RTC_GP(0));
        return;
    }

    writel(cid[0], RTC_GP(2));
    writel(cid[1], RTC_GP(3));
    writel(0xCAFEBABE, RTC_GP(5));      /* mark "card initialized" */
    writel(0xAA, RTC_GP(0));            /* success */
}
