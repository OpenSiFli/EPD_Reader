# SF32LB527 空白 Flash 串口首刷

此包用于 SF32LB527 + E0470A03-AF-S 阅读器的空白 Flash 首次安装，也可用于有意重装整套启动链。首刷流程会写入 Flash 表、首级 bootloader、USB 恢复程序、应用、内置字体、屏幕波形和初始升级状态。日常升级请使用 USB 恢复流程，不要运行首刷脚本。

也可使用 [Windows 图形升级工具](tools/flash_gui/README.md)：空片首刷选择“串口烧录”，加载此包的 ZIP、`sftool_param.json` 或 `ImgBurnList.ini`，写入全部七个分区。组装后的日常更新选择“原生 USB DFU”；该模式可以读取同一完整包，但只选取应用、字体和波形，不覆盖引导、恢复程序或初始状态镜像。

## 需要准备

- Windows 电脑及 SiFli `sftool`，运行 `sftool --version` 应能显示版本。
- 连接 SF32LB527 下载 UART 的串口工装，并确认其 COM 号。逻辑电平应符合板卡下载接口规格；接线时共地，UART TX/RX 交叉。
- 稳定的板卡供电，以及按板卡下载电路进入 ROM 串口下载状态的方式。

本包的串口写入使用 SoC 下载 UART。它不是应用启动后枚举出来的原生 USB CDC 恢复端口，也不是 `usb_dfu_527.py` 的日常升级流程。工作台串口可能显示为 `COM36`，实际操作请以 Windows 设备管理器里的下载 UART 为准。若下载口没有进入 ROM 下载状态，先按板卡工装的 BOOT/RESET 流程操作，再运行烧录脚本。

## 首次烧录

1. 解压整个固件包到一个普通目录。不要只拷贝某一个 BIN 文件；旁边的 manifest、脚本和其余镜像都要保留。
2. 将下载 UART 接到电脑，给板卡供电，并让芯片进入 ROM 串口下载状态。
3. 打开命令提示符，进入解压目录，运行下面命令（将 `COM36` 换成实际下载 UART 端口）：

   ```bat
   flash_factory.bat COM36
   ```

   也可以双击 `flash_factory.bat`，在提示时输入 `COM36`。脚本调用 SiFli `sftool`，目标芯片参数为 SDK 所用的 `SF32LB52`，NOR 模式开启写后校验。

4. 等待工具报告写入和校验完成。工具随后复位设备。首次启动期间请保持供电。

命令行等价写法如下：

```bat
sftool -p COM36 config sftool_param.json
```

也可以运行 `flash_factory.bat COM36`；它直接传入同一组镜像和地址。两种方式都启用写后校验。启动程序最后写入。不要添加 `--erase-all`，不要更改地址或打乱镜像；清单只列出应写入的目标区域。

## 包内镜像

| 镜像 | NOR 地址 | 用途 |
|---|---:|---|
| `ftab.bin` | `0x12000000` | Flash 分区表 |
| `bootloader.bin` | `0x12010000` | 首级启动程序 |
| `recovery.bin` | `0x12020000` | 受保护的 USB CDC 恢复更新程序 |
| `ER_IROM1.bin` | `0x12200000` | SF32LB527 应用程序 |
| `ER_IROM2.bin` | `0x12400000` | 内置字体资源 |
| `epd_waveform.bin` | `0x12EC0000` | E0470A03 屏幕波形 |
| `update_state.bin` | `0x12F00000` | 初始升级状态记录 |

`sftool_param.json` 是 SDK `sftool config` 可直接读取的 JSON 清单。`ImgBurnList.ini` 使用 SDK 串口下载工具的 `[FILEINFO]` 格式，列出相同的七个 BIN、地址和数量；将整个包目录作为工具工作目录，使相对文件名能找到同目录镜像。`images.json` 含各 BIN 的实际长度、地址、CRC32 和 SHA-256。

## 故障处理

- `sftool` 找不到：安装 SiFli SDK 工具并把 `sftool` 加入 PATH，重新打开命令行后再运行。
- 串口打不开：关闭占用该 COM 口的串口终端，核对端口号和下载 UART 接线。
- 无法同步芯片：确认板卡供电、共地、TX/RX、ROM 下载模式及 BOOT/RESET 操作。
- 写入或校验中断：保持下载工装连接，重新进入 ROM 下载模式，再完整运行首刷脚本；不要只补刷其中一个镜像。

首刷完成后，后续版本通过阅读器的 USB 升级菜单和原生 USB 接口更新。受保护恢复程序不能覆盖自身、bootloader 或 Flash 表。
