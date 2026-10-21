# Mosaico Ideas 框架检查

日期：2026-10-20（Asia/Shanghai）。项目 GamePlay V1.0，应用版本 0.1.0。状态为 experimental。

| 检查 | 结果 |
| --- | --- |
| 官方 load_manifest 校验网页元数据与资料路径 | 通过；封面仍为空，待提供实机素材 |
| 官方工作区解析及 utils 固定提交 | 通过，fc63d43f3ac72aa54e701b07b07fbed07a16b3ca |
| ESP-IDF gen_esp32part.py --verify | 通过，16 MiB 布局 |
| 保留分区、对齐、不重叠、Flash 范围及 ota_0 类型检查 | 通过 |
| 负向用例：修改 factory、应用重叠、超出 Flash、未接入的资源分区 | 四种均按预期拒绝；含正常布局共五项测试通过 |
| 网表、28 个 BOM 器件、九键固件映射与原生 EDA 导出 | 通过；没有改动键位映射 |
| 本地文档链接、Python 语法、VS Code 环境路径 | 通过 |
| ESP-Iris 宿主环境 | 与固定 requirements.lock 的指纹一致，官方 bundle --help 可运行 |
| git diff --check | 通过 |

复核入口：在应用目录执行 `python publish/validate.py`。分区表检查使用固定诊断 SDK 自带的 `components/partition_table/gen_esp32part.py`。

此次没有执行完整 ESP-IDF 配置/编译，没有生成新应用二进制或实际 .irisfw，没有设备安装、烧录、登录或上传。新增组件的依赖锁仍需在首次成功构建后更新复核；旧诊断构建记录不代表新框架已经编译通过。

框架接入的自动 OTA 接受行为来自官方应用适配器，仅属于启动协议，不等于显示、音频或游戏功能测试通过。正式发布前仍须完成安装、返回 Recovery、外接按键和游戏功能验收。
