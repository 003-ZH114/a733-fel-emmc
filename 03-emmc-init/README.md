# 03-emmc-init —— eMMC 初始化

| 文件 | 说明 |
|------|------|
| `emmc_init.c/.h` | 完整初始化：控制器复位 → 时钟 → 硬复位 → CMD0/1/2/3/7/6 → 提速 |
| `init_entry.c` | 初始化 payload 入口 `work()`：关看门狗、调用 `emmc_init`、写 RTC GP 状态 |
| `emmc_init_annotated.c` | 初始化函数的带中文注释摘录 |
| `emmc_init_annotated.s` | `-O0` 反汇编，逐条注释初始化流程（非最终机器码） |
| `ref/emmc_init_o0.dis` | 同上，未注释反汇编备份 |

## 初始化流程

1. PIO 复用（`02-clock`）
2. CCU 模块时钟 + gate/reset（`02-clock`）
3. 控制器软复位 `GCTRL=7`，轮询到清零
4. 强制 1-bit：`WIDTH=0`（软复位不清 WIDTH，BROM 可能遗留 8-bit）
5. 厂商寄存器：`CSDC=3`、`DBGC=0xdeb`、`THLDC=(512<<16)|5`、`SAMP_DL=0x80`
6. 400kHz（`div=29`），超时全开，中断屏蔽（轮询）
7. 硬复位 `HWRST=0→1`，发 80 个初始化时钟
8. CMD0 → CMD1 轮询 OCR → CMD2 取 CID → CMD3（RCA=1）→ CMD7
9. CMD6 强制回到 1-bit SDR
10. 尝试 12MHz；失败回退 400kHz/1-bit

## 状态寄存器（RTC GP）

| 寄存器 | 含义 |
|--------|------|
| GP0 | `0x01` 开始初始化，`0xAA` 成功，`0xEE` 失败 |
| GP1 | 失败时的错误码（`-rc`） |
| GP2/GP3 | CID[0]/CID[1] |
| GP5 | `0xCAFEBABE` 表示卡片已初始化 |
