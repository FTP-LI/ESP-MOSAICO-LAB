# 第三方来源与许可

Desk-Deck V1.0 原创代码与文档采用 GPL-3.0-only，完整文本见 [LICENSE](LICENSE)。构建时链接的第三方组件保留各自许可，完整版本锁定见 `dependencies.lock` 与 `project.json`；已收集的原始声明见 [第三方许可原文](docs/licenses/THIRD-PARTY-NOTICES.txt)。

| 来源 | 固定版本或位置 | 许可与用途 |
| --- | --- | --- |
| [ESP-IDF](https://github.com/espressif/esp-idf) | `1c42e514c90ae621af33bde3b6ad3dac22edcbfd` | 主体 Apache-2.0；SDK 中第三方组件各有许可，提供系统与硬件驱动 |
| [esp-mosaico-utils](https://github.com/esp-mosaico/esp-mosaico-utils) | `be151ae047014e79a9c7ae0092dd203844499003` | ESP-Iris 为 Apache-2.0；上游 LICENSES.md 提示 Recovery/host-tool 迁移快照缺少仓库级授权声明，本项目不替上游补授许可，也不复制分发完整工具仓库 |
| [esp-mosaico-bsp](https://github.com/esp-mosaico/esp-mosaico-bsp) | `5f88b91e1be055d1d3674364123b2779a1758377` | Apache-2.0，提供显示、触摸、IMU、马达支持 |
| [LVGL](https://github.com/lvgl/lvgl) | 9.5.0，组件锁定见 dependencies.lock | MIT，界面与内置字体/符号接口；字体各自许可另计 |
| [TinyUSB](https://github.com/hathach/tinyusb) | 组件锁定见 dependencies.lock | MIT，USB HID；Espressif 包装组件另见其 Apache-2.0 许可 |
| [Source Han Sans](https://github.com/adobe-fonts/source-han-sans) | 固定 LVGL 中 SourceHanSansSC-Normal.otf | SIL OFL 1.1；生成 16/20px 界面子集及发布卡片，完整许可见 docs/licenses/SourceHanSansSC-OFL.txt |
| LVGL 内置 Montserrat / Font Awesome 符号 | 固定 LVGL 的 built_in_font 来源及许可 | SIL OFL 1.1 字体；具体上游许可随原仓库保留 |

发布图片由 `publish/build_materials.py` 排版，屏幕内容来自本版本设备截图。使用本机 Microsoft YaHei/Segoe UI 绘制说明文字，仅分发图片，不分发系统字体。未使用 Time-Tick 的代码或图片，也不包含第三方角色素材。
