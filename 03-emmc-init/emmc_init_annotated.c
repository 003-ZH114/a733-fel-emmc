/*
 * A733 (sun60iw2p1) eMMC 初始化 —— 带注释摘录
 * 完整可编译源码见 fel-emmc-a733.c
 *
 * 运行环境：FEL 模式，非安全态，代码在 SRAM 0x57000，无 DRAM。
 */

#define BIT(x)          (1u << (x))
#define writel(v, a)    do { __asm__ volatile("dsb sy" ::: "memory"); \
                             (*(volatile unsigned int *)(a) = (v)); } while (0)
#define readl(a)        ({ unsigned int __v = (*(volatile unsigned int *)(a)); \
                            __asm__ volatile("dmb sy" ::: "memory"); __v; })
#define setbits(a, v)   writel(readl(a) | (v), (a))
#define clrbits(a, v)   writel(readl(a) & ~(v), (a))

/* ---- SMHC2 控制器 ---- */
#define SMHC2_BASE      0x04022000
#define S(off)          (SMHC2_BASE + (off))
#define SMHC_GCTRL      S(0x00)     /* 全局控制：bit0 软复位, bit1 FIFO, bit2 DMA */
#define SMHC_CLKCR      S(0x04)     /* 时钟：分频 + CLK_EN(bit16) + MASK_D0(bit31) */
#define SMHC_TMOUT      S(0x08)
#define SMHC_WIDTH      S(0x0C)     /* 0=1bit, 1=4bit, 2=8bit */
#define SMHC_CMD        S(0x18)     /* bit31 START, bit15 SEND_INIT_SEQ */
#define SMHC_CMDARG     S(0x1C)
#define SMHC_RESP0      S(0x20)
#define SMHC_RINT       S(0x38)
#define SMHC_HWRST      S(0x78)
#define SMHC_FIFO       S(0x200)

/* ---- PIO V3 (NCAT3)：PC 口 ---- */
#define PC_CFG0         0x02000180  /* PC0-PC7   */
#define PC_CFG1         0x02000184  /* PC8-PC15  */
#define PC_CFG2         0x02000188  /* PC16-PC23 */
#define PC_DRV0         0x020001A0
#define PC_DRV1         0x020001A4
#define PC_DRV2         0x020001A8
#define PC_PULL         0x020001B0

/* ---- CCU：SMHC2 时钟与门控 ---- */
#define CCU_SMHC2_CLK   0x02002D20  /* bit31 gate, mux[26:24], div M/P */
#define CCU_SMHC2_GATE  0x02002D2C  /* bit0 gate, bit16 reset */

static void udelay(unsigned int us) { volatile unsigned int i; for (i = 0; i < us * 10; i++); }

static int smhc_update_clk(void)
{
    unsigned int i;
    writel(BIT(31) | BIT(21), SMHC_CMD);       /* START | UPCLK_ONLY */
    for (i = 0; i < 100000; i++)
        if (!(readl(SMHC_CMD) & BIT(31)))
            return 0;
    return -1;
}

