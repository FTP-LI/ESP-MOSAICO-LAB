# Mosaico Ideas 发布框架

适用项目：GamePlay V1.0。当前应用版本为 `version.txt` 中的 `0.1.0`，属于诊断与发布框架阶段，尚无可发布的游戏版本。

依据 [网站上传页](https://mosaico-ideas.espressif.com/publish)及 [固定版本官方上传规则](https://github.com/esp-mosaico/esp-mosaico-utils/blob/fc63d43f3ac72aa54e701b07b07fbed07a16b3ca/mosaico-tools/skills/mosaico-ideas-upload/SKILL.md)。2026-10-19 查询时网站维护中且需要登录；本地校验通过不代表网站审核或实机测试通过。工具与 SDK 提交记录在 [sources.json](sources.json)。

## 文件职责

| 文件 | 用途 |
| --- | --- |
| [../mosaico-ideas.json](../mosaico-ideas.json) | 网页标题、分类、硬件标签、许可、资料路径及本次更新摘要 |
| [../docs/ideas-readme.md](../docs/ideas-readme.md) | 将上传到网页的用户说明；只描述实际功能 |
| [assets/](assets/README.md) | 实机封面与截图，当前尚未提供 |
| [../version.txt](../version.txt) | 构建版本的唯一手动输入；不要在外置 manifest 中增加 version |
| [../.mosaico.json](../.mosaico.json) | 官方 CLI 的工作区、应用和依赖位置，不含账号或平台 UUID |
| [../partitions.csv](../partitions.csv) | 保留 Vibe/Recovery 的 16 MiB 应用布局 |
| [../sdkconfig.application.defaults](../sdkconfig.application.defaults) | ESP-Iris USB 和普通应用配置，OTA 写入由 Recovery 承担 |
| [../CMakeLists.txt](../CMakeLists.txt) | 引入官方应用契约和 system-update-bundle 目标 |
| [validate.py](validate.py) | 只读本地检查，可调用官方检查器验证实际固件包 |
| [../THIRD_PARTY.md](../THIRD_PARTY.md) | 实际引入的依赖及许可依据 |

## 本地检查

以下命令都在 VS Code 打开的 `firmware/GamePlay V1.0` 目录执行：

```powershell
python publish/validate.py
```

检查官方工具提交、网页元数据、资料引用、版本格式、受保护分区及分区边界。没有固件包时会明确列出构建、实机测试与素材的待完成项。本轮结果见 [框架检查记录](../test-records/2026-10-20-ideas-framework.md)。

依赖使用仓库现有的 `examples/esp-mosaico-vibe/submodule/esp-mosaico-utils`，不复制一套打包器。新机器应按 [官方例程说明](../../../examples/README.md)初始化指定提交和子模块；软件 SDK/BSP 保持 [来源清单](sources.json)中的诊断基线，本轮新增应用集成尚需重新构建验证。

## VS Code 编译与打包

1. 用官方 ESP-IDF 扩展打开本应用目录，选择 [软件 README](../README.md)中固定的 SDK 和 `esp32s31`。本机 `.vscode/settings.json` 已配置 `build` 目录及独立 `build/sdkconfig`；这份机器配置由 Git 忽略。
2. 使用 `ESP-IDF: Build Your Project` 编译。组件管理器会按新组件声明解析依赖并更新 `dependencies.lock`；首次成功构建后复核并保存锁文件。现有锁文件只代表此前诊断基线，不表示新应用依赖已经锁定完成。
3. 在扩展提供的 ESP-IDF 终端执行官方打包目标：

```powershell
idf.py --preview -D "ESP_IRIS_PYTHON=$env:ESP_IRIS_PYTHON" system-update-bundle
```

本机扩展环境已指向检查过依赖锁的 ESP-Iris Python。新机器如尚无宿主环境，可在同一终端按官方锁文件创建：

```powershell
$irisRoot = (Resolve-Path '../../examples/esp-mosaico-vibe/submodule/esp-mosaico-utils/ESP-Iris').Path
python -m venv .venv-iris
& ./.venv-iris/Scripts/python.exe -m pip install -r "$irisRoot/components/esp_iris/tools/requirements.lock"
$env:ESP_IRIS_PYTHON = (Resolve-Path './.venv-iris/Scripts/python.exe').Path
```

输出为 `build/gameplay-system-update.irisfw`。随后检查真实包、构建描述和有效 SDK 配置：

```powershell
python publish/validate.py --bundle build/gameplay-system-update.irisfw --iris-python "$env:ESP_IRIS_PYTHON"
```

该步骤检查包大小、签名状态、芯片、组件种类、版本一致性以及包内应用与本次构建镜像的 SHA-256。底层使用官方 `esp_iris.py bundle inspect`，不会连接设备。

## 分区与安装边界

官方固定区域为 `otadata`、`phy_init`、`sysmeta`、`factory` 和 `coredump`，保持在前 2 MiB。GamePlay 使用 `nvs`（0x200000、64 KiB）和 `ota_0`（0x210000、0xdf0000）。当前没有外部 Flash 资源分区，界面和诊断数据编入应用；未提供 ROM 或游戏素材。

官方打包器生成一个 application 和一个 partition_table，目标是 ESP32-S31、16 MiB、无签名；不包含 bootloader、Recovery 或平台身份。分区表按官方格式纳入包，不手工拼接镜像。`minimum_recovery_version` 由固定版本的官方产品契约生成，发布前对照实际设备验证。

官方集成会阻止普通 `flash` / `app-flash`，因为 factory 保存 Vibe。编译仍由 ESP-IDF 扩展完成；应用安装使用 Mosaico/ESP-Iris 对应流程，不能沿用旧诊断工程的完整 UART 烧录。此次只准备本地框架，没有执行设备安装或升级。

将来增加资源分区时，需要真实的镜像生成目标，并设置官方 `MOSAICO_SYSTEM_UPDATE_DATA_LABELS`、`MOSAICO_SYSTEM_UPDATE_DATA_<label>_IMAGE` 和 `_TARGET` 属性。当前检查器会拒绝未接入验证的资源分区；届时应同时扩展检查规则，不用空镜像消除提示。

## 网页资料与正式交付

分类使用平台现有的 display/audio；boards 暂为空，自研按键板不是平台 camera/ethernet/battery 枚举。封面最多 9 张，每张不超过 10 MiB；其他资料每份不超过 20 MiB，合计最多 100 文件/100 MiB；固件最多 32 MiB。元数据路径只能指向本应用目录内的真实文件。

发布前逐项完成：真实封面与截图、游戏功能说明、准确的源码提交与依赖锁、第三方清单、安装/启动/返回 Recovery 测试、显示/音频/按键和目标游戏实测。当前只有框架与既有诊断源码，不能写成游戏机已完成。

确认账号和创建/更新目标后，可直接使用已有官方 launcher。命令仅供之后执行，本轮没有登录或上传：

```powershell
$mosaico = '../../examples/esp-mosaico-vibe/submodule/esp-mosaico-utils/mosaico-tools/mosaico.py'
python $mosaico --workspace . account login --server https://mosaico-ideas.espressif.com --open-browser
python $mosaico --workspace . project upload --project . --bundle build/gameplay-system-update.irisfw --version (Get-Content version.txt).Trim() --server https://mosaico-ideas.espressif.com --create
```

已有项目用 `--update <实际 application_id>` 替换 `--create`；上传成功首先是草稿，不能表述为已经公开发布。上传时三处版本（构建、包内 release、指定 version）必须完全一致。
