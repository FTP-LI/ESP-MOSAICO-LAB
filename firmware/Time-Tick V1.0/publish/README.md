# Time-Tick Mosaico 上传资料

当前准备版本 **0.1.1 开发预览**，V1.0 为界面系列名称。适配 Vibe Mode 0.2 / Recovery ABI 2。迁移记录见 [实机与构建验证](../test-records/2026-10-22-vibe-02.md)。网站草稿结果以 `upload-result.json` 记录为准，不把点击上传视为成功。

## 内容

- `../mosaico-ideas.json`：新版 `mosaico-ideas/project-release/1` 作者配置。
- `../docs/ideas-readme.md`、`../docs/time-tick-guide.md`、`../docs/changelog-0.1.1.md`：中文介绍、使用指南与变更说明。
- `../docs/media/01-cover.png` 至 `06-companion.png`：六张 1600×900 介绍图；同名 SVG 为可编辑源文件。
- `screens/`：原始共享 LVGL 电脑渲染截图，天气、电量、日期为模拟数据；没有重绘截图内的 UI。
- `device/zh-CN/`、`device/en/`：官方生成器输出的设备介绍和 MIFF 字体子集。
- `device-text.json`、`generate-device.py`：设备介绍原文和复现入口。
- `sources.json`、`materials-index.json`：固定依赖、素材来源与图像 SHA-256。

介绍组织参考用户指定的 [AURA 光谱声场](https://mosaico.ai/zh-hans/community/yLH9uqdaGTjq)，没有复制其内容或图片。

## 固件与发布包

旧 `build-idf62` 保留，不与新构建混用。先运行 `python tools/prepare_dependencies.py` 获取固定官方依赖，在匹配的 ESP-IDF / GCC 环境构建：

```powershell
idf.py --preview --no-ccache --no-configdep -B build-idf62-v02 build system-update-bundle
python publish/validate.py --build-dir build-idf62-v02 --bundle build-idf62-v02/time_tick-system-update.irisfw
python mosaico.py project package --project . --output build-assets/publish-20261022/time-tick-0.1.1-release.zip
python mosaico.py project validate build-assets/publish-20261022/time-tick-0.1.1-release.zip --json
```

校验及打包使用已安装官方 Iris 主机依赖和 `mosaico-tools/requirements-ideas.txt` 的 Python；构建设置 `ESP_IRIS_PYTHON` 指向该解释器。应用包为未签名 `esp-iris-system-update/0.2`，4,219,997 字节，SHA-256 `50a26923a28850472a048d25ecb1586d744490019f0dca31461fade8c1cb852b`。包只含分区表和正常应用，不替换基础固件或用户 NVS。

17/17 核心 / PC 回归通过（28.70 秒）。最终构建在实机通过完整安装、首次启动及 OTA 健康确认、正常重启与 60 次旋转/亮度切换；截图恢复正常，铃声播放日志无超时。设置旋转和触控已有用户确认；本轮没有重新进行主观音质验收。电量准确性、异常断电和长期续航仍待验证，不承诺一小时续航。

## 素材复现

运行 `node publish/build-materials.cjs`，需要 Node.js、sharp 与 Microsoft YaHei 系统字体。系统字体仅用于渲染，没有分发字体文件。输入截图保留，版本标注由排版脚本生成。

设备介绍使用 Noto Sans CJK SC，许可为 `../assets/fonts/OFL.txt`。原字体缓存放在 `build-font/NotoSansCJKsc-Regular.otf`，生成器校验固定 SHA-256。Linux / WSL 安装 libraqm 和 `shared/ideas/requirements.txt` 后运行 `python publish/generate-device.py`；正常打包读取已提交的预生成文件，无需安装字体排版库。

## 上传状态与流程

先验证完整 ZIP，再登录官方账号。此项目已有网站草稿，继续编辑该条目，不再次使用 `--create` 新建。

0.2 账号状态与旧环境隔离，不拷贝私有凭据。2026-10-22，旧接口 `/api/v1/firmware-projects` 查询及创建均返回 404；新版 `/api/v1/ideas/uploads` 无认证 GET 也返回 404。随后通过网站表单保存草稿，在账号软件列表与编辑页确认六张图、介绍、固件和指南均在。已有草稿代码为 `eB5iQGzwAJTu`，后续只更新这条草稿；最终固件须通过实机启动验证后替换。详细回执见 `upload-result.json`。

只保存草稿，不自动提交审核。用户在网页自行选择的授权选项予以保留。本版源码位于 [迁移分支](https://github.com/FTP-LI/ESP-MOSAICO-LAB/tree/codex/time-tick-vibe-02)，同步固定依赖、构建入口及许可。

## 来源

原创代码与文档沿用 GNU GPL v3，第三方组件保留各自许可。呆猫参考《怪物猎人》艾露猫 Kit T.，角色标识 ©CAPCOM；AI 重绘不等于获得角色授权。来源及生成提示词见 `../THIRD_PARTY.md` 和 assets 目录。当前不包含 AI 语音助手。
