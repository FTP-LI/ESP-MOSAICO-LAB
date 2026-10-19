# 游戏机套件开发快速开始

适用对象：ESP-Mosaico CoreBoard V1.2 / ESP32-S31。当前成果是显示、声音、触摸与按键诊断固件，以及 9 键扩展板原理图草案；项目保持 `experimental`，没有运行模拟器或完成设备实测。

## 获取工程

```powershell
git clone https://github.com/FTP-LI/ESP-MOSAICO-LAB.git
Set-Location ESP-MOSAICO-LAB
```

复现某次检查时，先切到 [构建记录](../../firmware/game-console/test-records/2026-10-19-build.md) 标注的源码提交。固件和原理图在同一仓库，按键映射发生变化时应一起更新。

| 要做的工作 | 文件入口 |
| --- | --- |
| 安装或激活 ESP-IDF | [tools/esp-idf/README.md](../../tools/esp-idf/README.md) |
| 构建、配置、诊断及烧录步骤 | [firmware/game-console/README.md](../../firmware/game-console/README.md) |
| 查看原理图、BOM 和恢复快照 | [hardware/gamepad-board/rev-0.1/README.md](../../hardware/gamepad-board/rev-0.1/README.md) |
| 讨论按键手感、布局和 H2 接口 | [interfaces.md](../../hardware/gamepad-board/rev-0.1/interfaces.md) |
| 查看已完成的软件检查 | [构建记录](../../firmware/game-console/test-records/2026-10-19-build.md) |
| 查看硬件待处理事项 | [设计检查](../../hardware/gamepad-board/rev-0.1/design-review.md) |
| 规划 FC、GB、PICO-8 移植 | [emulation-plan.md](emulation-plan.md) |

## 软件先构建

先按环境说明安装固定提交及其工具。已使用约定路径安装时，在仓库根目录执行：

```powershell
. ./tools/esp-idf/activate.ps1 -UseGlobalMirror
python tools/check-gamepad.py
Set-Location firmware/game-console
idf.py --preview build
```

预期结果：一致性检查输出 `PASS`，构建生成 `build/mosaico_diagnostics.bin`。这些命令不烧录设备。默认外接 GPIO 关闭，可先准备板载 AI/BOOT、触摸、色块和测试音的上板验证。

GitHub 保存源码、依赖锁文件和默认配置；ESP-IDF 安装目录、下载组件、生成的 sdkconfig、日志和固件二进制不进入版本控制。目前没有二进制 Release。

## 硬件同步确认

原理图使用 H2 的 9 路独立低有效按键，包含方向、A/B、Start/Select 和 Menu。先核对 BaseBoard 丝印、H2 Pin1、插接面、公母、高度、电平与供电，再做短线按键原型。按键中心距和固定孔先通过纸样确定，电气图的 3×3 排列不是实体布局。

当前仍有 10 项 DRC warning，尚未通过 API 定位到细节，开关和连接器封装也未冻结；**不可按此直接接线、采购装配或下单生产**。确认后的手柄构建配置见固件 README，不要将板载模式的通过结果当作外接手柄通过。

## 首次联调与后续推进

1. 登记设备原固件、恢复方式和 USB 串口；先用无外接按键板的配置验证主机显示、声音、触摸和 AI/BOOT。
2. 硬件条件确认后，关机连接按键原型，启用手柄配置，验证逐键、斜向加动作键、释放状态和去抖。
3. 记录实际现象、环境、源码提交及日志。只有实测通过的组合才能调整兼容性状态。
4. 再按运行方案移植首个 FC/GB 核心，验证存储、菜单与游戏控制，随后评估 PICO-8。

当前没有游戏运行速度、音频延迟、功耗或续航实测值。联调记录使用 [测试模板](../templates/test-record.md)，每次实测新建记录，保留已有构建记录。
