# 第三方来源

原创固件与工具沿用仓库 GPL-3.0-only。第三方组件保持自身许可，固定版本见 `dependencies.lock` 和 [BUILD.md](BUILD.md)。

- [esp-mosaico-utils](https://github.com/esp-mosaico/esp-mosaico-utils) 与 [esp-mosaico-bsp](https://github.com/esp-mosaico/esp-mosaico-bsp)：固定提交见构建说明；官方 Iris、BSP 和存储后端直接引用依赖源码，不改写上游文件。遵守各组件内 LICENSE。
- [ESP-IDF](https://github.com/espressif/esp-idf)：Apache-2.0 及各第三方组件许可。
- [TinyUSB](https://github.com/hathach/tinyusb)：MIT，复用 UAC2 描述符宏与 USB 类驱动。
- [LVGL](https://github.com/lvgl/lvgl)：MIT；使用实际 LVGL 部件与 Montserrat 字体。
- [esp_littlefs](https://github.com/joltwallet/esp_littlefs)：MIT；LittleFS 为 BSD-3-Clause。NAND 驱动及 Dhara 保留依赖中的许可证。
- Source Han Sans SC 字体：Adobe，SIL Open Font License 1.1；本项目仅生成小型位图字形子集，见 [完整许可](licenses/SourceHanSansSC-OFL.txt)。

`build-deps/`、`managed_components/` 与 `build-*` 为可恢复依赖/产物，未作为项目源码重复提交。发布二进制时应随包携带相应组件许可。