static int emmc_init(unsigned int *cid)
{
    unsigned int resp, i;

    /* ============================================================
     * 1. PIO：把 PC 口复用为 eMMC (sdc2, function 3)
     *    用到的引脚：PC0,1,5,6,8-11,13-16
     *    未用引脚写 0xf（PC2/3/4/7/12）
     * ============================================================ */
    writel(0xf33fff33, PC_CFG0);   /* PC0,1,5,6 = 3；其余未用 = f */
    writel(0x333f3333, PC_CFG1);   /* PC8-11,13-15 = 3；PC12 = f */
    writel(0x00000003, PC_CFG2);   /* PC16 = 3 */
    writel(0x11111111, PC_DRV0);   /* 驱动强度 */
    writel(0x11111111, PC_DRV1);
    writel(0x11111111, PC_DRV2);
    writel(0x54555946, PC_PULL);   /* 上下拉：大部分上拉 */

    /* ============================================================
     * 2. CCU：SMHC2 模块时钟
     *    mux[26:24]=0 -> sys24M，M/P 分频=0 -> 24MHz，
     *    bit31=1 -> 时钟使能
     * ============================================================ */
    writel(0x80000000, CCU_SMHC2_CLK);

    /* ============================================================
     * 3. CCU：SMHC2 门控/复位序列
     *    先 assert reset + gate off，再 gate on + deassert
     * ============================================================ */
    clrbits(CCU_SMHC2_GATE, BIT(16));   /* assert reset */
    clrbits(CCU_SMHC2_GATE, BIT(0));    /* gate off */
    udelay(10);
    setbits(CCU_SMHC2_GATE, BIT(0));    /* gate on */
    setbits(CCU_SMHC2_GATE, BIT(16));   /* deassert reset */
    udelay(100);

    /* ============================================================
     * 4. SMHC2 控制器复位/基础配置
     * ============================================================ */
    writel(BIT(0) | BIT(1) | BIT(2), SMHC_GCTRL);   /* 软复位 + FIFO + DMA */
    for (i = 0; i < 100000; i++)
        if (!(readl(SMHC_GCTRL) & (BIT(0) | BIT(1) | BIT(2))))
            break;

    /* BROM 可能把控制器留在 8-bit；软复位不会清 WIDTH，必须显式写 0 */
    writel(0, SMHC_WIDTH);

    writel(0x00000003, S(0x54));                     /* csdc：CRC status detect */
    writel(0x00000deb, S(0x50));                     /* dbgc：调试使能 */
    writel((512 << 16) | BIT(2) | BIT(0), S(0x100)); /* thldc：阈值 512B */
    writel(BIT(7), S(0x144));                        /* samp_dl：采样延迟链 */

    /* ============================================================
     * 5. 初始时钟 400kHz：24MHz / (2*(29+1))
     *    bit31 MASK_D0 在改时钟时屏蔽 DAT0，防止总线冲突
     * ============================================================ */
    writel(29 | BIT(31), SMHC_CLKCR);
    if (smhc_update_clk()) return -1;
    writel(29 | BIT(16) | BIT(31), SMHC_CLKCR);     /* + CLK_EN */
    if (smhc_update_clk()) return -1;
    clrbits(SMHC_CLKCR, BIT(31));                    /* 取消 MASK_D0 */
    if (smhc_update_clk()) return -1;

    writel(0xFFFFFFFF, SMHC_TMOUT);
    writel(0, SMHC_IMASK);

    /* ============================================================
     * 6. 硬复位 + 80 个初始化时钟
     * ============================================================ */
    writel(0, SMHC_HWRST);
    udelay(1000);
    writel(1, SMHC_HWRST);
    udelay(1000);

    writel(BIT(31) | BIT(15), SMHC_CMD);             /* START | SEND_INIT_SEQ */
    for (i = 0; i < 100000; i++)
        if (!(readl(SMHC_CMD) & BIT(31)))
            break;
    udelay(1000);

    /* ============================================================
     * 7. 卡片初始化命令
     * ============================================================ */
    smhc_cmd(0, 0, BIT(15), 0);                      /* CMD0 GO_IDLE_STATE */

    for (i = 0; i < 1000; i++) {                     /* CMD1 SEND_OP_COND */
        if (!smhc_cmd(1, 0x40FF8080, BIT(6), &resp) && (resp & BIT(31)))
            break;
        udelay(1000);
    }
    if (i >= 1000) return -2;

    smhc_cmd(2, 0, BIT(8) | BIT(7) | BIT(6), 0);     /* CMD2 ALL_SEND_CID */
    if (cid) {
        cid[0] = readl(SMHC_RESP0);
        cid[1] = readl(SMHC_RESP0 + 4);
        cid[2] = readl(SMHC_RESP0 + 8);
        cid[3] = readl(SMHC_RESP0 + 12);
    }
    smhc_cmd(3, 1 << 16, BIT(8) | BIT(6), 0);        /* CMD3 SET_RELATIVE_ADDR (RCA=1) */
    smhc_cmd(7, 1 << 16, BIT(8) | BIT(6), 0);        /* CMD7 SELECT_CARD */

    /* 先强制回 1-bit，兼容 BROM 启动时可能留下的 4/8-bit 状态 */
    smhc_cmd(6, 0x03B70000, BIT(8) | BIT(6), 0);
    for (i = 0; i < 500000; i++)
        if (!(readl(SMHC_STATUS) & BIT(9)))
            break;
    udelay(1000);

    /* ============================================================
     * 8. 提速：div=0 -> 24MHz / 2 = 12MHz
     *    失败则回退到 400kHz/1-bit
     * ============================================================ */
    {
        int ok = 0;
        writel(BIT(31), SMHC_CLKCR);                 /* MASK_D0 */
        if (smhc_update_clk() == 0) {
            writel(0 | BIT(31), SMHC_CLKCR);         /* div=0 */
            if (smhc_update_clk() == 0) {
                writel(0 | BIT(16) | BIT(31), SMHC_CLKCR);
                if (smhc_update_clk() == 0) {
                    unsigned int samp = readl(S(0x144));
                    samp &= ~0x3F;
                    samp |= BIT(7);
                    writel(samp, S(0x144));
                    clrbits(SMHC_CLKCR, BIT(31));
                    if (smhc_update_clk() == 0) ok = 1;
                }
            }
        }
        if (!ok) {
            writel(0, SMHC_WIDTH);
            writel(29, SMHC_CLKCR);
            smhc_update_clk();
            setbits(SMHC_CLKCR, BIT(16));
            smhc_update_clk();
        }
    }

    return 0;
}
