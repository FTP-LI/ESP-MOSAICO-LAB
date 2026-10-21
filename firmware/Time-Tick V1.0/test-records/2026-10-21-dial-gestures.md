# 倒计时圆盘手势

> 后续用户反馈轻点不灵敏，并要求取消底部按钮。新的交互、旋转坐标测试修正和触控验收进度见 `2026-10-21-dial-refinement.md`；下文保留初版实施记录，不作为当前操作说明。

用户要求倒计时期间单击圆盘暂停、长按停止；暂停后再次单击继续。保留原有暂停/继续、停止/开始按钮以及翻转到时提醒的规则。

## 实现

- `ui_focus.c` 增加覆盖数字及进度环的304×304圆形透明触控区，按圆角精确命中，不接管圆盘外的按钮与背景。
- 使用LVGL的 `SHORT_CLICKED` 切换运行/暂停，`LONG_PRESSED` 停止；不监听松手仍会发出的 `CLICKED` 或重复长按事件，防止一次长按产生两种动作。沿用当前LVGL约400ms长按判定，没有新增轮询定时器。
- 触发前滑出圆盘取消；停止、方向禁用时单击圆盘不启动计时。到时长按沿用停止命令清除提醒，单击不会重启。
- 运行/暂停页加入中英文操作提示并重新生成字形。功耗、动画、IMU、音频和显示传输配置未改变。

## 电脑验证

先加入真实LVGL指针测试，原实现于checkpoint 220失败（中心轻点未暂停）；完成后 `preview_timer / preview_brightness / core / alarm` 4/4通过，11.48秒。

覆盖中心轻点暂停、暂停时间保持、圆环边缘轻点继续、长按在松手前停止且只提交一次、长按松手不追加单击或开始、停止后点击不重启、触发前滑出、圆形包围框角落不响应、暂停时长按停止、三个计时方向和禁用方向。既有翻转/到时关闭及无效重绘回归继续通过。

随后补充到时边界的真实指针回归：单击不重启/不暂停完成状态，长按一次关闭声音、提醒和完成状态，松手仍保持停止。补充后的 `preview_timer` 通过。

已检查 `build-assets/dial-gestures-preview/countdown-zh.png`：中文提示完整、数字与圆盘排版不变。电脑测试不代替真实面板触控验收。

## 构建与设备状态

固件编译、官方元数据/受保护布局/更新包检查通过。

- BIN：8,720,160字节；SHA-256 `11759169ccb7c5ff09552ea5b059cedf7f7ab0b24021a7a233a7452d4c6c01de`。
- ELF SHA-256：`6d390f63aed79aac540c6eeeef573332a284074ebbea5d1fb7ec071f23aeb1ab`。
- 更新包：4,535,403字节；SHA-256 `9ebebcb439e03d24d874488cc2a5e409faadcc69dfcf28f10497d583f9f3276c`。
- 回退包：`build-assets/dial-gestures-rollback.irisfw`，即第五轮省电修复版，SHA-256 `dda9285db1b9788963de152b91c1e0fc68f70d385b2d050f8bf91385a25b63f2`，配套ELF已保留。

开始时设备boot为 `7646408713367174384`，运行242秒、crash_count=0；power RPC尚无有效放电段（sampled_seconds=0），不是新版续航测量结果。编译完成后设备离线，未发起更新。

## 重连后的安装与检查

用户随后回复“已接回USB，没有重启”。先于更新保存状态、耗电和crash证据，发现boot已变为 `5753649866735311477`，`reset_reason=1`（上电复位），但仍为第五轮省电固件。无软件崩溃记录；此前RAM放电数据为空，不能计算本次续航，也不能将重启归因于本次更新或用户操作。供电中断/复位原因待排查，详见 `2026-10-21-power-runtime.md` 的第五轮重连记录。

- 通过ESP-Iris default网关 `http://127.0.0.1:8443/`、develop模式安装至设备 `4553502d4952495301001c2904d0b31a`。操作 `b0d32529-10f5-4f5f-b8e8-1bcaec81dd14` 最终 `succeeded`、`validated=true`、`healthy=true`，版本0.1.0（界面产品版本V1.0）。
- 安装前boot `5753649866735311477` → Recovery `8429659385035649712` → 新boot `10866832303496660553`；新ELF为 `6d390f63aed79aac540c6eeeef573332a284074ebbea5d1fb7ec071f23aeb1ab`，与目标一致。无保留coredump；本次安装正常软件复位 `reset_reason=3`，`crash_count=0`。
- 设备自动恢复保存的10%亮度；首页内部截图 `build-assets/dial-gestures-device.png` 已检查，天气、时钟、呆猫、充电电量与入口显示正常。截图操作 `10d53a81-a868-4303-baad-7ecf5eabfe18`。
- 安装及运行证据 `build-assets/dial-gestures-install*.json*`、`dial-gestures-after-devices.json`、`dial-gestures-runtime-log.jsonl`。启动首个10秒性能窗口ui_miss=0，loop_max=81ms，含启动/网络工作；随后25/35/45秒窗口loop_max=44/31/27ms，ui_miss均为0。不据此声称长时稳定性或省电改善。
- 发出实物触控验证请求后，最终状态查询返回unknown device，设备列表为 `connected=false/cached=true/state=offline`，保留上述新boot与13.7秒时的缓存状态。不能将缓存当作在线健康，也不能仅凭USB离线判定卡死或再次重启；最后收到45秒运行日志。实物测试结果待用户反馈。

真实LVGL指针电脑检查已通过；已请用户在实物验证轻点暂停/继续、长按停止及松手不重新启动，实际触控验收待回复。
