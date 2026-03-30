# EPUB 电子书阅读器（SF32-OED-EPD）

## 项目简介

本项目基于 [atomic14/diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader) 进行适配，运行于 SiFli SF32-OED-EPD 系列开发板，用于 EPUB 电子书阅读与低功耗场景。

## 当前功能

### 电子书与文件系统

- 支持从内置文件系统与 TF 卡读取 EPUB（优先使用 TF 卡）。
- `disk/` 目录可打包示例 EPUB 到镜像中。

### 输入与显示

- 支持按键与触控双输入。
- 支持中文/英文显示。
- 为控制资源占用，默认未启用粗体/斜体中文字库。

### UI 页面与状态机

当前 UI 状态：

- `MAIN_PAGE`：主页面
- `SELECTING_EPUB`：书库页面
- `SELECTING_TABLE_CONTENTS`：目录页面
- `READING_EPUB`：阅读页面
- `READING_SETTINGS`：阅读设置页面（字体/字号/字重/行距/边距）
- `SETTINGS_PAGE`：功能设置页面
- `WELCOME_PAGE`：欢迎页面（超时熄屏）
- `LOW_POWER_PAGE`：低电量页面
- `CHARGING_PAGE`：充电页面
- `SHUTDOWN_PAGE`：关机页面

默认开机进入 `MAIN_PAGE`。

---

### 主页面

主页面包含 3 个入口：

1. 打开书库
2. 继续阅读
3. 进入设置

说明：

- "继续阅读"基于本次运行期间最近一次成功进入阅读的书籍索引，并自动恢复到上次所在章节。
- 若当前无可用记录，则显示"无阅读记录"。

---

### 功能设置页面

支持以下设置项：

1. 触控开关（同步触控硬件上电/下电）
2. 超时策略：`5分钟 / 10分钟 / 30分钟 / 1小时 / 不关机`
3. 全刷周期：`5次 / 10次 / 20次 / 每次(0)`
4. 阅读设置：进入阅读设置子页面，见下节
5. 确认保存并返回主页面

---

### 阅读设置页面

通过"功能设置 → 阅读设置"或"阅读页覆盖操作层 → 设置"进入，支持调节以下参数：

| 设置项 | 可选值 |
|--------|--------|
| 字体   | Default（内置）及 TF 卡 `/fonts` 目录中所有 `.ttf` / `.otf` 文件 |
| 字号   | 24 / 28 / 32 / 36 / 40 / 44 / 48 px |
| 字重   | 正常 / 中粗 / 粗体|
| 行距   | 1.0x / 1.2x / 1.4x / 1.6x / 1.8x / 2.0x |
| 边距   | 5 / 8 / 11 / 14 / 17 / 20 px |
| 保存退出 | 确认后应用并持久化到 TF 卡 `/settings.cfg` |

操作方式：`UP` / `DOWN` 切换设置项；`SELECT` 循环切换当前项的值；光标停在"保存退出"时 `SELECT` 保存并退出。

**持久化**：设置以 `key=value` 文本格式保存于 TF 卡 `/settings.cfg`，下次开机自动加载；若指定字体文件不可用，自动回退到内置默认字体。

---

### 动态字体加载

将 `.ttf` 或 `.otf` 字体文件放入 TF 卡的 `/fonts` 目录，开机后将自动被检测并出现在阅读设置的字体列表中（最多支持 64 个外部字体）。

- `Default`：编译时内置的 FreeType 字体（DroidSansFallback，存放于 Flash XIP）。
- 外部字体：FreeType 直接从 TF 卡文件读取。

---

### 阅读页覆盖操作层

在阅读页触发 `UPGLIDE`（上滑/映射动作）可呼出覆盖操作层，支持：

- 中心功能在"触控开关"与"全刷周期"间切换
- 跳页步进：`-5 / -1 / +1 / +5`
- 确认跳转到目标页
- 进入目录页
- 返回书库
- **进入阅读设置**（新增，可直接从阅读页调整字体/字号/行距/边距，退出后自动恢复到当前阅读位置）

---

### 书库页与目录页

阅读流程：**书库页 → 目录页 → 阅读页**。在书库页选中书籍后，将先进入目录页，再由目录页选择章节跳入阅读。

- 书库页：每页 4 项，底部支持"上一页 / 主页面 / 下一页"。
- 目录页：每页 6 项，底部支持"上一页 / 书库 / 下一页"。
- 触控采用"先选中再确认"机制以降低误触。

---

### 电量与低功耗

- 页面顶部显示电量与充电状态（闪电图标）；电量满时（≥98%）自动清除充电图标。
- 低电量进入 `LOW_POWER_PAGE` 并抑制普通用户操作。
- 充电状态变化可触发页面刷新（仅在百分比或充电状态真正变化时刷屏，避免无效刷新）。
- 用户无操作达到设置超时后进入 `WELCOME_PAGE`（类似熄屏）。
- 主循环默认 5 小时无交互进入 `SHUTDOWN_PAGE`（常量 `TIMEOUT_SHUTDOWN_TIME`）。

