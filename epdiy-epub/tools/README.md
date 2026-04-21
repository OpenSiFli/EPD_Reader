# Waveform XLSX 转 C 数组工具说明

本目录提供一个将墨水屏原厂波形表（xlsx）转换为 C 数组的工具脚本：

- 脚本: [waveform_xlsx_to_c.py](waveform_xlsx_to_c.py)
- 输入示例: [TM to SHINEVIEW-20250616.xlsx](TM%20to%20SHINEVIEW-20250616.xlsx)
- 输出示例: [waveform_generated.c](waveform_generated.c)

## 1. 作用

该脚本把 xlsx 中每个温区的波形数据转换为如下 C 结构：

1. 多个 `static const uint8_t xxx[frame_count][256]` 数组
2. 一个 `WaveTableEntry` 温区映射表

典型用于生成类似 [te067xjhe01.c](../../te067xjhe01.c) 里的多温区波形数组。

## 2. 依赖环境

1. Python 3（推荐使用 `py -3`）
2. openpyxl

安装依赖：

```powershell
py -3 -m pip install openpyxl
```

## 3. xlsx 期望格式

脚本按以下列读取数据：

1. B 列: Old gray level（0..15）
2. C 列: New gray level（0..15）
3. D 列: Frame（帧数，默认按参数 `--frame-count`）
4. E 及后续列: 每一帧波形值（默认支持 -15 / 0 / 15）

每个温区固定读取 256 行（16x16 的 old/new 组合）。

## 4. 快速使用

在仓库根目录执行：

```powershell
py -3 epdiy-epub\tools\waveform_xlsx_to_c.py "epdiy-epub\tools\TM to SHINEVIEW-20250616.xlsx" -o "epdiy-epub\tools\waveform_generated.c"
```

成功时会输出：

```text
Generated: epdiy-epub\tools\waveform_generated.c
```

## 5. 常用参数

```text
positional:
  xlsx                 输入 xlsx 文件路径

required:
  -o, --output         输出文件路径（必须提供）

optional:
  --sheet              工作表名，不填默认第一张
  --prefix             生成数组名前缀，默认 te067xjhe_wave_full
  --value-map          数值映射，默认 -15:2,0:0,15:1
  --start-row          第一个温区起始行，默认 2
  --frame-count        每温区帧数，默认 45
```

示例（指定 sheet 和前缀）：

```powershell
py -3 epdiy-epub\tools\waveform_xlsx_to_c.py "epdiy-epub\tools\TM to SHINEVIEW-20250616.xlsx" -o "epdiy-epub\tools\my_wave.c" --sheet GC16 --prefix my_panel_wave
```

## 6. 转换规则

1. 索引规则: `index = old_gray * 16 + new_gray`
2. 每帧输出长度固定 256
3. 默认值映射:
- `-15 -> 2`
- `0 -> 0`
- `15 -> 1`

## 7. 温区规则

脚本内置 11 个温区（可在脚本中修改 `DEFAULT_TEMP_RANGES`）：

1. 0-5
2. 5-10
3. 10-15
4. 15-20
5. 20-25
6. 25-30
7. 30-35
8. 35-40
9. 40-45
10. 45-50
11. 50-100（输出名后缀 `50_plus`）

## 8. 常见问题

1. 报错 `error: the following arguments are required: -o/--output`
- 原因: 命令只写了 `-o`，后面没给输出文件名。
- 解决: 补完整路径，例如 `-o epdiy-epub\tools\waveform_generated.c`。

2. 报错 `ModuleNotFoundError: No module named 'openpyxl'`
- 原因: 未安装依赖。
- 解决: 执行 `py -3 -m pip install openpyxl`。

3. 报错 `Unexpected raw waveform value ...`
- 原因: xlsx 里出现了映射表之外的值。
- 解决: 通过 `--value-map` 增加映射，或先清洗 xlsx 数据。

## 9. 建议流程

1. 先用脚本生成新文件，例如 [waveform_generated.c](waveform_generated.c)
2. 与目标驱动文件做 diff 校验
3. 再拷贝到正式驱动源文件中

这样可以减少误覆盖风险。
