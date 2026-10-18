# V1.2 顶部按键

适用范围：ESP-Mosaico CoreBoard V1.2。查询日期：2026-10-18。按键功能以 `POWER`、`BOOT`、`AI KEY` 标识识别，实际位置可参照 [官方外观与按键标注图](https://mosaico.espressif.com/images/guide/device-overview-zh.png)；本仓库尚未保存本机实物照片。

## 三个按键的作用

| 按键 | 硬件作用 | 官方出厂应用指南中的操作 |
| --- | --- | --- |
| POWER / 电源 | 通过开关机电路控制整机电源 | 短按切换开关机 |
| BOOT | 上电时选择下载模式 | 开机过程中按住，用于下载固件；此时黑屏属于正常现象 |
| AI KEY / 功能键 | 提供给应用处理的按键输入 | 短按返回；长按打开 AI Create |

操作来源：[出厂应用使用指南](https://mosaico.espressif.com/zh/guide/)，查询于 2026-10-18。**本机固件版本尚未登记，AI KEY 的这些界面动作尚未实测。** 自编译应用可以重新定义其短按、长按等事件；本页不假定所有固件使用同一长按时长。

## SAM8108 与替代方案

SAM8108 属于**单按键开关机控制芯片**（Pushbutton On/Off Controller）。它把轻触按键的一次动作变为保持的开关状态，无需运行应用程序：上电默认关闭；触发一次开启，再触发一次关闭。其输出为 NMOS 开漏，开启时拉低，关闭时为高阻，由外围电源电路完成整机供电切换。

所查盛明数据手册列出 1.2–6.5 V 工作范围及 SOT23-6 封装。该版本引脚为：1 GND、2 VDD、3 NC、4 KEY、5 NC、6 OUT。这里只用于选型对比，具体采购批次仍需核对对应数据手册和板上连接。[SAM8108 厂商数据手册（第三方镜像）](https://taoic.oss-cn-hangzhou.aliyuncs.com/9225/product/SAM8108_1633230792000.pdf)，查询于 2026-10-18。

下面是重新设计电路时的功能替代候选，**没有已验证可在 ESP-Mosaico 原板上直接焊换的型号**：

| 候选 | 适用需求 | 关键区别 |
| --- | --- | --- |
| [MAX16054](https://www.analog.com/en/products/max16054.html) | 单按键翻转开关，带按键消抖和 CLEAR 清除输入 | 2.7–5.5 V；6 脚 Thin SOT23。电源、地和信号针序与 SAM8108 不同；输出为互补 CMOS 电平，不能当作同针脚的开漏器件替换 |
| [LTC2950-1 / LTC2950-2](https://www.analog.com/en/products/ltc2950.html) | 可调按键确认时间，通知 MCU 保存数据后关机 | 2.7–26.4 V；8 脚；开漏使能输出。-1 / -2 的使能极性不同，需重新设计外围电路并处理 INT、KILL 与上电时序 |

候选参数依据 ADI 官方 [MAX16054 数据手册](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX16054.pdf)及 [LTC2950 数据手册](https://www.analog.com/media/en/technical-documentation/data-sheets/295012fd.pdf)，查询于 2026-10-18；选型建议尚未做电路验证。

若维修现有主板，应先核对原理图、封装与全部针脚，再确认电压、输出类型及上电默认状态。若设计新板，简单按一下开、再按一下关可评估 MAX16054；需要应用先保存状态再断电可评估 LTC2950。电源芯片改型还需重新检查 GPIO57 的关机控制方式，不能直接沿用原 BSP 的时序与电平假设。

## 固件开发对应关系

| 信号 | BSP 中的定义 | 开发含义 |
| --- | --- | --- |
| AI 按键 | GPIO7，按下为低电平 | 应用按键输入，可绑定自己的功能 |
| BOOT 按键 | GPIO61，按下为低电平 | 应用可按所用 BSP 处理运行时输入，但必须保留启动下载用途 |
| POWER_SWITCH | GPIO57，关机请求信号 | 与电源控制相关，不能直接把它当成普通按键输入使用 |

GPIO 定义依据 [BSP 主板头文件](https://github.com/esp-mosaico/esp-mosaico-bsp/blob/master/components/esp-mosaico-bsp/include/bsp/esp_mosaico.h)，查询于 2026-10-18。本板 GPIO0 用于外扩 I2C SDA；BOOT 为 GPIO61。

POWER 通过 SAM8108 开关机电路工作。官方指南要求 GPIO57 正常运行时保持高阻，关机请求时以开漏方式拉低。应用应通过匹配版本的 BSP 电源接口处理关机，不自行试探该管脚电平。[来源：V1.2 电源控制说明](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp-mosaico/user_guide.html#id44)

## 下载模式与恢复入口

正常下载失败、需要进入 ROM 下载模式时，官方给出的基本操作是：

1. 使用支持数据传输的 USB 线连接主机 Type-C 与电脑，确认设备处于关机状态。
2. 按住 BOOT，再通过 POWER 开机，进入下载模式。
3. 在电脑确认实际下载端口，按所选固件工程的说明进行烧录；本页不指定未经确认的 COM 端口或固件文件。
4. 完成后按烧录工具提示复位，或松开 BOOT 后重新开机，检查应用是否正常启动。

下载模式下屏幕黑屏不等同于硬件损坏。Type-C 的应用日志与自动下载能力依赖固件的 USB CDC 实现；更换固件后，不能仅凭“USB 已连接”认定仍有同样的控制台功能。

依据：[官方硬件指南的烧录说明](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp-mosaico/user_guide.html#id23) 和 [出厂应用 BOOT 说明](https://mosaico.espressif.com/zh/guide/)，查询于 2026-10-18。需要恢复出厂应用时，先查阅官方 [固件更新入口](https://mosaico.espressif.com/zh/firmware-update/)，核对设备、固件版本及配置保存要求。完整固件烧录可能清除已有数据，应先备份；本次未烧录或恢复设备。

## 待实测

- 登记当前固件版本，核对 POWER、BOOT、AI KEY 的实际行为。
- 记录 ROM 下载模式与应用模式下电脑识别到的端口。
- 用 [测试记录模板](../../../docs/templates/test-record.md) 记录操作、预期和实际结果，不将以上操作说明当作已经执行的测试。

返回 [设备资料](../README.md) · [左右接口](interfaces.md)。
