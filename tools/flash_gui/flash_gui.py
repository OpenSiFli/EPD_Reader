"""SiFli serial Flash and protected SF32LB527 native USB recovery GUI."""
from __future__ import annotations

from datetime import datetime
import os
from pathlib import Path
import queue
import subprocess
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from serial.tools import list_ports
from flash_core import AFTER, BEFORE, CHIPS, MEMORIES, Image, address, command, find_sftool, load_package, run, validate
from usb_core import Cancelled, prepare_update, recovery_port, run_usb, select_targets

VERSION = '1.1.1'


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title(f'SiFli 固件烧录工具  {VERSION}')
        self.geometry('1180x850')
        self.minsize(1180, 740)
        self.configure(bg='#f1f5f9')
        self.events = queue.Queue()
        self.cancel = threading.Event()
        self.mode = 'idle'
        self.rows = {}
        self.packages = []
        self.log_handle = None
        self.log_path = None
        self.port_names = {}
        self.controls = []
        self.transport = tk.StringVar(value='serial')
        self.active_transport = 'serial'
        self.transport_images = {}
        self.usb_only = False
        self.footer_text = tk.StringVar()
        self.port = tk.StringVar()
        self.chip = tk.StringVar(value='SF32LB52')
        self.memory = tk.StringVar(value='nor')
        self.baud = tk.StringVar(value='1000000')
        self.before = tk.StringVar(value='default_reset')
        self.after_reset = tk.StringVar(value='soft_reset')
        self.verify = tk.BooleanVar(value=True)
        self.tool = tk.StringVar(value=str(find_sftool() or ''))
        self.origin = tk.StringVar(value='支持完整首刷包、JSON / INI 清单及手动文件地址。')
        self.status = tk.StringVar(value='就绪 · 请加载固件并选择下载串口')
        self.summary = tk.StringVar(value='0 个文件')
        self.percent = tk.StringVar(value='当前阶段：—')
        self._style()
        self._build()
        self.scan_ports()
        self.apply_transport()
        self.after(80, self.poll)
        self.after(1500, self.rescan)
        self.protocol('WM_DELETE_WINDOW', self.close)

    def _style(self):
        style = ttk.Style(self)
        style.theme_use('clam')
        style.configure('.', font=('Microsoft YaHei UI', 10), background='#f1f5f9', foreground='#1e293b')
        style.configure('TFrame', background='#f1f5f9')
        style.configure('TLabelframe', padding=10)
        style.configure('TLabelframe.Label', font=('Microsoft YaHei UI', 10, 'bold'))
        style.configure('TButton', padding=(12, 6))
        style.configure('Accent.TButton', foreground='white', background='#2563eb', font=('Microsoft YaHei UI', 11, 'bold'))
        style.map('Accent.TButton', background=[('disabled', '#cbd5e1'), ('active', '#1d4ed8')])
        style.configure('Treeview', background='white', fieldbackground='white', rowheight=31)
        style.configure('Treeview.Heading', font=('Microsoft YaHei UI', 10, 'bold'), padding=6)
        style.configure('Muted.TLabel', foreground='#64748b')

    def button(self, parent, text, action, **kwargs):
        widget = ttk.Button(parent, text=text, command=action, **kwargs)
        self.controls.append(widget)
        return widget

    def combo(self, parent, variable, values, width):
        widget = ttk.Combobox(parent, textvariable=variable, values=values, state='readonly', width=width)
        self.controls.append(widget)
        return widget

    def _build(self):
        heading = ttk.Frame(self, padding=(20, 15, 20, 6))
        heading.pack(fill='x')
        ttk.Label(heading, text='SiFli 固件烧录', font=('Microsoft YaHei UI', 21, 'bold')).pack(side='left')
        ttk.Label(heading, text='串口首刷 / 原生 USB 升级', style='Muted.TLabel').pack(side='left', padx=18, pady=(10, 0))
        toolbar = ttk.Frame(self, padding=(20, 8))
        toolbar.pack(fill='x')
        self.button(toolbar, '加载 ZIP / JSON / INI', self.choose_package, style='Accent.TButton').pack(side='left')
        self.button(toolbar, '识别固件文件夹', self.choose_directory).pack(side='left', padx=8)
        self.button(toolbar, '添加固件 + 地址', self.add).pack(side='left')
        for title, value in [('原生 USB DFU', 'usb'), ('串口烧录', 'serial')]:
            radio = ttk.Radiobutton(toolbar, text=title, variable=self.transport, value=value, command=self.change_transport)
            radio.pack(side='right', padx=10)
            self.controls.append(radio)
        ttk.Label(self, textvariable=self.origin, style='Muted.TLabel', padding=(20, 0, 20, 6), wraplength=1120).pack(fill='x')

        settings = ttk.LabelFrame(self, text='连接与烧录参数', padding=10)
        self.serial_settings = settings
        settings.pack(fill='x', padx=20, pady=4)
        ttk.Label(settings, text='下载串口').grid(row=0, column=0, sticky='w')
        self.ports = self.combo(settings, self.port, [], 31)
        self.serial_ports = self.ports
        self.ports.grid(row=0, column=1, sticky='ew', padx=6)
        self.button(settings, '刷新', self.scan_ports).grid(row=0, column=2, padx=(0, 14))
        ttk.Label(settings, text='芯片').grid(row=0, column=3)
        self.combo(settings, self.chip, CHIPS, 13).grid(row=0, column=4, padx=6)
        ttk.Label(settings, text='存储').grid(row=0, column=5)
        self.combo(settings, self.memory, MEMORIES, 14).grid(row=0, column=6, padx=6)
        ttk.Label(settings, text='波特率').grid(row=0, column=7)
        baud = ttk.Combobox(settings, textvariable=self.baud, values=('115200', '460800', '1000000', '2000000'), width=12)
        baud.grid(row=0, column=8, padx=6)
        self.controls.append(baud)
        ttk.Label(settings, text='连接前操作').grid(row=1, column=0, sticky='w', pady=(9, 0))
        self.combo(settings, self.before, BEFORE, 20).grid(row=1, column=1, sticky='w', padx=6, pady=(9, 0))
        ttk.Label(settings, text='完成后操作').grid(row=1, column=3, columnspan=2, sticky='e', pady=(9, 0))
        self.combo(settings, self.after_reset, AFTER, 14).grid(row=1, column=6, padx=6, pady=(9, 0))
        check = ttk.Checkbutton(settings, text='写后校验', variable=self.verify)
        check.grid(row=1, column=7, columnspan=2, sticky='w', pady=(9, 0))
        self.controls.append(check)
        ttk.Label(settings, text='sftool.exe').grid(row=2, column=0, sticky='w', pady=(9, 0))
        tool = ttk.Entry(settings, textvariable=self.tool)
        tool.grid(row=2, column=1, columnspan=7, sticky='ew', padx=6, pady=(9, 0))
        self.controls.append(tool)
        self.button(settings, '选择工具', self.choose_tool).grid(row=2, column=8, pady=(9, 0))
        settings.columnconfigure(1, weight=1)

        self.usb_settings = ttk.LabelFrame(self, text='原生 USB DFU · SF32LB527 阅读器', padding=10)
        usb_row = ttk.Frame(self.usb_settings)
        usb_row.pack(fill='x')
        ttk.Label(usb_row, text='USB 恢复设备').pack(side='left')
        self.usb_ports = self.combo(usb_row, self.port, [], 43)
        self.usb_ports.pack(side='left', fill='x', expand=True, padx=8)
        self.button(usb_row, '刷新', self.scan_ports).pack(side='left')
        self.button(usb_row, '读取设备', lambda: self.usb_action('info')).pack(side='left', padx=8)
        self.button(usb_row, '重启退出升级', lambda: self.usb_action('reboot')).pack(side='left')
        ttk.Label(self.usb_settings, text='先在阅读器设置中进入 USB 升级，或按住 PA33 再复位；接入原生 USB 后自动识别恢复设备。',
                  style='Muted.TLabel', wraplength=1090).pack(fill='x', pady=(9, 0))
        ttk.Label(self.usb_settings, text='仅更新应用、字体、波形，强制设备端读回校验；引导与恢复程序受保护。长按电源键约 10 秒可硬件重启。',
                  style='Muted.TLabel', wraplength=1090).pack(fill='x', pady=(5, 0))

        files = ttk.LabelFrame(self, text='烧录列表 · 地址为十六进制，可双击编辑', padding=8)
        self.files_frame = files
        files.pack(fill='both', expand=True, padx=20, pady=(8, 4))
        files.columnconfigure(0, weight=1)
        files.rowconfigure(0, weight=1)
        self.table = ttk.Treeview(files, columns=('index', 'file', 'address', 'size', 'path'), show='headings', height=7)
        for name, label, width in [('index', '#', 40), ('file', '固件文件', 185), ('address', '烧录地址', 125), ('size', '大小', 95), ('path', '文件路径', 510)]:
            self.table.heading(name, text=label)
            self.table.column(name, width=width, minwidth=40, stretch=name == 'path')
        self.table.grid(row=0, column=0, sticky='nsew')
        scrollbar = ttk.Scrollbar(files, orient='vertical', command=self.table.yview)
        scrollbar.grid(row=0, column=1, sticky='ns')
        self.table.configure(yscrollcommand=scrollbar.set)
        self.table.bind('<Double-1>', lambda event: self.edit())
        row = ttk.Frame(files)
        row.grid(row=1, column=0, columnspan=2, sticky='ew', pady=(7, 0))
        for title, action in [('编辑', self.edit), ('删除', self.remove), ('上移', lambda: self.move(-1)), ('下移', lambda: self.move(1)), ('清空列表', self.clear_files)]:
            self.button(row, title, action).pack(side='left', padx=(0, 5))
        ttk.Label(row, textvariable=self.summary, style='Muted.TLabel').pack(side='right')

        action = ttk.Frame(self, padding=(20, 7))
        action.pack(fill='x')
        self.start_button = self.button(action, '开始烧录', self.start, style='Accent.TButton')
        self.start_button.pack(side='left')
        self.stop_button = ttk.Button(action, text='停止烧录', command=self.stop, state='disabled')
        self.stop_button.pack(side='left', padx=8)
        self.progress = ttk.Progressbar(action, maximum=100)
        self.progress.pack(side='left', fill='x', expand=True, padx=12)
        ttk.Label(action, textvariable=self.percent, width=20).pack(side='right')
        status_label = ttk.Label(self, textvariable=self.status, padding=(20, 0, 20, 4), wraplength=1120)
        status_label.pack(fill='x')

        logs = ttk.LabelFrame(self, text='运行日志', padding=8)
        logs.pack(fill='both', expand=True, padx=20, pady=(3, 12))
        self.log = tk.Text(logs, height=8, bg='#0f172a', fg='#e2e8f0', font=('Consolas', 10), relief='flat', state='disabled', wrap='word')
        self.log.pack(side='left', fill='both', expand=True)
        scroll = ttk.Scrollbar(logs, command=self.log.yview)
        scroll.pack(side='right', fill='y')
        self.log.configure(yscrollcommand=scroll.set)
        footer = ttk.Frame(self, padding=(20, 0, 20, 10))
        footer.pack(fill='x')
        ttk.Label(footer, textvariable=self.footer_text, style='Muted.TLabel').pack(side='left')
        ttk.Button(footer, text='保存日志', command=self.save_log).pack(side='right')
        # Reserve controls and logs first; the scrollable image list shrinks
        # on shorter displays instead of hiding the lower controls.
        for panel in (footer, logs, status_label, action):
            panel.pack_configure(side='bottom', before=heading)

    def scan_ports(self):
        previous = self.port.get().split(' ', 1)[0]
        ports = sorted(list_ports.comports(), key=lambda p: (len(p.device), p.device))
        if self.transport.get() == 'usb':
            ports = [p for p in ports if recovery_port(p)]
        names = [f'{p.device}  {p.description}' for p in ports]
        self.port_names = {name: p.device for name, p in zip(names, ports)}
        self.ports['values'] = names
        match = next((name for name in names if self.port_names[name] == previous), None)
        self.port.set(match or (names[0] if len(names) == 1 else ''))

    def rescan(self):
        if self.mode == 'idle' and self.transport.get() == 'usb':
            self.scan_ports()
        self.after(1500, self.rescan)

    def apply_transport(self):
        usb = self.transport.get() == 'usb'
        self.serial_settings.pack_forget()
        self.usb_settings.pack_forget()
        panel = self.usb_settings if usb else self.serial_settings
        panel.pack(fill='x', padx=20, pady=4, before=self.files_frame)
        self.ports = self.usb_ports if usb else self.serial_ports
        self.start_button['text'] = '开始 USB 升级' if usb else '开始烧录'
        self.stop_button['text'] = '停止升级' if usb else '停止烧录'
        self.files_frame['text'] = ('USB 升级列表 · 仅应用、字体和波形三个固定分区' if usb
                                    else '烧录列表 · 地址为十六进制，可双击编辑')
        self.footer_text.set('USB 升级写入固定的三个分区；校验通过后自动重启。' if usb else '空片首刷与完整重烧使用下载 UART；原生 USB 端口请切换到 USB DFU 模式。')
        self.scan_ports()

    def change_transport(self):
        if self.mode != 'idle':
            return
        previous = self.active_transport
        selected = self.transport.get()
        if previous != selected:
            current = [self.rows[key] for key in self.table.get_children()]
            self.transport_images[previous] = current
            images = self.transport_images.get(selected)
            if images is None:
                images = select_targets(current) if selected == 'usb' else current
            self.replace_rows(images)
            self.active_transport = selected
        self.apply_transport()
        self.status.set('USB 模式：请选择三个升级镜像或加载完整固件包，并连接恢复设备。' if selected == 'usb'
                        else '串口模式：核对文件地址与下载串口后开始烧录。')

    def replace_rows(self, images):
        for key in self.table.get_children():
            self.table.delete(key)
        self.rows.clear()
        for image in images:
            self.put(image)
        self.refresh()

    def choose_tool(self):
        value = filedialog.askopenfilename(title='选择 SiFli sftool', filetypes=[('sftool', '*.exe'), ('所有文件', '*.*')])
        if value:
            self.tool.set(value)

    def choose_package(self):
        path = filedialog.askopenfilename(title='加载完整固件包或烧录清单', filetypes=[('固件包与清单', '*.zip *.json *.ini'), ('ZIP', '*.zip'), ('sftool JSON', '*.json'), ('ImgBurnList INI', '*.ini')])
        if path:
            self.import_path(Path(path))

    def choose_directory(self):
        path = filedialog.askdirectory(title='选择包含烧录清单的固件文件夹')
        if path:
            self.import_path(Path(path))

    def busy(self, mode):
        self.mode = mode
        for widget in self.controls:
            if mode != 'idle':
                widget.state(['disabled'])
            else:
                widget.state(['!disabled'])
        self.stop_button['state'] = 'normal' if mode == 'flashing' else 'disabled'

    def import_path(self, path):
        if self.mode != 'idle':
            return
        self.busy('loading')
        self.status.set('正在读取清单 / 解压并校验固件…')
        def worker():
            try:
                self.events.put(('imported', load_package(path)))
            except Exception as error:
                self.events.put(('error', f'加载失败：{error}'))
        threading.Thread(target=worker, daemon=True).start()

    def refresh(self):
        total = 0
        for index, key in enumerate(self.table.get_children(), 1):
            item = self.rows[key]
            size = item.path.stat().st_size if item.path.is_file() else 0
            total += size
            self.table.item(key, values=(index, item.path.name, f'0x{item.address:08X}', f'{size/1024:,.1f} KB', str(item.path)))
        self.summary.set(f'{len(self.rows)} 个文件 · {total/1024/1024:.2f} MB')

    def put(self, item):
        key = self.table.insert('', 'end')
        self.rows[key] = item
        self.refresh()

    def clear_files(self):
        if self.mode != 'idle':
            return
        for key in self.table.get_children():
            self.table.delete(key)
        self.rows.clear()
        for package in self.packages:
            package.close()
        self.packages.clear()
        self.transport_images.clear()
        self.usb_only = False
        self.origin.set('手动烧录列表')
        self.refresh()

    def add(self):
        path = filedialog.askopenfilename(title='选择固件文件', filetypes=[('固件二进制', '*.bin *.img *.dat'), ('所有文件', '*.*')])
        if path:
            self.editor(Image(Path(path).resolve(), 0x12200000))

    def edit(self):
        if self.mode == 'idle' and self.table.selection():
            key = self.table.selection()[0]
            self.editor(self.rows[key], key)

    def editor(self, image, key=None):
        dialog = tk.Toplevel(self)
        dialog.title('固件文件与地址')
        dialog.geometry('710x200')
        dialog.transient(self)
        dialog.grab_set()
        filename = tk.StringVar(value=str(image.path))
        target = tk.StringVar(value=f'0x{image.address:08X}')
        frame = ttk.Frame(dialog, padding=18)
        frame.pack(fill='both', expand=True)
        frame.columnconfigure(1, weight=1)
        ttk.Label(frame, text='固件文件').grid(row=0, column=0, padx=(0, 12))
        ttk.Entry(frame, textvariable=filename).grid(row=0, column=1, sticky='ew')
        def browse():
            value = filedialog.askopenfilename(parent=dialog)
            if value:
                filename.set(value)
        ttk.Button(frame, text='选择', command=browse).grid(row=0, column=2, padx=(6, 0))
        ttk.Label(frame, text='十六进制地址').grid(row=1, column=0, pady=18)
        entry = ttk.Entry(frame, textvariable=target, width=25)
        entry.grid(row=1, column=1, sticky='w')
        entry.focus_set()
        def accept():
            try:
                path = Path(filename.get()).resolve()
                item = Image(path, address(target.get()), image.sha256 if path == image.path else None)
                validate([item])
                if key:
                    self.rows[key] = item
                    self.refresh()
                else:
                    self.put(item)
                dialog.destroy()
            except Exception as error:
                messagebox.showerror('文件或地址无效', str(error), parent=dialog)
        ttk.Button(frame, text='保存', command=accept, style='Accent.TButton').grid(row=2, column=1, sticky='e')
        dialog.bind('<Return>', lambda event: accept())

    def remove(self):
        if self.mode != 'idle':
            return
        for key in self.table.selection():
            self.table.delete(key)
            self.rows.pop(key)
        self.refresh()

    def move(self, direction):
        if self.mode != 'idle' or not self.table.selection():
            return
        key = self.table.selection()[0]
        self.table.move(key, '', max(0, self.table.index(key) + direction))
        self.refresh()

    def append_log(self, value):
        self.log['state'] = 'normal'
        self.log.insert('end', value + '\n')
        self.log.see('end')
        self.log['state'] = 'disabled'
        if self.log_handle:
            self.log_handle.write(value + '\n')
            self.log_handle.flush()

    def start(self):
        if self.mode != 'idle':
            return
        if self.transport.get() == 'usb':
            self.usb_action('update')
            return
        try:
            if self.usb_only:
                raise ValueError('update.json 是 USB 升级清单，请切换到原生 USB DFU；串口首刷请选择 sftool_param.json 或 ImgBurnList.ini。')
            images = [self.rows[key] for key in self.table.get_children()]
            port = self.port_names.get(self.port.get(), '')
            args = command(Path(self.tool.get()), port, self.chip.get(), self.memory.get(),
                           self.baud.get(), self.before.get(), self.after_reset.get(), self.verify.get(), images)
            log_dir = Path(os.environ.get('LOCALAPPDATA', Path.home()))/'SiFliFlashTool/logs'
            log_dir.mkdir(parents=True, exist_ok=True)
            self.log_path = log_dir/(datetime.now().strftime('%Y%m%d-%H%M%S-%f') + '.log')
            self.log_handle = self.log_path.open('w', encoding='utf-8')
        except Exception as error:
            messagebox.showerror('无法开始烧录', str(error), parent=self)
            return
        self.cancel.clear()
        self.progress['value'] = 0
        self.percent.set('当前阶段：0%')
        self.busy('flashing')
        self.status.set(f'正在烧录 {len(images)} 个文件，请保持串口和供电连接。')
        self.append_log('\n' + datetime.now().isoformat(timespec='seconds') + ' 开始烧录')
        self.append_log(subprocess.list2cmdline(args))
        self.append_log(f'自动日志：{self.log_path}')
        def worker():
            try:
                code = run(args, lambda kind, value: self.events.put((kind, value)), self.cancel, images[0].path.parent)
                self.events.put(('done', (code, self.cancel.is_set())))
            except Exception as error:
                self.events.put(('error', f'烧录执行失败：{error}'))
        threading.Thread(target=worker, daemon=True).start()

    def usb_action(self, action):
        if self.mode != 'idle' or self.transport.get() != 'usb':
            return
        try:
            port = self.port_names.get(self.port.get(), '')
            available = [p.device for p in list_ports.comports() if recovery_port(p)]
            if port not in available:
                raise ValueError('未找到所选 SF32LB527 USB 恢复设备。请先进入 USB 升级模式，再接入原生 USB 数据线。')
            images = [self.rows[key] for key in self.table.get_children()]
            prepared = prepare_update(images) if action == 'update' else None
            log_dir = Path(os.environ.get('LOCALAPPDATA', Path.home()))/'SiFliFlashTool/logs'
            log_dir.mkdir(parents=True, exist_ok=True)
            self.log_path = log_dir/(datetime.now().strftime('%Y%m%d-%H%M%S-%f') + '.log')
            self.log_handle = self.log_path.open('w', encoding='utf-8')
        except Exception as error:
            messagebox.showerror('无法开始 USB 操作', str(error), parent=self)
            return
        self.cancel.clear()
        self.progress['value'] = 0
        self.percent.set('当前阶段：0%')
        self.busy('flashing')
        self.status.set('正在连接原生 USB 恢复设备…')
        self.append_log('\n' + datetime.now().isoformat(timespec='seconds') + f' USB {action} · {port}')
        self.append_log('USB 使用受保护的固定分区协议，不调用 sftool，不写引导与恢复程序。')
        def worker():
            try:
                result = run_usb(port, action, prepared, lambda kind, value: self.events.put((kind, value)), self.cancel)
                self.events.put(('usb_done', result))
            except Cancelled as error:
                self.events.put(('usb_done', str(error)))
            except Exception as error:
                self.events.put(('error', f'USB 操作失败：{error}；请保持恢复模式并重新尝试。'))
        threading.Thread(target=worker, daemon=True).start()

    def stop(self):
        prompt = ('停止后设备保留在 USB 恢复模式，可能需要重新完整升级。是否停止？' if self.transport.get() == 'usb'
                  else '停止后固件可能不完整，需要重新完整烧录。是否停止？')
        if self.mode == 'flashing' and messagebox.askyesno('停止当前操作', prompt, parent=self):
            self.cancel.set()
            self.stop_button['state'] = 'disabled'
            self.status.set('正在停止烧录…')

    def finish_log(self):
        if self.log_handle:
            self.log_handle.close()
            self.log_handle = None

    def poll(self):
        # Bound each tick so a verbose subprocess cannot starve UI events.
        for _ in range(300):
            try:
                kind, value = self.events.get_nowait()
            except queue.Empty:
                break
            if kind == 'log':
                self.append_log(value)
            elif kind == 'progress':
                self.progress['value'] = value
                self.percent.set(f'当前阶段：{value}%')
            elif kind == 'phase':
                self.status.set(value)
                self.append_log(value)
            elif kind == 'imported':
                self.busy('idle')
                self.clear_files()
                self.packages.append(value)
                self.usb_only = value.usb_only
                if value.usb_only:
                    self.transport.set('usb')
                    self.active_transport = 'usb'
                    self.apply_transport()
                self.chip.set(value.chip)
                self.memory.set(value.memory)
                self.verify.set(value.verify)
                self.transport_images['serial'] = value.images
                self.transport_images['usb'] = select_targets(value.images)
                images = self.transport_images[self.transport.get()]
                self.replace_rows(images)
                self.origin.set('已加载：' + value.origin)
                self.status.set('升级包检查通过 · 连接原生 USB 恢复设备后开始升级' if self.transport.get() == 'usb'
                                else '清单与文件检查通过 · 可编辑地址及顺序，确认串口后开始烧录')
                self.append_log(f'加载成功：{value.origin} · {len(value.images)} 个文件')
                if self.transport.get() == 'usb' and len(value.images) != len(images):
                    self.append_log(f'USB 仅选取 {len(images)} 个升级镜像，跳过 {len(value.images)-len(images)} 个其他分区；保护引导与恢复程序。')
            elif kind == 'usb_done':
                self.busy('idle')
                self.status.set(value)
                self.append_log(value)
                self.finish_log()
                self.scan_ports()
            elif kind == 'done':
                code, cancelled = value
                self.busy('idle')
                if cancelled:
                    result = '已停止 · 固件可能不完整，请重新完整烧录'
                elif code == 0:
                    result = '烧录完成，sftool 返回成功' + ('，写后校验已启用' if self.verify.get() else '（未启用写后校验）')
                    self.progress['value'] = 100
                    self.percent.set('完成')
                else:
                    result = f'烧录失败（退出码 {code}），请检查日志、下载模式及串口连接'
                self.status.set(result)
                self.append_log(result)
                self.finish_log()
            elif kind == 'error':
                self.busy('idle')
                self.status.set(value)
                self.append_log(value)
                self.finish_log()
                messagebox.showerror('操作失败', value, parent=self)
        self.after(80, self.poll)

    def save_log(self):
        path = filedialog.asksaveasfilename(title='保存运行日志', defaultextension='.log', filetypes=[('日志', '*.log'), ('文本', '*.txt')])
        if path:
            try:
                Path(path).write_text(self.log.get('1.0', 'end'), encoding='utf-8')
            except OSError as error:
                messagebox.showerror('保存失败', str(error), parent=self)

    def close(self):
        if self.mode != 'idle':
            messagebox.showinfo('正在执行任务', '请等待加载完成，或先停止烧录，再关闭工具。', parent=self)
            return
        self.finish_log()
        for package in self.packages:
            package.close()
        self.destroy()


if __name__ == '__main__':
    try:
        import ctypes
        ctypes.windll.shcore.SetProcessDpiAwareness(1)
    except (AttributeError, OSError):
        pass
    import sys
    app = App()
    if len(sys.argv) == 2:
        app.after(200, lambda: app.import_path(Path(sys.argv[1])))
    app.mainloop()
