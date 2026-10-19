# ESP-Mosaico V1.2 复古游戏运行方案

本套件优先运行现成的 FC、GB 和 PICO-8 游戏，采用外接按键控制板，复用主机屏幕、电池与声音输出。建议首先验证 Retro-Go 路线的 FC／NES、GB，再验证 GBC；PICO-8 作为后续独立运行时接入。该顺序是开发建议，当前所有平台在本套件上均未验证。

这类需求需要模拟器核心或兼容运行时。通用游戏引擎适合制作新的游戏，不能直接代替 FC／GB 的硬件模拟或 PICO-8 的 API 与语言兼容实现。本方案聚焦设备本地运行、游戏选择、存档和操作体验。查询日期：2026-10-19（Asia/Shanghai）。

## 候选与选择

| 候选 | 已查到的上游依据 | 本套件中的定位 |
| --- | --- | --- |
| Retro-Go 与 Retro-Go S31 | [Retro-Go](https://github.com/ducalex/retro-go) 提供 NES、GB、GBC 等系统及菜单、缩放、存档；[S31 移植](https://github.com/PIGEON-SOFT/retro-go-s31) 面向 ESP32-S31-Korvo-1 | FC／GB 优先评估路线；需要 Mosaico 板级移植 |
| femto8 | [项目说明](https://github.com/benbaker76/femto8) 面向嵌入式 PICO-8 兼容；[esp-box-emu](https://github.com/esp-cpp/esp-box-emu) 提供 ESP32-S3 上的集成实例 | PICO-8 优先试验候选；先测指定卡带，不预设完整兼容 |
| FAKE-08 | [项目说明](https://github.com/jtothebell/fake-08) 提供 PICO-8 卡带兼容运行，列出的发布平台不包含 Mosaico | 作为 PICO-8 对照候选；仍需移植与性能测试 |

Retro-Go S31 的现有目标使用 800 × 480 RGB 屏、ES8389 音频和 SD 卡；Mosaico 使用 480 × 480 QSPI 屏、ES8311 和板载 SPI NAND。相同主控只减少芯片适配工作，不能直接使用 Korvo 的板级配置或固件。依据：[S31 项目说明](https://github.com/PIGEON-SOFT/retro-go-s31/blob/575a5c3071864cdce2224a940adb7c545cfe835d/README.md)、[Mosaico V1.2 官方指南](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp-mosaico/user_guide.html)。

PICO-8 官方发行面向 Windows、Mac、Linux 和 Raspberry Pi；本方案使用兼容实现，不包含官方编辑器或完整在线浏览功能。[官方规格](https://www.lexaloffle.com/pico-8.php) 列出其 128 × 128 显示与专用 P8 Lua。普通 Lua 解释器本身不足以运行现有卡带。esp-box-emu 已说明复杂卡带的运行速度受限，其 S3 性能不能直接推定为 Mosaico 性能。

## 主机资源与移植任务

官方 V1.2 指南列出 ESP32-S31、16 MB PSRAM、16 MB NOR 和 128 MB SPI NAND。NOR、NAND 是存储，不能相加当成运行内存；PSRAM 中还要容纳画面、音频、模拟器及系统数据。以下任务在代码选型后逐项验证。

| 部分 | 本套件需要完成的工作 |
| --- | --- |
| 工具链 | 固定支持 `esp32s31` 的 ESP-IDF、BSP 和模拟器提交；先跑通主机显示与声音，再导入核心 |
| 显示 | 将模拟器帧缓冲接到 CO5300 QSPI 显示路径；验证缩放、颜色格式、缓冲和传输调度 |
| 音频 | 通过 ES8311 输出核心产生的音频；处理采样率、缓冲、暂停与音画同步 |
| 输入 | 将外接实体按键转为方向、A、B、Start、Select 和菜单动作；验证组合键、去抖及扫描延迟 |
| 存储 | 提供游戏浏览、ROM／卡带读取、存档和配置读写；选择底层介质并完成文件系统适配 |
| 分区与切换 | 复核多应用镜像在 16 MB NOR 中的布局；明确进入游戏、返回菜单是否重启以及存档时机 |
| 电源 | 在约定亮度、音量和游戏场景下测量功耗与续航，再决定性能和无线功能配置 |

Retro-Go 的 [移植说明](https://github.com/ducalex/retro-go/blob/master/PORTING.md) 定义了设备 target、输入映射和驱动接入方式。Mosaico 移植应沿用该机制，避免把电气针号散落在各个模拟器核心中。

## 游戏存储先做验证

初期建议用小型、允许测试的自制游戏验证核心，不立即增加存储硬件。Retro-Go S31 文档提供内置 NES 游戏的验证路径，但其现有外部游戏和存档流程依赖 SD 卡；内置游戏能运行不等于存档系统已经可用。依据：[固定版本快速开始](https://github.com/PIGEON-SOFT/retro-go-s31/blob/575a5c3071864cdce2224a940adb7c545cfe835d/README_S31.md)。

优先评估复用板载 NAND，测量挂载、读取、写入和异常中断后的行为，并调查原固件已经占用的空间与格式。若 NAND 适配成本或容量不满足游戏库需求，再评估控制板增加 microSD。microSD 尚未进入硬件定案，不能在引脚和 BOM 中预先当作既定器件。

设备上的游戏文件与开发仓库分开管理。存储布局在实现时写入 `storage-layout.md`，区分游戏内容、游戏自身存档、模拟器即时存档、封面与用户配置。即时存档应关联 ROM 哈希、核心版本和格式版本，升级后不假设旧状态仍然兼容。测试样例记录来源和许可；个人游戏库与个人存档不进入仓库。

## 优先改善的体验

- **实体操作**：首轮建议配置方向键、A、B、Start、Select；菜单使用独立按键或明确的组合键。X、Y、肩键根据后续平台再决定。验证斜向与动作键同时按下，避免漏键和误触菜单。
- **画面比例**：保留原始比例和整数缩放选项，不默认把所有游戏拉成正方形。PICO-8 的 128 × 128 画面可按 3 倍显示为 384 × 384，在 480 × 480 屏幕四边各留 48 像素；这是显示布局建议，性能待测。FC、GB 分别按核心输出和显示比例配置。
- **稳定时序**：核心按原平台节奏运行，显示刷新与模拟步进分别统计。必要时减少显示帧，不通过拖慢模拟时钟掩盖性能不足。PICO-8 按卡带所需的更新节奏测试。
- **可靠存档**：区分游戏电池存档与即时存档；验证退出保存、重启恢复、升级兼容与写入中断。通过测试后再提供自动恢复上次游戏。
- **顺手的菜单**：先保证列表选择、暂停、音量、退出和错误提示可用，再考虑封面、收藏与最近游玩；优先复用上游已有功能。

## 文件规划调整

套件的 [文件规划](README.md) 继续负责硬件、结构和通用文档，[首轮需求](requirements.md) 规定硬件与软件的并行工作和汇合点。固件由通用游戏程序转为复古游戏运行平台，补充以下文件；目前已有需求和方案文档，代码与实测记录在实际移植时创建。

```text
docs/game-console-kit/
├── emulation-plan.md                    本方案与候选证据
├── requirements.md                      确认目标平台、按键与体验要求
├── game-compatibility.csv               逐游戏的兼容性结果
└── test-records/                        实机记录与证据

firmware/game-console/
├── README.md                            平台范围、构建入口与兼容板卡
├── upstream.md                          引入来源、固定提交、改动与许可范围
├── docs/
│   ├── porting.md                       显示、音频、输入和存储移植状态
│   ├── storage-layout.md                分区、文件系统、游戏及存档路径
│   ├── board-mapping.md                 物理键到逻辑键的映射
│   └── build-and-flash.md               多应用构建、备份与恢复流程
├── launcher/                            采用 Retro-Go 时保留的上游菜单工程
├── retro-core/                          采用 Retro-Go 时保留的模拟器工程
└── components/
    └── retro-go/
        └── targets/
            └── esp-mosaico-v1.2/        新增板级 target，配置按上游约定建立
```

上述是职责示意，不是完整构建树。采用 Retro-Go 时保留其完整的多应用工程结构、构建工具、核心和许可证；不将其强行改成单个 `main/` 工程，也不把其他平台支持当成首版交付承诺。引入源码后，使用本仓库 Git 跟踪移植改动，在 `upstream.md` 记录明确的基准提交。PICO-8 原型通过后，再决定注册为独立应用还是集成到现有应用；不预先编造通用引擎插件接口。

`game-compatibility.csv` 计划记录：平台、游戏标识、版本与哈希、来源、核心及提交、套件固件与板卡修订、画面设置、运行速度、音频、输入、存档、结论和实测记录链接。NES 另记 mapper，GB／GBC 另记卡带控制器与 RTC 等相关特性，PICO-8 另记卡带 API／多卡带依赖。文件在首次建立真实测试条目时创建。

## 验证顺序

1. **板级基线**：固定工具链与 BSP，运行显示、声音和输入诊断，记录可用内存；必要时先用触摸或测试输入，独立推进控制板开发。
2. **FC／GB 原型**：先接入少量测试游戏，验证加载、速度、音画和退出流程；GBC 在 GB 路径成立后增加。到这一步仍不声明整个平台全面兼容。
3. **游戏库与存档**：落实 NAND 或 microSD 方案，测试不同游戏规模、保存恢复及异常中断；复核固件分区和恢复方式。
4. **PICO-8 原型**：优先测试 femto8 集成路线，覆盖 `.p8`、`.p8.png`、音效、音乐、卡带数据保存和退出；记录不支持的 API 或卡带行为。
5. **整机体验**：固定亮度、音量、无线状态和代表游戏，记录模拟速度、显示帧时间、音频欠载、按键到画面延迟、内存峰值及续航。

较重平台如 GBA、SNES 暂作为后续评估项。某个核心存在、某块其他开发板能运行或某款游戏通过，都不等于本套件已经兼容该平台。验证标准应同时包含可玩性和持续运行稳定性。

## 来源版本

下列提交是本次查询基准，尚未引入为项目依赖。已检查 Retro-Go S31 与 esp-box-emu 的完整主仓库文件树，均未发现名称含 `mosaico` 的路径；这只说明本次查阅范围，不能证明不存在其他移植。

| 来源 | 查询到的提交 | 使用范围 |
| --- | --- | --- |
| [Retro-Go S31](https://github.com/PIGEON-SOFT/retro-go-s31/tree/575a5c3071864cdce2224a940adb7c545cfe835d) | `575a5c3071864cdce2224a940adb7c545cfe835d` | S31、多应用构建、现有板卡及存储约束 |
| [esp-box-emu](https://github.com/esp-cpp/esp-box-emu/tree/66834cfd677cc67a1a13ad94fa1c1be839c4efd9) | `66834cfd677cc67a1a13ad94fa1c1be839c4efd9` | femto8 的 ESP32-S3 集成参考 |
| [femto8](https://github.com/benbaker76/femto8/tree/f9e5097c882d0c9203616689f3de5531e0597c43) | `f9e5097c882d0c9203616689f3de5531e0597c43` | 嵌入式 PICO-8 候选；不假设与集成项目内的副本一致 |
| [FAKE-08](https://github.com/jtothebell/fake-08/tree/814991a2571ad3970e386cef48f3b148aa1c27b9) | `814991a2571ad3970e386cef48f3b148aa1c27b9` | PICO-8 兼容与移植对照候选 |

本方案为仓库自行整理的文档，沿用 [LICENSE](../../LICENSE)。引入的模拟器、运行时、依赖和游戏素材分别保留原有许可与归属，在引入时登记；根目录许可证不覆盖第三方声明。
