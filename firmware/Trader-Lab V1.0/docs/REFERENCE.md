# Genius-Trader-Lab 核对记录

2026-10-25 只读检查本机 `Genius-Trader-Lab`，当时 HEAD 为 `c033c444f05f59ae73355bc14ec8dae090d6d2fa`，工作区干净。没有启动服务、修改源码、读取个人持仓文件或请求真实行情。

## 已确认模块

| 参考模块 | 当前职责 | 本工程用途 |
| --- | --- | --- |
| Trader-Map | Web / Windows / 鸿蒙共享行情和研究界面 | 主要功能及数据质量参考 |
| Analysis | 独立基金组合分析、数据门禁与报告上下文 | 暂不接入；不依赖其个人档案 |
| Detect_support_and_resistance_levels | 独立支撑阻力研究工具 | 后续评估，不作为首版依赖 |

## 查看过的入口

以参考工程仓库根目录为基准：

- `README.md`、`PROJECT_STATUS.md`、`Trader-Map/README.md`、`Trader-Map/site/README.md`。
- `Trader-Map/site/app/TraderMapApp.tsx`：总览、自选、资金、风险、资讯、机构六模块。
- `app/api/watchlist/route.ts`：ETF 批量报价与可选分析；单标的报价、历史和分析时效分别检查。
- `app/api/market/route.ts`：单标的报价、K 线和资金数据；分字段来源及备用历史。
- `app/api/dashboard/route.ts`：市场广度、资金、机构、两融、VIX 的聚合数据。
- `app/market-types.ts`、`app/dashboard-types.ts`、`app/hotspot-types.ts`：时间、来源、空值与分析字段。
- `app/public-quotes.ts`、`app/watchlist-quotes.ts`、`app/watchlist-freshness.ts`：多源补齐、单标的缓存、时间倒退和质量判定。
- `scripts/desktop-runtime.mjs`：现有桌面服务绑定本机回环地址，不能直接当作独立设备的公网接口。

以上 `app/` 和 `scripts/` 均位于 `Trader-Map/site/`。部分 README 包含早期规划或旧限制，具体当前接口以核对提交的源码为准。

## 移植边界

React、Node/.NET 桌面服务和鸿蒙桥不能直接在 ESP32-S31 固件运行。独立联网方案需要用设备支持的 HTTP/TLS、解析、存储和 LVGL 实现小屏客户端；先保留报价原始时间、来源、缺失/缓存和部分失败语义。

2026-10-26 继续只读核对 `watchlist-quotes.ts`，参考公开行情字段和完整性语义，以 C 独立实现解析和跨价状态机。没有搬入个人持仓、图片或跨仓库服务依赖。电脑真实响应交给本工程 C 解析器验证，不把参考工程测试计作本设备结果。

## 本轮复用记录

- `Desk-Deck`：本仓库 `edf0d3f8e612b41302475d10d15490caa746580f` 的官方启动／恢复、截图后端、IMU 映射、锁外旋转与亮度、依赖获取和构建脚本；适配至本工程，GPL-3.0-only。
- `Time-Tick V1.0/main/core/alarm_model.c/.h`：复制已修正削波和包络的旋律合成，保留 GPL-3.0-only。播放队列和取消思路来自 `main/platform/interaction.c`，单独实现 TX 音频任务。
- `Time-Tick V1.0/main/platform/sdk_compat.c`：复用 Vibe Mode 0.2.1 与当前 IDF 的 REF160M 门控兼容修正；仅复制到本工程，不修改原应用或共享 SDK，GPL-3.0-only。
- 官方 BSP `5f88b91e1be055d1d3674364123b2779a1758377` 的 `onboard/audio.c`：参考 ES8311 配置与引脚，通过公开驱动创建 TX-only 扬声器，避免闲置 RX DMA 常开。原代码 Copyright 2026 Espressif Systems (Shanghai) CO LTD，Apache-2.0，见 `licenses/Apache-2.0.txt`。
- 字体：LVGL 9.5.0 所带 Source Han Sans SC，Adobe，SIL OFL 1.1；许可证在 `licenses/SourceHanSansSC-OFL.txt`，脚本生成常用汉字子集。
- `gbk.c`：由 Python 标准 GBK 编码表生成码点映射，无在线转码服务。

参考应用未修改，官方依赖保持干净；依赖下载在本工程，不连接其他项目生成目录。

## 全球指数接口核验（2026-10-27）

使用[腾讯公开全球指数报价](https://qt.gtimg.cn/q=sh000001,sz399001,sz399006,hkHSI,gzN225,gzKS11,usINX,usIXIC,usDJI,gzGDAXI,gzFTSE,gzFCHI)，电脑端真实响应经 C 解析器核对。属于公开端点，无签约稳定性或商用授权保证；设备端结果见本轮实机记录。Yahoo 方案在实机返回 HTTP 403，已从固件移除，不绕过访问限制。
