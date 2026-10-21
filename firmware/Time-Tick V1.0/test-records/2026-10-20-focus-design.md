# 2026-10-20：田园主题倒计时表盘

## 改动

- 新增 `main/ui/ui_focus.c/.h`，PC 和设备共用同一组件。奶油白底、圆形表盘、细刻度、枝叶与沙漏均由 LVGL 绘制，不增加图片文件或独立动画循环。
- 大数字下显示方向及预设，外环和百分比表示剩余时间；运行用鼠尾草绿，暂停用暖棕色，停止待启用用灰绿色，到时用金色与完成勾选图标。状态始终同时显示文字。
- 暂停/继续、停止、开始、结束提醒保留原命令；设置入口和正放提示保留。通常使用 84 px 数字，100 分钟及以上使用 52 px，支持 `1440:00`。
- 中英文文案同步补齐并重生成字体。翻转计时、正放暂停、养成低打扰和闹钟优先规则保持原设计。

## 自动与视觉验证

Windows MinGW GCC、SDL2 2.32.10、LVGL 9.5.0，`-Wall -Wextra -Werror` 编译通过。CTest **8/8 通过**：

- `core`、`settings`、`pet`、`animation`、`weather`。
- `preview_smoke`、`preview_configured`、`preview_timer`。

`preview_timer` 实际经过模拟 IMU 滤波、各方向预设及旋转、暂停/继续、正放暂停与再次翻转重启、停止/手动开始、禁用方向、养成/设置上的到时提醒、设置草稿保留、运动/校准限制。新增推进单调时间 60 秒后检查 `04:00` 和 `Remaining 80%`；最长24小时检查按实际字号测量数字能完整显示。

从真实 SDL/LVGL 渲染器导出并检查了中英文倒计时、剩余80%、中文暂停、中文到时和最长24小时。检查文字、数字、进度环、按钮及边缘刻度。评审图片临时转到正向便于阅读，导出后立即恢复原显示旋转；实际设备方向逻辑未改变。

可在正常 PC 构建完成后复现：

```powershell
New-Item -ItemType Directory -Force build-assets/timer-proof
$env:TT_PREVIEW_PROOF_DIR=(Resolve-Path build-assets/timer-proof).Path
ctest --test-dir build-pc -R preview_timer --output-on-failure
Remove-Item Env:TT_PREVIEW_PROOF_DIR
```

产物是 `build-assets/timer-proof/` 下的 BMP；本轮另转换为 PNG 便于查看，包括 `countdown-zh`、`progress-zh`、`paused-zh`、`alarm-zh`、`maximum-duration`。这是共用源码的运行画面，不是概念效果图。测试使用 dummy 视频驱动，不操作桌面、不读写日常桌宠存档。

本轮旧预览仍在运行并占用 exe，因此临时把 PC 构建输出定向到 `build-pc/check/` 完成检查，随后已清除 CMake 输出覆盖。**关闭旧预览后在 VS Code 按 F5**，会增量构建到正常位置并加载新界面；桌宠存档保留。没有强制结束旧进程。

## 官方固件构建

使用项目固定的 ESP-IDF 6.2 开发版 `1c42e514c90ae621af33bde3b6ad3dac22edcbfd`、RISC-V GCC 16.1、目标 `esp32s31`：

```powershell
idf.py --preview --no-ccache --no-configdep -B build-idf62 build
python publish/validate.py
```

编译与发布框架检查通过。`build-idf62/time_tick.bin`：

- 大小：10,128,432 字节，较上一轮增加 4,368 字节。
- SHA-256：`9323aa77e318247df919432cd193a2425950fb92d54e56695d59440c11118468`。
- `ota_0`：偏移 `0x210000`，容量14,614,528字节，剩余4,486,096字节。

SDK 仍提示其开发版依赖/Kconfig 项，以及通用 factory 区域容量提示。factory 为受保护的 Vibe 区域，本应用适用于官方应用安装流程中的 ota_0；没有执行普通 flash、擦除或烧录。

## 验证边界

本轮没有连接实物，实际屏幕色彩、绘制帧率、触摸、IMU 和振动待实机验收。PC 到时反馈仅为视觉显示。发布检查只覆盖框架，`.irisfw`、发布封面和正式上传尚未完成。先前功能验证见 [计时优先记录](2026-10-20-timer-first.md)。
