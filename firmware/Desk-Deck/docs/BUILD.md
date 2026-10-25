# 构建与实机调试

Desk-Deck 使用自身的依赖、构建目录和 NVS 命名空间 `deskdeck`。网页原型仍可独立运行，不会发送电脑快捷键。

## 工具链

- ESP-IDF：`1c42e514c90ae621af33bde3b6ad3dac22edcbfd`，ESP32-S31，RISC-V GCC 16.1.0。
- esp-mosaico-utils：`be151ae047014e79a9c7ae0092dd203844499003`。
- esp-mosaico-bsp：`5f88b91e1be055d1d3674364123b2779a1758377`。
- 注册表组件固定于 `dependencies.lock`。
- 官方基础固件：Vibe Mode 0.2.1，`esp-mosaico/v2` / recovery ABI 2。

在项目目录执行：

```powershell
python tools/prepare_dependencies.py
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Bundle
```

`build.ps1` 使用本机 `C:/Espressif` 下的工具链；其他电脑需先修改对应路径。它生成 `main/profiles.inc`，再构建 `build-idf62/desk_deck-system-update.irisfw`，不会自动烧录。

中文使用 `main/deck_font.c` 中的 16/20px 字库子集，字体位图共约 60KB。常规构建直接使用已生成文件；修改界面用字后，先安装 Python Pillow，再运行 `python tools/generate_fonts.py`。字体来源于本项目固定 LVGL 依赖中的 Source Han Sans，许可随项目保存在 `docs/licenses/SourceHanSansSC-OFL.txt`。

## 安装

关闭设备，按住 AI 键开机进入 Vibe Mode，再松开。使用数据线连接电脑。在项目目录用官方入口检查、安装：

```powershell
python mosaico.py iris run --project .
# 在另一终端执行：
python mosaico.py iris device-status --json
python mosaico.py iris system-update --bundle build-idf62/desk_deck-system-update.irisfw
python mosaico.py iris device-status --json
```

同一设备只运行一个正常应用。安装 Desk-Deck 会替换原应用；正常安装包不包含 bootloader、Vibe、sysmeta、coredump、NVS 数据镜像，不整片擦除。要恢复其他应用，进入 Vibe Mode 后用对应应用自己的官方入口和已验证安装包恢复。

## USB 结构

Iris 仍是唯一 TinyUSB 安装者。`main/usb_hid.c` 通过链接器包装 `tinyusb_driver_install`，检查并扩展其配置描述符；不修改固定版本依赖。

- 接口 0–3：保留官方两组 CDC 及其端点。
- 接口 4：HID，IN 端点 `0x85`；报告 1 为键盘，报告 2 为 Consumer Control。
- ESP32-S31 DWC2 HS 控制器有 16 个端点编号、8 个 IN 端点；本配置使用 5 个非控制 IN 端点。
- 同时生成全速及高速配置，主机以实际枚举结果为准。
- 待发送队列上限 4；断连/挂起清空。每次按下约 25ms 后发送释放。重连先发送两类空报告，再允许操作。
- 设备“已发送”表示报告已完成，不代表电脑当前应用已执行该快捷键。

## 调试

```powershell
python mosaico.py iris logs --snapshot
python mosaico.py iris device-status --json
python mosaico.py iris screenshot docs/images/device-home.png --json
python mosaico.py iris rpc --help
```

应用诊断 RPC 服务 `0x4444`、方法 `1` 接收 UTF-8 文本：`status` 查询 HID、方向、IMU 和内部堆；`p0` 至 `p3` 切换场景；`screen-test` 执行 24 次方向/亮度组合；`stop` 停止显示测试。这些命令不会发送快捷键。显示测试结束后恢复保存的亮度和实物方向，请勿在测试中操作设置。

需要直接输出诊断 JSON 时用 `python mosaico.py --json iris rpc 0x4444 1 --payload status`，响应中的 `payload_base64` 为 UTF-8 JSON。当前未开启逐任务内存观察，`iris memory` 会返回 `not_supported`，使用 status 中的堆空间读数。

当前固件使用四套编译期默认动作。电脑配置导入、远程修改动作和断电回读验证仍属于后续配置工具阶段。
