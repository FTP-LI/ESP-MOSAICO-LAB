# Desk-Deck 0.1.0-dev · 首个开发预览

把 ESP-Mosaico 变成 Windows USB 快捷键面板：常用、开发、办公和媒体四页，每页六键，支持翻转与触控切换。

- 六键中文界面、长按说明、锁定、亮度即时预览与保存、日间主题和轻振动。
- USB 键盘/媒体 HID 与官方双 CDC 调试通道共存。
- 修正 IMU 采样启动与正放/倒置映射，保留平放方向与翻转防抖。
- 本次发布保持实物验收固件不变；安装包 666,420 字节，适配 Vibe Mode 0.2 / ABI 2，实测 0.2.1。

验证：官方更新和健康检查、枚举、截图、24 步旋转/亮度组合、恢复和重启通过；用户确认复制/粘贴、正放/倒置及设置页触控正常。

已知限制：按键固定于编译期，网页配置尚未连接设备；其他动作的逐项兼容性、长期运行、USB 拔插/挂起恢复、功耗及温升仍待验证。此版本标为预发布。

下载 `.irisfw` 为可安装固件，`-release.zip` 为含固件和介绍资源的 Mosaico 发布包。请先阅读随附 `desk-deck-guide.md`，用 `SHA256SUMS.txt` 核验文件。

源码：[Desk-Deck](https://github.com/FTP-LI/ESP-MOSAICO-LAB/tree/desk-deck-v0.1.0-dev/firmware/Desk-Deck)。许可：GPL-3.0-only，第三方许可见工程 THIRD_PARTY.md。
