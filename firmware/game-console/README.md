# ESP-Mosaico V1.2 诊断固件

非官方 `experimental` 工程，当前实现显示、声音和按键诊断，尚未接入 FC、GB 或 PICO-8。目标为 ESP32-S31 / CoreBoard V1.2，启动时由官方 BSP 读取 eFuse 版本；识别失败或不是 V1.2 时停止，不强制套用引脚。BaseBoard 实物版本、原固件和设备功能尚未验证。

## 已实现

- 480×480 LVGL 诊断页：RGB、白、黑色块与纯色切换，运行时间、内部剩余堆、按键状态和触摸按钮计数。
- AI 或屏幕 TONE 按钮触发 250 ms、440 Hz 测试音，48 kHz / 16 bit / 双声道，音量 30；首尾有淡入淡出。API 返回成功不能证明扬声器实际发声。
- BOOT 或外接 MENU 切换测试颜色；外接 A 也可触发测试音。
- 默认只读取板载 AI/BOOT。手柄配置启用 9 路 H2 GPIO，5 ms 采样、稳定 20 ms 去抖，串口记录按下/松开，界面显示完整位图，允许多键同时按下。

POWER 不作为游戏键。复位时不要按住 BOOT。触摸计数只覆盖 TONE/COLORS 控件，不是全屏触控校准。亮度设为 50，声音与输入也有串口日志。

## 环境与构建

工具安装及固定版本见 [ESP-IDF 环境说明](../../tools/esp-idf/README.md)。目标芯片仍使用 ESP-IDF 6.2 开发分支固定提交，需要 `--preview`。不要使用其他芯片的 ESP-IDF 环境或 EIM 遗留的旧工具配置。

从仓库根目录执行：

```powershell
. ./tools/esp-idf/activate.ps1 -UseGlobalMirror
Set-Location firmware/game-console
idf.py --preview build
```

仅在核对 [H2 接口草案](../../hardware/gamepad-board/rev-0.1/interfaces.md) 后使用手柄配置；下面的命令只构建，不烧录：

```powershell
idf.py --preview -B build-gamepad -D SDKCONFIG=build-gamepad/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.gamepad.defaults' build
```

默认配置在 `sdkconfig.defaults`，手柄开关在 `main/Kconfig.projbuild`，开启配置覆盖在 `sdkconfig.gamepad.defaults`。生成的 `sdkconfig` 不提交；修改默认值后，已有 sdkconfig 的值可能优先，应使用新的构建目录及 SDKCONFIG 文件。依赖版本由 `dependencies.lock` 固定；BSP 随源码保存，来源见 [UPSTREAM.md](components/esp-mosaico-bsp/UPSTREAM.md)。

## 分区与烧录边界

当前沿用 IDF `partitions_singleapp_large.csv`：NVS 24 KiB、PHY 4 KiB、factory 应用 1500 KiB；没有 OTA 或游戏存储分区。构建的 Flash 参数为 DIO / 80 MHz / 2 MB；这只是诊断工程配置，不是实测 Flash 容量。PSRAM 250 MHz 与 XIP 配置沿用该 BSP 的 HMI 示例，必须通过上板启动验证。

尚未烧录。先登记原设备固件与恢复方式、确认数据线和串口，并核对芯片/Flash 信息。烧录会替换引导、分区表和应用；需要保留的原固件与数据应先备份。确认后把下面的 `COM实际端口` 替换为设备端口：

```powershell
idf.py --preview -p COM实际端口 flash monitor
```

启用手柄的构建需要带上前述 `-B build-gamepad -D SDKCONFIG=build-gamepad/sdkconfig` 参数。退出串口监视器用 Ctrl+]。恢复需要原厂适用固件及其分区/烧录说明，本仓库尚未归档设备备份。

## 联调顺序

1. 无外接按键板，验证 eFuse 识别、屏幕颜色/方向、触摸控件、AI 测试音、BOOT 切色和 10 分钟稳定运行。
2. 核对 BaseBoard 丝印、H2 Pin1、插接方向、OUT_3V3 和输入电平后，关机连接短线按键原型，再使用手柄配置。
3. 逐键按下/松开，验证斜向+A/B 和菜单键；检查松手后位图归零。记录音量、亮度、去抖表现及日志。

当前接线条件未确认，**不可按此接线**。编译检查和设备测试分别记录，见 [验证记录](test-records/2026-10-19-build.md)。

## 映射与许可

按键顺序和 GPIO 位于 `main/diagnostics.c` 的 `key_names`、`key_pins`；电气定义统一引用 [pin-map.csv](../../hardware/gamepad-board/rev-0.1/pin-map.csv)。运行 `python tools/check-gamepad.py`（仓库根目录）核对 EDA 网表、接口表与源码。

主机 LCD、触摸、I²S、ES8311、功放和电源控制交给固定版本 BSP，不在应用中复制初始化。没有修改板载电源键行为，也没有访问个人游戏库、NAND 存档或网络。

本项目原创源码与说明沿用根目录 GPL v3；BSP 保留 Apache-2.0 许可证，注册表组件各自保留许可证。尚未引入模拟器、ROM 或外部游戏资源。
