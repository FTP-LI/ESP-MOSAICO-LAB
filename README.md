# ESP-MOSAICO-LAB

基于 ESP-Mosaico 的非官方软硬件开发仓库。目前包含 **GamePlay V1.0** 游戏机、**Time-Tick V1.0** 翻转计时器、**Desk-Deck V1.0** 桌面快捷控制台，以及 **Mic-Lab V1.0** USB 麦克风四个独立项目。

项目均处于实验或规划阶段，名称中的 V1.0 不表示已发布稳定版。GamePlay 已有显示、声音、触摸和按键诊断，以及九键原理图和结构预布局，尚未接入游戏核心或完成整机。Time-Tick 已实现翻转倒计时、圆盘轻点/长按、可选铃声与翻转停铃、天气时钟、可配置昼夜主题、电量显示及呆猫陪伴；PC回归与官方ESP-IDF构建通过，已安装到实机。长期稳定性、一小时电池续航与整机验收仍待完成，语音助手归档至下一版本。Desk-Deck V1.0 已提供四场景六键独立调试固件及网页原型，实机双 CDC + HID 枚举、截图、旋转/亮度组合与重启检查通过；复制/粘贴、修正后的正放/倒置和设置页触控已获用户确认，其他动作和长期验收仍待完成。

## 目录

```text
ESP-MOSAICO-LAB/
├── examples/                 官方工程和例程
├── firmware/
│   ├── Desk-Deck V1.0/            桌面快捷控制台固件与交互原型
│   ├── GamePlay V1.0/        游戏机软件
│   ├── Mic-Lab V1.0/         USB 麦克风、音量表与持久化短录音
│   ├── Time-Tick V1.0/       翻转计时器软件
│   └── LICENSE
├── hardware/
│   ├── Desk-Deck V1.0/            控制台板载复用与桌面结构需求
│   ├── GamePlay V1.0/        按键板、外壳与设计记录
│   ├── Mic-Lab V1.0/         板载音频、USB 与录音存储需求
│   ├── Time-Tick V1.0/       计时器硬件需求、结构与验证记录
│   ├── esp-mosaico/          主机硬件参考资料
│   ├── README.md
│   └── LICENSE
├── README.md
└── CHANGELOG.md
```

- [官方工程](examples/README.md)：Vibe、Claw、BSP 的本机参考副本与固定来源。
- [Mic-Lab V1.0](firmware/Mic-Lab%20V1.0/README.md)：USB 麦克风音频基础开发版，实时电平、增益、静音及 30 秒录音断电保存；DIY 主屏幕尚未实现，实机验收待完成；[换电脑开发交接](firmware/Mic-Lab%20V1.0/docs/HANDOFF.md)；[硬件需求](hardware/Mic-Lab%20V1.0/README.md)。
- [Desk-Deck V1.0 软件与原型](firmware/Desk-Deck%20V1.0/README.md)：Windows USB 快捷控制台调试版，四套场景、六键触控、翻转切页与本地可点击原型；[硬件需求](hardware/Desk-Deck%20V1.0/README.md)。
- [GamePlay V1.0 软件](firmware/GamePlay%20V1.0/README.md)：源码、ESP-IDF 配置、构建记录和 Mosaico Ideas 发布资料。
- [GamePlay V1.0 硬件](hardware/GamePlay%20V1.0/README.md)：按键板、接口、BOM、结构尺寸与设计源文件。
- [Time-Tick V1.0 软件](firmware/Time-Tick%20V1.0/README.md)：翻转计时、双语时钟天气、可调昼夜主题、呆猫专注陪伴、自动旋转和 Mosaico Ideas 发布框架；[电脑预览启动](firmware/Time-Tick%20V1.0/pc/README.md)。
- [Time-Tick V1.0 硬件](hardware/Time-Tick%20V1.0/README.md)：板载 IMU/屏幕/振动复用、姿态校准和结构验收要求。
- [主机资料与硬件状态](hardware/README.md)；[变更记录](CHANGELOG.md)。

## 克隆后开始

1. 克隆本仓库，选择 `firmware/Time-Tick V1.0` 或 `firmware/GamePlay V1.0` 作为VS Code工作区。
2. 按[官方依赖说明](examples/README.md)初始化固定版本Vibe及子模块；本仓库不重复打包上游完整工程。
3. 按所选项目README准备SDK。Time-Tick可先运行[电脑预览](firmware/Time-Tick%20V1.0/pc/README.md)，再通过官方系统更新包装入设备。
4. 当前能力与验证边界以项目README、`test-records/`、[变更记录](CHANGELOG.md)为准。

Desk-Deck V1.0 可直接运行 `python tools/preview.py` 查看模拟原型；固件另有独立工具链、依赖与构建入口，见[构建说明](firmware/Desk-Deck%20V1.0/docs/BUILD.md)。安装到同一设备会替换运行中的应用，保留系统保护区与其他应用设置，不修改 Time-Tick 工程。

源码、固定依赖清单、BSP、原始素材、可直接构建的生成C数组、硬件设计、测试及功能归档进入Git。`build-*`、SDK/依赖缓存、下载的官方仓库、本机配置、日志、设备设置与宠物存档不上传。不要在源码中填写实际Wi-Fi密码或API密钥。

## 开发约定

软件与硬件使用同一项目名称；源文件、说明和检查工具就近放在项目内，硬件修订号单独保留。编译、烧录使用 VS Code 官方 ESP-IDF 扩展，并按各工程 README 选择固定 SDK。构建产物和依赖缓存不提交。

每项验证记录日期、版本、步骤、结果和未验证项。修改接口时同步更新针号表和固件映射；发布前核对源码版本、第三方来源、安装包及设备测试。各项目的具体发布要求见对应软件 README。

## 归档与许可

旧目录、包装脚本、模板和构建缓存已移至仓库外的同级归档 `ESP-MOSAICO-LAB-archive-2026-10-19-231502`，保留迁移清单、编辑前备份和校验信息。历史记录中的旧路径以当时快照为准。根目录另保留 Git 管理所需的 `.git`、`.gitignore`。

原创内容沿用已有 GPL v3：[软件许可](firmware/LICENSE)、[硬件及文档许可](hardware/LICENSE)。官方工程、BSP、原理图 PDF 和外部模型保留各自许可证与归属。
