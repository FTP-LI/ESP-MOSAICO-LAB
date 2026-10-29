# Desk-Deck 0.1.0-dev 使用与安装指南

发布日期：2026-10-25。适用：ESP32-S31 版 ESP-Mosaico / Windows。

## 安装前

- 准备一根可以传输数据的 USB-C 线。
- 基础固件需要支持 Vibe Mode 0.2、Recovery ABI 2；本版实测为 0.2.1。
- 安装会替换设备上正在运行的普通应用，不会同时运行 Time-Tick。
- 使用官方应用安装入口，不用 ROM 擦除或整片烧录。包内仅包含同布局分区表和 Desk-Deck 应用，不包含 NVS 或恢复固件。

## 安装

从 GitHub Release 下载 `desk-deck-0.1.0-dev.irisfw`，或使用 Mosaico 对应项目的安装入口。文件 SHA-256：

`00c5152959add89523b814889447815290c1d069c26b69ce7cac926837ee3c23`

将设备关机，按住 AI 键开机，进入 Vibe Mode 后松开，保持 USB 连接。使用支持 `.irisfw` 的官方工具安装；源码用户可在 Desk-Deck 目录执行：

```powershell
python tools/prepare_dependencies.py
python mosaico.py iris run --project .
# 另一终端中，将路径替换为下载的安装包位置：
python mosaico.py iris system-update --bundle <desk-deck-0.1.0-dev.irisfw的路径>
python mosaico.py iris device-status --json
```

安装成功后设备自动启动，首页应显示「USB 就绪」。如果 USB 未就绪，请检查数据线和电脑 USB 连接；仅充电或电池供电不能向电脑发送按键。首次枚举请等待 Windows 完成识别。

## 操作

| 动作 | 结果 |
| --- | --- |
| 轻点按钮 | 向当前电脑应用发送一次快捷键 |
| 按住按钮约 0.6 秒 | 查看说明，不发送快捷键 |
| 左右滑动/点击底部标签 | 切换四个场景 |
| 正放/右转/倒置/左转，保持稳定 | 切换常用/开发/办公/媒体场景并旋转画面 |
| 平放 | 保留上一次屏幕方向和场景 |
| 点击锁定/长按解锁 | 暂停/恢复控制台操作 |
| 设置中拖动亮度 | 立即预览，返回撤销，保存后保留 |

关闭「翻转切换场景」后，屏幕仍会跟随姿态旋转；场景改用滑动或标签选择。设置页旋转不发送快捷键。轻触振动、日间主题和翻转开关在保存后生效。

## 默认快捷键

| 场景 | 操作与键位 |
| --- | --- |
| 常用 | 复制 Ctrl+C；粘贴 Ctrl+V；截图 Win+Shift+S；撤销 Ctrl+Z；重做 Ctrl+Y；显示桌面 Win+D |
| 开发 | 运行调试 F5；停止 Shift+F5；终端 Ctrl+反引号；格式化 Shift+Alt+F；全局搜索 Ctrl+Shift+F；命令面板 Ctrl+Shift+P |
| 办公 | 保存 Ctrl+S；查找 Ctrl+F；全选 Ctrl+A；新建标签 Ctrl+T；恢复标签 Ctrl+Shift+T；切换窗口 Alt+Tab |
| 媒体 | 上一曲、播放/暂停、下一曲、静音、音量减、音量加，均为标准 Consumer Control 报告 |

先把目标软件置于前台。开发页面向 VS Code 默认键位，浏览器动作需要在浏览器中使用。「已发送」是 USB 报告完成状态，不是电脑应用的执行回执。

## 当前版本边界

已实测复制/粘贴、正放/倒置、设置页旋转触控；安装、USB 枚举、恢复、重启与显示组合检查通过。所有 24 项动作尚未逐项验收，媒体兼容性和长时间运行仍待验证。

按键配置目前在源码 `configs/default-profiles.json` 中，修改后需要重新构建。网页原型中的编辑/导出只是模拟，不支持写入设备。本版没有 Wi-Fi 配置、蓝牙 HID、AI、自动识别电脑前台应用或多步宏。

## 恢复和反馈

按住 AI 键开机进入 Vibe Mode，可重新安装本应用或其他已验证应用。不要为恢复应用擦除整片 Flash。

反馈时提供版本、Windows/目标软件、所处场景、具体动作与复现步骤；不要提交账号密钥或整份私人电脑日志。

[项目源码与详细构建说明](https://github.com/FTP-LI/ESP-MOSAICO-LAB/tree/desk-deck-v0.1.0-dev/firmware/Desk-Deck)
