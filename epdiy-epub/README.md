# epub 电子书阅读器

## 概述
* 基于[开源EPUB阅读器](https://github.com/atomic14/diy-esp32-epub-reader), 适配到我们的SF32-OED-EPD开发板，支持如下功能：
1. 从内置flash或者T卡读取电子书文件（优先从T卡读取）
2. 提供3个按键（上、下、选择）
3. 支持中文，英文显示。由于中文点阵字库较大，所以去掉了斜体、粗体的字库(见SF32Paper.cpp, `Renderer *SF32Paper::get_renderer() `函数内)

## 使用指南
### 硬件连接
硬件连接主要是开发板与屏幕的连接，找到相应的卡扣，扣紧（注意查看引脚顺序）就可以连接成功。
`SF32-OED-6'-EPD_V1.1`和`SF32-OED-6'-EPD_V1.2`硬件上只是布线和个别电阻调整的区别,对软件来说可以用同一个板子配置(即`epdiy-epub\sf32-oed-epd_v11`目录)

### 编译环境要求
目前epub电子书只验证过[SiFli-ENV 1.1.2](https://docs.sifli.com/projects/sdk/latest/sf32lb52x/quickstart/install/lacey_install.html)模式下使用armclang编译方式，并且Keil的版本要求是5.32。已知在Keil 5.42版本上会有C语法兼容问题， GCC模式下也有兼容问题和空间越界问题，待后续版本更新。

#### 程序编译与烧录
切换到例程project目录，运行scons命令执行编译：
```
scons --board=sf32-oed-epd_v11 --board_search_path=.. -j8 
```
运行`build_sf32-oed-epd_hcpu\uart_download.bat`，按提示选择端口即可进行下载：
```
build_sf32l-oed-epd_v11_hcpu\uart_download.bat
Uart Download
please input the serial port num: 5  (填写相应的端口号)
```
这里只做简单讲解，详细请查看[编译烧录链接](https://docs.sifli.com/projects/sdk/latest/sf32lb52x/quickstart/build.html)
#### menuconfig配置
首先查看屏幕的型号，确定menuconfig配置相同型号（默认是`6.0 rect electronic paper display(EPD YZC052_V1.05 1032x758)`），然后再进行编译烧录，查看方式如下图
切换到例程project目录，运行menuconfig命令执行编译：
```
menuconfig --board=sf32-oed-epd_v11 --board_search_path=..
```
![alt text](src/assets/menuconfig.png)

## 阅读指南
### 按键指南
1. 主界面阅读EPUB文件选择
  上下移动选择：按K3键上移光标，按K1键下移光标，浏览EPUB电子书目录列表。
2. 进入/退出电子书的阅读界面
  进入书籍：光标选中目标电子书后，按下K2键即可打开。
  退出返回：阅读中按下K2键，直接退回主界面。
3. 内容翻阅与设置
  翻页操作：进入电子书后，按K1键向下翻页，按K3键向上翻页。
4. 操作逻辑总结
  K1键：用于基础功能向上移动、翻页；
  K2键：层级递进操作确认键；以及返回主界面；
  K3键：用于基础功能向上移动、翻页。

### 触控指南
* 在屏幕左上角有三个按钮可以实现触摸控制，本质的功能和按键实现功能相同。
1. 向上的箭头`▲`: 向上移动或者向上翻页。
2. 向下的箭头`▼`: 向下移动或者向下翻页。
3. 圆圈`●`: 确认或者返回键。


为了适配低功耗，现版本支持动态开关触控功能：主界面屏幕下方有触控开关选项，将光标移动到下方开关区域后可通过选择操作来进行对触控功能的开启与关闭，需要注意的是，触控默认是关闭的，并且每过一段时间用户无操作进入屏保状态下后，触控也会自动关闭。

### 状态管理指南
* 系统在5分钟无操作后自动进入屏保界面，此时触控功能关闭，可通过按下任意按键返回阅读
* 系统在5小时无操作后自动关机，此时可通过长按k3键进行开机
* 系统检测到低电量时自动进入低电量状态，此时按键与触控均无响应。需插入充电器并且当电量高于阈值时自动进入屏保状态，此时可通过按键进行阅读

## 软件框架
主要的程序框架讲解，具体内容可阅读[开源EPUB阅读器](https://github.com/atomic14/diy-esp32-epub-reader)
```
epdiy-epub
│  
├──disk                             # 内置flash，存放少量EPUB电子书文件
├──lib                              # 存放项目依赖的第三方库或自定义基础库
│   ├──epdiy                        # EPD驱动核心库
│   ├──Epub                         # EPUB 解析专用库，负责解析 EPUB 格式文件（解压、读取目录、解析内容结构、提取文本 / 图片等），是阅读器功能基础。 
│   │   ├──EpubList                 # EPUB 文件管理模块 
│   │   ├──Renderer                 # 渲染器模块，将 EPUB 解析后的内容（文本、图片）转换为电子书可显示的格式
│   │   ├──RubbishHtmlParser        # HTML 解析模块，以 XHTML 格式存储，此模块负责解析 HTML 结构，提取文本、图片、样式
│   │   ├──ZipFile                  # 补充解压逻辑（配合 miniz），处理 EPUB 压缩包内的 HTML 及资源文件
│   ├──Fonts                        # 存放字体数据文件，储字符的像素映射
│   ├──Images                       # 图片处理模块，负责加载、解码
│   ├──miniz-2.2.0                  # 轻量级 ZIP 解压库，处理 EPUB 压缩包
│   ├──png                          # PNG 图像解码库，为 PNGHelper 提供底层解码能力。
│   └──tjpgd3                       # JPEG 图像解码库，为 JPEGHelper 提供底层解码能力。
|
├──project                          # 编译脚本为项目编译、调试、部署提供工具链支持
|
├──sf32-oed-epd_v11                 # SF32-OED-6'-EPD_V1.1`和`SF32-OED-6'-EPD_V1.2`开发板公用的相关配置文件
├──sf32-oed-epd_v12_spi             # `SF32-OED-6'-EPD_V1.2`开发板搭配SPI墨水屏的相关配置文件
|
├──src                              # 项目核心源码目录，实现阅读器业务逻辑
    ├──boards                       # 硬件板级<br>
    │    ├──battery                 # 电池管理模块，包含电量检测
    │    ├──controls                # 输入控制模块（如按键、触摸），处理用户交互（翻页、选菜单等）
    │    └──display                 # 实现屏幕显示的最终输出，且可能针对不同屏幕型号做适配
    ├──epub_mem.c                   # 内存管理模块，为 EPUB 解析、渲染分配 / 释放内存，适配嵌入式设备内存限制。
    │       
    └──main.cpp                     # 程序入口（main 函数），初始化硬件、加载库、启动阅读器主逻辑（如打开 EPUB 文件、进入阅读界面 ）

```
## 开发指南
### 添加EPUB电子书
* 添加少量EPUB电子书文件，只需要在`epdiy-epub\disk`目录下进行添加相应文件，注意这里存放的空间很小，
* 增加大量EPUB电子书文件，主要通过读取TF卡内容，实现大量文件读取功能

### 更换屏幕
* 已有的三个屏幕型号分别是`6.0 rect electronic paper display(EPD R7D005_-1.30 1448x1072)`、`6.0 rect electronic paper display(EPD YZC052_V1.05 1032x758)`、`6.7 rect electronic paper display(TiamMa TE067XJHE01 V1.0  1920x960)`
可以通过menuconfig配置选择不同的屏幕<br>
#### 添加新的屏幕屏驱  
1、复制`src\board\display\epd_configs_yzc085.c`以新的屏幕名称命名这个文件<br>
2、根据屏幕波形文档，将波形数据转成一个数组例如全刷波形数组`static const uint8_t yzc085_wave_forms_full[32][256] = {}`和局刷波形数组`static const uint8_t yzc085_wave_forms_partial[12][256] = {}`<br>
3、对应屏驱文档，修改下列函数
* 1、如果波形多个温度，可通过波形表生成不同温度的多个二维波形数组，可参考如下代码,通过温度控制不同刷新不同的数据
```c
// 定义波形表条目结构体
typedef struct {
    int min_temp;           
    int max_temp;          
    uint32_t frame_count; 
    const uint8_t (*wave_table)[256];  
} WaveTableEntry;
// 原始波形数据
static const uint8_t te067xjhe_wave_full_0_5[45][256] = {};
static const uint8_t te067xjhe_wave_full_5_10[45][256] = {};
//...
static const uint8_t te067xjhe_wave_full_50_100[45][256] = {};
// 全刷波形表 - 按温度区间组织
static const WaveTableEntry te067xjhe_wave_forms_full[] = {
    {0,   5,  45, &te067xjhe_wave_full_0_5[0]},
    {5,  10,  45, &te067xjhe_wave_full_5_10[0]},
    //...
    {50, 100, 45, &te067xjhe_wave_full_50_plus[0]},  // The range above 50 degrees
};
static const uint8_t *p_current_wave_from = NULL;
uint32_t epd_wave_table_get_frames(int temperature, EpdDrawMode mode)
{
    const WaveTableEntry *wave_table;
    size_t table_size;
    
    wave_table = te067xjhe_wave_forms_full;
    table_size = sizeof(te067xjhe_wave_forms_full) / sizeof(WaveTableEntry);

    // Find the interval corresponding to the temperature
    for (size_t i = 0; i < table_size; i++) {
        if (temperature >= wave_table[i].min_temp && temperature < wave_table[i].max_temp) {
            p_current_wave_from = (const uint8_t *)(*wave_table[i].wave_table);
            return wave_table[i].frame_count;
        }
    }

    p_current_wave_from = (const uint8_t *)(*wave_table[0].wave_table);
    return wave_table[0].frame_count;
}

```
* 2、将8位波形转换为32位的扩展线性插值（Epic）查找表值，提供给硬件进行波形数据选择，以达到显示效果。
```c
void epd_wave_table_fill_lut(uint32_t *p_epic_lut, uint32_t frame_num)
{
    //从当前选中的波形表（p_current_wave_from）中，取出第 frame_num 帧的波形数据的起始地址，赋值给 p_frame_wave。
    const uint8_t *p_frame_wave = p_current_wave_from + (frame_num * 256);//每一帧的波形数据有 256 个字节（[256]）。frame_num * 256 就是第 frame_num 帧的起始偏移

    //Convert the 8-bit waveforms to 32-bit epic LUT values
    for (uint16_t i = 0; i < 256; i++)
        p_epic_lut[i] = p_frame_wave[i] << 3;
}
```
* 3、根据屏驱文档对照下列参数，调整时序及频率。对照文档获取频率SDCLK->sclk_freq,frame clock->fclk_freq,提供驱动芯片的工作时钟频率参数，根据不同的驱动配置不同的频率。
```c
const EPD_TimingConfig *epd_get_timing_config(void)
{
    static const EPD_TimingConfig timing_config = {
        .sclk_freq = 24,  //像素时钟（列时钟） 单位 MHz
        .SDMODE = 0, //SD模式选择
        .LSL = 0,    // 行开始信号需要的clock数
        .LBL = 0,    // 行起始的空clock数
        .LDL = LCD_HOR_RES_MAX/4,  //有效数据的clock数，因为是2bit，8数据线所以需要除以4
        .LEL = 1,   //行结束的空clock数

        .fclk_freq = 83, //行时钟，单位 KHz
        .FSL = 1,  //起始行信号需要的clock数量
        .FBL = 3,  //起始空行的数量
        .FDL = LCD_VER_RES_MAX,  //有效数据的行数
        .FEL = 5,   //结束空行的数量
    };

    return &timing_config;
}
```

* 4、提供电子书显示所需的参考电压，确保显示效果稳定。新屏幕的VCOM电压可能不同，需要查阅新屏幕规格书，修改返回值。
```c
uint16_t epd_get_vcom_voltage(void)
{
    return 2100;
}
```

4、在Kconfig文件中增加驱动IC的宏定义以及为新的屏幕模组添加menuconfig选项，
打开`epd_reader\epdiy-epub\project\Kconfig.proj`文件，添加新的配置可以参考[添加IC宏和menuconfig选项](https://wiki.sifli.com/tools/%E5%B1%8F%E5%B9%95%E8%B0%83%E8%AF%95/%E6%B7%BB%E5%8A%A0%E5%B1%8F%E5%B9%95%E6%A8%A1%E7%BB%84%EF%BC%883%20%E5%A4%96%E7%BD%AE%EF%BC%89.html)具体内容可参考当前驱动内容进行修改。  

### 修改字体流程
先生成指定的unicode码范围数组，然后用这个数组去指定的ttf里面生成字库
注意：生成的字体的unicdoe编码需要从小到大排列，否则查找会有问题。

#### 1. 生成unicode码范围[可选]
当前`fontconvert.py`里面的数组`intervals`存储的是包括常用的英文字符以及GB2312一级字库的unicode码。

##### 获取指定ttf字体里面可用的全部unicode码范围
1. 获取ttf字体unicode码范围：`python3 get_intervals_from_font.py abc.ttf > interval.h`
2. 将生成的`interval.h` 覆盖 `generate_fonts.sh`里面的数组 `intervals`

##### 指定GB2312一级字库的unicode码范围
1. `python3 generate_gb2312_L1_intervals.py` 将生成GB2312 一级汉字的unicode码范围
2. 将生成的数组修改`generate_fonts.sh`里面的数组 `intervals`

#### 2.根据unicode码范围生成字体
从abc.ttf生成15号字体,名字为`regular_font`，覆盖../lib/Fonts/regular_font.h, 命令如下
`python3 fontconvert.py regular_font 15 abc.ttf  > ../lib/Fonts/regular_font.h`
