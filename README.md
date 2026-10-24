# A733 FEL eMMC Toolkit

**Allwinner A733（sun60iw2p1）FEL 模式裸机 eMMC 读写 / 存储提取工具**

无需 root、无需 ADB、无需解锁 bootloader，直接在 BROM FEL 阶段把一小段 ARM 代码放进 SRAM，
驱动 SMHC2 控制器读写 eMMC。适用于安全研究、设备取证、固件分析与维修。

> 本项目仅应在你拥有或已获书面授权的设备上使用。

---

## Summary (EN)

A bare-metal eMMC driver for Allwinner A733 (sun60iw2p1) that runs entirely from SRAM in
FEL mode. It initializes the SMHC2 controller, enumerates the eMMC card, and provides
chunked raw read/write over the FEL USB protocol — with no root, no ADB, no DRAM, and no
bootloader unlock. Includes a persistent pyusb FEL client, partition dumper, and detailed
A733 register/SRAM documentation. Tested on a 星学伴 A733 Pro3 tablet (Android 15).

---

## 1. 为什么做这个 / 社区贡献

Allwinner A733 是较新的平板 SoC，公开资料很少，且大量设备开启了 secure boot。社区现状：

- 主线的 `xfel` / `sunxi-fel` 对 A733 的 DDR 初始化尚未实现（`chip_ddr` 为空），
  FEL 批量写入在部分地址会被 BROM 拒绝甚至卡死。
- A523 上已有成熟的 FEL eMMC 方案（`A523-root` 的 `fel-emmc`），但 A733 的
  PIO/CCU/SMHC 寄存器布局不同，不能直接复用。
- A733 的 BROM 受保护区、可写 SRAM 边界、FEL 协议细节没有公开文档。

本项目补齐了这些空白，并给出可复现的实现：

1. **A733 专用的 SRAM 裸机 eMMC 驱动**：SMHC2 + PIO/FIFO，不依赖 DRAM。
2. **完整的 A733 寄存器与内存布局记录**：
   - PIO V3（NCAT3）PC 口偏移 `0x180/0x184/0x188/0x1A0/0x1A4/0x1A8/0x1B0`
   - CCU SMHC2 时钟 `0x02002D20`、门控/复位 `0x02002D2C`
   - SMHC2 控制器 `0x04022000`
   - **可用 SRAM 从 `0x51000` 起；`0x47000` 是 BROM 受保护区，写入会卡死**
3. **FEL 主机侧踩坑总结**（对任何 A733 FEL 工具都适用）：
   - BROM FEL 是 USB 1.1 全速（12 Mbps），实测 300–350 KiB/s
   - pyusb 打开设备时**不能调用 `libusb_set_configuration()`**，否则会把 BROM 搞挂
   - 大块客户端写入可能触发 BROM 异常；推荐“xfel 上传代码 + 客户端只写 16 字节命令块”
   - `smc #0` 在 A733 上不会切换到安全态（H3/H5/A64 的 workaround 不适用）
4. **可复用的主机工具**：
   - `felclient.py`：常驻 pyusb FEL 客户端（避免每块启动 3 次 xfel 进程）
   - `dump.py` / `fastdump.py`：按 GPT 分区或任意 LBA 批量导出
   - GPT / Android boot / vendor_boot / vbmeta / DTBO 解析示例
5. **实测 AVB 校验范围**（供后续研究与取证参考）：
   - 整镜像哈希：`boot`、`dtbo`、`init_boot`、`vendor_boot`
   - dm-verity：`system`、`vendor`、`product`、`system_dlkm`、`vendor_dlkm`
   - `misc`、`env`、`metadata`、`private`、`frp`、`pstore` 等不受 AVB 校验
6. **说明它“不能”做什么**（避免误导）：不绕过 secure boot / AVB，不解锁 bootloader，
   不提供 root。它只提供 FEL 阶段的原始存储读写能力。

---

## 2. 适用范围

### 支持

- SoC：**Allwinner A733 / sun60iw2p1**
- 存储：**eMMC**（用户区；boot 分区可通过 `CMD_BOOT_READ` 读取）
- 模式：设备能进入 **FEL**（USB `1f3a:efe8`）
- 已实测设备：星学伴 A733 Pro3（`xingxueban/a733_pro3_arm64` / `seewo`，
  Android 15，`AP3A.241105.008`，secure boot 开启）
- 典型用途：
  - 全分区备份 / 恢复
  - 固件提取、APK 与系统分析
  - 设备取证（在授权范围内）
  - 维修与数据恢复
  - 研究 A733 的 BROM/FEL/AVB 行为

