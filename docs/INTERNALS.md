# Allwinner A733 (sun60iw2p1) 底层逆向整理

本目录是对 A733 设备（`xingxueban/a733_pro3_arm64`，Android 15）的 FEL 裸机
逆向成果整理：在 FEL 模式下把一段 ARM 代码放入 SRAM，直接驱动 SMHC2 读写 eMMC，
不依赖 DRAM、不依赖 boot0。

整理原则：**可编译/可烧录的核心文件**按启动、时钟、初始化、完整 payload、主机工具
分类；抓取的设备镜像、日志、缓存、重复文件、旧构建产物统一放进 `99-junk/`。

---

## 1. 目录结构

| 目录 | 内容 | 性质 |
|------|------|------|
| `01-startup/` | 汇编启动 `start.S`、链接脚本 `a733.ld` | **核心** |
| `02-clock/` | PIO 引脚复用 + CCU 时钟配置 `clock_config.c/.h` | **核心** |
| `03-emmc-init/` | eMMC 初始化 `emmc_init.c/.h`、独立 init 入口、带注释反汇编 | **核心** |
| `04-payload/` | 完整读写 payload 源码、已编译 `fel.bin/.elf`、反汇编 | **核心/参考** |
| `05-host/` | 主机侧 pyusb / xfel 导出脚本 | **工具** |
| `06-device-dumps/` | 从设备抓取的分区镜像（`*.img`） | 数据，非源码 |
| `99-junk/` | 日志、临时文件、pycache、重复文件、旧构建产物、原始压缩包 | **垃圾/归档** |
| `docs/` | 原始 README 备份 | 参考 |

根目录另有 `build-init.sh`：把 `01/02/03` 三个核心模块编译成独立的
eMMC 初始化 payload。

## 2. 必需文件（核心）

### 2.1 汇编启动 `01-startup/start.S`

FEL `exec` 直接跳到链接地址 `0x57000` 的 `main`。启动代码必须：

1. 保存 BROM 的 `sp`；
2. 切换到自己的 SRAM 栈 `0x000A0000`；
3. 调用 C 入口 `work()`；
4. 恢复 BROM 的 `sp` 后 `bx lr` 返回。

> 不恢复 BROM 的栈会破坏 FEL USB 处理程序，只能断电重启恢复。

### 2.2 链接脚本 `01-startup/a733.ld`

```
ENTRY(main)
. = 0x57000;
.text : { *(.start) *(.text*) }
```

`a733_attr.ld` 是保留 `.ARM.attributes` 的变体，仅调试/分析时使用。

### 2.3 时钟与引脚 `02-clock/`

- `a733_emmc_pio_init()`：PC0/1/5/6/8-11/13-16 复用为 `sdc2`（mux=3），
  设置驱动强度与上下拉；未使用引脚（PC2/3/4/7/12）写 `0xf`。
- `a733_emmc_ccu_init()`：`0x02002D20 = 0x80000000`（24MHz HOSC、分频 0、门控开），
  然后对 `0x02002D2C` 做 gate/reset 脉冲。
- `a733_smhc_clock_set(div, enable)`：用 `MASK_D0` 屏蔽 DAT0 后更新分频，
  400kHz 对应 `div=29`（24MHz / (2*(29+1))）。
- `a733_smhc_clock_fast()`：切到 `div=0`，即 12MHz（24MHz/2），并刷新采样延迟。

### 2.4 eMMC 初始化 `03-emmc-init/`

- `emmc_init.c`：完整的初始化序列
  - 控制器软复位（`GCTRL=7`），强制 1-bit（`WIDTH=0`）
  - 厂商时序寄存器 `0x50/0x54/0x100/0x144`
  - 400kHz → 硬复位 → 80 个初始化时钟
  - CMD0 → CMD1 → CMD2 → CMD3 → CMD7 → CMD6(1-bit)
  - 提速到 12MHz，失败回退 400kHz
- `init_entry.c`：初始化 payload 的 `work()`，关闭看门狗、调用 `emmc_init()`，
  把状态/CID 写入 RTC GP 寄存器。
- `emmc_init_annotated.c/.s`、`ref/emmc_init_o0.dis`：逐条注释的初始化说明
  （`-O0` 反汇编与最终 `-O2` 机器码不同，仅用于阅读）。

### 2.5 完整 payload `04-payload/fel-emmc-a733.c`

