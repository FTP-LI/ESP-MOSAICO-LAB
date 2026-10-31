# Mic-Lab V1.0 开发交接

更新：2026-10-31（北京时间）。本次目标是保存当前工程并上传，方便换电脑继续开发；不新增 DIY 屏幕实现，不刷写设备。

## 先读这段

用户想以影石 Mic Pro 为参考，使用现有 ESP-Mosaico，主要连接电脑。已选择 USB 麦克风、实时音量表、增益、静音、短时录音回放，并明确要求录音断电保存。工程名为 **Mic-Lab V1.0**，内部固件版本仍是 `0.1.0-dev`。

**关键更正：用户随后强调“可 DIY 的主屏幕”才是参考产品的核心。当前实现只有固定音频控制页，缺少这个功能，项目不能标记为完整 V1.0。下一阶段应围绕 DIY 首页展开。**

已提出的方向是展示首页（自选图片、Logo、文字）、独立音频控制页、电脑端编辑上传与素材持久化。这些是建议，用户尚未选定具体内容、格式、布局编辑范围及交互方式。不要把动画、GIF、拖拽编辑器或无线传图当作已确认需求。参考产品的静态壁纸和标签功能见 [官方说明](https://onlinemanual.insta360.com/micpro/en-us/camera/using-app/e-ink-display)。

## 当前代码状态

| 部分 | 已实现 / 待验证 |
| --- | --- |
| USB 音频 | UAC2，48 kHz / 16-bit 单声道；与 Iris 双 CDC 共存。真实 Windows 枚举、连续人声录制待验证 |
| 音量表 | 固定 LVGL 首页显示输入峰值/RMS dBFS 与过载；不是声压计 |
| 增益/静音 | ES8311 0–42 dB，6 dB 一档，默认 24 dB；电脑另有 −60 到 0 dB 数字衰减；本地增益/静音保存到 NVS |
| 短时录音 | 最长 30 秒，PSRAM 暂存，停止或录满后保存最近一段；录制时 USB 继续工作 |
| 断电保存 | 官方 NAND LittleFS 后端，独立 `/nand/mic-lab/`；临时文件→同步→读回 CRC→原子替换。实际 NAND 断电测试待执行 |
| 回放 | 板载扬声器 35% 音量，USB 静音、暂停采集；结束前排空 40 ms，恢复采集前静音保护 200 ms |
| DIY 首页 | **未实现**；没有图片导入、标签编辑、用户素材存储或电脑编辑器 |
| 整机状态 | 未刷写本项目固件，未进行实际声学、USB 稳定性或断电恢复验收 |

只有显示“已保存”才表示本段写入完成。录音/保存中断电可能丢失新录音，上一段已保存文件应保留，须通过实机中断测试验收。禁止因挂载失败格式化 NAND，禁止改写其他应用的数据或分区。

## 工程与硬件

目标是 **ESP32-S31，不是 ESP32-S3**；CoreBoard V1.2、480×480 彩色触摸屏、单路板载模拟麦克风、ES8311、NS4150B 扬声器功放、16 MB PSRAM、16 MB NOR 和 128 MB SPI NAND。彩屏断电后熄灭；保存素材后重新上电恢复展示，不等于墨水屏断电保留画面。

| 路径 | 职责 |
| --- | --- |
| `main/mic_core.[ch]` | 可在电脑测试的电平、静音、录音及回放状态机 |
| `main/mic_audio.[ch]` | 唯一 codec 所有者，I2S 采集/回放与状态快照 |
| `main/usb_layout.c`、`usb_mic.c` | 扩展双 CDC 描述符、UAC 控制与音频 FIFO |
| `main/clip_file.c`、`mic_storage.c` | 文件格式/CRC/安全替换，以及独立存储任务 |
| `components/mic_storage_backend/` | 直接引用固定上游存储后端；未修改上游文件 |
| `main/mic_ui.c` | 当前固定控制界面；未来 DIY 首页的主要改动入口 |
| `main/main.c` | 启动、NVS、只读状态 RPC、截图后端和健康确认 |
| `tests/` | 四组逻辑测试及真实 LVGL 离屏界面检查 |
| `docs/PRODUCT.md` | 功能目标及 DIY 需求更正 |
| `docs/VERIFICATION.md` | 验证证据、包校验与待实机清单 |
| `docs/IMPLEMENTATION.md` | 已完成的音频基础任务；勾选不包含新增 DIY 需求 |

已修正过的坑：ES8311 实际以 6 dB 分档；USB Feature Unit 仅宣告已实现的主通道控制；USB 任务与音频任务固定 core 0，USB 优先级 7 高于音频 6，短临界区保护 FIFO；禁用 FIFO 覆盖并计数；回放末尾需要排空 DMA；USB 重新枚举要清除旧激活状态并继续转发 Iris 原回调。

## 从另一台 Windows 电脑开始

上传分支为 `codex/mic-lab-v1-handoff`，仓库为 `https://github.com/FTP-LI/ESP-MOSAICO-LAB.git`。本次不合并到 main，因此不要只克隆默认分支。

```powershell
git clone --branch codex/mic-lab-v1-handoff https://github.com/FTP-LI/ESP-MOSAICO-LAB.git
Set-Location 'ESP-MOSAICO-LAB/firmware/Mic-Lab V1.0'
python tools/prepare_dependencies.py
```

已有该仓库时先保存自己的未提交工作，再 `git fetch origin` 并切换此分支。不要用强制覆盖来切换。

### 准备固定 ESP-IDF

需要 Git、Python（本机验证为 3.12）和 ESP-IDF 所需工具。可使用已安装的匹配 SDK；其提交必须为 `1c42e514c90ae621af33bde3b6ad3dac22edcbfd`。下面示例新建独立 SDK 目录，可替换盘符/位置，不能覆盖已有 SDK。

```powershell
$micSdkPath = 'C:/Espressif/esp-idf-mosaico'
git clone --no-checkout https://github.com/espressif/esp-idf.git $micSdkPath
git -C $micSdkPath checkout --detach 1c42e514c90ae621af33bde3b6ad3dac22edcbfd
git -C $micSdkPath submodule update --init --recursive
& "$micSdkPath/install.ps1" esp32s31
. "$micSdkPath/export.ps1"
```

激活环境后确认 `IDF_PATH`、`IDF_PYTHON_ENV_PATH` 已设置，CMake/Ninja/RISC-V 工具链在 PATH。原电脑验证的工具版本为 RISC-V GCC `esp-16.1.0_20260609`、CMake 4.0.3、Ninja 1.12.1；固定 SDK 的安装脚本负责匹配工具版本。VS Code 可打开工程的 `.code-workspace`，通过官方 ESP-IDF 扩展选择这套 SDK；工作区不再写死本机 SDK 路径。

### 准备 Iris 打包解释器并构建

以下命令继续在 `firmware/Mic-Lab V1.0` 内执行。Iris 解释器独立于 SDK，目录已忽略提交。

```powershell
python -m venv .venv-iris
& './.venv-iris/Scripts/python.exe' -m pip install -r 'build-deps/esp-mosaico-utils/ESP-Iris/components/esp_iris/tools/requirements.lock'
$env:ESP_IRIS_PYTHON = (Resolve-Path './.venv-iris/Scripts/python.exe').Path
& './tools/build.ps1' -UseEnvironment -Bundle
```

只编译固件时可省略 `-Bundle`，不需要 Iris 打包解释器。`-UseEnvironment` 保留新电脑已激活的环境；省略它则使用原电脑的 `C:/Espressif` 和本机 Iris runtime 路径。

固定依赖：Mosaico utils `be151ae047014e79a9c7ae0092dd203844499003`；BSP `5f88b91e1be055d1d3674364123b2779a1758377`；托管组件见 `dependencies.lock`。不要先升级组件或 SDK再排错。已生成字体源码纳入 Git，常规构建不需要 Pillow；改字库时才需要。

### 检查

安装原生 Windows GCC（本机使用 MinGW-w64），并使 `gcc` 可执行。先完成构建以下载固定 TinyUSB/LVGL 依赖，然后运行：

```powershell
& './tools/test.ps1'
cmake -S tests -B build-ui -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug
cmake --build build-ui
New-Item -ItemType Directory -Force build-ui/screens
Push-Location build-ui/screens
../ui_check.exe
Pop-Location
```

这些检查验证逻辑、描述符、控制请求/FIFO和界面；不能替代 USB 时序、声音及断电实测。检查命令任一步失败，应先处理该步输出。

## 上传与本地文件边界

上传源码、测试、字体及许可、配置/依赖锁定、预览 PNG 和交接文档。`build-deps/`、`managed_components/`、`build-*`、Python 虚拟环境、串口日志和本机设置不上传，可按上面步骤恢复。

已有固件位于 `build-idf62/mic_lab.bin`，更新包位于 `build-idf62/mic_lab-system-update.irisfw`，它们是本机产物，**不随 Git 分支上传**。2026-10-29 构建快照的校验值保存在 `BUILD-ARTIFACT.json`；重新打包的时间元数据可能改变包文件哈希。新电脑应从本分支重建。此前未获取真实设备录音，仓库不包含用户录音数据。

本次仅提交 Mic-Lab 及对应索引内容，工作区另有 Trader-Lab 未提交开发内容，属于另一个项目，未打入此交接分支。现有已提交的其他项目仍随仓库正常存在。

## 接续顺序

1. 先确认固定 SDK、依赖和电脑检查能在新电脑运行。
2. 继续询问用户 DIY 首页的具体内容与编辑方式，形成清楚的界面方案；保留已有音频基础功能。
3. 在音频任务外完成素材解码/保存和 UI 更新，防止图像上传或动画阻塞 USB 音频；继续遵守现有 NAND 不格式化和分区不变约束。
4. DIY 功能完成后做回归，再接设备执行 `VERIFICATION.md` 的实机清单。当前尚未授权本轮刷机。

可将下面这段直接交给新电脑上的开发助手：

> 继续 ESP-MOSAICO-LAB 中的 Mic-Lab V1.0。先读取 firmware/Mic-Lab V1.0/docs/HANDOFF.md、PRODUCT.md 和 VERIFICATION.md。现有音频基础代码已构建并通过电脑测试，尚未实机验证。用户强调可 DIY 主屏幕是核心，当前完全未实现；具体展示内容/编辑方式尚待确认。保留 ESP32-S31 固定 SDK、Iris 双 CDC、录音断电保存和其他项目文件，不格式化 NAND，也不要自动刷机。先恢复构建环境，再推进 DIY 首页设计。
