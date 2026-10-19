# rev-0.1 器件选型与采购核验

2026-10-19（Asia/Shanghai），`experimental`。母版 J1 已通过 API 换为 **XFCN PZ254R-12-20P / C492438，2×10、2.54 mm、90° 公排针**。现货查询与机械匹配分开记录；本轮未下单。

| 用途 | 本轮选择与购买链接 | 查询证据与边界 |
| --- | --- | --- |
| J1 | [C492438 弯排针](https://item.szlcsc.com/502994.html) | 国内商品结构化数据记录库存 35,403；查询时可售，未锁库存。已取得匹配型号的符号、封装和厂家尺寸图 |
| 硅胶导电胶 | [FunnyPlaying GBC Replacement Silicon Pads，ORIGINAL COLOR](https://funnyplaying.com/products/gbc-replacement-silicone-pads?variant=39901960863805) | 商品 API 的具体变体 `39901960863805` 为 `available=true`；建议样品 1 套，含十字、A/B、Start/Select 胶件 |
| 配套键帽 | [FunnyPlaying GBC Custom Buttons，Black](https://funnyplaying.com/products/cgb-custom-buttons?variant=39333911593021) | 具体变体 `39333911593021` 为 `available=true`；建议样品 1 套，十字及 A/B 配套。附带电源拨片不用于主机 |
| SW1–SW9 | [C231644 / KAN0444-0201B/102](https://item.szlcsc.com/231470.html) | 继续作为九键电气原型；不是最终硅胶触点或好手感的证明 |
| R1–R9 | [C25804 / 0603WAF1002T5E](https://item.szlcsc.com/26547.html) | 10 kΩ ±1% 100 mW 0603，型号不变；本轮未重查库存 |
| R10–R18 | [C21190 / 0603WAF1001T5E](https://item.szlcsc.com/21904.html) | 1 kΩ ±1% 100 mW 0603，型号不变；本轮未重查库存 |

商品 API 的变体、时间、返回字段及来源 SHA-256 见 [结构来源记录](../../gamepad-enclosure/rev-0.1/sources.json)，结构清单见 [parts.csv](../../gamepad-enclosure/rev-0.1/parts.csv)。胶垫与键帽属于结构件，没有虚构立创 C 编号。商品可售不等于已经验证向用户地址配送；未提交订单或进行结算。

H2 已由官方底板原理图确认为 Ckmtw **B-2200R20P-B120**，双排弯排母。两者的公母、针数、2.54 mm 节距／排距对应；厂家未给出此跨品牌组合的啮合认证。用户提供的 STL 得到中框 **44.90×44.90×11.10 mm** 和双排侧孔，详见 [尺寸核对](../../gamepad-enclosure/rev-0.1/dimensions.md)。Pin1、有效插入深度、壳体干涉、整机厚度及 PCB 安装高度仍需确认。

J1 库封装钻孔导出约 Ø1.10 mm，而厂家建议图为 Ø1.02 mm；已明确登记差异，未声称封装完全通过生产审查。母 PCB 的 J1 暂停放在检查位置，文档层保存纵向预布局，尚无铜触点、板框或布线。

所选 GBC 套件支持十字、A/B 和 Start/Select 八路胶触点的设计方向；Menu 仍需独立按键。触点电极、支点、定位柱、键帽导向与行程必须围绕样件设计。第三方 MIT 参考几何用于空间安排，不能替代供应商公差、导电粒接触电阻或力—行程数据。当前 10k/1k 与 20 ms 去抖也须随样件验证。

原型开关商品所链 [规格书](https://atta.szlcsc.com/upload/public/pdf/source/20180622/C231644_8E779A5E99ACC98584838CDB900BEA24.pdf) 标为 `KAN0444-0201B5 定位柱`，与 `/102` 后缀不同；该问题继续保留，不能据此冻结 Menu 或轻触开关封装。

此前的 C62032 与 C3202442 已查到停售，不进入新采购候选；直排针 C492427 已从当前母版 J1 替换。历史记录仍保留原料号，避免改写历史。当前 28 个电气原型元件仍参与 BOM/PCB 转换，没有借排除元件清除 DRC。

母版修改、保存重开、连接核对与实时 DRC 见 [本轮检查记录](checks/2026-10-19-sourcing-layout.md)。可按上述链接购买**验证样品**，尚不能把它作为整机生产采购单。**不可按此接线或下单生产**。第三方商品和厂家图纸保留各自归属，只链接引用。
