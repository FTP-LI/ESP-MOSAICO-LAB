# BSP 来源

来源：https://github.com/esp-mosaico/esp-mosaico-bsp

基准提交：`5f88b91e1be055d1d3674364123b2779a1758377`，获取日期 2026-10-19。
本目录复制上游 `components/esp-mosaico-bsp/`，上游源码未修改；本说明为新增文件。
原许可证为 Apache-2.0，见 LICENSE；源码版权声明保留。它不因本仓库根许可证变为 GPL。

随工程保存该组件，避免构建时下载带有大量示例资源的整个上游仓库。其注册表依赖仍由组件管理器下载，最终版本由工程 `dependencies.lock` 固定。
更新时按新提交重新核对 V1.2 引脚、eFuse 识别、电源控制和全部诊断功能。
