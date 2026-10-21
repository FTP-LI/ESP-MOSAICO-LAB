# gamepad-board rev-0.1

状态：`experimental`，电气原型草案。未完成设备验证、PCB、板框、连接器配合或生产审查，**不可按此接线或下单生产**。

嘉立创EDA 工程 `ESP-MOSAICO-LAB` 的原 `Schematic1 / P1` 已改名为 `gamepad-board-rev-0.1 / 01-buttons-and-host-interface`。页面包含 J1、SW1–SW9、R1–R18、网络标记和实验版说明。原有页面只有图框，修改前源快照保存在 `eda/snapshots/before-2026-10-19.json`。

已直接在母版修复全部 10 项原理图 DRC 警告，严格检查通过；临时诊断副本已删除。原母 PCB 已命名为 `GamePlay V1.0`，UUID 与关联原理图保持不变。该 PCB 已放置 J1 弯排针候选和文档层纵向尺寸布局；尚无完整电路布局，文档名中的 V1.0 不表示生产版发布，硬件资料仍归属未发布的 rev-0.1。

后续形态已确定为纵向优先、横向保留，主游戏键采用硅胶导电胶方向，详见 [掌机形态设计](../../gamepad-enclosure/form-factor.md)。母版 J1 已通过 API 改为 C492438 弯排针，选用 FunnyPlaying GBC 胶垫和配套黑色键帽进行样品验证。STL 尺寸、72×112 mm 纵向布局见 [结构资料](../../gamepad-enclosure/rev-0.1/README.md)。SW1–SW9 仍为轻触开关电气原型，尚未替换为最终铜触点。

## 文件与恢复

- [eda/gamepad-board-rev-0.1.epro2](eda/gamepad-board-rev-0.1.epro2)：API 导出的原生工程，包含原理图、器件库、含预布局的 PCB `GamePlay V1.0` 和 Panel1。扩展名已按 API 实际格式纠正；母版后续直接修改，归档用于版本追溯。
- `eda/gamepad-board-rev-0.1.esch`：当前图页的原始源文本，用于审计；不是完整工程容器。
- `eda/gamepad-board-rev-0.1.net`：嘉立创EDA 导出的 JSON 网表，用于核对实际电气连接。
- `eda/GamePlay-V1.0.epcb`：母 PCB 原始源文本，包含 J1 和文档层参考图；不是可制造 PCB。
- `eda/drc.json`：EDA 检查返回值，详见设计检查记录。
- [eda/schematic-preview.png](eda/schematic-preview.png)：上轮形态决策的历史图页预览（仍显示旧 J1）；本轮 SCH 图像 API 返回全透明图。当前图以 `.esch`、网表及原生工程为准。
- [eda/pcb-layout-preview.png](eda/pcb-layout-preview.png)：本轮保存重开后的母 PCB 预布局截图。
- [checks/2026-10-19-drc.md](checks/2026-10-19-drc.md)：历史 10 项警告修复与复核，对应旧提交及旧归档哈希。
- [checks/2026-10-19-layout-decision.md](checks/2026-10-19-layout-decision.md)：历史形态说明更新检查，对应当时归档。
- [checks/2026-10-19-sourcing-layout.md](checks/2026-10-19-sourcing-layout.md)：本轮弯排针、STL 尺寸与纵向预布局检查；同名 JSON 记录当前归档哈希。
- `interfaces.md`、`pin-map.csv`：硬件与固件共用的接口约定。
- [bom.csv](bom.csv)：含商城料号和链接的候选 BOM；[parts-selection.md](parts-selection.md) 说明更换依据与规格书差异，不作为已批准采购单。

设计工具：嘉立创EDA 专业版，源文件记录 `3.2.149.88089769`。通过本机 EasyEDA API bridge 编辑并调用保存；再次打开图页仍可读到新增器件。没有创建制造输出。

## 使用与验收

先核对 BaseBoard 丝印、H2 Pin1、插接面、公母和高度，再确认电平、GPIO 阈值及电源预算。确认后使用短线按键原型逐键联调，固件配置见 [game-console](../../../../firmware/GamePlay%20V1.0/README.md)。关机插拔。

采购候选和封装差异见 [parts-selection.md](parts-selection.md)。当前 J1 暂停放在检查位置，Pin1、啮合深度与 Z 高度待确认；无 Gerber 和生产发布。

在仓库根目录运行 `python "hardware/GamePlay V1.0/gamepad-board/check.py"`，核对网表与针号及固件定义、BOM 型号、重复 NET 属性、原生工程与页源一致性及 PCB 名称。它检查已归档的 DRC 返回值，不会启动实时 EDA DRC，不能替代物理封装复核或上电测试。

官方依据及未知条件见 [接口说明](interfaces.md)；原型验收见 [设计检查](design-review.md)。