### 不支持 / 边界

- **UFS / NAND 存储**：本驱动是 eMMC（SMHC2）；UFS 需要完全不同的驱动。
- **绕过 secure boot / AVB**：不修改签名链；修改被 AVB 校验的分区会导致开机失败。
- **解锁 bootloader / root**：不提供；FEL 原始写入不等于解锁。
- **DRAM 初始化**：本项目不需要 DRAM；`xfel ddr` 对 A733 尚未实现。
- 未在其它 A733 板型上验证；不同板型的 DRAM/存储/引脚可能不同。
- 不适用于非 Allwinner SoC。

### 合规声明

仅在你**拥有**或**获得明确书面授权**的设备上使用。不要用于未经授权的设备、
规避他人设备的安全控制，或侵犯他人数据与隐私。使用者自行承担全部责任。

---

## 3. 原理

```
Host (Linux)                 Device (A733)
┌──────────────┐   USB FS   ┌─────────────────────────────┐
│ xfel / pyusb │ <--------> │ BROM FEL (12 Mbps)          │
└──────────────┘            │   └─ SRAM @0x57000 payload  │
                            │        └─ SMHC2 @0x04022000 │
                            │             └─ eMMC card    │
                            └─────────────────────────────┘
```

1. `xfel write 0x57000 fel.bin` 把 payload 放进 SRAM。
2. `xfel exec 0x57000` 执行；payload 保存/恢复 BROM 的 SP，初始化 PIO/CCU/SMHC2/eMMC。
3. 主机在 `0x5AFF0` 写 16 字节命令块，再次 `exec`。
4. payload 按命令读/写指定 LBA，把数据放在 `0x5B000`，状态写入 RTC GP 寄存器。
5. 主机读回数据（`xfel read` 或 pyusb）。

---

## 4. 内存布局（SRAM）

| 地址 | 用途 |
|------|------|
| `0x57000` | payload 代码（约 6 KB） |
| `0x5AFF0` | 16 字节命令块 |
| `0x5B000` | 数据缓冲（最大 512 扇区 = 256 KB） |
| `0xA0000` | 栈顶（向下增长） |
| `0xEE800` | BROM FEL scratchpad（不要占用） |

**重要**：A733 上 `< 0x51000` 的 SRAM 写入会被 BROM 拒绝/卡死；`0x47000` 是受保护区。

---

## 5. 关键寄存器

| 模块 | 地址 | 说明 |
|------|------|------|
| PIO CFG0/1/2 | `0x02000180/184/188` | PC0-7 / PC8-15 / PC16-23 复用 |
| PIO DRV0/1/2 | `0x020001A0/A4/A8` | 驱动强度 |
| PIO PULL | `0x020001B0` | PC bank 上下拉 |
| CCU SMHC2 CLK | `0x02002D20` | bit31 门控；mux[26:24]；M/P 分频 |
| CCU SMHC2 GATE/RST | `0x02002D2C` | bit0 门控，bit16 复位 |
| SMHC2 | `0x04022000` | GCTRL/CLKCR/WIDTH/CMD/FIFO… |
| RTC GP | `0x07090100 + n*4` | 跨 FEL exec 保存状态 |
| 看门狗 | `0x02050018/0x020500B8` | 关闭，避免长操作复位 |

SMHC2 偏移：`GCTRL=0x00, CLKCR=0x04, TMOUT=0x08, WIDTH=0x0C, CMD=0x18,
CMDARG=0x1C, RESP0=0x20, RINT=0x38, HWRST=0x78, FIFO=0x200`。

eMMC 引脚（设备 DT）：PC0, PC1, PC5, PC6, PC8, PC9, PC10, PC11, PC13, PC14,
PC15, PC16，功能 `sdc2`（mux=3）。

---

## 6. 初始化流程

1. **PIO**：PC 口复用为 `sdc2`，设置驱动强度/上下拉
2. **CCU 时钟**：`0x02002D20 = 0x80000000`（gate on，mux=sys24M，分频=1）
3. **CCU 门控/复位**：`0x02002D2C` 清 bit16/bit0，延时后置 bit0/bit16
4. **控制器复位**：`GCTRL=7`，轮询清零；`WIDTH=0` 强制 1-bit
5. **400kHz**：`CLKCR=0x8000001D`（div=29），`UPCLK_ONLY` 更新，清 `MASK_D0`
6. **硬复位 + 80 个初始化时钟**
7. **卡片初始化**：CMD0 → CMD1 → CMD2(CID) → CMD3(RCA=1) → CMD7 → CMD6(1-bit SDR)
8. **提速**：`CLKCR` div=0（12MHz），重配采样延迟；失败回退 400kHz
9. 返回 BROM FEL

