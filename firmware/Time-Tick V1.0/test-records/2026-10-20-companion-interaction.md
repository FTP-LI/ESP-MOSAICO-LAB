# 2026-10-20：艺术字、专注成就与硬件交互

## 本轮交付

- 欢迎页移除三宠展示，加入独立手绘艺术字，V1.0缩为辅助文字。选定原图、完整imagegen提示词与LVGL转换器保存在 `assets/brand/`，像素数据230,496字节。
- 修复4×3待机姿态表均匀裁切混入相邻小鸡的问题，改用透明连通轮廓；不覆盖原始图片。系统伙伴选择截图中狗狗脚下杂影已消失。
- 主屏伙伴活动y范围260–294，角色底部避开402px处按钮；移动/召唤不显示自动台词。背景使用双图层650ms交叉淡入，角色不参与混合。
- 计时设置改为方向选择、启用开关、分钟/秒和5/15/25/60分钟快捷键，保留最长24小时与原有正放策略。
- 六项专注成就、累计完成次数和分钟、永久奖励与到时提示；只有自然完成至少60秒才计入，5秒演示和提前停止不奖励，重复刷新不重复结算。
- 互动不再扣减其他状态，不显示维护负担；伙伴schema 2保留schema 1存档，设置schema 3迁移schema 1/2并增加声音偏好。
- 官方BSP接入轻提示音、短振动与按住麦克风音量检测。音频在独立任务中运行，麦克风最长5秒、松手/返回/翻转中止。对话页明确“模型未接入”；没有ASR、模型请求或TTS。

## PC 验证

MinGW GCC、SDL2 2.32.10、LVGL 9.5.0，`-Wall -Wextra -Werror` 构建通过。CTest **8/8通过**，最后一轮15.08秒：

- `core`、`settings`、`pet`、`animation`、`weather`。
- `preview_smoke`、`preview_configured`、`preview_timer`。

验证覆盖旧设置schema 1/2和桌宠schema 1迁移、纯正反馈、长期无人维护、六项成就与满级、分钟/秒及总时长边界、保存失败、原有网络键盘、六帧动作、IMU到计时页面、暂停/重启/禁用方向、到时覆盖设置与陪伴、草稿恢复、自然完成只奖励一次、麦克风5秒模拟超时与翻转关闭、背景中间透明度。

从实际SDL/LVGL渲染器检查欢迎页、伙伴选择、计时设置、专注成就、语音页、成就提示和天气过渡。临时转正向导出后恢复显示旋转，未改实际设备方向逻辑。检查产物位于 `build-assets/timer-proof/`，包括：

- `welcome-zh.png`、`companion-picker.png`。
- `timer-settings-zh.png`、`achievements-zh.png`、`voice-zh.png`。
- `achievement.png`、`weather-before.png`、`weather-blend.png`、`weather-after.png`。
- `weather-transition-*.bmp`：14个实际绘制帧，另打包为GIF供观察。

复现：

```powershell
cmake --build build-pc --parallel 8
$env:TT_PREVIEW_PROOF_DIR=(Resolve-Path build-assets/timer-proof).Path
ctest --test-dir build-pc --output-on-failure
Remove-Item Env:TT_PREVIEW_PROOF_DIR
```

本轮使用 `build-pc/check` 临时输出避免覆盖运行中的旧exe，检查后恢复普通构建输出配置。正常关闭旧预览、在VS Code按F5即可重编译加载新版；没有强制结束进程，原桌宠存档保留。无窗口测试不调用桌面、电脑麦克风或扬声器。

## 官方 ESP-IDF 构建

ESP-IDF 6.2开发版 `1c42e514c90ae621af33bde3b6ad3dac22edcbfd`，RISC-V GCC 16.1，目标 `esp32s31`：

```powershell
idf.py --preview --no-ccache --no-configdep -B build-idf62 build
python publish/validate.py
```

编译与发布框架校验通过。最终 `build-idf62/time_tick.bin`：

- 大小：10,459,392字节。
- SHA-256：`df83ef98fda9c86c25f1a04bba53ff142966b4b4829aeb1c355cee711b9b1fb2`。
- `ota_0`偏移 `0x210000`，容量14,614,528字节，剩余4,155,136字节。

SDK仍有开发版依赖/Kconfig提示及factory分区容量通用提示；factory是保留的Vibe区域，当前应用面向官方ota_0安装流程。未擦除、烧录、安装或上传。

## 未验证范围

硬件音频、录放切换、麦克风灵敏度、振动、IMU、触摸、背景混合帧率和电池功耗需连接设备验证。PC提供明确模拟，不证明实机效果。实时模型对话、语音识别、TTS尚未接入；接口边界与实测步骤见 [交互说明](../docs/HUMAN_INTERACTION.md)。`.irisfw`、发布封面和正式上传仍待完成。