在初始化基础上增加命令协议：读、写、信息、Boot 分区、RPMB、EXT_CSD 修改。
已编译产物在 `04-payload/binaries/`。

## 3. 内存布局（A733 FEL/SRAM）

| 地址 | 用途 |
|------|------|
| `0x57000` | payload 代码（`fel.bin`，约 6KB） |
| `0x5AFF0` | 16 字节命令块 |
| `0x5B000` | 数据缓冲（512 扇区 = 256KB） |
| `0xA0000` | 栈顶（向下增长） |
| `0xEE800` | BROM FEL scratchpad，勿占用 |

`< 0x51000` 的 SRAM 写入会被 BROM 拒绝/卡死；可用区从 `0x51000` 开始。

## 4. 关键寄存器速查

| 模块 | 地址 |
|------|------|
| PIO PC CFG0/1/2 | `0x02000180/184/188` |
| PIO PC DRV0/1/2 | `0x020001A0/A4/A8` |
| PIO PC PULL | `0x020001B0` |
| CCU SMHC2 CLK | `0x02002D20` |
| CCU SMHC2 GATE/RST | `0x02002D2C` |
| SMHC2 控制器 | `0x04022000` |
| RTC GP | `0x07090100 + n*4` |
| 看门狗 | `0x02050018` / `0x020500B8` |

SMHC2 偏移：`GCTRL=0x00, CLKCR=0x04, WIDTH=0x0C, CMD=0x18, CMDARG=0x1C,
RESP0=0x20, RINT=0x38, STATUS=0x3C, HWRST=0x78, FIFO=0x200`。

## 5. 构建

需要 `arm-none-eabi-gcc/ld/objcopy/objdump`。

```sh
# 独立初始化 payload -> build/a733-init.bin
sh build-init.sh

# 完整读写 payload -> 04-payload/build/fel.bin
sh 04-payload/build.sh
```

已编译好的完整 payload 在 `04-payload/binaries/fel.bin`，可直接使用。

## 6. 运行

独立初始化 payload：

```sh
xfel write 0x57000 build/a733-init.bin
xfel exec 0x57000
xfel read32 0x07090100      # 0xAA = 成功，0xEE = 失败
xfel read32 0x07090104      # 失败时的错误码
xfel read32 0x07090114      # 0xCAFEBABE = 已初始化
```

完整 payload 的读命令（命令块在 `0x5AFF0`）：

```sh
xfel write 0x57000 04-payload/binaries/fel.bin
python -c "import struct;open('cmd.bin','wb').write(struct.pack('<IIII',1,0,256,0))"
xfel write 0x5AFF0 cmd.bin
xfel exec 0x57000
xfel read32 0x07090100      # 0xAA
xfel read 0x5B000 131072 out.bin
```

主机脚本：

```sh
python 05-host/dump.py <lba> <sectors> <outfile>
python 05-host/fastdump.py <lba> <sectors> <outfile>
```

## 7. 文件取舍说明

**保留为核心**：启动汇编、链接脚本、PIO/CCU 时钟配置、eMMC 初始化、完整 payload
源码/二进制、主机工具。

**归入 `99-junk/`**：
- `archives/`：原始 `a733-emmc-init.tar.gz`
- `duplicates/`：与核心文件完全重复的副本（哈希一致）
- `legacy-build/`：`-O0/-O2` 试验用 `.o/.elf`
- `logs/`：`super*.log`、`super.pid`
- `tmp/`：`.chunk.bin`、`.cmd.bin` 抓取临时文件
- `__pycache__/`：Python 字节码缓存

**归入 `06-device-dumps/`**：`boot_a.img`、`super.img` 等设备抓取镜像，属于数据，
不是逆向源码，可单独存放或删除以释放空间（`super.img` 约 1.2GB）。

## 8. 注意事项 / 已知坑

- 源码顶部注释仍写着 A523/T527 与 `0x20000` 布局，这是早期版本残留；
  **A733 实际使用 `0x57000`**，以本 README 和链接脚本为准。
- A733 BROM FEL 是 USB 1.1 全速，实测约 300–350 KiB/s。
- 跨进程打开设备时不要调用 `libusb_set_configuration`，只做 detach + claim。
- `smc #0` 在 A733 上不会切到安全态，H3/H5/A64 的 workaround 不适用。
