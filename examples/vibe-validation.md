# Vibe Mode 的 VS Code 编译与烧录记录

日期：2026-10-19，Asia/Shanghai。整体状态仍为 `experimental`；本记录确认本次原生扩展构建、ROM 烧录校验和重启后的 USB 枚举，未代替完整硬件功能验收。

## 环境与入口

- VS Code 扩展：Espressif ESP-IDF 2.3.0。
- Vibe 工作区：`dd63f3a742686d90046dd4eb003a4f8eb1ad05f1`。
- utils 子模块：`fc63d43f3ac72aa54e701b07b07fbed07a16b3ca`。
- ESP-IDF：`7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe`，本机目录 `C:\Espressif\esp-idf-vibe-7b9cc1a`。
- 固件：Vibe Mode `0.1.4`，应用名 `factory`，目标 `esp32s31`。
- 工程：`esp-mosaico-vibe/submodule/esp-mosaico-utils/esp-mosaico-recovery/firmware/recovery`。
- 本地配置及复现方法见 [官方工程入口](README.md#已验证的-vibe-入口)。

通过电脑控制在 VS Code 命令面板选择 `ESP-IDF: Build Your Project` 和 `ESP-IDF: Flash (UART) Your Project`。本次没有通过本仓库 `tools/esp-idf/` 的 Vibe 构建或烧录包装脚本执行操作。

## 构建结果

官方扩展完成 1,527 步 Ninja 构建，随后完成内存用量报告；`.ninja_log` 本次最长结束时间为 214,458 ms，约 3 分 34 秒。`factory.bin` 生成时间为 22:46:02。

| 镜像 | 大小（字节） | 烧录地址 |
| --- | ---: | --- |
| bootloader.bin | 24,432 | `0x2000` |
| partition-table.bin | 3,072 | `0x8000` |
| ota_data_initial.bin | 8,192 | `0x9000` |
| factory.bin | 1,821,104 | `0x20000` |

本地 `factory.bin` SHA-256：`8b8e964d9ea2c4661b0083d6ad25f69a5e51b309d1759d757af16caa58653f0a`。

Windows 下 GSPC 的扩展路径依赖文件兼容修复已随本次构建验证；生成的 `bundle.d` 使用 `C:/...` 路径，官方 GSPC 0.5.0 可执行文件保持原样。源码差异为 `main/CMakeLists.txt` 的生成后处理，以及新增 `cmake/normalize_gsp_depfile.cmake`。CMake 曾报告工程路径过长警告，但本次完整构建和容量检查均完成。

## 烧录与重启结果

设备已手动进入 ROM 下载模式，端口 COM87，实际识别为 ESP32-S31 rev 0.0、USB-OTG。第一次默认 `default-reset` 在连接阶段出现 Windows `ClearCommError`，未开始写入。将扩展读取的 `flasher_args.json` 的 `before` 改为 `no-reset` 后，通过同一扩展命令成功连接；该选择同时保存到本地构建目录的 `sdkconfig`。

使用 460800 波特率完成上述四个镜像写入，每个镜像均输出 `Hash of data verified`。主固件 1,821,104 字节的写入时间为 47.4 秒，终端随后输出 `Hard resetting with a watchdog...`，扩展输出 `Flash Done`。

设备复位后，Windows 串口从 ROM 的 COM87（VID `303A`、PID `0020`）变为 COM88（VID `303A`、PID `4002`），与 ESP-Iris 运行时 USB 配置相符。COM88 是 ESP-Iris 协议接口；此固件的普通控制台日志使用 UART0，不能把 COM88 的普通串口监视输出当作完整日志验收。

屏幕显示、触摸、音频、Wi-Fi 和应用安装功能未在本记录中验收。Hello World 未通过默认 `flash` 写入，以免覆盖 Vibe 的 `factory` 分区。
