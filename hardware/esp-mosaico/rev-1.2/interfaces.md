# V1.2 左右扩展接口

适用范围：ESP-Mosaico CoreBoard V1.2 与官方对应 BaseBoard 接口定义。查询日期：2026-10-18。当前只做文档核对，BaseBoard 实物版本、插接方向及电气测量尚未确认。

## 左右的主要区别

左右沿用官方标注：左侧 `H2`，右侧 `H1`。翻转设备后观察方向会改变，应结合丝印和 [官方设备外观图](https://mosaico.espressif.com/images/guide/device-overview-zh.png) 识别。两侧均为 2 × 10P、2.54 mm 连接器，但并非全部引脚等价。

| 比较项 | 左侧 H2 | 右侧 H1 |
| --- | --- | --- |
| 调试信号 | GPIO33 / GPIO34：USB Serial/JTAG D− / D+ | GPIO59 / GPIO58：UART RX / TX |
| 标准模块 EEPROM | 0x50；GPIO14 选择低电平 | 0x51；GPIO39 选择高电平 |
| 外扩 I2C | GPIO0 SDA、GPIO1 SCL | 与左侧共用同一总线 |
| 板载音频冲突 | 不占用右侧列出的 I2S 引脚 | GPIO49、52、54、37、40 与 ES8311 音频共用 |
| 安装方向 | 按模块与 H2 的配合方向 | 相对左侧旋转 180°，仍须核对 Pin1 |

上表与下方针号表依据 [官方 V1.2 模块接口定义](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp-mosaico/user_guide.html#id59)。保留板载音频时，右侧这五个共用 GPIO 不可分配给扩展功能。

## 引脚对照

下表按针号数值排序，用于查表，**不是连接器的正面、背面或 PCB 焊接面布局图**。实际 Pin1、插接面、电平及供电预算未核对前，**不可按此接线**。

| Pin | 左侧 H2 | 右侧 H1 | 说明 |
| --- | --- | --- | --- |
| 1 | GPIO55 | GPIO49 | 右侧与 I2S_WS 共用 |
| 2 | GPIO53 | GPIO46 | GPIO |
| 3 | GPIO19 | GPIO52 | 右侧与 I2S_DIN 共用 |
| 4 | GPIO48 | GPIO47 | GPIO |
| 5 | GPIO18 | GPIO54 | 右侧与 I2S_MCLK 共用 |
| 6 | GPIO13 | GPIO11 | GPIO |
| 7 | GPIO17 | GPIO37 | 右侧与 I2S_BCK 共用 |
| 8 | GPIO12 | GPIO10 | GPIO |
| 9 | GPIO16 | GPIO40 | 右侧与 I2S_DOUT 共用 |
| 10 | GPIO14 | GPIO39 | 标准模块 EEPROM 地址选择 |
| 11 | GPIO15 | GPIO38 | GPIO |
| 12 | GPIO4 | GPIO5 | GPIO / DAC 能力 |
| 13 | GPIO33 / USJ_DN | GPIO59 / RX0 | 左：USB D−；右：主机 UART 接收 |
| 14 | GPIO1 / SCL | GPIO1 / SCL | 共用外扩 I2C 时钟 |
| 15 | GPIO34 / USJ_DP | GPIO58 / TX0 | 左：USB D+；右：主机 UART 发送 |
| 16 | GPIO0 / SDA | GPIO0 / SDA | 共用外扩 I2C 数据 |
| 17 | 5V_IN | 5V_IN | 外部向主机输入电源 |
| 18 | 5V_OUT | 5V_OUT | 主机向模块输出电源 |
| 19 | VCC_3V3 | VCC_3V3 | 主机向模块输出 3.3 V |
| 20 | GND | GND | 地 |

`5V_IN` 与 `5V_OUT` 的方向不同；UART 的 RX / TX 按主机方向命名。GPIO 电压容限、允许灌拉电流、模块接口电平及电源允许偏差未在本次工作中完成核对，不能从电源针脚名称推断。

## 电源与共享资源

出厂应用指南将 `IN` 标为 5 V 输入，将 `5V` 和 `3V` 分别标为最大 100 mA 的输出。该页面未进一步区分单槽与双槽合计预算，本仓库不据此声明“两槽各可取 100 mA”，也不将 Type-C 的输出额定值套到扩展接口。[来源：出厂应用指南](https://mosaico.espressif.com/zh/guide/)，查询于 2026-10-18；这些数值不是本机测量结果。

板级控制定义中，GPIO60 拉低开启受控电源。V1.2 板载 I2C0 使用 GPIO56 / GPIO3；外扩 I2C1 使用 GPIO0 / GPIO1。因此两侧模块共享的是外扩总线，不能把左右槽理解为两路独立 I2C。驱动开发应复用 BSP 的电源和总线管理，并检查设备地址冲突。[来源：BSP 主板定义](https://github.com/esp-mosaico/esp-mosaico-bsp/blob/master/components/esp-mosaico-bsp/include/bsp/esp_mosaico.h)，查询于 2026-10-18。

## 模块选择

| 模块或需求 | 使用位置 | 需要核对的限制 |
| --- | --- | --- |
| 官方摄像头模块 | 仅左侧 H2 | 占用 GPIO33/34 后，左侧 USB Serial/JTAG 不可用；板载 Type-C USB-OTG 为独立通道 |
| 官方多功能交互子板 | 左侧或右侧 | 右侧需按对应方向安装；发现功能依赖有效 EEPROM 信息 |
| 自制 I2C 模块 | 按实际设计选择 | 核对电平、地址、上拉、电源及两槽共用总线的影响 |
| 需要保留板载音频的扩展 | 按实际设计选择 | 避开右侧与音频共用的五个 GPIO |

模块约束来源：[摄像头模块指南](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp-mosaico/user_guide_camera.html)、[交互子板指南](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp-mosaico/user_guide_interact.html)，查询于 2026-10-18。自制模块两项为本仓库的设计检查建议，不代表已验证兼容。

官方模块指南要求安装前关机或关闭对外电源。软件能够检测插拔，不自动证明任意自制板具备带电插拔能力。当前未确认持有哪种扩展模块，也没有经过验证的自制板接线方案。

返回 [设备资料](../README.md) · [顶部按键](buttons.md)。
