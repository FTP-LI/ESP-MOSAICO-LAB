# 桌宠天气状态验证 — 2026-10-20

## 改动范围

- 猫、狗、小鸡在主屏与陪伴页响应晴、多云、雨、雪、雾、雷雨；行为、速度和轻装饰随天气变化。
- 雨伞固定落地，响应草地召唤后伙伴自行慢步返回；雪天更常蜷卧在软垫上，雾天慢行伴小灯，晴天更活跃并有蝴蝶。
- 天气仅影响视觉和自主活动，不扣需求、不改变经验/亲密、不增加提醒或维护。主动休息、喂食/玩耍/洗澡及翻转计时保持优先。
- 原画来自 Codex 内置 imagegen，原图与完整提示词见 [天气道具](../assets/pets/weather/README.md)。两张静态贴图共66,080字节，其余轻效果由LVGL绘制，未增加后台任务或修改存档结构。

## PC 编译与检查

MinGW GCC、SDL2 2.32.10、项目锁定的LVGL 9.5.0，480×480原生界面。

```powershell
cmake --build build-pc --parallel 8
$env:TT_PREVIEW_PROOF_DIR=(Resolve-Path 'build-assets/timer-proof').Path
ctest --test-dir build-pc --output-on-failure
Remove-Item Env:TT_PREVIEW_PROOF_DIR
```

**9/9通过，共32.27秒**：`preview_smoke`、`preview_configured`、`preview_timer`、`preview_weather`、`core`、`settings`、`pet`、`animation`、`weather`。

新增天气回归覆盖：

- 三种伙伴×六类天气、主屏活动边界和完整实际渲染；
- 晴天与雾天步速区别、各天气中草地召唤可用；
- 雪/雷雨自主休息，主动休息不被天气切换唤醒；
- 天气不增加/扣除经验与亲密、无天气时回退；
- 雷雨陪伴页翻转后进入正在运行的倒计时。

原有界面回归继续检查天气快速切换不截断六帧喂食/玩耍/洗澡，翻转计时、暂停/恢复、到时覆盖陪伴/设置和中英文引导均通过。

实际LVGL截图位于忽略目录 `build-assets/timer-proof/pet-weather-<伙伴>-<天气>.png`。伙伴0/1/2分别为猫/狗/小鸡，天气0–5依次晴、多云、雨、雪、雾、雷雨。已人工查看雨天猫、雪天狗、晴天小鸡、雾天小灯及陪伴页，确认侧柱避开身体、软垫位于蜷卧姿态下方、道具不侵入底部按钮。

构建检查先使用 `build-pc/check` 独立输出，随后恢复普通输出路径；不操作日常桌宠存档、不调用电脑麦克风或扬声器。

## 官方 ESP-IDF 构建

ESP-IDF 6.2开发版 `1c42e514c90ae621af33bde3b6ad3dac22edcbfd`，RISC-V GCC 16.1，目标 `esp32s31`。官方命令：

```powershell
idf.py --preview --no-ccache --no-configdep -B build-idf62 build
python publish/validate.py
```

编译与发布框架本地校验通过，最终 `build-idf62/time_tick.bin`：

- 大小：10,528,256字节。
- SHA-256：`191f9da201ba54532426cc2595d73bf11a6b6bd8b3e57e0c99f4c037210001f9`。
- `ota_0`偏移 `0x210000`，容量14,614,528字节，剩余4,086,272字节。

SDK保留开发版Kconfig提示和factory容量通用提示；factory是Vibe保留区，当前应用面向官方ota_0安装流程。未烧录或上传。发布检查仍提示封面、`.irisfw`验收和实机测试未完成。

## 设备待验

本轮证明PC表现和ESP32-S31编译通过；屏幕实际帧率、动画功耗、真实天气刷新、触摸和IMU仍需连接设备验证。可先在PC控制窗口点 `Next weather` 逐一查看；主动行走/照料会先完成，再衔接天气动作。
