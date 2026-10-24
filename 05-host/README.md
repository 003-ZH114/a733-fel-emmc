# 05-host —— 主机侧 FEL 工具

| 文件 | 说明 |
|------|------|
| `felclient.py` | pyusb 常驻 FEL 客户端（VID `0x1f3a` / PID `0xefe8`） |
| `dump.py` | 通过 `xfel` CLI 按块导出 eMMC 到文件 |
| `fastdump.py` | 通过 `felclient.py` 常驻连接批量导出 |

脚本已改为从仓库相对路径读取 `04-payload/binaries/fel.bin`。

```sh
python 05-host/dump.py     <lba> <sectors> <outfile>
python 05-host/fastdump.py <lba> <sectors> <outfile> [--append] [--nocode]
```

**注意**：不要调用 `libusb_set_configuration()` / `set_configuration()`，
否则会导致 BROM FEL 挂死；只做 detach + claim interface。
