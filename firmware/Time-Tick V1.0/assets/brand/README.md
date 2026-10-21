# Time-Tick 艺术字

`time-tick-wordmark.png`：2026-10-20 使用 Codex 内置 imagegen 生成并进行一次透明边缘修订。它是 AI 生成的独立艺术字，不来自商业游戏；`prompts.json` 保留完整生成与修订提示词。原始 RGBA 图保留生成的透明度。

欢迎页使用此艺术字代替三个伙伴预览，版本号保持小号辅助文字；伙伴选择仍在系统设置中。艺术字只用于欢迎页，不用于倒计时文字。

```powershell
python assets/brand/convert_brand.py
```

转换器仅缩放和打包为 392×147 ARGB8888，输出 `main/generated/ui_brand.c/.h`。PC 与 ESP-IDF 使用相同数组，不需运行时 PNG 解码。欢迎页用原生文本保留准确的项目名和版本，艺术字装饰中的沙漏不承担信息功能。
