# 中英文字体

界面嵌入 `main/generated/fonts/font_16.c`、`font_18.c`、`font_24.c` 、`font_52.c` 和 `font_84.c`，分别用于辅助文字、正文、标题、时钟和倒计时数字。均为 Noto Sans CJK SC Regular 的 4 位灰度字形子集，保留平滑边缘；场景和动物使用 [生成插画](../pets/README.md)。

- 来源：[Noto CJK 官方仓库](https://github.com/notofonts/noto-cjk)，[字体原文件](https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf)。
- 原文件版权信息：© 2014–2021 Adobe (http://www.adobe.com/)。生成字形使用 [SIL OFL 1.1](OFL.txt)，不改为项目 GPL 许可。
- 原文件 SHA-256：`2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`。生成脚本校验此哈希。
- 已验证生成环境：Python 3.12、Pillow 12.3.0（FreeType 渲染）。

修改界面中文后，在本项目目录运行：

```powershell
python assets/fonts/generate_font.py build-font/NotoSansCJKsc-Regular.otf
```

完整 OTF 仅放在忽略的 `build-font/` 缓存中，不进入固件。普通 PC / ESP-IDF 编译直接使用已提交的 C 字形文件，不需要联网下载或运行字体生成器。18/24 px 子集包含 ASCII 与项目中文文案；16 px 额外包含 GB2312 的 6763 个汉字，作为中文 SSID 的回退字体，共 6870 个字形、878808 字节位图。52/84 px 各包含时间所需的 13 个字符。罕见字及其他文字可能显示缺字框，并非完整 Unicode 字库。界面支持中英文切换；文本输入键盘仍为字母/数字键盘，城市建议填拼音或英文。
