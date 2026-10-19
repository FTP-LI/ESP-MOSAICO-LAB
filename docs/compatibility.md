# 兼容性索引

当前已登记 ESP-Mosaico CoreBoard V1.2，并完成 [原理图与 BSP 交叉分析](../hardware/esp-mosaico/rev-1.2/design-analysis.md)，尚无设备测试记录。参考模块原理图的归档不代表已经持有或测试该模块。文档核对不能作为功能兼容性证明；所有尚未测试的软硬件组合均标为“未验证”。

## 记录约定

每个测试组合单独记录，链接项目 README 与实际测试记录。`verified` 仅适用于记录中列出的功能及环境；没有测试的版本必须标为“未验证”。

| 项目与 README | 项目状态 | 设备型号与版本 | 扩展板修订版 | 设备固件版本 | 主机环境或工具链 | 验证的功能 | 验证结论 | 测试记录与源码提交 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| [esp-mosaico 主机参考资料](../hardware/esp-mosaico/README.md) | `experimental`（资料整理） | ESP-Mosaico CoreBoard V1.2，用户确认；BaseBoard 丝印待登记 | 未登记外接扩展板 | 未登记 | 文档，无构建环境验证 | 官方接口与按键资料核对；设备功能未测 | 实物未验证 | 无实测记录；来源和查询日期见项目 README |
| [game-console 诊断](../firmware/game-console/README.md) / [按键板](../hardware/gamepad-board/README.md) | `experimental` | CoreBoard V1.2；BaseBoard 待核对 | rev-0.1 草案，无实物 | diagnostics 0.1.0，未烧录 | Windows / IDF 6.2.0 固定开发提交 / GCC 16.1.0 | 两种配置构建、网表与 GPIO 一致性 | 软件检查通过；设备未验证；DRC 有警告 | [2026-10-19 验证记录](../firmware/game-console/test-records/2026-10-19-build.md) |

增加具体固件或扩展板后，为实际测试组合单独新增数据行；不要把参考资料、模板或计划写成验证结果。测试记录使用 [测试记录模板](templates/test-record.md)。

## 维护方式

- `experimental` 项目写清已完成的阶段与待验证项。
- `verified` 项目必须提供可追溯的测试记录。
- `deprecated` 项目保留历史记录，并在项目 README 中说明停止维护原因及替代方案。
- 硬件修订、设备固件、接口或配置发生变化时，重新确认受影响的组合；旧记录不自动证明新版本兼容。
- 尚未确认连接方式的说明必须标注“不可按此接线”。
