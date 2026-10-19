# ESP-IDF 环境

为 ESP-Mosaico V1.2 的 ESP32-S31 诊断工程固定工具链。状态为 `experimental`；本机 Windows 已安装并激活，设备运行尚未验证。环境脚本只作用于当前 PowerShell 会话，不修改系统 PATH。

## 固定版本与本机路径

| 项目 | 当前基准 |
| --- | --- |
| ESP-IDF | 开发版 6.2.0，提交 `1c42e514c90ae621af33bde3b6ad3dac22edcbfd` |
| IDF_PATH | `C:\Espressif\esp-idf-v6.2-mosaico\esp-idf` |
| IDF_TOOLS_PATH | `C:\Espressif\tools` |
| Python 环境 | `C:\Espressif\tools\python\esp-idf-v6.2-mosaico\venv`，Python 3.12.10 |
| RISC-V GCC | `esp-16.1.0_20260609`，GCC 16.1.0 |
| BSP | `5f88b91e1be055d1d3674364123b2779a1758377`，随工程保存 |
| 目标 | `esp32s31`，IDF 命令带 `--preview` |

以上是开发分支固定提交，不是 ESP-IDF 6.2 稳定发布版。使用 [官方源码](https://github.com/espressif/esp-idf/tree/1c42e514c90ae621af33bde3b6ad3dac22edcbfd) 与其 `tools/tools.json`，不要仅按版本名称选择编译器。

## 使用已有环境

```powershell
. ./tools/esp-idf/activate.ps1 -UseGlobalMirror
idf.py --version
riscv32-esp-elf-gcc --version
```

脚本先检查 IDF 提交是否相同，再调用官方 `export.ps1` 选择匹配工具。可用 `-IdfPath`、`-ToolsPath`、`-PythonEnv` 指向其他安装位置。退出终端即可结束本会话环境。

默认组件存储镜像为 `components-file.espressif.cn`，`-UseGlobalMirror` 改用 `.com`。此次 CN 下载成功、随后元数据请求出现 TLS 错误，最终构建使用 `.com`。遇到下载失败可切换镜像重试；不关闭证书验证。镜像变量依据 [组件管理器官方配置文档](https://docs.espressif.com/projects/idf-component-manager/en/latest/use/how_to_configuration.html)。

## 在新机器复现

安装 Git、Python 3.12 和官方 ESP-IDF 安装工具后，取上述固定提交及其递归子模块，再按该提交安装工具。以下操作在新的专用安装目录执行，路径可按机器调整：

```powershell
git clone --no-checkout https://github.com/espressif/esp-idf.git C:/Espressif/esp-idf-v6.2-mosaico/esp-idf
git -C C:/Espressif/esp-idf-v6.2-mosaico/esp-idf checkout --detach 1c42e514c90ae621af33bde3b6ad3dac22edcbfd
git -C C:/Espressif/esp-idf-v6.2-mosaico/esp-idf submodule update --init --recursive
$env:IDF_TOOLS_PATH = 'C:\Espressif\tools'
$env:IDF_PYTHON_ENV_PATH = 'C:\Espressif\tools\python\esp-idf-v6.2-mosaico\venv'
python C:/Espressif/esp-idf-v6.2-mosaico/esp-idf/tools/idf_tools.py install --targets esp32s31
python C:/Espressif/esp-idf-v6.2-mosaico/esp-idf/tools/idf_tools.py install-python-env
```

然后使用本仓库 `activate.ps1` 并按 [固件说明](../../firmware/game-console/README.md) 构建。不要直接覆盖其他项目正在使用的 IDF checkout。组件锁文件应保留；本地 BSP 更新需要重新审核引脚与依赖。

本机初次 EIM 0.14.1 镜像安装得到较旧的源码及 GCC 15.2；现已切到上述固定提交、补齐子模块，并由官方 `idf_tools.py` 安装 GCC 16.1。EIM 留下的旧 PowerShell profile 不作为本项目入口；使用仓库激活脚本可避免混用。

## 检查范围

Python 依赖检查、工具激活、诊断工程构建与硬件网表检查见 [验证记录](../../firmware/game-console/test-records/2026-10-19-build.md)。安装工具链不会自动烧录设备。

本目录原创脚本与文档沿用根目录 GPL v3；ESP-IDF、安装工具和依赖遵循各自许可证。
