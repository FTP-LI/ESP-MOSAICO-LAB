# Time-Tick 发布框架

使用 Mosaico Ideas 的官方 `mosaico-ideas/project-upload/v1` 元数据格式，复用固定 utils 中的应用契约、Recovery 接口和 `system-update-bundle` 目标。

- [元数据](../mosaico-ideas.json)：标题、display/sensors 分类、说明及外部链接。
- [网页说明](../docs/ideas-readme.md)：当前实际状态；不宣称实机验收通过。
- [来源清单](sources.json)：SDK、BSP、Vibe/utils 固定提交。
- [本地检查](validate.py)：读取官方检查器，检查元数据、工作区、版本与受保护分区；不会上传或烧录。

在本项目目录运行 `python publish/validate.py`。在 VS Code 官方 ESP-IDF 扩展中完整构建后，通过扩展终端执行 `idf.py --preview -B build-idf62 system-update-bundle`。以该目标实际输出的 `.irisfw` 路径执行：

```powershell
python publish/validate.py --build-dir build-idf62 --bundle "实际生成的文件.irisfw" --iris-python "官方 ESP-Iris 环境的 python.exe"
```

实际包必须通过版本、目标、分区、SHA-256 与官方 bundle 校验。按固定版本官方规则：ESP32-S31、未签名、最多 32 MiB，恰好一个 application；可选 partition_table/data，不能携带 bootloader、recovery 或平台身份分区。来源：[官方上传规则](https://github.com/esp-mosaico/esp-mosaico-utils/blob/fc63d43f3ac72aa54e701b07b07fbed07a16b3ca/mosaico-tools/skills/mosaico-ideas-upload/SKILL.md)。

发布前完成硬件验收表、生成精确源码版本及依赖清单，并添加真实封面/截图；空 covers/boards 目前表示没有素材和未经验证的扩展板声明。版本来自 `version.txt`，不要在外置元数据中重复手填 version。

2026-10-20 查询 [平台](https://mosaico-ideas.espressif.com/)时首页显示维护中。当前仅建立本地资料，没有登录、创建平台草稿、上传或公开发布。
