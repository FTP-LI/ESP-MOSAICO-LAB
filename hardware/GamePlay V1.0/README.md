# GamePlay V1.0 — 硬件

面向 ESP-Mosaico CoreBoard V1.2 的掌机按键与结构项目，状态 `experimental`。配套软件位于 [firmware/GamePlay V1.0](../../firmware/GamePlay%20V1.0/README.md)。

| 目录 | 内容 |
| --- | --- |
| [gamepad-board](gamepad-board/README.md) | 九键原理图、EDA 源文件、BOM、H2 针号表、设计检查及网表检查脚本 |
| [gamepad-enclosure](gamepad-enclosure/README.md) | 纵横形态方案、主机 STL 尺寸分析、72×112 mm 纵向预布局及参考模型 |

现阶段先做纵向掌机，保留横向设计；主游戏键按硅胶导电胶方案规划。母 PCB 名为 GamePlay V1.0，硬件修订仍为 rev-0.1；尚无最终触点、布线或可生产板框，也没有可打印的完整外壳。

在仓库根目录检查网表与固件映射：

```powershell
python "hardware/GamePlay V1.0/gamepad-board/check.py"
```

该脚本核对已保存的 EDA 导出与 DRC 结果，不执行实时 DRC。连接器方向、Pin1、电气条件和机械配合仍需实物核验，当前不可按草案直接接线。

主机参考见 [ESP-Mosaico](../esp-mosaico/README.md)，验证记录要求见 [硬件说明](../README.md#验证记录)，原创设计许可见 [LICENSE](../LICENSE)。
