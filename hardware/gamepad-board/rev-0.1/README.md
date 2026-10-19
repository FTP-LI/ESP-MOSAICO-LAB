# gamepad-board rev-0.1

状态：`experimental`，电气原型草案。未完成设备验证、PCB、板框、连接器配合或生产审查，**不可按此接线或下单生产**。

嘉立创EDA 工程 `ESP-MOSAICO-LAB` 的原 `Schematic1 / P1` 已改名为 `gamepad-board-rev-0.1 / 01-buttons-and-host-interface`。页面包含 J1、SW1–SW9、R1–R18、网络标记和实验版说明。原有页面只有图框，修改前源快照保存在 `eda/snapshots/before-2026-10-19.json`。

已直接在母版修复全部 10 项原理图 DRC 警告，严格检查通过；临时诊断副本已删除。原母 PCB 已命名为 `GamePlay V1.0`，UUID 与关联原理图保持不变。该 PCB 尚无布局，文档名中的 V1.0 不表示生产版发布，硬件资料仍归属未发布的 rev-0.1。

## 文件与恢复

- [eda/gamepad-board-rev-0.1.epro2](eda/gamepad-board-rev-0.1.epro2)：API 导出的原生工程，包含原理图、器件库、空 PCB `GamePlay V1.0` 和 Panel1。扩展名已按 API 实际格式纠正；母版后续直接修改，归档用于版本追溯。
- `eda/gamepad-board-rev-0.1.esch`：当前图页的原始源文本，用于审计；不是完整工程容器。
- `eda/gamepad-board-rev-0.1.net`：嘉立创EDA 导出的 JSON 网表，用于核对实际电气连接。
- `eda/drc.json`：EDA 检查返回值，详见设计检查记录。
- [eda/schematic-preview.png](eda/schematic-preview.png)：重新打开母版后通过 API 获取的当前图页预览。
- [checks/2026-10-19-drc.md](checks/2026-10-19-drc.md)：本轮修复、复核和限制；同目录 JSON 保存 API 检查结果及文件 SHA-256。
- `interfaces.md`、`pin-map.csv`：硬件与固件共用的接口约定。
- [bom.csv](bom.csv)：含商城料号和链接的候选 BOM；[parts-selection.md](parts-selection.md) 说明更换依据与规格书差异，不作为已批准采购单。

设计工具：嘉立创EDA 专业版，源文件记录 `3.2.149.88089769`。通过本机 EasyEDA API bridge 编辑并调用保存；再次打开图页仍可读到新增器件。没有创建制造输出。

## 使用与验收

先核对 BaseBoard 丝印、H2 Pin1、插接面、公母和高度，再确认电平、GPIO 阈值及电源预算。确认后使用短线按键原型逐键联调，固件配置见 [game-console](../../../firmware/game-console/README.md)。关机插拔。

在仓库根目录运行 `python tools/check-gamepad.py`，核对网表与针号及固件定义、BOM 型号、重复 NET 属性、原生工程与页源一致性及 PCB 名称。它检查已归档的 DRC 返回值，不会启动实时 EDA DRC，不能替代物理封装复核或上电测试。

官方依据及未知条件见 [接口说明](interfaces.md)；原型验收见 [设计检查](design-review.md)。
