# SiFli 固件烧录工具 1.2.0

Windows 10/11 x64 图形工具，同时支持 sftool 串口烧录和 SF32LB527 阅读器的原生 USB DFU。已内置修正后的 sftool 0.2.5，无需安装 Python，也不需要手动替换后端。

1.1.1 修复 SF32LB52 整片擦除完成后重复等待 `OK` 导致误报超时，以及默认 64KiB 下载分包超出 u16 帧长上限的问题（改为 32KiB）。修正版后端已在 COM36 新板验证整片擦除、4KiB 写入及独立读回、重新加载驱动；测试后 16MiB 全片读回与原始备份完全一致。内置后端来源、补丁及 EXE 哈希见 `backend-build.json`。原生 USB 仍复用独立的受保护恢复协议。

1.2.0 新增界面上的“擦除 Flash…”入口，支持整片 NOR 擦除和指定范围擦除。

## 擦除 Flash

1. 选择“串口烧录”，选好目标串口、芯片与 NOR 存储类型，无需加载固件文件。
2. 点击“开始烧录”旁的“擦除 Flash…”，选择“指定范围”或“整片擦除”。
3. 范围擦除填写十六进制起始地址和长度，均须按 4KB 对齐。例如 `0x12FFF000` / `0x1000` 表示 527 的 16MB 布局最后 4KB。整片擦除填写 Flash 映射起址（527 为 `0x12000000`），不要填写应用地址 `0x12200000`；容量由下载程序识别。
4. 点击“执行擦除”，再次核对确认窗口中的串口、芯片和范围后执行，进度与日志显示在主窗口。

整片擦除会删除引导、USB 恢复程序和应用，执行前请保存需要的数据。擦除完成以 sftool 的设备应答和退出码为准，不额外执行全范围读回验空。“停止等待”只能停止上位机进程，不能撤销已发出的擦除命令，设备可能仍在擦除。该入口目前仅支持 NOR；原生 USB 模式禁用擦除，保持原来的受保护升级协议。

## 原生 USB 升级（组装后的阅读器）

1. 在阅读器设置中进入 USB 升级，或按住 PA33 按键后复位、上电，继续保持至少 2 秒再松开。用数据线连接板子的原生 USB 接口。
2. 运行 `SiFliFlashTool.exe`，选择顶部“原生 USB DFU”。工具每 1.5 秒自动识别恢复设备，只有一个时自动选中，无需调试串口。
3. 加载完整固件 ZIP、固件目录、`sftool_param.json`、`ImgBurnList.ini` 或 `update.json`。完整首刷包会自动筛出应用、字体和波形三个镜像，不写入引导和恢复程序。
4. 点击“开始 USB 升级”。依次写入三个镜像，设备端读回校验全部通过后自动请求重启阅读器。

“读取设备”仅查询恢复程序信息；“重启退出升级”会由设备先检查现有固件，再请求返回阅读器。也可先松开 PA33，再长按电源键（PA34）约 10 秒硬件重启；PA34 不用于选择升级模式。若升级中断导致固件不完整，设备会保留恢复模式，需重新完整升级后才能返回阅读器。

USB 模式只识别 VID `38F4` / PID `0001` / 序列号 `EPD527-RECOVERY-1` 的现有 SF32LB527 阅读器恢复固件。它使用项目自己的 USB CDC 恢复协议，不依赖 sftool 或 USB 转串口芯片，也不是通用 USB DFU Class 工具。应用尚未进入恢复模式时，上位机不能通过 USB 命令强制它进入；请使用设置菜单或 PA33 入口。

USB 模式也支持手动选择文件和地址，但必须提供当前布局的完整三个镜像：应用 `0x12200000`、字体 `0x12400000`、波形 `0x12EC0000`。工具先检查分区长度、应用向量；含升级元数据时检查板型、布局、SHA-256 和 CRC。USB 地址不可用于写其他分区，设备端协议同样不接受任意地址。

