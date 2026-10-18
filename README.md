# ESP-MOSAICO-LAB

面向 ESP-Mosaico 设备的**非官方扩展项目**，用于长期维护扩展板、配套固件、自动化程序和使用文档。本仓库不代表官方产品或官方支持功能。

当前已登记 ESP-Mosaico CoreBoard V1.2，并整理主机硬件、左右扩展接口及顶部按键。资料状态为 `experimental`：已核对官方文档，尚未完成设备实测；目前没有自研扩展板、固件或自动化程序。

## 先从这里看

- [当前设备与板载硬件](hardware/esp-mosaico/README.md)
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
│   └── esp-mosaico/                 当前主机的参考资料
│       ├── README.md
│       └── rev-1.2/
│           ├── interfaces.md       左右接口与电源
│           ├── buttons.md          顶部按键、电源芯片与下载模式
│           ├── design-analysis.md  原理图分析与资料差异
│           ├── components.csv      关键器件表；非完整 BOM
│           ├── sources.json        来源、版本与文件校验值
│           └── upstream/           9 份原始 PDF 及归档说明
├── firmware/                       配套固件；待添加项目
├── automation/                     自动化程序；待添加项目
├── tools/                          烧录、诊断与配置工具；待添加工具
├── examples/                       独立示例；待添加示例
├── docs/                           跨项目索引与通用模板
│   ├── compatibility.md
│   └── templates/
└── .github/                        Issue、PR 模板及后续工作流
```

分类规则统一见 [仓库规范](REPOSITORY_GUIDELINES.md#2-目录结构与命名)。空的分类目录以 `.gitkeep` 保留；有实际项目后删除占位文件，建立 `<board-id>`、`<firmware-id>` 或 `<automation-id>` 子目录，并提供项目 README。

## 项目索引

| 类别 | 目录入口 | 当前内容与状态 |
| --- | --- | --- |
| 扩展板与硬件资料 | [hardware/](hardware/) | [ESP-Mosaico V1.2 参考资料](hardware/esp-mosaico/README.md)，`experimental` |
| 配套固件 | [firmware/](firmware/) | 尚无固件项目 |
| 自动化程序 | [automation/](automation/) | 尚无自动化项目 |
| 烧录、诊断和配置工具 | [tools/](tools/) | 尚无工具程序 |
| 独立使用示例 | [examples/](examples/) | 尚无可执行示例 |
| 跨项目文档 | [docs/](docs/) | [兼容性索引](docs/compatibility.md)及通用模板 |
| GitHub 协作 | [.github/](.github/) | Issue、PR 模板；尚无工作流 |

新增项目时，在此索引中添加项目 README 链接与状态：`experimental`、`verified` 或 `deprecated`。`verified` 必须链接对应测试记录；未测试的设备和版本必须标为“未验证”。

## 快速开始

1. 阅读 [仓库规范](REPOSITORY_GUIDELINES.md)，了解目录、验证和发布要求。
2. 从 [设备说明](hardware/esp-mosaico/README.md) 了解当前主机，核对 [兼容性记录](docs/compatibility.md)。接口资料用于查阅，尚无本仓库验证过的接线方案或可烧录固件。
3. 新建项目时，在对应类别下使用小写英文及连字符命名目录，将 [项目 README 模板](docs/templates/project-readme.md) 复制为该目录的 `README.md`，填写实际信息并加入根目录索引。
4. 验证时使用 [测试记录模板](docs/templates/test-record.md)；硬件项目另使用 [硬件检查记录模板](docs/templates/hardware-review.md)。没有执行的检查须标为“未执行”。

硬件设计按修订版保存，例如 `hardware/<board-id>/rev-1.0/`。只有存在实际内容时才建立目录；模板中的占位说明不能作为参数依据或测试结论。连接方式尚未确认时必须注明“不可按此接线”。

## 协作与发布入口

- [问题反馈](.github/ISSUE_TEMPLATE/bug-report.md)、[功能提案](.github/ISSUE_TEMPLATE/feature-request.md)、[PR 模板](.github/pull_request_template.md)。
- [变更记录](CHANGELOG.md)与[发布说明模板](docs/templates/release-notes.md)。当前尚未发布版本。
- 分支、提交、配置和验证要求统一见 [仓库规范](REPOSITORY_GUIDELINES.md)。当前尚无项目构建、设备测试或自动检查工作流。

## 来源与许可

本仓库沿用已有的 [GNU GPL v3 许可证文本](LICENSE)。新增项目须在 README 中明确源码、硬件设计和文档的许可范围。

硬件分析由本仓库整理，文中标明来源与查询日期。官方原理图和供应商规格图集中在 [upstream 归档](hardware/esp-mosaico/rev-1.2/upstream/README.md)，保留原有归属与限制，不适用本仓库 GPL 许可；带明确分发限制的手册只保留官方链接。源码和其他图片通过链接引用。仓库规范及模板不替代许可证。
