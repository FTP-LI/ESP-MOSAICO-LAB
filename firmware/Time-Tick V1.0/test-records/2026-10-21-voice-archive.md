# 语音助手退出V1.0并归档

用户要求当前版本不加入语音助手，下一版本再加入。本轮只调整功能范围，保留已验收的圆盘手势、翻转计时、闹铃与夜间桌宠效果。

## 归档与移除

- `archive/voice-assistant-2026-10-21.zip` 保存33个原路径文件，223,343字节，SHA-256 `68317027663f89bd5a6df9f24d4c889bf7878d8bf52c0f7920d34d2be2aa1e36`。逐文件清单在同名manifest JSON；ZIP完整性、大小与SHA-256均核对，删除活动目录中的专用文件前再次核对原文件哈希。
- 归档包括语音后端、配置网页、模型及许可/打包脚本、配置校验，以及主循环、音频、UI、PC、构建依赖、说明文档的接入前快照。未导出设备NVS或实际密钥。
- 当前构建移除语音源文件、网页/唤醒模型嵌入、ESP-SR和语音WebSocket依赖。官方组件管理器更新依赖锁，仅移除ESP-SR、ESP-DL、dl_fft、WebSocket相关依赖，其他版本/哈希未变。
- 删除设备语音入口、PC模拟语音按钮、语音状态快照与主循环命令、麦克风检测/收发、云端任务与本地唤醒调用；没有配置开关或隐藏入口继续运行它。旧语音NVS命名空间不主动擦除，本版不读取。
- 音频任务只等待铃声/操作提示音队列，使用 `portMAX_DELAY`，取消语音轮询。已有PCM生成、音色、音量、淡入淡出、10毫秒取消检查、播放结束关闭codec及I2S空闲回收代码与归档逐段比较一致。没有改变计时、马达、显示、亮度或电源管理策略。
- 清理语音专用翻译并重新生成字体；保留GB2312城市/网络字符集。归档说明、原详细教程、恢复步骤见 [下一版本语音归档](../archive/voice-assistant/README.md)。当前README、PC与交互说明同步撤下启用教程。

## 检查

- PC全量16/16通过，32.11秒；场景与语音配置的合并测试拆为当前场景测试，语音测试原文留在归档。
- 计时指针回归继续覆盖长按后立即轻点、晃动时手动开始、三个预设、暂停/继续、到时关闭及设置草稿。新增首页中英文语音入口均不存在的检查。
- 实际LVGL截图 `build-assets/voice-archive-preview/home-without-voice-zh.png` 已目视确认：首页左下没有语音按钮，时钟、天气、电量、设置和陪伴正常显示。
- `build-assets/voice-archive-audit.json`：33个归档文件完整性通过、铃声PCM与codec路径一致；改动对照 `voice-archive-integration.diff`。
- 首次IDF依赖重解析成功，但启用configdep时在带空格路径生成 `.cdep` 失败；使用工程既有 `--no-configdep` 兼容选项重新构建。没有因此改SDK、显示驱动或电源参数。

## 固件与安装

使用兼容选项后完整构建通过，官方元数据、受保护布局、版本与更新包内容检查通过。应用从8,720,816字节降至8,050,288字节，减少670,528字节（7.69%）。最终ELF定义符号中没有 `tt_voice_*`、唤醒模型/网页嵌入、语音WebSocket或麦克风收发入口；构建组件中没有ESP-SR、ESP-DL、dl_fft、WebSocket组件，应用二进制中没有原千问模型名/服务地址。证据 `build-assets/voice-archive-firmware-check.json`。

- 应用SHA-256：`3dabdd94117e8dbbbad60575ac1d42af5e968c31b2c5e2529a34f97116a3df15`
- ELF SHA-256：`9348472374f4cfb8e3cd0c684c94716729d55e605688d77dce47f59ed1f15d8e`
- 更新包4,208,993字节，SHA-256：`20b3cd881bb90822a2632065e36fe4b7bf103ede69d52fb6f3a22c1d79eff045`
- 构建日志：`build-assets/voice-archive-build-compatible.log`；校验日志：`voice-archive-validate.log`。

升级前设备仍为已验收圆盘修正版：boot `16415792706600091710`、552.9秒、crash_count=0。保留 `build-assets/voice-archive-rollback.irisfw` 与同名ELF。升级前另保存RAM电池数据：60秒平均131.1mA，稳定段0秒，当前已接USB；该段没有受控亮度/页面条件，不用来比较优化收益或估计续航。证据 `voice-archive-before-status.json / voice-archive-before-power.json`。

经ESP-Iris default网关 `http://127.0.0.1:8443/`、develop模式安装到设备 `4553502d4952495301001c2904d0b31a`。操作 `5743c43c-382e-46e1-b5f3-f1002ca9d070` 最终succeeded、validated/healthy=true，版本0.1.0（产品V1.0）。旧boot `16415792706600091710` → Recovery `6897253708493567709` → 新boot `5530477068032935938`；运行ELF与本次构建一致，无保留coredump。39.6秒状态在线、crash_count=0，PSRAM可用7,964,584字节。证据 `build-assets/voice-archive-install*.json* / voice-archive-after-status.json`。

实机内部截图 `build-assets/voice-archive-device.png`（操作 `2aa01c3f-b7a7-4fcc-9b8c-a0535ed339fc`）已检查：语音入口消失，上海天气、夜间呆猫、电量、设置及陪伴仍显示。本次没有重设用户位置、计时或亮度。

通过官方gateway私有RPC `0x5454/2`、payload `speaker:7` 测试65%音量、3秒、无振动；兼容性由本项目负责。首次日志 `audio=ok`，同时截图期间记录2次音频合成超过10ms，因此另行避开截图补测。声音数值/合成代码未修改，日志只能验证播放通路，不替代听感验收。

补测在同一boot的66510ms启动、67781ms播放结束，`audio=ok`、最大合成1192μs、超过10ms次数0，播放1251ms。空闲音频线程能由队列正常唤醒；44–74秒窗口ui_miss均为0、主循环最大间隔25–35ms。证据 `build-assets/voice-archive-speaker*.json / voice-archive-final-log.jsonl`。本次完成归档、源码/依赖移除、电脑回归、固件静态排除检查、实机安装及短时验证；没有进行真实AI测试或长期稳定性验收。

本轮不将固件瘦身换算为续航提升；一小时续航仍需实际电池测试。
