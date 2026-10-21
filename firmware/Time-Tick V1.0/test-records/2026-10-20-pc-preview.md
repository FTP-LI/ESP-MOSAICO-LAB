# Time-Tick PC 预览验证（2026-10-20）

## 像素田园改版补验（当前版本）

- 时钟、倒计时、天气卡、设置页和屏幕键盘统一为木框/纸色/叶绿风格。原创像素场景和字形使用 LVGL 绘制，PC 与固件共用。
- 最终源码 `cmake --build build-pc --parallel 8` 成功；`ctest --test-dir build-pc --output-on-failure` **2/2 通过**。渲染检查增加 24 小时、负温度、长城市名、长状态和不同天气图标；设置检查按按钮文字寻找控件，不再依赖装饰对象顺序。
- VS Code F5 实际启动改版预览。实窗检查时钟/天气、设置表单与数值键盘；Right 270° 旋转后触摸 Pause，确实暂停并显示 Resume；Home 恢复 0°；五秒 Flat A 到时显示 `00:00` 和金色提示，Stop 清除提示。
- 以官方 `idf.py --preview --no-ccache --no-configdep -B build-idf62 build` 编译最终共享源码成功。镜像 **1,868,512 字节**，SHA-256 `4e510decb272515fb818a69e6ded324f337d852972abd9614cd5f9db4b3ea559`。
- 通用 factory 分区大小警告仍存在；本项目继续使用既定 Iris / ota_0 安装路径。本轮未打包、烧录或上传，实机显示性能与触摸体验尚待验证。

以下是改版前预览接入的原始记录，旧镜像哈希仅用于保留验证历史。

## 环境与范围

- Windows x64，MinGW-w64 GCC 16.1.0，CMake/Ninja。
- LVGL 9.5.0 直接复用固件组件；SDL2 2.32.10 官方 MinGW 开发包，SHA-256：`f15cff5fca62ec9381a016ef1d42a95c638cd72d2f226ba5781c76fe43dbd1ac`。
- VS Code 中按 F5，实际执行 Configure/Build PC preview 任务并进入 GDB 调试，出现设备与控制窗口。
- 用户选择“稍后连接，先完成预览”。串口检查仅发现 COM1；没有烧录、安装或上传。

## 自动检查

`ctest --test-dir build-pc --output-on-failure`：**2/2 通过**。

1. `core`：实际计时/姿态核心测试，包括方向校准组合、稳定判定、时间策略、暂停、停止、到时及边界条件。
2. `preview_smoke`：在 SDL dummy 视频驱动中运行实际 LVGL，绘制六面、旋转、告警、离线和设置页；通过真实 UI 回调验证超限 86401 秒拒绝保存、5 秒接受保存并返回、Back 返回。

PC 应用源码以 `-Wall -Wextra -Werror` 编译。首次检查发现 Windows 的 `localtime_s` 接口差异，以及 PC 默认 printf 不支持浮点；已分别加入平台适配并将 PC 的 LVGL sprintf 配置对齐固件的 C 库实现。

## 实际窗口交互

- 控制窗口选择 Right：模拟 +X 加速度通过共享稳定判定，进入 Right，画面旋转为 270°，倒计时约 300 秒。
- 在旋转画面点击 Pause/Resume：确实暂停，剩余时间保持；说明该方向的鼠标/触摸坐标转换可用。
- 回到 Home：恢复时钟，显示 Paused 与剩余时间。
- 再次选择 Right：剩余时间从约 271 秒重新回到约 300 秒；运行中返回 Home 自动暂停。
- 打开 Settings：发现深色面板标签对比度不足，已修正共享 `ui.c` 的标签和复选框文字颜色，重编译后可清楚阅读。
- 点击 Right 秒数输入框，实际使用屏幕键盘清空 300、输入 5、点击 Save；返回时钟并提示当前会话已保存。
- 再次 Right：控制窗口显示约 4816 ms，随后设备窗口显示 `Time is up! / 00:00 / Finished`，背景切换告警色。
- 点击旋转后的 Stop/Dismiss：告警消失，恢复时钟与“翻到另一方向启动”的提示。

## 尚需实物验证

共享源码修改后，使用同一套 ESP-IDF 6.2 / GCC 16.1 官方工具链重新完整编译通过。实际命令为 `idf.py --preview --no-ccache --no-configdep -B build-idf62 build`；本次复编通过终端直接调用官方 `idf.py`，电脑预览则已经从 VS Code F5 启动验证。EIM 环境发现的 configdep 0.2.3 对含空格目录产生错误转义，因此此次显式关闭该可选构建加速器，保留普通增量构建。

- 新固件大小：1,955,760 字节。
- SHA-256：`12f03a3d61dcd8360cb724c7f19f146687b84060562537ef07c36f9e1c4ee347`。
- 官方发布框架校验通过；没有生成 `.irisfw`。
- 通用 IDF 输出提示 factory 分区不足，本应用应安装至预留的 `ota_0`；不要照通用输出执行 flash，仍遵循项目既定的官方 Iris 打包/安装流程。

BMI270 实际轴向、晃动阈值、面板及触摸方向、振动、Wi-Fi/SNTP/HTTPS 天气、NVS 跨重启保存及电源行为。PC 使用虚拟校准、电脑时间、示例天气和会话内设置，不能作为这些硬件功能已验收的证据。
