# 构建与检查

目标为 ESP32-S31，使用 ESP-Mosaico 对应 ESP-IDF 6.2 开发版本。不能直接以 ESP32-S3 目标替代。所有命令从本工程目录执行。

## 固定依赖

| 来源 | 版本 |
| --- | --- |
| esp-mosaico-utils | `be151ae047014e79a9c7ae0092dd203844499003` |
| esp-mosaico-bsp | `5f88b91e1be055d1d3674364123b2779a1758377` |
| ESP-IDF | `1c42e514c90ae621af33bde3b6ad3dac22edcbfd` |

```powershell
python tools/prepare_dependencies.py
powershell -ExecutionPolicy Bypass -File tools/build.ps1 -Bundle
```

`prepare_dependencies.py` 克隆本项目专用 `build-deps/` 并核对提交，不修改其他项目。组件版本保存在 `dependencies.lock`。`tools/build.ps1` 默认保留原工作站的 `C:/Espressif` 路径；**换电脑时先激活固定 SDK，再使用 `-UseEnvironment`**，不会覆盖新电脑的 SDK、工具或 Iris 解释器位置。完整安装和接续步骤见 [HANDOFF.md](HANDOFF.md)。脚本核对 ESP-IDF 提交，防止误用 ESP32-S3 或其他 SDK。

构建目录 `build-idf62/`；更新包由官方 `system-update-bundle` 生成。构建脚本不会烧录。应用沿用原分区布局，不打包 NAND 数据，不擦除 NVS；安装前需按官方 Vibe Mode 流程确认更新包与当前设备匹配。

## 电脑测试

先完成一次构建以获取固定 TinyUSB/LVGL 组件。将 MinGW GCC、CMake 和 Ninja 加入 PATH：

```powershell
powershell -ExecutionPolicy Bypass -File tools/test.ps1
cmake -S tests -B build-ui -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug
cmake --build build-ui
New-Item -ItemType Directory -Force build-ui/screens
Push-Location build-ui/screens
../ui_check.exe
Pop-Location
```

四组逻辑测试使用 `-Wall -Wextra -Werror`，验证音频状态、录音文件、USB 描述符及控制请求/FIFO。LVGL 检查使用实际 `mic_ui.c` 与字库，模拟音频/USB 状态，输出 BMP 并检查按钮行为。它不证明真实音频、USB 时序或触控硬件正常。

字库已提交，常规构建无需重新生成。修改中文文字后，安装 Pillow 并运行 `python tools/generate_fonts.py`，然后重新检查界面。

## 诊断接口

Iris RPC 服务 `0x4d4c`、方法 `1`，请求正文 ASCII `status`，只读返回模式、电平、增益、静音、录音样本数、持久化状态、存储/音频错误、USB 激活、写入丢弃样本及短包计数。FIFO 禁止覆盖，丢弃数统计未接受的样本；短包指不足 47 个样本的传输，含启动或主机额外轮询，不是完整欠载计数。固定 TinyUSB 在少量边界情形会提交比实际读取量多一个样本的长度，因此计数为零也不能证明音频连续，必须检查电脑实际录音。屏幕截图复用 Iris 后端。
