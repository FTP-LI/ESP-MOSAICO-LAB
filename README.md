# ESP-MOSAICO-LAB

基于 ESP-Mosaico 的非官方软硬件开发仓库。目前包含 **GamePlay V1.0** 游戏机和 **Time-Tick V1.0** 翻转计时器两个独立项目。

两个项目均处于实验阶段，名称中的 V1.0 不表示已发布稳定版。GamePlay 已有显示、声音、触摸和按键诊断，以及九键原理图和结构预布局，尚未接入游戏核心或完成整机。Time-Tick 已实现翻转倒计时、圆盘轻点/长按、可选铃声与翻转停铃、天气时钟、可配置昼夜主题、电量显示及呆猫陪伴；PC回归与官方ESP-IDF构建通过，已安装到实机。长期稳定性、一小时电池续航与整机验收仍待完成，语音助手归档至下一版本。

## 目录

```text
ESP-MOSAICO-LAB/
├── examples/                 官方工程和例程
├── firmware/
│   ├── GamePlay V1.0/        游戏机软件
│   ├── Time-Tick V1.0/       翻转计时器软件
│   └── LICENSE
├── hardware/
│   ├── GamePlay V1.0/        按键板、外壳与设计记录
│   ├── Time-Tick V1.0/       计时器硬件需求、结构与验证记录
│   ├── esp-mosaico/          主机硬件参考资料
│   ├── README.md
│   └── LICENSE
├── README.md
└── CHANGELOG.md
```

- [官方工程](examples/README.md)：Vibe、Claw、BSP 的本机参考副本与固定来源。
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

源码、固定依赖清单、BSP、原始素材、可直接构建的生成C数组、硬件设计、测试及功能归档进入Git。`build-*`、SDK/依赖缓存、下载的官方仓库、本机配置、日志、设备设置与宠物存档不上传。不要在源码中填写实际Wi-Fi密码或API密钥。

## 开发约定

软件与硬件使用同一项目名称；源文件、说明和检查工具就近放在项目内，硬件修订号单独保留。编译、烧录使用 VS Code 官方 ESP-IDF 扩展，并按各工程 README 选择固定 SDK。构建产物和依赖缓存不提交。

每项验证记录日期、版本、步骤、结果和未验证项。修改接口时同步更新针号表和固件映射；发布前核对源码版本、第三方来源、安装包及设备测试。各项目的具体发布要求见对应软件 README。

## 归档与许可

旧目录、包装脚本、模板和构建缓存已移至仓库外的同级归档 `ESP-MOSAICO-LAB-archive-2026-10-19-231502`，保留迁移清单、编辑前备份和校验信息。历史记录中的旧路径以当时快照为准。根目录另保留 Git 管理所需的 `.git`、`.gitignore`。

原创内容沿用已有 GPL v3：[软件许可](firmware/LICENSE)、[硬件及文档许可](hardware/LICENSE)。官方工程、BSP、原理图 PDF 和外部模型保留各自许可证与归属。
