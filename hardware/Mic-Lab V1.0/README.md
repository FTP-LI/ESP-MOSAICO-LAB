# Mic-Lab V1.0 硬件

首版复用 ESP-Mosaico CoreBoard V1.2，不新增 PCB。软件见 [Mic-Lab V1.0](../../firmware/Mic-Lab%20V1.0/README.md)，主机来源见 [ESP-Mosaico 资料](../esp-mosaico/README.md)。

| 资源 | 用途 |
| --- | --- |
| ESP32-S31 原生 USB | 双 CDC + UAC2 麦克风，电脑为主机 |
| 板载模拟麦克风与 ES8311 | 单声道采集、硬件增益、扬声器音频输出 |
| NS4150B 与现有扬声器 | 短时录音回放；回放时停止采集 |
| 480 × 480 触摸屏 | 实时电平、增益、静音、录音和回放操作 |
| 16 MB PSRAM | 最大 30 秒 PCM 缓冲，约 2.88 MB |
| 128 MB SPI NAND | 复用现有 LittleFS，仅写 `/nand/mic-lab/` |
| NOR NVS | 独立 `miclab` 命名空间，保存本地增益和静音 |

USB 线必须支持数据传输。桌面摆放应避开麦克风孔和扬声器孔，降低桌面振动与线缆摩擦传入；适合近距离语音实验。板载麦克风数量、腔体与模拟链路限制其效果，首版没有阵列降噪或无线领夹功能。

尚未进行本项目设备安装和声学测试。验收应覆盖 USB 枚举、连续收音、静音、30 秒录满、扬声器回放、保存后断电恢复、保存中断电、存储满、USB 重连及设备温升。具体步骤见 [验证记录](../../firmware/Mic-Lab%20V1.0/docs/VERIFICATION.md)。
