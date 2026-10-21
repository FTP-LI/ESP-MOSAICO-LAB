# Time-Tick 桌宠动画与目录整理验证

日期：2026-10-20。范围：电脑预览、共享动画、资源与源码整理；未连接或烧录实机，未打包上传。

## 改动

- 猫、狗、小鸡各有六帧行走、进食、玩球和洗澡；行走以实际距离推进，48 px完成一循环，左右方向共享步态相位。进食1.8秒，玩耍/洗澡2.7秒；动作中禁止新的照料或地面移动打断，完成后恢复自主活动。
- 四张动画原图通过内置imagegen生成；提示词在 `assets/pets/animation-prompts.json`。转换器按透明连通区域提取完整角色/道具，修复均分切片带入相邻脚尖、耳朵和截断小球的问题。三种伙伴的全部帧已逐帧总览检查，背景透明。
- 源码归入 `main/core`、`platform`、`ui`、`generated`；`sources.cmake` 统一PC/设备源码列表。移除未使用的5×7像素字绘制和旧角色包装函数，将背景/天气主题与逐帧角色拆开。
- PC正常退出通过退出回调保存待保存的状态；启动流程、四步引导、重编译、退出、依赖、存档和故障处理写入 `pc/README.md`，主README更新目录表。

## 自动验证

Windows x64 / MinGW GCC 16.1、CMake、Ninja；固件解析出的LVGL 9.5.0；SDL2 2.32.10。

```powershell
cmake -S pc -B build-pc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build-pc --parallel 8
ctest --test-dir build-pc --output-on-failure
python publish/validate.py
```

**6/6通过**：core、settings、pet、animation、preview_smoke、preview_configured。覆盖移动距离与六帧步态、换向不重置相位、时钟回绕、动作时长/重触发、进食/玩球/洗澡全部帧、动作中重复点击和召唤不打断、位置边界，以及原有网络键盘、错误重试、保存失败回滚、双语、校准/旋转、计时和成长记录。

界面测试起初使用随机散步15秒采样，路径恰好很短，只出现前两帧；已改为先指定长路径观察完整步态，再观察自主活动。动画代码没有为满足此测试而改变行为。自动测试不读写用户桌宠存档。

发布框架校验通过；真实封面、发布截图、.irisfw包检查与实机验收仍待完成。

## 可见窗口验证

在VS Code的本项目窗口按F5，成功经过配置/增量构建启动两个SDL窗口。使用鼠标依次完成中文、离线、猫咪、虚拟校准引导，进入主屏和养成页。实际检查到迈步姿态、食盆、玩球、浴盆与搓洗帧，照料期间按钮禁用，结束后恢复自主活动。没有边界串帧或角色裁切。

点击设备窗口右上角关闭后，进程正常退出，124字节桌宠存档更新；猫咪成长记录保留XP36。再次F5启动成功并显示预期的首次引导（系统设置仍为会话级）。按用户要求保留存档，未重置其他伙伴。实窗照料改变了本地猫咪进度，自动回归使用独立内存状态。

## ESP-IDF编译

实际调用官方ESP-IDF 6.2开发版 `1c42e514c90ae621af33bde3b6ad3dac22edcbfd` 的idf.py，目标ESP32-S31，GCC16.1，现有 `build-idf62` 增量编译；本次是官方命令行构建，不是通过扩展按钮触发。

```powershell
idf.py --preview --no-ccache --no-configdep -B build-idf62 build
```

构建成功，`time_tick.bin` 为 **7,561,408字节**，SHA-256：`c56f06ab4038438e7e0ce5871a482083cbbff6a3587bc5a19ec981665a021cbd`。0xdf0000字节的ota_0剩余 **7,053,120字节**。动画增加约3.64MB镜像体积；资源为flash常量，不将所有动画帧复制到RAM。

官方通用尺寸检查仍提示factory分区较小；该分区预留Vibe，Time-Tick应通过官方应用安装包写入ota_0。未执行输出中的普通flash命令，未改分区。SDK本身的Kconfig弃用与Wi-Fi私有包含路径警告仍存在。

## 清理

删除旧GCC配置目录 `build/`（36文件，3,206,924字节）、重复主机测试目录 `build-host/`（69,539字节）、12张旧角色单帧、背景缩略图及build-pc根目录3个已迁移的旧测试exe（共686,872字节）。合计53文件、**3,963,335字节**。移除的像素绘制源码另计。

保留有效 `build-pc/`、`build-idf62/`、必要组件依赖、原始图片、字体缓存、许可证、历史验证记录和 `preview_pet_save.bin`；当前动作检查图/GIF集中在忽略的 `build-assets/`。四组测试产物统一放在build-pc/core-tests，不再同时维护build-host。

## 实机待验

真实屏幕上的流畅度/色彩、PSRAM与绘制开销、功耗、触摸、网络、IMU、NVS重启恢复和安装包尚未验证。本次没有实机烧录或发布。
