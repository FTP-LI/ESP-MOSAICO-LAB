# ESP-MOSAICO-LAB

面向 ESP-Mosaico 设备的**非官方扩展项目**，用于长期维护扩展板、配套固件、自动化程序和使用文档。本仓库不代表官方产品或官方支持功能。

当前已登记 ESP-Mosaico CoreBoard V1.2，并建立游戏机按键板原理图草案和显示、声音、按键诊断固件。状态为 `experimental`：两种诊断配置已编译通过，网表与固件映射核对通过，[原理图严格 DRC 已通过](hardware/gamepad-board/rev-0.1/checks/2026-10-19-drc.md)。母 PCB 文档名为 `GamePlay V1.0`，尚无布局；器件机械配合、烧录、打样和设备实测尚未完成。

## 先从这里看

- [游戏机套件开发快速开始：获取、构建与软硬件联调](docs/game-console-kit/quick-start.md)
- [当前设备与板载硬件](hardware/esp-mosaico/README.md)
- [诊断固件：显示、声音、板载与外接按键](firmware/game-console/README.md)
- [ESP-IDF 环境与激活脚本](tools/esp-idf/README.md)
- [按键板原理图、接口和布局](hardware/gamepad-board/README.md)
- [游戏机套件文件规划](docs/game-console-kit/README.md)
- [游戏机套件从哪里开始：硬件与软件并行开发](docs/game-console-kit/requirements.md)
- [FC、GB 与 PICO-8 运行方案](docs/game-console-kit/emulation-plan.md)
- [V1.2 设计分析与关键器件](hardware/esp-mosaico/rev-1.2/design-analysis.md)
- [官方原理图与规格图归档](hardware/esp-mosaico/rev-1.2/upstream/README.md)
- [左侧 H2 与右侧 H1 的区别、引脚和电源](hardware/esp-mosaico/rev-1.2/interfaces.md)
- [顶部 POWER、BOOT、AI KEY 按键](hardware/esp-mosaico/rev-1.2/buttons.md)
- [SAM8108 是什么、有哪些替代方案](hardware/esp-mosaico/rev-1.2/buttons.md#sam8108-与替代方案)
- [兼容性与验证状态](docs/compatibility.md)

## 目录结构

```text
ESP-MOSAICO-LAB/
├── README.md
├── REPOSITORY_GUIDELINES.md
├── CHANGELOG.md
├── LICENSE
├── hardware/                       硬件资料与扩展板
│   ├── gamepad-board/               rev-0.1 原理图、接口草案与 BOM
│   └── esp-mosaico/                 当前主机的参考资料
│       ├── README.md
│       └── rev-1.2/
│           ├── interfaces.md       左右接口与电源
│           ├── buttons.md          顶部按键、电源芯片与下载模式
│           ├── design-analysis.md  原理图分析与资料差异
│           ├── components.csv      关键器件表；非完整 BOM
│           ├── sources.json        来源、版本与文件校验值
│           └── upstream/           9 份原始 PDF 及归档说明
├── firmware/                       game-console 显示、声音与按键诊断
├── automation/                     自动化程序；待添加项目
├── tools/                          ESP-IDF 激活与网表/固件一致性检查
├── examples/                       独立示例；待添加示例
├── docs/                           跨项目索引与通用模板
│   ├── compatibility.md
│   ├── game-console-kit/           游戏机套件文件规划
│   └── templates/
└── .github/                        Issue、PR 模板及后续工作流
```

分类规则统一见 [仓库规范](REPOSITORY_GUIDELINES.md#2-目录结构与命名)。空的分类目录以 `.gitkeep` 保留；有实际项目后删除占位文件，建立 `<board-id>`、`<firmware-id>` 或 `<automation-id>` 子目录，并提供项目 README。

## 项目索引

| 类别 | 目录入口 | 当前内容与状态 |
| --- | --- | --- |
| 扩展板与硬件资料 | [hardware/](hardware/) | [主机参考资料](hardware/esp-mosaico/README.md)、[按键板 rev-0.1](hardware/gamepad-board/README.md)，均为 `experimental` |
| 配套固件 | [game-console](firmware/game-console/README.md) | 显示、声音、按键诊断；`experimental`，构建通过、实机未测 |
| 自动化程序 | [automation/](automation/) | 尚无自动化项目 |
| 烧录、诊断和配置工具 | [ESP-IDF 环境](tools/esp-idf/README.md) | 固定工具链激活；[网表一致性检查](tools/check-gamepad.py) |
| 独立使用示例 | [examples/](examples/) | 尚无可执行示例 |
| 跨项目文档 | [docs/](docs/) | [游戏机套件](docs/game-console-kit/README.md)，`experimental`（诊断与电气原型）；[兼容性索引](docs/compatibility.md)及通用模板 |
| GitHub 协作 | [.github/](.github/) | Issue、PR 模板；尚无工作流 |

新增项目时，在此索引中添加项目 README 链接与状态：`experimental`、`verified` 或 `deprecated`。`verified` 必须链接对应测试记录；未测试的设备和版本必须标为“未验证”。

## 快速开始

1. 阅读 [仓库规范](REPOSITORY_GUIDELINES.md)，了解目录、验证和发布要求。
2. 从 [设备说明](hardware/esp-mosaico/README.md) 了解主机，核对 [兼容性记录](docs/compatibility.md)。软件从 [诊断工程](firmware/game-console/README.md) 构建开始；硬件从 [接口草案](hardware/gamepad-board/rev-0.1/interfaces.md) 核对实物方向，尚不可直接接线。
3. 新建项目时，在对应类别下使用小写英文及连字符命名目录，将 [项目 README 模板](docs/templates/project-readme.md) 复制为该目录的 `README.md`，填写实际信息并加入根目录索引。
4. 验证时使用 [测试记录模板](docs/templates/test-record.md)；硬件项目另使用 [硬件检查记录模板](docs/templates/hardware-review.md)。没有执行的检查须标为“未执行”。

硬件设计按修订版保存，例如 `hardware/<board-id>/rev-1.0/`。只有存在实际内容时才建立目录；模板中的占位说明不能作为参数依据或测试结论。连接方式尚未确认时必须注明“不可按此接线”。

## 协作与发布入口

- [问题反馈](.github/ISSUE_TEMPLATE/bug-report.md)、[功能提案](.github/ISSUE_TEMPLATE/feature-request.md)、[PR 模板](.github/pull_request_template.md)。
- [变更记录](CHANGELOG.md)与[发布说明模板](docs/templates/release-notes.md)。当前尚未发布版本。
- 分支、提交、配置和验证要求统一见 [仓库规范](REPOSITORY_GUIDELINES.md)。已有本地构建及网表检查，尚无设备实测或 CI 工作流。

## 来源与许可

本仓库沿用已有的 [GNU GPL v3 许可证文本](LICENSE)。新增项目须在 README 中明确源码、硬件设计和文档的许可范围。

硬件分析由本仓库整理，文中标明来源与查询日期。官方原理图和供应商规格图集中在 [upstream 归档](hardware/esp-mosaico/rev-1.2/upstream/README.md)，保留原有归属与限制，不适用本仓库 GPL 许可；带明确分发限制的手册只保留官方链接。源码和其他图片通过链接引用。仓库规范及模板不替代许可证。
