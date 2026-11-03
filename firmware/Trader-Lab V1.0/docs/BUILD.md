# 构建、安装与调试

## 独立构建

在本工程目录运行：

```powershell
python tools/prepare_dependencies.py
./tools/build.ps1 -Bundle
python tools/verify_bundle.py
```

固定依赖：ESP-IDF `1c42e514c90ae621af33bde3b6ad3dac22edcbfd`，官方 utils `be151ae047014e79a9c7ae0092dd203844499003`，BSP `5f88b91e1be055d1d3674364123b2779a1758377`；其余组件见 `dependencies.lock`。本机工具链路径保存在 `tools/build.ps1`，Windows ESP-IDF 6.2 / esp32s31。

依赖只写本目录，不链接其他应用产物。生成的字体和 GBK 映射已保存，普通构建不用 Pillow。修改界面文字时运行 `python tools/generate_fonts.py`，需先解析 LVGL 并安装 Pillow。保持 `LV_FONT_FMT_TXT_LARGE` 启用，防止字库索引溢出。

输出为 `build-idf62/trader_lab.bin` 和 `build-idf62/trader_lab-system-update.irisfw`。

## 电脑验证

```powershell
./tools/test.ps1
python tools/probe_quotes.py --save
./tools/test.ps1 -LiveFixtures
./tools/check_ui.ps1
```

在线探测只读取公开报价，无个人账户。响应存在忽略目录 `tests/fixtures.local/`；在线 C 回归要求两个源均返回两只 ETF，失败不得替换成模拟价格。

界面检查编译同一 `main/main.c` 的 LVGL UI，生成 `build-ui/screens/*.bmp`，测试编辑、返回撤销、自选和提醒。主机替身不运行 ESP Wi-Fi／I2S／IMU，不能替代实机。

## 设备恢复后安装

设备需 Vibe Mode 0.2（此前已知为 0.2.1），恢复 ABI 2／布局 5。先正常开机连接 USB；当前应用没有 USB 通信时，关机后按住 AI 键开机进入 Vibe Mode。不执行整片擦除或直接 `idf.py flash`。

```powershell
$env:PYTHONUTF8 = '1'
$env:NO_PROXY = 'localhost,127.0.0.1,::1'
python mosaico.py iris run --project .
python mosaico.py iris device-status --json
python mosaico.py iris system-update --bundle build-idf62/trader_lab-system-update.irisfw
python mosaico.py iris logs --snapshot
python mosaico.py iris screenshot docs/images/device-home.png --json
python mosaico.py --json iris rpc 0x5452 1 --payload status
```

先记录当前应用和恢复版本；拒绝布局／协议时不强制绕过。官方更新包保留 Vibe Mode、Time-Tick／Desk-Deck 命名空间和身份信息。

本工程的 `main/sdk_compat.c` 恢复 ESP32-S31 在 PVT 模式下应开启的 REF160M 门控，兼容旧 Vibe Mode 0.2.1 的软件跳转。它必须链接在 IRAM，不能遗漏 `--wrap=esp_clk_tree_initialize`；没有修改共享 SDK。升级 IDF 或恢复固件时重新验证冷启动、普通重启和恢复模式返回应用。

RPC 服务 `0x5452`、方法 `1` 支持 `status`、`refresh`、`p0`–`p1`、`screen-test`、`stop`。屏幕测试做 24 轮旋转／亮度并恢复，不制造价格或真实提醒；状态不含密码。

启用 `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y`，将 TLS 分配放入 PSRAM，留出内部内存供 LCD 的 DMA 中转缓冲。保留证书校验和原显示时序；不能只查看绘制后的截图，需要检查启动及刷新期间是否出现显示 `ESP_ERR_NO_MEM`。

## 实机验收

1. 启动、Wi-Fi 扫描／输入／状态，PC 关闭后仍刷新；与同源同一时刻报价对照。
2. 错密码、断开 AP、重连、上游超时期间 UI 可操作，不用旧价格补报。
3. 初始 47 只／最多 64 只自选增删与重启恢复、12 项全球指数、四方向旋转和触控，设置保留草稿。
4. 65% 音量试听和振动；真实跨价需等交易时段，不把主机模拟计作实机证据。
5. 旋转／亮度、长时间内存、供电切换、电流与发热记录，确认后再评估发布。
