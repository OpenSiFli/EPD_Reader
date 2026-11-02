# epub 电子书阅读器

SF32LB527 + E0470A03-AF-S 新板使用 `sf32lb527-oed-e0470a03_hcpu` 目标，
竖屏 UI 为 684×1216。构建方法、显示/触摸/冷暖前光/TF 接线和验证范围见
[新板说明](BOARD_SF32LB527.md)。
支持菜单或开机按住 PA33 按键进入[USB 恢复升级](DFU_SF32LB527.md)；PA34 电源键长按约 10 秒用于硬件重启，不参与升级检测。
日常 USB 升级只更新应用、字体和波形，保留启动与恢复程序。[Windows 图形升级工具](tools/flash_gui/README.md)同时支持串口首刷、原生 USB 升级和 ZIP / JSON / INI 固件包导入。
阅读空闲时提前准备下一张文字页的显示波形，降低翻页响应延迟；[耗时与连续翻页验证](VALIDATION_PREFETCH_527.md)。
进一步按全屏重复像素组合共享波形，密集正文准备从约1.7秒降到约0.25–0.32秒；[波形准备优化记录](VALIDATION_WAVEPREP_527.md)。
矢量字体使用 PSRAM 缓存并复用已解析章节，任务结束后自动进入保留状态的深睡；[字体切换与休眠验证](VALIDATION_FONT_POWER_527.md)。

目录介绍：
```
EPD_Reader
│  
├──epdiy-epub                             # epub阅读器代码目录
└──SiFli-SDK                              # 思澈平台的SDK
```

  epub阅读器的详细介绍在[epdiy-epub](epdiy-epub/)目录。 
