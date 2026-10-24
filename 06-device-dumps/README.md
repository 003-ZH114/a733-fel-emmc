# 06-device-dumps —— 设备镜像（数据）

从 A733 设备抓取的分区镜像，**不是逆向源码**：

```
boot_a.img            bootloader_a.img      dtbo_a.img
env_a.img             init_boot_a.img       misc.img
super.img (约 1.2GB)  vbmeta_a.img          vbmeta_system_a.img
vbmeta_vendor_a.img   vendor_boot_a.img
```

这些文件仅用于分析/备份，可按需删除以释放空间。
