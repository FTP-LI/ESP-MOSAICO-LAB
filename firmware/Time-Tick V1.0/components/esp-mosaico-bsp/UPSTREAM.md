# BSP 来源

来源：https://github.com/esp-mosaico/esp-mosaico-bsp

基准提交：`5f88b91e1be055d1d3674364123b2779a1758377`，获取日期 2026-10-19。
本目录复制上游 `components/esp-mosaico-bsp/`，保留以下项目内修改：

- 2026-10-20，`onboard/display.c`：ESP32-S31 的 QSPI 颜色数据启用 PSRAM 直接 DMA，并将传输队列从10缩至2。实机原配置在 Wi-Fi 初始化后反复出现 `ESP_ERR_NO_MEM`；十个8行RGB565中转缓冲最多需要76,800字节内部RAM。首轮更新后该错误已消失，界面耗时仍继续单独检查。引脚、时序、TE同步和触摸坐标保持上游配置。
原许可证为 Apache-2.0，见 LICENSE；源码版权声明保留。它不因本仓库根许可证变为 GPL。

- 2026-10-21，`onboard/display.c`：LVGL 固定运行在双核 S31 的核心1，避免软件绘图占用计时/IMU主任务所在核心。保留原有任务优先级和面板 TE 同步；效果见项目实机验证记录。

- 2026-10-21，`onboard/display.c`：亮度参数写入使用有界LVGL锁，与当前TE刷屏串行；旋转暂停等待限定500ms。应用把运行时亮度/旋转请求交给LVGL工作任务，防止两个任务共享SPI事务池产生竞态。

- 2026-10-21，`onboard/display.c`：直接PSRAM DMA的最大分块从8行增至64行，480行帧的分块上限由60次降为8次，减少事务/中断开销。保持队列2、40MHz、单缓冲及TE同步，不增加内部RAM中转帧缓冲。

随工程保存该组件，避免构建时下载带有大量示例资源的整个上游仓库。其注册表依赖仍由组件管理器下载，最终版本由工程 `dependencies.lock` 固定。

- 2026-10-21，`onboard/audio.c` / `include/bsp/audio.h`：增加单音频所有者的空闲回收入口，在所有编解码器流关闭后停用仍在运行的I2S收发DMA；修复初始双向启用后未使用RX常驻的问题。保留供电、初始化、播放/录音格式与引脚。
更新时按新提交重新核对 V1.2 引脚、eFuse 识别、电源控制和全部诊断功能。

- 2026-10-21，`onboard/sensors.c` / `include/bsp/imu.h`：增加一次读取加速度与陀螺仪的接口，供Time-Tick的20ms采样循环使用，避免连续两次读取完整BMI270数据。保持100Hz传感器ODR、原量程/单位及旧单独读取接口，不降低翻转采样频率。

- 2026-10-21，`onboard/display.c`：启用DFS后，整帧绘制、TE等待和直接PSRAM DMA完成前持有CPU_MAX锁。当前固定版本的TE flush同步等待传输结束，才允许在RENDER_READY释放；若改用异步flush则持续持锁直到显示删除。首次未加保护的DFS候选在实物出现花屏，已回退，完整证据见 `../../test-records/2026-10-21-power-runtime.md`；内部截图正常不能代替实物验收。
