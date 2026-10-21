# 第三方来源

原创源码及说明沿用 [GPL v3](../LICENSE)。本表记录实际复用的入口，构建后还须按依赖锁文件核验最终链接组件的版权与分发要求。

| 内容 | 来源/版本 | 本地使用与许可依据 |
| --- | --- | --- |
| 中英文字体子集 | [Noto Sans CJK SC](https://github.com/notofonts/noto-cjk)，© 2014–2021 Adobe | `main/generated/fonts/font_16.c` / `font_18.c` / `font_24.c` / `font_52.c` / `font_84.c` 使用 SIL OFL 1.1，详见 [字体来源与复现](assets/fonts/README.md) 和 [许可证](assets/fonts/OFL.txt)；这些字体数据保留 OFL，不改用项目 GPL |
| SDL2（仅电脑预览） | [libsdl-org/SDL 2.32.10](https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10)，官方 MinGW 开发包 | zlib 许可证；仅位于 `build-pc/_deps/sdl2-src`，CMake 固定下载 SHA-256。分发桌面预览时同时保留 SDL 的 LICENSE.txt 和 LVGL 的 MIT LICENSE；不进入 ESP32 固件 |
| ESP-IDF | espressif/esp-idf，`1c42e514c90ae621af33bde3b6ad3dac22edcbfd` | 外部 SDK；保留其 LICENSE 和第三方声明 |
| ESP-Mosaico BSP | `5f88b91e1be055d1d3674364123b2779a1758377` | 已复制固定副本，[UPSTREAM](components/esp-mosaico-bsp/UPSTREAM.md)、[Apache-2.0 LICENSE](components/esp-mosaico-bsp/LICENSE) |
| ESP-Iris / app recovery / 打包集成 | esp-mosaico-utils，`fc63d43f3ac72aa54e701b07b07fbed07a16b3ca` | 引用 examples 内官方子模块，保留各目录声明；未另写烧录/构建包装器 |
| LVGL、cJSON、BMI270 及 BSP 驱动依赖 | [实际依赖锁文件](dependencies.lock)：LVGL 9.5.0、cJSON 1.7.19~2、BMI270 0.4.1、esp_lvgl_adapter 0.6.2、esp_tinyusb 2.4.0、tinyusb 0.21.0~2 等 | 由本工程组件管理器解析；各组件保留其许可证文件，发布前按最终链接结果汇总完整分发清单 |
| Open-Meteo / GeoNames | [天气 API](https://open-meteo.com/en/docs)、[城市 API](https://open-meteo.com/en/docs/geocoding-api) | 网络数据；UI 和网页说明标注 Open-Meteo，城市来源在文档保留；使用公共非商业服务，发行用途变化时复核服务条款 |
| 时间小方 TK3 | [厂商产品页](https://www.nllano.cn/products) | 仅交互参考，没有导入固件、美术或机械源文件 |

精确仓库 URL 与固定提交在 [publish/sources.json](publish/sources.json)。发布时提供与二进制一致的源码提交、必要许可和修改说明；目前尚无发布产物。

## 生成美术

当前桌宠仅保留呆猫，由 Codex 内置 imagegen 按《怪物猎人》艾露猫 Kit T. 外观生成。角色标识 ©CAPCOM；本项目记录 AI 重绘来源，不将角色声明为原创或已获额外授权，也未从游戏提取素材。[源图、完整提示词与转换方法](assets/pets/daimao/README.md)随工程保留。旧三宠、服装和道具保存在 [归档](archive/README.md)，不进入固件。

六种全屏天气背景及完整提示词在 [assets/weather](assets/weather/README.md)。欢迎页手绘艺术字与提示词在 [assets/brand](assets/brand/README.md)，转换为 `main/generated/ui_brand.c`。两者均由 Codex 内置 imagegen 生成；字体仍遵守上表 OFL 许可。