---

## 使用指南

### 硬件连接

- 将开发板与墨水屏通过对应连接器连接，注意排线方向。
- `SF32-OED-EPD_V1.1` 与 `SF32-OED-EPD_V1.2` 可共用 `sf32-oed-epd_base` 软件板级配置。

### 编译与烧录

进入 `epdiy-epub/project` 后执行：

```bash
scons --board=sf32-oed-epd_v11 --board_search_path=.. -j8
```

编译完成后下载：

```bat
build_sf32-oed-epd_v11_hcpu\uart_download.bat
```

按脚本提示输入串口号。

参考：[SiFli 官方编译/烧录文档](https://docs.sifli.com/projects/sdk/latest/sf32lb52x/quickstart/build.html)

### menuconfig

```bash
scons --board=sf32-oed-epd_v11 --board_search_path=.. --menuconfig
```

---

## 操作说明

### 按键动作语义

- `UP`：上移 / 上一项 / 向前翻页
- `DOWN`：下移 / 下一项 / 向后翻页
- `SELECT`：确认 / 进入 / 循环切换值
- `UPGLIDE`：阅读页呼出覆盖操作层

### 触控操作

- 主页面：点击左右区域切换选项，点击中间区域确认。
- 书库页：点击书籍项选中，再次点击确认，进入目录页。
- 目录页：点击目录项选中，再次点击确认跳转阅读。
- 阅读页：左右区域翻页；上滑呼出覆盖层；覆盖层内执行跳页/目录/返回书库/进入阅读设置。
- 阅读设置页：点击设置项选中并切换值，点击"保存退出"保存。

---

## 目录结构

```
│
├── disk/                           # 内置 Flash，存放少量 EPUB 电子书文件
├── lib/                            # 第三方库与自定义基础库
│   ├── epdiy/                      # EPD 驱动核心库
│   ├── Epub/                       # EPUB 解析专用库
│   │   ├── EpubList/               # EPUB 文件管理模块（含 tinyxml2）
│   │   ├── Renderer/               # 渲染器模块（帧缓冲、字体、图片合成）
│   │   ├── RubbishHtmlParser/      # XHTML 解析模块（TextBlock / ImageBlock）
│   │   └── ZipFile/                # 包内文件寻址（配合 miniz）
│   ├── Images/                     # 内置图片数据（欢迎/低电量/充电/关机页）
│   ├── miniz-2.2.0/                # 轻量级 ZIP 解压库
│   ├── png/PNGdec/                 # PNG 解码库
│   └── tjpgd3/                     # JPEG 解码库
│
├── project/                        # 编译脚本（SCons + Kconfig）
│
├── sf32-oed-epd_base/              # V1.1 与 V1.2 公用板级配置
├── sf32-oed-epd_v11/               # V1.1 板级配置
├── sf32-oed-epd_v12/               # V1.2 板级配置
├── sf32-oed-epd_v12_spi/           # V1.2 + SPI 墨水屏配置
│
├── scripts/                        # 工具脚本（字体生成、图片转换）
│
├── src/                            # 核心业务逻辑
│   ├── boards/                     # 硬件抽象层
│   │   ├── battery/                # 电池管理（ADC 采样 + 电量表）
│   │   ├── controls/               # 输入控制（按键 / 触控 / 动作语义）
│   │   ├── display_dbi/            # DBI 8-bit 并行墨水屏驱动
│   │   ├── display_spi/            # SPI 墨水屏驱动
│   │   └── touch/                  # 触控芯片驱动（FT5446U / FT6336U / GT967）
│   ├── assets/                     # 内置位图资源（C 数组）
│   ├── epub_mem.c                  # 嵌入式内存管理
│   ├── epub_screen.cpp             # 各页面绘制与事件处理
│   ├── font_manager.c              # 字体管理器（动态加载 TF 卡字体）
│   ├── reading_settings.cpp        # 阅读设置页面（字体/字号/字重/行距/边距 + 持久化）
│   ├── UIRegionsManager.cpp        # 触控区域管理（动态注册与命中检测）
│   └── main.cpp                    # 程序入口（UI 状态机主循环）
│
├── tools/                          # 辅助工具（字体转换脚本）
└── waveform/                       # 墨水屏波形文件及解码库
```

---

## 二次开发

### 添加 EPUB

- 少量样书：放入 `disk/`，随文件系统镜像打包。
- 大量书籍：建议使用 TF 卡。

### 添加外部字体

将 `.ttf` 或 `.otf` 文件直接复制到 TF 卡的 `/fonts` 目录，重启后即可在"阅读设置 → 字体"中选用。

### 添加新的屏幕驱动

1. 复制已有屏驱配置文件并改名（建议以 `src/boards/display_dbi/epd_configs_opm060d.c` 为模板，按新屏型号命名）。
2. 根据屏幕波形文档，将波形数据转换为数组，例如：
   - 全刷波形：`static const uint8_t xxx_wave_forms_full[32][256] = {}`
   - 局刷波形：`static const uint8_t xxx_wave_forms_partial[12][256] = {}`
3. 按屏驱文档修改关键函数（波形选择、LUT 转换、时序频率、VCOM）。

#### 3.1 多温区波形选择（可选）

如果波形按温度分段，可组织为多组二维数组，通过温度选择对应波形：

```c
typedef struct {
    int min_temp;
    int max_temp;
    uint32_t frame_count;
    const uint8_t (*wave_table)[256];
} WaveTableEntry;

static const uint8_t te067xjhe_wave_full_0_5[45][256]    = {};
static const uint8_t te067xjhe_wave_full_5_10[45][256]   = {};
static const uint8_t te067xjhe_wave_full_50_100[45][256] = {};

static const WaveTableEntry te067xjhe_wave_forms_full[] = {
    {0,   5,  45, &te067xjhe_wave_full_0_5[0]},
    {5,  10,  45, &te067xjhe_wave_full_5_10[0]},
    {50, 100, 45, &te067xjhe_wave_full_50_100[0]},
};

static const uint8_t *p_current_wave_from = NULL;

uint32_t epd_wave_table_get_frames(int temperature, EpdDrawMode mode)
{
    const WaveTableEntry *wave_table = te067xjhe_wave_forms_full;
    size_t table_size = sizeof(te067xjhe_wave_forms_full) / sizeof(WaveTableEntry);

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

#### 3.2 波形转换为 32 位 Epic LUT

```c
void epd_wave_table_fill_lut(uint32_t *p_epic_lut, uint32_t frame_num)
{
    const uint8_t *p_frame_wave = p_current_wave_from + (frame_num * 256);
    for (uint16_t i = 0; i < 256; i++)
        p_epic_lut[i] = p_frame_wave[i] << 3;
}
```

#### 3.3 调整时序与频率参数

```c
const EPD_TimingConfig *epd_get_timing_config(void)
{
    static const EPD_TimingConfig timing_config = {
        .sclk_freq = 24,              // 像素时钟，单位 MHz
        .SDMODE = 0,
        .LSL = 0,
        .LBL = 0,
        .LDL = LCD_HOR_RES_MAX / 4,  // 有效数据 clock 数（2bit+8数据线，需除以4）
        .LEL = 1,
        .fclk_freq = 83,              // 行时钟，单位 KHz
        .FSL = 1,
        .FBL = 3,
        .FDL = LCD_VER_RES_MAX,
        .FEL = 5,
    };
    return &timing_config;
}
```

#### 3.4 设置 VCOM 电压

```c
uint16_t epd_get_vcom_voltage(void)
{
    return 2100;
}
```

4. 在 `project/Kconfig.proj` 中新增屏幕宏与 `menuconfig` 选项，并将新配置文件加入编译条件。

#### 板子必须补充的 `Kconfig.proj` 配置

以 `sf32-oed-epd_v12` 为例，新增屏驱后按以下顺序修改：

1. 在 `choice "Custom LCD driver"` 下新增屏幕选项（建议命名带 `V12`）。
2. 在该选项中 `select` 对应触控驱动、EPD 类型宏和总线宏（一般为 `BSP_LCDC_USING_EPD_8BIT`）。
3. 在 `LCD_HOR_RES_MAX` / `LCD_VER_RES_MAX` / `LCD_DPI` 中补上新屏默认值。
4. 若希望 v12 默认选中新屏，在 `choice` 的 `default ... if BSP_USING_BOARD_SF32_OED_EPD_V12` 中切换为新宏。

```kconfig
choice
    prompt "Custom LCD driver"
    default LCD_USING_EPD_YOUR_PANEL_V12 if BSP_USING_BOARD_SF32_OED_EPD_V12

    config LCD_USING_EPD_YOUR_PANEL_V12
        bool "6.x rect electronic paper display(YourPanel 1234x567) for V1.2 board"
        select TSC_USING_GT967 if BSP_USING_TOUCHD
        select LCD_USING_OPM060D
        select BSP_LCDC_USING_EPD_8BIT
endchoice

config LCD_HOR_RES_MAX
    int
    default 1234 if LCD_USING_EPD_YOUR_PANEL_V12

config LCD_VER_RES_MAX
    int
    default 567 if LCD_USING_EPD_YOUR_PANEL_V12

config LCD_DPI
    int
    default 300 if LCD_USING_EPD_YOUR_PANEL_V12
```

完成后建议执行一次以确认新屏选项可见并选中：

```bash
menuconfig --board=sf32-oed-epd_v12 --board_search_path=..
```

参考：[SiFli 屏幕模块适配说明](https://wiki.sifli.com/tools/%E5%B1%8F%E5%B9%95%E8%B0%83%E8%AF%95/%E6%B7%BB%E5%8A%A0%E5%B1%8F%E5%B9%95%E6%A8%A1%E7%BB%84%EF%BC%883%20%E5%A4%96%E7%BD%AE%EF%BC%89.html)



## 致谢

- 上游项目：[atomic14/diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader)
- SiFli SDK 与开发文档支持
