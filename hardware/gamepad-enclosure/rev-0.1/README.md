# GamePlay 结构 rev-0.1

2026-10-19，`experimental`。配合未发布的 gamepad-board rev-0.1 / GamePlay V1.0。

- [尺寸与模型分析](dimensions.md)：主机中框 44.90 × 44.90 × 11.10 mm、侧孔与安装孔坐标、H2 厂家图纸核对。
- [纵向布局](vertical-layout.svg)：72 × 112 mm 设计包络；[参数](vertical-layout.json) 与母 PCB 文档层使用相同坐标。它不是 PCB 板框、外壳开孔或制造图。
- [结构采购清单](parts.csv)：FunnyPlaying GBC 胶垫原色套装、配套黑色键帽。仅选型，未下单。
- [来源记录](sources.json)：具体商品变体、可售状态、图纸与输入模型 SHA-256。
- [主机尺寸图](dimensions/host-frame-dimensions.png) 与 [机器可读测量](dimensions/host-frame-measurements.json)。

STL 由用户提供，原文件位于 `C:/Users/27348/Downloads/ESP-MOSAICO.stl`。尚未取得其原始下载网址、修订说明及再分发许可，因此仓库只保存数值测量和尺寸图，不把该文件标为乐鑫官方发布。模型不包含屏幕、PCB、电池或完整后盖，不能用中框厚度替代整机厚度。

复现测量：安装 `numpy`、`trimesh`、`networkx`、`matplotlib` 后，在仓库根目录运行：

```powershell
python tools/analyze-mosaico-stl.py "C:/Users/27348/Downloads/ESP-MOSAICO.stl" hardware/gamepad-enclosure/rev-0.1/dimensions
```

分析脚本只读 STL，不修改源模型；针对这份模型的 X=±21、Z=-3 截面查找侧孔和安装孔，不能当成任意 STL 的通用识别工具。原始 XYZ 保持不变，未自动指定电气 Pin1。

原创文字、脚本与布局沿用仓库许可证；商品和厂商图纸只链接引用。GBC 参考尺寸来自 MIT 许可的 esp-cpp/esp-box-emu，归属与许可见 [reference/README.md](reference/README.md)。尚未进行实物装配、手感、插接或寿命验证。
