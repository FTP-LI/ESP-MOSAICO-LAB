# 游戏机按键扩展板

本仓库的非官方 ESP-Mosaico V1.2 游戏机套件控制板，复用主机屏幕、电池和扬声器。

状态：`experimental`。已建立 [rev-0.1 原理图与接口草案](rev-0.1/README.md)，尚未设计 PCB、打样或进行实测。首轮采用左侧 H2 的独立 GPIO 按键，不依赖 EEPROM 自动发现；只能搭配本仓库诊断固件手动启用。

- [接口与按键布局](rev-0.1/interfaces.md)
- [针号与逻辑动作](rev-0.1/pin-map.csv)
- [诊断固件](../../firmware/game-console/README.md)
- [设计检查记录](rev-0.1/design-review.md)

适用主板为 CoreBoard V1.2；BaseBoard 实物版本与插接方向待核对。当前不可依据草案直接接线。原创设计及文档沿用根目录 GPL v3；器件库符号与厂商资料保留原归属，库中的型号不代表完成选型或允许直接采购。
