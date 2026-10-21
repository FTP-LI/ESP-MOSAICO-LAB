# 硬件

- [GamePlay V1.0](GamePlay%20V1.0/README.md)：当前开发的按键板和掌机结构。
- [Time-Tick V1.0](Time-Tick%20V1.0/README.md)：翻转计时器的硬件复用、稳定支撑面与六方向验收要求，目前为需求阶段。
- [ESP-Mosaico 主机](esp-mosaico/README.md)：CoreBoard V1.2 的接口、器件分析和原始资料。

当前 GamePlay 为实验原型：原理图严格 DRC 已通过，PCB 只有连接器候选和预布局；外壳、打样、装配及上电联调尚未完成。主机参考资料核对不等于实物兼容性验证。

## 验证记录

检查记录就近保存到对应硬件修订版下，包含日期、主机与硬件版本、固件提交、连接方式、操作步骤、预期与实际结果、日志或照片，以及未执行项。区分网表检查、实时 DRC、机械复核和设备实测。

旧 JSON 检查记录中的路径及哈希保留历史值。旧 `hardware/gamepad-board/`、`hardware/gamepad-enclosure/` 现在位于 `hardware/GamePlay V1.0/` 内；原始文件可从工程外归档恢复比对。

原创设计及说明沿用 [GPL v3](LICENSE)；厂商资料和第三方模型按各自来源声明处理。
