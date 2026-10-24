# 04-payload —— 完整 eMMC 读写 payload

| 路径 | 说明 |
|------|------|
| `fel-emmc-a733.c` | 完整源码：初始化 + 读/写/信息/Boot 分区/RPMB/EXT_CSD |
| `build.sh` | 构建到 `build/fel.bin`（链接地址 `0x57000`） |
| `binaries/fel.bin` | 已编译 payload（6120 字节） |
| `binaries/fel.elf` | 带符号 ELF |
| `disassembly/payload.dis` | 最终 `-O2` 全量反汇编 |
| `disassembly/init.dis` | 早期构建反汇编（地址/布局略有差异） |

该源码自带 `main()`（naked 函数）和完整的 `work()`，是最终实际烧录的版本；
`01/02/03` 是从中拆分出的可读、可独立构建的初始化模块。

## 命令协议

主机在 `0x5AFF0` 写 16 字节小端命令块：

| 偏移 | 字段 |
|------|------|
| +0x00 | cmd：1=read，2=write，3=info，4=boot_read，5=rpmb_read，6=fix_extcsd |
| +0x04 | 起始 LBA / EXT_CSD 索引 |
| +0x08 | 扇区数（最大 512）/ 写入值 |
| +0x0C | flags |

构建：

```sh
sh 04-payload/build.sh
```
