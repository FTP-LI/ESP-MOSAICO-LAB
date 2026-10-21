# 首次设置、方向与天气定位验证 — 2026-10-20

## 本轮结果

- 引导拆为语言、网络、伙伴、显示、声音、天气、方向校准七页。点伙伴图片或名字播放六帧玩球动作；系统设置改为独立分类入口。
- 四个竖放朝向旋转屏幕，屏幕朝上平放保留之前角度；屏幕朝下不启动/重置计时。保留原计时状态及截止时间。
- 设置schema 4迁移1/2/3，保留网络、校准、其他方向时长及偏好，清除旧朝下预设。WGS84经纬度保存为百万分之一度整数，校验空值、范围、非法字符和非有限数。
- 天气使用原有Open-Meteo HTTPS接口；启用坐标时跳过城市查询。PC模拟控制台及VS Code启动/编译/测试任务改为中文。
- 对话页增加可返回的手机配置方案说明。配置网页、二维码、密钥保存、云端会话尚未实现；现阶段仅有本地麦克风音量检测。

## PC检查

MinGW GCC、SDL2 2.32.10、LVGL 9.5.0，生产共用UI与核心代码。

```powershell
cmake --build build-pc --parallel 8
$env:TT_PREVIEW_PROOF_DIR=(Resolve-Path 'build-assets/timer-proof').Path
ctest --test-dir build-pc --output-on-failure
Remove-Item Env:TT_PREVIEW_PROOF_DIR
```

**10/10通过，17.12秒。** 五组核心测试：core/settings/pet/animation/weather；五组界面回归：preview_smoke/preview_configured/preview_timer/preview_weather/preview_setup。

新增或扩展的检查：

- 选择狗狗后实际显示全部六个动作帧，未选猫保持站立；中英文七页流程可完成。
- 无坐标/越界坐标阻止继续；有效坐标完成引导后保存准确，分类页返回保留草稿，退出设置不保存修改。
- 旧schema迁移、经纬度边界、朝下拒绝手动启动；平放保留0/90/180/270全部上一角度。
- 实际模拟IMU到计时页：朝下不改变计时截止时间；到时覆盖天气设置后，关闭提醒仍保留城市草稿。
- 对话方案说明可打开/返回，原麦克风按住/超时/翻转停止回归通过。
- 原有三伙伴×六天气、喂食/玩耍/洗澡、永久成就、计时优先级回归通过。

使用SDL dummy驱动导出实际LVGL画面，无桌面点击、不读写日常伙伴存档。已查看中英文伙伴/显示/声音/天气/方向页面、中文系统分类、对话方案与480×720中文控制台，未见文字或按钮裁切。截图位于忽略目录 `build-assets/timer-proof/`，例如 `setup-companion-zh.png`、`setup-weather-zh.png`、`controls-zh.png`、`voice-guide-zh.png`。

测试曾使用 `build-pc/check` 输出，完成后已恢复普通路径并重新链接 `build-pc/time_tick_preview.exe`；VS Code F5可正常使用新的中文运行配置。

## 天气接口联网检查

在当前电脑上以公开示例坐标30.274100、120.155100请求生产同样的Open-Meteo字段，HTTPS成功返回温度、相对湿度、WMO天气代码，时间格式为Unix秒、间隔900秒。返回网格坐标30.263618、120.14051，说明精确请求坐标仍匹配模型网格；不应宣传为用户门口实测天气。

此检查不等于ESP32-S31设备联网验证。没有使用用户位置、API密钥或账号。PC产品预览仍使用固定模拟天气。

## 官方ESP-IDF构建

ESP-IDF 6.2开发版 `1c42e514c90ae621af33bde3b6ad3dac22edcbfd`，RISC-V GCC16.1，目标esp32s31。

```powershell
idf.py --preview --no-ccache --no-configdep -B build-idf62 build
python publish/validate.py
```

两项均通过。最终 `build-idf62/time_tick.bin`：

- 大小：10,551,744字节。
- SHA-256：`df39b2b8c21de3f236e809ca635b18474d03b6c54c9877bd38ae3d379b47c7ce`。
- 应用ota_0偏移0x210000，容量14,614,528字节，剩余4,062,784字节。

SDK仍提示factory分区容量不足；该分区属于Vibe保留区，应用应经官方ota_0安装流程，不执行构建输出中的通用flash建议。发布检查仅验证框架，封面、.irisfw验收及实机检查仍待完成。未烧录、安装或上传。

## 待实机确认

重点确认朝上平放的真实轴符号、四个角度的触摸映射、预设迁移和坐标断电保存、Wi-Fi/TLS/SNTP、天气刷新与缓存，以及扬声器/麦克风/振动。没有用PC表现替代设备验收。
