# 官方硬件资料归档

查询与下载日期：2026-10-18（Asia/Shanghai）。共取得 11 份 PDF，其中 9 份原样归档（29 页、7,056,041 字节），2 份因文档内的分发限制仅保留链接。当前设备仅确认 CoreBoard V1.2；底板与扩展模块的实物版本未确认。

这些文件来自官方指南的下载链接。它们是参考原理图或规格书，**不是本仓库设计的硬件，也不是可编辑工程或生产文件**。整理结论见 [设计分析](../design-analysis.md)，逐文件来源、原始文件名、版本、字节数与 SHA-256 见 [sources.json](../sources.json)。

## 主机资料

| 文件 | 页数 | 图内版本与日期 | 核对情况 |
| --- | --- | --- | --- |
| [CoreBoard 原理图](mainboard/coreboard-v1.2.1-schematic.pdf) | 5 | CoreBoard V1_2_1；更新 2026-09-24 | 已逐页检查总览、MCU、接口、音频、电源 |
| [BaseBoard 原理图](mainboard/baseboard-v1.2.1-schematic.pdf) | 1 | BaseBoard_A V1_2_1；图内更新 2026-09-04 | 下载文件名日期为 2026-09-24，保留这一区别 |
| [屏幕规格书](display/h0216f002amt004-1-v1-specification.pdf) | 17 | H0216F002AMT004-1 V1；修订记录 2025-08-18，页脚 Rev V0 | 已检查结构、接口、电气规格；存在连接器与参数不一致项 |

来源：[官方 V1.2 用户指南的相关文档](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp-mosaico/user_guide.html#id65)。

## 扩展设计参考

本节归档用于研究扩展接口，不表示用户已经拥有这些模块。模块修订号独立于主板修订号。

| 文件 | 页数 | 图内标识 | 核对情况 |
| --- | --- | --- | --- |
| [OV3640 摄像头板原理图](reference-modules/camera-ov3640-v1.2-schematic.pdf) | 1 | V1.2；2026-09-21 | DVP、2.8 V / 1.5 V、24 MHz、EEPROM、补光电路 |
| [SC101IOT 摄像头板原理图](reference-modules/camera-sc101iot-v1.4a-schematic.pdf) | 1 | 工程名 V1_4A，版本栏 V1.4；2026-09-21 | DVP、2.8 V、20 MHz、EEPROM、补光电路 |
| [交互子板原理图](reference-modules/interaction-v1.0-schematic.pdf) | 1 | V1.0；2026-09-24 | 两槽映射、EEPROM、按键、PIR、红外与 RGB 灯 |
| [OV3640 模组尺寸图](reference-modules/ov3640-module-mechanical.pdf) | 1 | 仁恒；2026-07-24 | 模组外形、24 针 FPC 与镜头尺寸 |
| [SC101IOT 矮镜头模组尺寸图](reference-modules/sc101iot-low-lens-mechanical.pdf) | 1 | AS-AGTO08AI101D4-21-V2.0；2026-09-02 | 模组外形、FPC 与镜头尺寸 |
| [SC101IOT 高镜头模组尺寸图](reference-modules/sc101iot-high-lens-mechanical.pdf) | 1 | AS-AGT08AI101D4-21，F2.4；2026-08-26 | 模组外形、FPC 与镜头尺寸 |

来源：[官方摄像头模块指南](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp-mosaico/user_guide_camera.html)及[交互子板指南](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp-mosaico/user_guide_interact.html)。

## 仅保留链接的文件

- [OV3640 CSP2 v1.1 数据手册](https://dl.espressif.com/ae/mosaico/hardware/OV3640_CSP_1.1.pdf)：64 页，PDF 第 3 页明确限制接收方再分发，因此不上传副本。
- [SC101IoT v0.5 数据手册](https://dl.espressif.com/ae/mosaico/hardware/SC101IoT_Datasheet_V0.5.pdf)：33 页，封面标注 `Confidential - NDA only`，因此不上传副本。

两份文件的 SHA-256 和字节数仅用于识别已查询版本；本仓库不提供绕过原权利方要求的镜像。摄像头板的电路分析以本目录公开原理图和官方使用指南为依据。

## 来源、许可与完整性

- 乐鑫原理图保留原有图框、标志、版本和作者归属；供应商规格图保留原有归属。归档时只改成本仓库的小写英文文件名，PDF 内容未改动。
- 未在这些外链 PDF 中确认独立的开放硬件许可；不得将其标为本仓库 GPL 授权或已取得任意再设计、制造授权。原文件及相关权利属于各自权利方。
- `esp-dev-kits` 仓库中的文档、图和图片声明为 CC BY-SA 4.0；该范围不自动扩展到 `dl.espressif.com` 外链的独立 PDF。这里记录事实，不替权利方新增许可。参见[固定提交的许可说明](https://github.com/espressif/esp-dev-kits/blob/1f6204a571b39b3bd439720baed5d20f5b9fd4b7/README.md#open-source-licensing)及[官方版权声明](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/disclaimer-and-copyright.html)。
- 后续替换上游文件前，应核对图内版本和哈希，更新来源清单及变更记录；不得把新版文件覆盖后继续沿用旧的分析结论。

## 尚未取得的设计文件

截至查询日，在官方指南、官方产品入口以及 [sources.json](../sources.json) 列出的 5 个公开仓库完整文件树中，未找到 ESP-Mosaico 的可编辑原理图 / PCB、完整 BOM、Gerber / 钻孔 / 贴片坐标、叠层说明或整机 STEP / STL。这 5 个仓库的 Releases 接口均未返回发布记录或附件；`esp-mosaico-matter` 的文件树接口返回空仓库错误，不能作为已检查到设计内容的依据。

屏幕和摄像头 PDF 中的二维尺寸图不是整机外壳模型；PCB 照片和原理图也不能用于推导精确板框、孔位、封装、层叠或阻抗。若需复刻或替换主板，这些制造资料仍须向上游取得；本次未向第三方发送请求。

返回 [主机资料](../../README.md)。
