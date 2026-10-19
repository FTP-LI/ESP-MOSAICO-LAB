# gamepad-board rev-0.1

状态：`experimental`，电气原型草案。未完成设备验证、PCB、板框、连接器配合或生产审查，**不可按此接线或下单生产**。

嘉立创EDA 工程 `ESP-MOSAICO-LAB` 的原 `Schematic1 / P1` 已改名为 `gamepad-board-rev-0.1 / 01-buttons-and-host-interface`。页面包含 J1、SW1–SW9、R1–R18、网络标记和实验版说明。原有页面只有图框，修改前源快照保存在 `eda/snapshots/before-2026-10-19.json`。

## 文件与恢复

- `eda/gamepad-board-rev-0.1.epro`：API 导出的原生工程，包含未改动的空 PCB1 和 Panel1；可导入为独立工程检查。
- `eda/gamepad-board-rev-0.1.esch`：当前图页的原始源文本，用于审计；不是完整工程容器。
- `eda/gamepad-board-rev-0.1.net`：嘉立创EDA 导出的 JSON 网表，用于核对实际电气连接。
- `eda/drc.json`：EDA 检查返回值，详见设计检查记录。
- `interfaces.md`、`pin-map.csv`：硬件与固件共用的接口约定。
- `bom.csv`：电气草案 BOM，开关与连接器封装待定，不作为采购单。

设计工具：嘉立创EDA 专业版，源文件记录 `3.2.149.88089769`。通过本机 EasyEDA API bridge 编辑并调用保存；再次打开图页仍可读到新增器件。没有创建制造输出。

## 使用与验收

先核对 BaseBoard 丝印、H2 Pin1、插接面、公母和高度，再确认电平、GPIO 阈值及电源预算。确认后使用短线按键原型逐键联调，固件配置见 [game-console](../../../firmware/game-console/README.md)。关机插拔。

在仓库根目录运行 `python tools/check-gamepad.py`，核对网表与针号及固件定义。它只证明保存资料之间一致，不能替代 EDA DRC、物理封装复核或上电测试。

官方依据及未知条件见 [接口说明](interfaces.md)；原型验收见 [设计检查](design-review.md)。