---

## 7. 命令协议

主机在 `0x5AFF0` 写入 16 字节（小端）：

| 偏移 | 字段 |
|------|------|
| +0x00 | cmd：1=read，2=write，3=info，4=boot_read，5=rpmb_read，6=fix_extcsd |
| +0x04 | 起始 LBA |
| +0x08 | 扇区数（最大 512） |
| +0x0C | flags |

状态（RTC GP）：

| 寄存器 | 含义 |
|--------|------|
| GP0 | 0x01=init，0x02=ready，0xAA=done，0xEE=error |
| GP1 | 进度/错误码 |
| GP2/GP3 | CID[0]/CID[1] 或诊断 |
| GP5 | `0xCAFEBABE` = 卡已初始化（跨 exec 跳过重复初始化） |

---

## 8. 快速开始

### 构建

```sh
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi
./src/build.sh              # 生成 fel.bin（链接地址 0x57000）
```

### 运行（读 GPT）

```sh
xfel version
xfel write 0x57000 fel.bin

python3 -c "import struct;open('/tmp/cmd','wb').write(struct.pack('<IIII',1,0,34,0))"
xfel write 0x5AFF0 /tmp/cmd
xfel write32 0x07090100 0
xfel exec 0x57000
xfel read32 0x07090100        # 0xAA = 成功
xfel read 0x5B000 17408 gpt.bin
```

### 批量导出（推荐）

```sh
# felclient.py + fastdump.py：常驻 USB，速度更高
python3 host/fastdump.py <start_lba> <sectors> <out.img>
```

---

## 9. 实测数据

- USB：BROM FEL 枚举为 **全速 12 Mbps**
- 持续读：约 **300–350 KiB/s**
- 8 MB `init_boot_a`：约 30 秒（xfel 方案）/ 更快（pyusb 方案）
- 4.83 GB `super`：约 4 小时
- 已成功导出并校验：`boot`、`init_boot`、`vendor_boot`、`dtbo`、`vbmeta*`、
  `env`、`misc`，以及完整 GPT

---

## 10. 已知坑

- `0x47000` 写入会让 BROM 卡死（受保护区）；可用区从 `0x51000` 起
- pyusb 打开设备时不要 `set_configuration()`，只 `detach + claim_interface`
- 客户端大块写入可能触发 BROM 异常；代码用 xfel 上传更稳
- `smc #0` 不会在 A733 上解锁安全态
- secure boot 开启时，FEL exec 仍可运行代码，但从存储启动会校验签名
- 修改 `system/vendor/product/...` 会触发 dm-verity；`misc/env` 不受 AVB 校验
- 不同板型的 DRAM/引脚/存储可能不同，需自行核对

---

## 11. 路线图

- [ ] eMMC 写路径的完整验证与安全擦除
- [ ] boot 分区（boot0/boot1）读取工具
- [ ] UFS 支持（另一套驱动）
- [ ] 更高速的 FEL 传输（批量/流水线）
- [ ] 更多 A733 板型的寄存器/引脚适配
- [ ] 自动解析 GPT / AVB / Android boot 的一体化工具

---

## 12. 致谢

- [chrislennon/A523-root](https://github.com/chrislennon/A523-root) — `fel-emmc` 原始思路与协议
- [xboot/xfel](https://github.com/xboot/xfel) — FEL 工具与 A733 支持
- [linux-sunxi/sunxi-tools](https://github.com/linux-sunxi/sunxi-tools) — FEL 协议与 SoC 资料
- [smaeul/sunxi-blobs](https://github.com/smaeul/sunxi-blobs)、Icenowy — A733 BROM dump
- dlan17 — sunxi-tools A733 FEL PR
- [GrecAndrei/sun60iw2-upstream](https://github.com/GrecAndrei/sun60iw2-upstream) — A733 CCU/pinctrl 驱动
- ZZC-jeizuchin — A733 eMMC 移植经验分享
- SyterKit — SMHC 初始化参考

---

## 13. 许可

MIT License。见 `LICENSE`。

本项目基于 A523-root（MIT）的 `fel-emmc` 思路进行 A733 适配；使用时请同时遵守上游项目的许可。

---

## 14. 免责声明

本项目按“现状”提供，不附带任何明示或暗示的担保。作者不对因使用本工具造成的
设备损坏、数据丢失、法律风险或其它后果承担责任。请在合法授权范围内使用。