# 本地唤醒资源

使用依赖锁中的 `espressif/esp-sr 2.6.0`，只打包 `model/wakenet_model/wn9s_nihaoxiaozhi`（你好小智）。`pack_wake.py` 调用组件自带 `pack_model.py`，模型参数保持原样，输出到构建目录 `tt_voice/wake_model.bin`。

CMake将模型作为只读数据嵌入应用镜像，没有添加模型分区。组件和模型使用随附 `LICENSE.esp-sr`，限定用于Espressif产品；PC预览不链接或执行此模型。SDK/组件版本以依赖锁为准，更新依赖后需重新做实机唤醒验收。
