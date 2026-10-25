# Desk-Deck 发布资料

版本：**0.1.0-dev 开发预览**。源码标签：`desk-deck-v0.1.0-dev`。本版直接发布 2026-10-25 已获实物确认的固件，不为包装或版本文案重新编译应用。

## 内容

- `description.md`：Mosaico 中文正文。
- `guide.md`：安装步骤、24 项默认快捷键与版本边界。
- `release-notes.md`：GitHub 预发布说明。
- `screens/`：本版设备四场景原始截图。
- `media/`：四张 1600×900 介绍图；`build_materials.py` 为可复现排版入口。
- `materials-index.json`：截图来源、固件 ELF 标识与图片校验值。
- `device/zh-CN/`、`device/en/`：官方生成器产出的双语设备卡片。
- `../mosaico-ideas.json`：官方单 ZIP 发布包配置。
- `release-manifest.json`：本地成品大小与 SHA-256。
- `upload-result.json`：上传回执，以实际保存/发布结果为准。

## 校验与打包

使用已配置官方 Iris 依赖及 `mosaico-tools/requirements-ideas.txt` 的 Python，在工程目录执行：

```powershell
python publish/collect_notices.py
python mosaico.py project package --project . --output build-assets/release-0.1.0-dev/desk-deck-0.1.0-dev-release.zip
python mosaico.py project validate build-assets/release-0.1.0-dev/desk-deck-0.1.0-dev-release.zip --json
```

正常打包读取已提交的卡片，无需重新生成字体。本机打包使用官方 Iris Python，配合 `build-assets/python-win` 中独立安装的 Ideas 依赖。

介绍图：Windows / Pillow，运行 `python publish/build_materials.py`，仅使用本机系统字体排版，不分发字体文件。设备卡片：Ubuntu 24.04 / libraqm ≥0.9，安装 `shared/ideas/requirements.txt` 后运行 `python publish/generate_device.py`，读取本项目固定 LVGL 中的 Source Han Sans。

## 发布范围

GitHub 发布为 prerelease，附件为 `.irisfw`、官方校验通过的 `-release.zip`、中文指南、第三方许可声明与 SHA256SUMS。源码、文档、原始截图、可重建素材及锁定依赖进入 Git；SDK、构建缓存、日志、凭据、网关状态均忽略。

Mosaico 上传相同 `.irisfw`、指南、四张图与中文正文；保存后从账号软件列表重新打开确认。网站审核与公开发布状态分别记录。上传规则确认与可选开发者共创授权遵循用户在网页上的选择。

当前功能与实测边界见 [实机记录](../docs/HARDWARE-TEST-2026-10-25.md)。网页原型的配置修改不会同步到设备。
