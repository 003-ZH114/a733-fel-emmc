# Allwinner A733 (sun60iw2p1) FEL 裸机 eMMC 初始化 / 读写 payload

在 FEL 模式下把一小段 ARM 代码放进 SRAM，直接驱动 SMHC2 控制器读写 eMMC。
不依赖 DRAM、不依赖 boot0、不改写设备固件。

设备实测：星学伴 A733 Pro3（`xingxueban/a733_pro3_arm64`，Android 15）。

## 1. 内存布局（SRAM）

| 地址 | 用途 |
|------|------|
| `0x57000` | payload 代码（本包 `fel.bin`，约 6KB） |
| `0x5AFF0` | 16 字节命令块（主机每块写入） |
| `0x5B000` | 数据缓冲（最大 512 扇区 = 256KB） |
| `0xA0000` | 栈顶（向下增长） |
| `0xEE800` | BROM FEL scratchpad（不要占用） |

注意：A733 上 **`< 0x51000` 的 SRAM 写入会被 BROM 拒绝/卡死**（`0x47000` 是 BROM 受保护区）。

## 2. 关键寄存器

| 模块 | 地址 | 说明 |
|------|------|------|
| PIO CFG0/1/2 | `0x02000180/184/188` | PC0-7 / PC8-15 / PC16-23 复用 |
| PIO DRV0/1/2 | `0x020001A0/A4/A8` | 驱动强度 |
| PIO PULL | `0x020001B0` | PC bank 上下拉 |
| CCU SMHC2 CLK | `0x02002D20` | bit31 门控；mux[26:24]；M/P 分频 |
| CCU SMHC2 GATE/RST | `0x02002D2C` | bit0 门控，bit16 复位 |
| SMHC2 控制器 | `0x04022000` | GCTRL/CLKCR/WIDTH/CMD/FIFO… |
| RTC GP | `0x07090100 + n*4` | 跨 FEL exec 保存状态 |
| 看门狗 | `0x02050018/0x020500B8` | 关闭，避免长操作被复位 |

SMHC2 寄存器偏移：`GCTRL=0x00, CLKCR=0x04, TMOUT=0x08, WIDTH=0x0C, CMD=0x18, CMDARG=0x1C, RESP0=0x20, RINT=0x38, HWRST=0x78, FIFO=0x200`。

PIO 引脚（来自设备 DT）：PC0, PC1, PC5, PC6, PC8, PC9, PC10, PC11, PC13, PC14, PC15, PC16，功能 `sdc2`（mux=3）。

## 3. 初始化流程

1. **PIO**：PC 口复用为 `sdc2`，设置驱动强度/上下拉
   - `0x02000180 = 0xf33fff33`、`0x02000184 = 0x333f3333`、`0x02000188 = 0x3`
   - 未使用引脚写 `0xf`（PC2/3/4/7/12）
2. **CCU 时钟**：`0x02002d20 = 0x80000000`（gate on，mux=sys24M，分频=1）
3. **CCU 门控/复位**：`0x02002d2c` 清 bit16(复位)/bit0(门控)，延时后再置 bit0/bit16
4. **控制器复位**：`GCTRL=7`（软复位+FIFO+DMA），轮询清零；`WIDTH=0` 强制 1-bit
5. **400kHz**：`CLKCR=0x8000001d`（div=29），`UPCLK_ONLY` 更新，清 `MASK_D0`
6. **硬复位**：`HWRST=0→1`，发 80 个初始化时钟（`CMD=0x80008000`）
7. **卡片初始化**：CMD0 → CMD1(轮询 OCR) → CMD2(取 CID) → CMD3(RCA=1) → CMD7 → CMD6(1-bit SDR)
8. **提速**：`CLKCR` div=0（24MHz/2 = 12MHz），重新配置采样延迟；失败回退 400kHz
9. 返回 BROM FEL（恢复 BROM 的 SP）

## 4. 命令协议

主机在 `0x5AFF0` 写入 16 字节（小端）：

| 偏移 | 字段 |
|------|------|
| +0x00 | cmd：1=read，2=write，3=info，4=boot_read，5=rpmb_read，6=fix_extcsd |
| +0x04 | 起始 LBA |
| +0x08 | 扇区数（最大 512） |
| +0x0C | flags |

状态通过 RTC GP 读回：

| 寄存器 | 含义 |
|--------|------|
| GP0 | 0x01=init，0x02=card ready，0xAA=done，0xEE=error |
| GP1 | 进度/错误码 |
| GP2/GP3 | CID[0]/CID[1] 或诊断 |
| GP5 | `0xCAFEBABE` = 卡片已初始化（跨 exec 跳过重复初始化） |

## 5. 构建

```sh
./build.sh
# 生成 fel.bin（链接地址 0x57000）
```

## 6. 运行

```sh
xfel write 0x57000 fel.bin
# 命令块：cmd=1(read), lba=0, count=256
python3 -c "import struct;open('/tmp/cmd','wb').write(struct.pack('<IIII',1,0,256,0))"
xfel write 0x5AFF0 /tmp/cmd
xfel write32 0x07090100 0
xfel exec 0x57000
xfel read32 0x07090100        # 0xAA = 成功
xfel read 0x5B000 131072 out.bin
```

本包 `host/` 下提供了：
- `felclient.py`：pyusb 常驻 FEL 客户端（**不要调用 `set_configuration()`**，否则会把 BROM 搞挂）
- `dump.py` / `fastdump.py`：按分区/扇区批量导出

## 7. 已踩过的坑

- A733 BROM FEL 是 **USB 1.1 全速（12Mbps）**，实测约 300–350 KiB/s
- FEL `write` 到 `0x47000` 会卡死（BROM 受保护区）；可用区从 `0x51000` 起
- `smc #0` 在 A733 上不会切到安全态（H3/H5/A64 的 workaround 不适用）
- 大块客户端写入可能触发 BROM 异常；**代码用 xfel 上传，客户端只写 16 字节命令块**最稳
- 跨进程打开设备时不要 `libusb_set_configuration`，只 `detach + claim_interface`

## 8. 文件

```
bin/fel.bin                 编译好的 payload
bin/fel.elf                 ELF（含符号）
src/fel-emmc-a733.c         完整 C 源码
src/emmc_init_annotated.c   初始化函数带注释摘录
src/emmc_init_o0.dis        emmc_init 单独反汇编（-O0）
src/emmc_init_annotated.s   带注释的初始化汇编
src/payload.dis             整包 -O2 反汇编
src/a733.ld / build.sh      链接脚本 / 构建
host/                       主机侧 FEL 客户端与导出脚本
```