USB 进度 0–90% 为按总字节数统计的写入进度，随后执行设备端校验与重启。拔线、超时、停止或校验失败均不会自动启动不完整固件；重新进入恢复模式后从头升级，不支持断点续传。

## 串口烧录（空片首刷 / 工厂维护）

1. 运行 `SiFliFlashTool.exe`，选择“串口烧录”和板子的串口下载端口。
2. 点击“加载 ZIP / JSON / INI”选择完整固件 ZIP、`sftool_param.json` 或 `ImgBurnList.ini`。也可以选择固件目录自动识别清单。
3. 手动烧录时点击“添加文件”，选择文件并输入十六进制地址。地址可带 `0x` 前缀；双击条目可修改，支持多个文件及调整写入顺序。
4. 核对芯片、存储类型和文件列表，点击“开始烧录”。默认启用写后校验；导入 JSON 时沿用清单的校验设置。

ZIP 自动解压并保留目录结构，优先使用 JSON，同时存在 INI 时检查二者一致性。完整首刷包沿用清单顺序，bootloader 最后写入。支持路径中的中文及空格。

串口模式使用 **sftool 串口下载协议**，用于空片首刷及串口重烧，可写完整引导和恢复分区。原生 USB 恢复端口应使用 USB DFU 模式。SF32LB527 板此前调试串口为 COM34，实际端口以当前电脑为准。

默认波特率 1000000，烧录前 `default_reset`，烧录后 `soft_reset`。握手或载入程序超时时，可降低至 115200，并按硬件 RESET 后重试。高级下拉项可以选择不复位。

串口界面进度为 sftool 当前阶段进度，多文件烧录/校验时可能重新计数；只有进程成功退出才显示烧录完成。停止会中断当前写入，需要重新完整烧录。

运行日志自动写入 `%LOCALAPPDATA%\SiFliFlashTool\logs`，也可点击保存日志。导入时检查文件存在、非空、地址范围、重叠；包内含 `images.json` 时同时检查匹配项的长度、地址和 SHA-256。

## 从源码运行和构建

需要 Python 3.11+、`pyserial`、PyInstaller，Tkinter 使用 Python Windows 安装包自带组件。

```powershell
python -m pip install pyserial pyinstaller
python tools/flash_gui/flash_gui.py
python -m unittest discover -s tools/flash_gui -p 'test_*.py' -v
# 首次构建先生成固定版本的修正版后端，需要 Git、Rust/Cargo、Windows C++ 编译工具
./tools/flash_gui/build_backend.ps1
# 也可从本机已有源码克隆固定提交，不改动原工作区：
# ./tools/flash_gui/build_backend.ps1 -SourceDirectory E:/agent/sftool
./tools/flash_gui/build.ps1 -OutputDirectory C:/output/SiFliFlashTool
```

`build_backend.ps1` 在独立临时目录检出 `backend/source.json` 固定的上游提交，应用随附补丁，运行擦除应答回归并编译；生成 `backend/sftool.exe` 与 `backend/backend-build.json`。默认使用已验证的 debug 开发构建，可指定 `-Profile release` 优化构建。

`build.ps1` 只打包与构建清单、补丁哈希一致的后端，不再默认使用用户目录或 PATH 中的旧工具。自定义后端路径时须同时指定 `-Sftool` 和对应的 `-BackendManifest`。打包时统一为内置 `sftool.exe`，运行后优先选择该内置文件；仍可在界面手动选择外部工具。上游项目为 https://github.com/OpenSiFli/sftool ，许可证见随附 `LICENSE-sftool`。

USB 帧、CRC、序号和板型校验直接复用 `tools/usb_dfu_527.py`，GUI 后端增加可中止读取和总进度。发布包 `source/` 包含这份共享模块；单独复制源码运行时请保持它与 `usb_core.py` 同目录，或保留仓库的目录结构。
