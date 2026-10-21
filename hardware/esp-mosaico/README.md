# ESP-Mosaico 主机资料

本目录整理当前开发主机的组成与使用入口，供后续扩展板、固件和自动化项目共同引用。这是 ESP-MOSAICO-LAB 的非官方参考资料，主机为乐鑫官方设备。

## 设备登记与状态

| 项目 | 当前记录 |
| --- | --- |
| 设备 | ESP-Mosaico |
| 项目标识 | `esp-mosaico` |
| CoreBoard | V1.2，用户已确认 |
| BaseBoard | 实物丝印尚未登记；接口参考官方 V1.2 配套资料 |
| 数量 | 尚未登记 |
| 当前设备固件版本 | 尚未登记；不能直接认定与最新出厂应用一致 |
| 已有外接模块 | 尚未登记；摄像头和交互子板仅作为参考，不代表已经拥有 |
| 资料状态 | `experimental`：归档 9 份 PDF，完成原理图与 BSP 交叉核对；尚未执行设备测试 |
| 工具链 | 本目录为文档，无构建依赖；尚无本仓库验证过的软件环境 |
| 核对日期 | 2026-10-18（Asia/Shanghai） |

## 板载硬件概览

下表列的是该型号的官方配置，尚未逐项检查本机实物。CoreBoard 承载主控及主要功能电路，BaseBoard 承载电池、扬声器和外扩连接器。

| 部分 | 器件或配置 | 开发用途 |
| --- | --- | --- |
| 主控与程序存储 | ESP32-S31；16 MB PSRAM；16 MB NOR flash | 固件运行、界面与数据处理 |
| 显示与输入 | 480 × 480 QSPI 触摸屏，CO5300 显示驱动 | 图形界面与触摸交互 |
| 声音 | ES8311、NS4150B、麦克风、扬声器 | 录音、播放和语音交互 |
| 运动与磁场 | BMI270、两颗 BMM150 | 姿态与磁场感知 |
| 资源存储 | 1 Gbit / 128 MB SPI NAND flash | 图片等大容量资源 |
| 电源 | USB Type-C、锂电池、充电与电量计电路 | 供电、充电和电量显示 |
| 外部连接与操作 | 左右模块接口、背部触点、三个按键、振动马达 | 扩展、调试与人机交互 |

配置依据：[官方 V1.2 硬件指南](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp-mosaico/user_guide.html)，查询于 2026-10-18。已取得的主板、底板、扩展模块原理图和模组规格图见 [资料归档](rev-1.2/upstream/README.md)，电源与接口分析见 [设计分析](rev-1.2/design-analysis.md)，关键位号见 [器件表](rev-1.2/components.csv)。

## 阅读与使用

1. 先核对 CoreBoard、BaseBoard 的丝印及设备当前固件版本。
2. 阅读 [左右接口说明](rev-1.2/interfaces.md)，确认 H2 / H1、Pin1 和连接器观察方向。
3. 阅读 [顶部按键说明](rev-1.2/buttons.md)，区分开关机、下载模式和应用按键。
4. 阅读 [设计分析](rev-1.2/design-analysis.md)，核对独立 OUT_3V3 电源、总线资源和已发现的资料差异。
5. 开发扩展板时，将自己的设计放入新的 `hardware/<board-id>/`，配套代码放入 `firmware/<firmware-id>/`；本目录保留主机参考资料。

首次只识别设备和接口，无需安装软件。准备下载固件时需要可传数据的 USB 线、电脑和与所用工程匹配的开发环境；本仓库尚无可运行的最小固件示例。

## 验证记录与待确认项

本次完成的是资料核对与文件整理，不是设备测试。实物功能结论均为“未验证”。

- 待登记 BaseBoard 丝印、设备固件版本和实际持有的扩展模块。
- 待核对连接器 Pin1、插接方向与实物照片。
- 待验证按钮动作、电源输出、通信和具体软硬件组合。
- 实测时使用 [验证记录要求](../README.md#验证记录)，保存到本项目对应修订版下，再更新 [硬件状态](../README.md)。

## 已知差异与恢复入口

- 本目录只针对 CoreBoard V1.2 资料范围；V1.0 的部分板载管脚不同，不能套用。
- 已归档 PDF 不能替代 PCB 工程、完整 BOM、制造输出和整机结构模型；这些文件尚未取得。
- 屏幕规格书与主板连接器的针数、部分电压参数存在差异，详见 [设计分析](rev-1.2/design-analysis.md#显示音频与存储)。
- AI KEY 的界面动作随固件改变。出厂应用指南描述的行为须结合本机固件核对。
- 充满电时的指示灯颜色在两份官方指南中存在差异：硬件指南写绿色，出厂应用指南写白色；本机颜色待确认，不将其中一种写成实测结论。
- 下载或日志异常时，先查看 [BOOT 与下载模式](rev-1.2/buttons.md#下载模式与恢复入口)，并核对应用的 USB 控制台配置。

## 来源与许可

以下链接均于 2026-10-18 查询；`latest` 页面和 `master` 分支会变化，后续功能实现需固定所依赖的软件版本或提交。

| 来源 | 本目录使用范围 |
| --- | --- |
| [官方文档总入口](https://documentation.espressif.com/esp-dev-kits/zh_CN/latest/esp32s31/esp-mosaico/index.html) | 主板与模块文档分类 |
| [V1.2 硬件指南](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp-mosaico/user_guide.html) | 设备配置、连接器、按键与电源 |
| [出厂应用使用指南](https://mosaico.espressif.com/zh/guide/) | 按键应用行为、扩展电源标注和外观图 |
| [BSP 主板定义](https://github.com/esp-mosaico/esp-mosaico-bsp/blob/master/components/esp-mosaico-bsp/include/bsp/esp_mosaico.h) | AI / BOOT 按键 GPIO、I2C 与电源控制定义 |
| [摄像头模块指南](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp-mosaico/user_guide_camera.html) | 左槽限制及 USB Serial/JTAG 资源冲突 |
| [交互子板指南](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp-mosaico/user_guide_interact.html) | 两侧插槽使用与插入方向 |

本目录自行整理的文档和关键器件表沿用仓库现有 [LICENSE](../LICENSE)。`rev-1.2/upstream/` 中的第三方 PDF 保留原有权利，不纳入本仓库 GPL 授权；文件归属、分发限制与取得情况见 [归档说明](rev-1.2/upstream/README.md)。固定提交、文件版本及 SHA-256 见 [来源清单](rev-1.2/sources.json)。

返回 [仓库首页与项目索引](../../README.md)。
