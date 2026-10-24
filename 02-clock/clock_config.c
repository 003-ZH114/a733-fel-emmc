/*
 * A733 (sun60iw2p1) -- eMMC pin mux and clock configuration
 *
 * Extracted from the proven FEL payload (04-payload/fel-emmc-a733.c).
 *
 * Initial speed: 24MHz HOSC / (2 * (29 + 1)) = 400kHz
 * Fast speed:    24MHz HOSC / 2               = 12MHz (1-bit SDR)
 */

#include "clock_config.h"

void a733_udelay(unsigned int us)
{
    volatile unsigned int i;
    for (i = 0; i < us * 10; i++)
        ;
}

int a733_smhc_update_clk(void)
{
    unsigned int i;

    writel(BIT(31) | BIT(21), SMHC_CMD);    /* START | UPCLK_ONLY */
    for (i = 0; i < 100000; i++)
        if (!(readl(SMHC_CMD) & BIT(31)))
            return 0;
    return -1;
}

void a733_emmc_pio_init(void)
{
    /* PC0, PC1, PC5, PC6 -> sdc2; unused PC2/3/4/7 = 0xf */
    writel(0xf33fff33, PC_CFG0);
    /* PC8-PC11, PC13-PC15 -> sdc2; unused PC12 = 0xf */
    writel(0x333f3333, PC_CFG1);
    /* PC16 -> sdc2 */
    writel(0x00000003, PC_CFG2);

    /* Drive strength: 0x1 = default for every pin */
    writel(0x11111111, PC_DRV0);
    writel(0x11111111, PC_DRV1);
    writel(0x11111111, PC_DRV2);

    /* Pull-up/down configuration for the PC bank */
    writel(0x54555946, PC_PULL);
}

void a733_emmc_ccu_init(void)
{
    /* mux[26:24] = 0 (sys24M), M/P divider = 0, bit31 = gate on */
    writel(0x80000000, CCU_SMHC2_CLK);

    /* Gate/reset pulse: assert reset + gate off, then gate on + deassert */
    clrbits(CCU_SMHC2_GATE, BIT(16));   /* assert reset */
    clrbits(CCU_SMHC2_GATE, BIT(0));    /* gate off     */
    a733_udelay(10);
    setbits(CCU_SMHC2_GATE, BIT(0));    /* gate on      */
    setbits(CCU_SMHC2_GATE, BIT(16));   /* deassert reset */
    a733_udelay(100);
}

int a733_smhc_clock_set(unsigned int div, int enable)
{
    /* MASK_D0 (bit31) is held while the divider changes to avoid bus
     * contention on DAT0. */
    writel(div | BIT(31), SMHC_CLKCR);
    if (a733_smhc_update_clk())
        return -1;

    writel(div | (enable ? BIT(16) : 0) | BIT(31), SMHC_CLKCR);
    if (a733_smhc_update_clk())
        return -1;

    clrbits(SMHC_CLKCR, BIT(31));       /* release MASK_D0 */
    if (a733_smhc_update_clk())
        return -1;

    return 0;
}

int a733_smhc_clock_fast(void)
{
    unsigned int samp;

    writel(BIT(31), SMHC_CLKCR);                    /* mask DAT0, div -> 0 */
    if (a733_smhc_update_clk())
        return -1;

    writel(0 | BIT(31), SMHC_CLKCR);                /* 24MHz / 2 = 12MHz */
    if (a733_smhc_update_clk())
        return -1;

    writel(0 | BIT(16) | BIT(31), SMHC_CLKCR);      /* + CLK_EN */
    if (a733_smhc_update_clk())
        return -1;

    /* Refresh the sample delay for the higher clock rate */
    samp = readl(SMHC_SAMP_DL);
    samp &= ~0x3F;
    samp |= BIT(7);
    writel(samp, SMHC_SAMP_DL);

    clrbits(SMHC_CLKCR, BIT(31));
    if (a733_smhc_update_clk())
        return -1;

    return 0;
}
