# 官方工程与例程

这里保存三个官方仓库的本机参考副本。下载时的完整提交、子模块和来源见 [sources.json](sources.json)；仓库副本由本目录 `.gitignore` 排除，不会随外层 Git 提交重复保存。

| 目录 | 官方来源 | 下载提交 |
| --- | --- | --- |
| [esp-mosaico-vibe](esp-mosaico-vibe/) | [Vibe 工作区](https://github.com/esp-mosaico/esp-mosaico-vibe) | `dd63f3a742686d90046dd4eb003a4f8eb1ad05f1` |
| [esp-mosaico-claw](esp-mosaico-claw/) | [Claw 固件](https://github.com/esp-mosaico/esp-mosaico-claw) | `2e1017e42d507cd9216d4da5b2c7d45cb2007971` |
| [esp-mosaico-bsp](esp-mosaico-bsp/) | [板级支持与示例](https://github.com/esp-mosaico/esp-mosaico-bsp) | `a7a93d290be28d18fbfd32361c90aa20b0cd1c67` |

自研软件位于 [Time-Tick V1.0](../firmware/Time-Tick%20V1.0/README.md) 和 [GamePlay V1.0](../firmware/GamePlay%20V1.0/README.md)。

## 新机器准备 Time-Tick 依赖

在仓库根目录执行以下命令，适用于尚无 `examples/esp-mosaico-vibe` 的新检出目录；已有本机副本时先检查其改动，不覆盖现有工作。

```powershell
git clone https://github.com/esp-mosaico/esp-mosaico-vibe.git examples/esp-mosaico-vibe
git -C examples/esp-mosaico-vibe checkout dd63f3a742686d90046dd4eb003a4f8eb1ad05f1
git -C examples/esp-mosaico-vibe submodule update --init --recursive
git -C examples/esp-mosaico-vibe/submodule/esp-mosaico-utils rev-parse HEAD
```

最后一条应输出 `fc63d43f3ac72aa54e701b07b07fbed07a16b3ca`。Time-Tick 的 CMake 通过这一相对目录使用官方 ESP-Iris 打包集成，缺失时固件构建会报错。其 ESP-IDF 与编译器版本以 [Time-Tick 构建说明](../firmware/Time-Tick%20V1.0/README.md) 为准；下面的恢复工程记录是另一套历史工具链。仅运行电脑预览时，按 [PC README](../firmware/Time-Tick%20V1.0/pc/README.md) 准备依赖即可。

Claw 与独立 BSP 目录是参考资料，按需从表中地址克隆并切到对应提交，再初始化子模块，无需为 Time-Tick 重复下载。

## 已验证的 Vibe 入口

使用 VS Code 打开：

```text
examples/esp-mosaico-vibe/submodule/esp-mosaico-utils/esp-mosaico-recovery/firmware/recovery
```

本机 ESP-IDF 为 `C:\Espressif\esp-idf-vibe-7b9cc1a`，固定提交 `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`；Python 环境位于 `C:\Espressif\tools\python\esp-idf-vibe-7b9cc1a\venv`，目标为 `esp32s31`。该基线与 GamePlay 诊断工程不同。

使用官方扩展命令 `ESP-IDF: Build Your Project`、`ESP-IDF: Flash (UART) Your Project`。本地扩展配置路径已随迁移更新，原 sdkconfig 单独保存，旧构建缓存已归档；下次构建会重新生成产物。设备手动进入 ROM USB-OTG 下载模式时，本次成功烧录采用 `no-reset`；端口应按当次设备枚举确认。

[完整构建与烧录记录](vibe-validation.md)保存了 Vibe 0.1.4 的构建、镜像哈希和 COM87 烧录结果。Windows 下 GSPC 依赖文件路径兼容修复保留在本地 recovery 源码中，官方 GSPC 可执行文件未替换。这些本地修改不属于原始下载提交。
