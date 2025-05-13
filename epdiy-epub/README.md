# epub 电子书阅读器

## 概述
* 基于[开源EPUB阅读器](https://github.com/atomic14/diy-esp32-epub-reader), 适配到我们的SF32-OED-EPD开发板，支持如下功能：
1. 从内置flash或者T卡读取电子书文件（优先从T卡读取）
2. 提供3个按键（上、下、选择）



## 修改字体流程
### 获取ttf的unicode码范围
1. 获取ttf字体unicode码范围：`python3 get_intervals_from_font.py abc.ttf > interval.h`
2. 将生成的`interval.h` 覆盖 `generate_fonts.sh`里面的数组 `intervals`

### 已知需要的unicode码范围生成字体
从abc.ttf生成15号字体,名字为`regular_font`，输出到regular_font.h, 命令如下
`python3 fontconvert.py regular_font 15 abc.ttf  > regular_font.h`


