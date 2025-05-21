# epub 电子书阅读器

## 概述
* 基于[开源EPUB阅读器](https://github.com/atomic14/diy-esp32-epub-reader), 适配到我们的SF32-OED-EPD开发板，支持如下功能：
1. 从内置flash或者T卡读取电子书文件（优先从T卡读取）
2. 提供3个按键（上、下、选择）
3. 支持中文，英文显示。由于中文点阵字库较大，所以去掉了斜体、粗体的字库(见SF32Paper.cpp, `Renderer *SF32Paper::get_renderer() `函数内)



## 修改字体流程
先生成指定的unicode码范围数组，然后用这个数组去指定的ttf里面生成字库
注意：生成的字体的unicdoe编码需要从小到大排列，否则查找会有问题。

### 1. 生成unicode码范围[可选]
当前`fontconvert.py`里面的数组`intervals`存储的是包括常用的英文字符以及GB2312一级字库的unicode码。

#### 获取指定ttf字体里面可用的全部unicode码范围
1. 获取ttf字体unicode码范围：`python3 get_intervals_from_font.py abc.ttf > interval.h`
2. 将生成的`interval.h` 覆盖 `generate_fonts.sh`里面的数组 `intervals`

#### 指定GB2312一级字库的unicode码范围
1. `python3 generate_gb2312_L1_intervals.py` 将生成GB2312 一级汉字的unicode码范围
2. 将生成的数组修改`generate_fonts.sh`里面的数组 `intervals`



### 2.根据unicode码范围生成字体
从abc.ttf生成15号字体,名字为`regular_font`，覆盖../lib/Fonts/regular_font.h, 命令如下
`python3 fontconvert.py regular_font 15 abc.ttf  > ../lib/Fonts/regular_font.h`