# 02-clock —— PIO 引脚复用与 SMHC2 时钟

## 初始化顺序

1. **PIO**：PC0、PC1、PC5、PC6、PC8-PC11、PC13-PC16 复用为 `sdc2`（mux=3）；
   未使用引脚 PC2/3/4/7/12 写 `0xf`。驱动强度 `0x11111111`，上下拉 `0x54555946`。
2. **CCU 模块时钟**：`0x02002D20 = 0x80000000`
   - bit31 = 1：时钟门控打开
   - mux[26:24] = 0：源为 24MHz HOSC
   - M/P 分频 = 0：24MHz
3. **CCU gate/reset**：`0x02002D2C`
   - 清 bit16（assert reset）、清 bit0（gate off）
   - 延时后置 bit0（gate on）、置 bit16（deassert reset）
4. **控制器分频**：`SMHC_CLKCR`
   - 400kHz：`div=29`，即 `24MHz / (2*(29+1))`
   - 12MHz：`div=0`，即 `24MHz / 2`
   - 改分频期间置 bit31 `MASK_D0`，防止 DAT0 总线冲突

## API

```c
void a733_udelay(unsigned int us);
int  a733_smhc_update_clk(void);                 /* START | UPCLK_ONLY */
void a733_emmc_pio_init(void);
void a733_emmc_ccu_init(void);
int  a733_smhc_clock_set(unsigned int div, int enable);
int  a733_smhc_clock_fast(void);                 /* 12MHz + 采样延迟 */
```
