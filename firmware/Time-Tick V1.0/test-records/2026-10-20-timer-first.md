# 2026-10-20：计时优先、低打扰陪伴与全屏天气

## 修正范围

- 正放显示时钟天气，定时方向显示独立的 84 px 倒计时页，覆盖最长 `1440:00`。停止后保留计时页与“开始”，不再因为 IDLE 状态误回天气时钟。
- 主屏取消四按钮底栏，设置移至右上，养成缩为草地角落入口。倒计时页仅提供计时操作。
- 养成页不再参与 `tt_ui_editing()` 的姿态冻结，翻转后立即回到计时。到时提醒覆盖养成和设置；设置被覆盖时保留草稿，结束提醒后返回。
- 增加明确的手动启动命令，检查校准、稳定姿态和非零方向预设；停止/结束后保持原姿态不会自动重启。
- 需求不随时间下降；低状态自动恢复，亲密度/经验不衰减，取消饥饿和洗澡催促。原存档结构及三种伙伴独立记录保留。
- 六张 480×480 天气插画由 manifest 映射，换天气不重置桌宠动作；PC 示例温湿度随场景变化，示例数据不标记为真实缓存。
- 启动引导使用 Time-Tick + V1.0；新增天气素材 README，更新主 README、PC 启动/排查、界面规范、养成规则、来源清单和 CHANGELOG。

## 自动验证

Windows MinGW GCC、SDL2 2.32.10、与固件相同的 LVGL 9.5.0。`-Wall -Wextra -Werror` 编译通过，CTest **8/8 通过**：

| 测试 | 验证 |
| --- | --- |
| core | 稳定方向、运动拒绝、传感器间隙、六面映射、计时策略、显式重启和禁用方向 |
| settings | 设置有效性与旧版解码 |
| pet | 操作/保存模型、独立伙伴、休息恢复；连续14天不照料时舒适状态、亲密度与经验保持 |
| animation | 连续步态与照料动作时长 |
| weather | 天气代码与未知值回退 |
| preview_smoke / preview_configured | 引导、双语、网络键盘/重试、照料动画、保存失败、天气切换及边界 |
| preview_timer | 使用真实方向滤波驱动五个定时方向，校验暂停/继续、正放暂停、翻转重启、停止后同方向手动启动、0秒禁用、养成中翻转、到时覆盖养成/设置、恢复草稿、运动与校准限制 |

`preview_timer` 额外核对 `1440:00` 的文字尺寸适合 440×108 数字区域。正常输出目录运行过全部8项；为保留用户正在使用的窗口，最后的排版校验使用同一构建树的 `build-pc/check/` 输出，也通过全部8项，随后恢复普通输出设置。

## 画面检查

VS Code F5 启动新版预览，查看项目名称/版本引导、简化后的全屏时钟页和显示明确状态的控制面板。模拟面板曾显示 Unknown / settling，说明旧画面的旋转角度本身不能证明当前姿态已校准或稳定。

在无窗口 SDL 渲染中导出实际画面，检查中英文倒计时、到时提醒及最长时长；修复英文 Settings 按钮换行。检查图位于忽略目录 `build-assets/timer-proof/`，不是设计效果图。导出时只将显示朝向临时调为0°供阅读，之后恢复原角度；计时状态不改变。

需要复现检查图时，先关闭普通预览并按 PC README 构建，再运行：

```powershell
New-Item -ItemType Directory -Force build-assets/timer-proof
$env:TT_PREVIEW_PROOF_DIR=(Resolve-Path build-assets/timer-proof).Path
ctest --test-dir build-pc -R preview_timer --output-on-failure
Remove-Item Env:TT_PREVIEW_PROOF_DIR
```

CTest 使用 dummy 视频驱动，输出 BMP，不读写日常桌宠存档。普通启动不导出图片。

## 固件与发布检查

使用项目固定的官方 ESP-IDF 6.2 开发版（`1c42e514c90ae621af33bde3b6ad3dac22edcbfd`）、RISC-V GCC 16.1、目标 `esp32s31`：

```powershell
idf.py --preview --no-ccache --no-configdep -B build-idf62 build
python publish/validate.py
```

编译及发布框架检查通过，`build-idf62/time_tick.bin`：

- 大小：10,124,064 字节。
- SHA-256：`b513f9865c6e6217c7a8fd0db8a8b16b18d238525788bcf1054a78b94b58aca8`。
- `ota_0`：偏移 `0x210000`，容量14,614,528字节，剩余4,490,464字节。

SDK 仍有开发版依赖/Kconfig 提示，以及通用检查报告 factory 区域较小；factory 是受保护的 Vibe 区域，本应用只适用于官方应用安装流程中的 ota_0。未执行普通 flash、擦除、烧录或上传。

## 未验证范围

实物 IMU 轴向与姿态、旋转后触摸、3秒振动、实际网络/天气、屏幕帧率/功耗和断电恢复仍需接设备验收。PC 天气与网络为模拟，闹钟只有视觉反馈。发布包、网站封面及上传未完成。本轮仅六种日间天气背景，不包含参考产品的昼夜/暮色、天气预报分页或自动定位。
