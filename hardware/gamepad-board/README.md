# 游戏机按键扩展板

本仓库的非官方 ESP-Mosaico V1.2 游戏机套件控制板，复用主机屏幕、电池、扬声器和振动马达。先实现纵向掌机，保留横向方案；十字键和 A/B 按硅胶导电胶结构规划。

状态：`experimental`。已建立 [rev-0.1 原理图与接口草案](rev-0.1/README.md) 及母 PCB 预布局，尚未完成 PCB 电路设计、打样或实测。首轮采用左侧 H2 的独立 GPIO 按键，不依赖 EEPROM 自动发现；只能搭配本仓库诊断固件手动启用。

原理图严格 DRC 已通过，J1 已替换为 C492438 弯排针；母 PCB `GamePlay V1.0` 包含 J1 候选与文档层纵向布局，尚无铜触点、布线和板框。该名称不改变实验硬件修订号。

- [接口与按键布局](rev-0.1/interfaces.md)
- [纵向与横向方案、弯排针和硅胶按键配合](../../docs/game-console-kit/form-factor.md)
- [针号与逻辑动作](rev-0.1/pin-map.csv)
- [诊断固件](../../firmware/game-console/README.md)
- [设计检查记录](rev-0.1/design-review.md)
- [10 项 DRC 警告修复与复核](rev-0.1/checks/2026-10-19-drc.md)
- [形态决策后的母版检查](rev-0.1/checks/2026-10-19-layout-decision.md)
- [本轮弯排针与尺寸布局检查](rev-0.1/checks/2026-10-19-sourcing-layout.md)
- [STL 中框尺寸与掌机结构](../gamepad-enclosure/rev-0.1/README.md)
- [器件商城来源与待确认项](rev-0.1/parts-selection.md)

适用主板为 CoreBoard V1.2；BaseBoard 实物版本与插接方向待核对。当前不可依据草案直接接线。原创设计及文档沿用根目录 GPL v3；器件库符号与厂商资料保留原归属，库中的型号不代表完成选型或允许直接采购。
