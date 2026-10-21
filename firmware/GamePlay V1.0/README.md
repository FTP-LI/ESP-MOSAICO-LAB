# GamePlay V1.0 — 软件

面向 ESP-Mosaico CoreBoard V1.2 / ESP32-S31 的非官方游戏机项目，目前为 `experimental`。**GamePlay V1.0 是项目名称**；构建名为 `gameplay`，应用版本由 [version.txt](version.txt)统一提供，当前为 `0.1.0`，尚未发布游戏固件。

已建立 [Mosaico Ideas 发布框架](publish/README.md)：官方应用契约、保留 Vibe 的分区、ESP-Iris 返回 Recovery 接口、`.irisfw` 打包目标、网页资料与本地检查。发布框架尚需完整构建和设备验收；此前诊断镜像的测试结论不能直接用于新布局。

## 当前功能

- 480×480 LVGL 诊断页：色块、运行时间、剩余堆、按键状态及触摸计数。
- AI、屏幕 TONE 或外接 A 触发 250 ms / 440 Hz 测试音；48 kHz、16 bit、双声道，音量 30。
- BOOT 或外接 MENU 切换颜色。默认使用板载 AI/BOOT；可选九路 H2 输入，每 5 ms 采样、稳定 20 ms 去抖。
- BSP 读取 eFuse 主板版本；识别失败或不是 V1.2 时停止初始化。

旧单应用布局的两种诊断配置曾编译通过，见 [构建记录](test-records/2026-10-19-build.md)。新框架保留这些诊断功能，并在启动时接入官方 ESP-Iris 应用适配器。本软件尚未上板验收，FC/GB/PICO-8 核心、菜单、存档及游戏性能均未实现。官方 Vibe 的烧录记录另见 [例程验证](../../examples/vibe-validation.md)。

## 使用官方 ESP-IDF 扩展

在 VS Code 中直接打开本目录 `firmware/GamePlay V1.0`，使用 Espressif 的 ESP-IDF 扩展。

| 项目 | 诊断基线 |
| --- | --- |
| ESP-IDF | `1c42e514c90ae621af33bde3b6ad3dac22edcbfd`，6.2 开发版 |
| 本机 SDK | `C:\Espressif\esp-idf-v6.2-mosaico\esp-idf` |
| Python 环境 | `C:\Espressif\tools\python\esp-idf-v6.2-mosaico\venv` |
| 工具目录 | `C:\Espressif\tools` |
| BSP | `5f88b91e1be055d1d3674364123b2779a1758377`，见 [来源](components/esp-mosaico-bsp/UPSTREAM.md) |
| 目标 | `esp32s31` |

在扩展中选择上述 SDK，确认目标为 ESP32-S31，然后使用 `ESP-IDF: Build Your Project`。这一目标需要预览支持；若使用扩展提供的 ESP-IDF 终端，构建命令为 `idf.py --preview build`。旧自定义激活、构建与烧录包装脚本已归档。

默认配置见 `sdkconfig.defaults`。九键配置在核对接口后通过以下官方命令构建，终端工作目录为本工程：

```powershell
idf.py --preview -B build-gamepad -D SDKCONFIG=build-gamepad/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.application.defaults;sdkconfig.gamepad.defaults' build
```

生成的 sdkconfig 可能覆盖 defaults；默认使用 `build/sdkconfig`，旧根目录 sdkconfig 已归档。对不同配置使用独立构建目录。本轮新增 ESP-Iris 组件后须重新解析并复核 `dependencies.lock`；现有锁文件仍是旧诊断依赖记录。本轮未执行完整编译或烧录。目录含空格，在终端引用路径时使用引号。

## 分区与联调

当前使用 [partitions.csv](partitions.csv)中的 16 MiB Mosaico 应用布局，前 2 MiB 保留官方固定区域，应用位于 `ota_0`（0x210000）。官方集成阻止普通 `flash` / `app-flash`，避免把应用写入保存 Vibe 的 factory 分区；应用通过 `.irisfw` 流程安装。配置容量仍须与目标设备和平台 Header 一致。

实机联调先验证主板识别、显示、触摸、AI 测试音、BOOT 切色及稳定运行，再核对 BaseBoard 版本、H2 Pin1、插接方向和电平后连接短线按键原型，测试每个按键、多键组合和松开归零。当前接线条件未确认，不可按草案直接接线。

硬件入口：[GamePlay V1.0](../../hardware/GamePlay%20V1.0/README.md)、[接口](../../hardware/GamePlay%20V1.0/gamepad-board/rev-0.1/interfaces.md)、[针号表](../../hardware/GamePlay%20V1.0/gamepad-board/rev-0.1/pin-map.csv)。修改键位后，在仓库根目录运行：

```powershell
python "hardware/GamePlay V1.0/gamepad-board/check.py"
```

## Mosaico Ideas 发布要求

框架文件和操作步骤集中在 [publish/README.md](publish/README.md)。[平台元数据](mosaico-ideas.json)、[网页说明草案](docs/ideas-readme.md)和[第三方清单](THIRD_PARTY.md)均已建立；尚未生成本框架的实际 `.irisfw`，未上传或发布。

首个游戏候选版本需固定实际使用的 SDK、BSP 和模拟核心提交，保存默认配置、依赖锁和构建记录；适配官方 Vibe 应用布局，再按官方工具生成 `.irisfw`。当前记录的上传格式为 ESP32-S31、未签名、最多 32 MiB、恰好一个 application，可选 partition_table/data，不含 bootloader、recovery 或平台身份分区。发布时复核 [本地官方规则](../../examples/esp-mosaico-vibe/submodule/esp-mosaico-utils/mosaico-tools/skills/mosaico-ideas-upload/SKILL.md)。

发布版本号须与实际构建、安装包和平台版本一致，并关联精确源码提交。逐项保存第三方来源与许可；完成应用安装、启动、按键、显示、声音、游戏和恢复测试后，再填写兼容性结论与网页素材。

原创源码及说明沿用 [GPL v3](../LICENSE)，BSP 与组件保留自身许可证。
