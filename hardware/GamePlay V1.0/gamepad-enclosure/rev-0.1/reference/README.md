# GBC 胶垫几何参考

来源：[esp-cpp/esp-box-emu](https://github.com/esp-cpp/esp-box-emu/tree/66834cfd677cc67a1a13ad94fa1c1be839c4efd9)，固定提交 `66834cfd677cc67a1a13ad94fa1c1be839c4efd9`，MIT 许可，Copyright (c) 2022 esp-cpp。原文见 [LICENSE](LICENSE)。

三个未修改的 KiCad 胶垫参考文件来自 `ecad/elec/src/parts/GBC_*_MEMBRANE/`。该工程的 README 也选用 FunnyPlaying GBC 胶垫和键帽，因此可用于首轮空间安排；它不是 FunnyPlaying 公差图或乐鑫主机图纸。

本轮仅取 D-pad 和 A/B 小圆的参考中心用于文档层定位，未复制铜电极或钻孔到 GamePlay PCB。A/B 文件的 B 大圆与小圆中心存在偏差，不能将大圆当成已验证的键帽开孔中心。Select/Start 的完整几何尚未采用，布局仍为区域预留。到货后复测导电粒、定位孔、键帽行程和支点，才可设计最终触点与壳体。
