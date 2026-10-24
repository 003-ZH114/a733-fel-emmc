# 01-startup —— 汇编启动与链接

| 文件 | 说明 |
|------|------|
| `start.S` | FEL 入口 `main`：保存 BROM `sp`，切换到 `0xA0000` 栈，调用 `work()`，恢复后返回 |
| `a733.ld` | 链接地址 `0x57000`，`.start` 在最前，丢弃 `.comment`/`.ARM.attributes` |
| `a733_attr.ld` | 同上，但保留 `.ARM.attributes`（调试/分析用） |

启动汇编由原 payload 的 naked `main()` 提取而来，语义与原版一致
（`ldr =` 会经文字池装载栈顶，编码与原版的 `mov` 略有不同）：

```asm
mov r4, sp
ldr sp, =0x000A0000
push {r4, lr}
bl work
pop {r4, lr}
mov sp, r4
bx lr
```

链接脚本与源码分离后，`work` 由 `03-emmc-init/init_entry.c` 提供；
`04-payload/fel-emmc-a733.c` 使用同一链接脚本但自带 `main()`。
