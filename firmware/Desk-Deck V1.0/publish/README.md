# Desk-Deck 发布资料

2026-10-29 工程已更名为 **Desk-Deck V1.0**。本目录及 `mosaico-ideas.json` 保留 2026-10-25 已上传的 `0.1.0-dev` 发布快照：原名称、标签链接、截图、设备卡片、固件和校验记录均对应当时发布内容；工程更名不代表已重新上传或发布 1.0 稳定版。

更名后重新构建的 `build-idf62/` 产物不属于上述历史发布包。复现历史发布时使用 `build-assets/release-0.1.0-dev/` 中通过原校验的附件；下一次发布需先同步新名称的介绍图、设备卡片、版本和校验清单，再运行下方打包命令。

版本：**0.1.0-dev 开发预览**。源码标签：`desk-deck-v0.1.0-dev`。本版直接发布 2026-10-25 已获实物确认的固件，不为包装或版本文案重新编译应用。

## 发布状态（2026-10-25）

- [GitHub 开发预览版](https://github.com/FTP-LI/ESP-MOSAICO-LAB/releases/tag/desk-deck-v0.1.0-dev)已公开，5 个附件的大小和 GitHub 返回的 SHA-256 与本地一致。固件对应源码提交 `0ed065b21f1713e0f8affd86a50a34fb8d1f79b4`。
- Mosaico 项目 **200105**（`R5tTkWfKZ3bc`）已由用户提交审核，账号软件列表已核对为**待审核**。四张展示图、介绍、固件和指南已上传，审核通过后自动同步社区。
- 当前尚未确认 Mosaico 审核通过或公开发布；状态以 [上传回执](upload-result.json) 和平台实际结果为准。

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

Mosaico 上传相同 `.irisfw`、指南、四张图与中文正文；保存后从账号软件列表重新打开确认。网站审核与公开发布状态分别记录；本次相关授权及最终提交由用户在网页上完成。

当前功能与实测边界见 [实机记录](../docs/HARDWARE-TEST-2026-10-25.md)。网页原型的配置修改不会同步到设备。
