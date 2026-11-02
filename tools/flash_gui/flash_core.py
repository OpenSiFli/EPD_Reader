"""Manifest import and the sftool subprocess backend; no GUI dependencies."""
from __future__ import annotations

import configparser
from dataclasses import dataclass, field
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import threading
import zipfile

CHIPS = ('SF32LB52', 'SF32LB55', 'SF32LB56', 'SF32LB57', 'SF32LB58')
MEMORIES = ('nor', 'nor_type1', 'nand', 'nand_type1', 'nand_nobbm_type1', 'sd', 'sd_type1')
BEFORE = ('default_reset', 'no_reset', 'no_reset_no_sync')
AFTER = ('soft_reset', 'no_reset')


def address(value: str | int) -> int:
    try:
        if isinstance(value, bool):
            raise ValueError()
        number = value if isinstance(value, int) else int(value.strip(), 16)
        if not 0 <= number <= 0xffffffff:
            raise ValueError()
        return number
    except (ValueError, TypeError, AttributeError) as error:
        raise ValueError(f'无效的十六进制地址：{value}') from error


@dataclass
class Image:
    path: Path
    address: int
    # Keep package provenance when a user edits an imported row.
    sha256: str | None = None


@dataclass
class Package:
    images: list[Image]
    chip: str = 'SF32LB52'
    memory: str = 'nor'
    verify: bool = True
    origin: str = ''
    temporary: tempfile.TemporaryDirectory | None = field(default=None, repr=False)
    usb_only: bool = False

    def close(self):
        if self.temporary:
            self.temporary.cleanup()
            self.temporary = None


def validate(images: list[Image]) -> None:
    if not images:
        raise ValueError('请至少添加一个烧录文件。')
    intervals = []
    for item in images:
        if not item.path.is_file():
            raise ValueError(f'文件不存在：{item.path}')
        start = address(item.address)
        size = item.path.stat().st_size
        if size == 0 or start + size > 0x100000000:
            raise ValueError(f'文件为空或超出地址范围：{item.path.name}')
        if any(ch in str(item.path) for ch in '\r\n\x00'):
            raise ValueError('文件路径不能包含控制字符。')
        if item.sha256 and hashlib.sha256(item.path.read_bytes()).hexdigest() != item.sha256.lower():
            raise ValueError(f'文件 SHA-256 校验失败：{item.path.name}')
        intervals.append((start, start + size, item.path.name))
    intervals.sort()
    for previous, current in zip(intervals, intervals[1:]):
        if current[0] < previous[1]:
            raise ValueError(f'烧录地址重叠：{previous[2]} 与 {current[2]}')


def read_text(path: Path) -> str:
    data = path.read_bytes()
    encodings = ('utf-16',) if data.startswith((b'\xff\xfe', b'\xfe\xff')) else ('utf-8-sig', 'gb18030')
    for encoding in encodings:
        try:
            return data.decode(encoding)
        except UnicodeError:
            pass
    raise ValueError(f'无法识别文本编码：{path.name}')


def resolve_file(manifest: Path, name: str, boundary: Path | None) -> Path:
    if not isinstance(name, str) or not name.strip():
        raise ValueError('清单中的文件路径为空。')
    result = (manifest.parent / name.replace('\\', '/')).resolve()
    if boundary and not result.is_relative_to(boundary.resolve()):
        raise ValueError(f'压缩包清单引用了包外文件：{name}')
    return result


def load_manifest(path: Path, boundary: Path | None = None) -> Package:
    path = path.resolve()
    images = []
    if path.suffix.lower() == '.json':
        data = json.loads(read_text(path))
        if isinstance(data, dict) and {'board', 'layout', 'protocol', 'images'} <= data.keys():
            from usb_core import load_update_manifest
            return load_update_manifest(path)
        if not isinstance(data, dict) or 'write_flash' not in data:
            raise ValueError('请选择 sftool_param.json 或 USB 升级清单 update.json；images.json 不是烧录清单。')
        chip = str(data.get('chip', 'SF32LB52')).upper()
        memory = str(data.get('memory', 'nor')).lower()
        write = data['write_flash']
        if not isinstance(write, dict) or not isinstance(write.get('files'), list):
            raise ValueError('JSON 缺少 write_flash.files 列表。')
        verify = write.get('verify', True)
        if not isinstance(verify, bool):
            raise ValueError('write_flash.verify 必须是 true 或 false。')
        for item in write['files']:
            images.append(Image(resolve_file(path, item['path'], boundary), address(item['address'])))
    else:
        ini = configparser.ConfigParser(interpolation=None)
        ini.read_string(read_text(path))
        section = next((ini[name] for name in ini.sections() if name.upper() == 'FILEINFO'), None)
        if section is None:
            raise ValueError('INI 缺少 [FILEINFO]。')
        count = int(section['NUM'])
        if not 1 <= count <= 256:
            raise ValueError('INI 的 NUM 应为 1–256。')
        chip, memory, verify = 'SF32LB52', 'nor', True
        for index in range(count):
            images.append(Image(resolve_file(path, section[f'FILE{index}'], boundary), address(section[f'ADDR{index}'])))
    if chip not in CHIPS or memory not in MEMORIES:
        raise ValueError(f'不支持的芯片或存储类型：{chip} / {memory}')
    # Optional SDK package hashes bind each imported row to its firmware bytes.
    hashes = path.parent / 'images.json'
    if hashes.is_file():
        data = json.loads(read_text(hashes))
        if isinstance(data, list):
            for image in images:
                for entry in data:
                    if isinstance(entry, dict) and entry.get('path') == image.path.name:
                        if 'address' in entry and address(entry['address']) != image.address:
                            raise ValueError(f'清单与 images.json 地址不一致：{image.path.name}')
                        if 'size' in entry and entry['size'] != image.path.stat().st_size:
                            raise ValueError(f'清单中的文件长度不符：{image.path.name}')
                        image.sha256 = entry.get('sha256')
    validate(images)
    return Package(images, chip, memory, verify, str(path))


def find_manifest(root: Path) -> Path:
    candidates = [p for p in root.rglob('*') if p.is_file() and p.name.lower() in ('sftool_param.json', 'imgburnlist.ini')]
    if not candidates:
        candidates = [p for p in root.rglob('*') if p.is_file() and p.name.lower() == 'update.json']
    if not candidates:
        # Also accept a differently named, valid sftool JSON at package root.
        for p in root.glob('*.json'):
            try:
                data = json.loads(read_text(p))
                if isinstance(data, dict) and 'write_flash' in data:
                    candidates.append(p)
            except (ValueError, OSError):
                pass
    if not candidates:
        raise ValueError('未找到 sftool_param.json 或 ImgBurnList.ini，请手动添加文件和地址。')
    parents = {p.parent for p in candidates}
    if len(parents) != 1:
        raise ValueError('包内存在多套固件，请解压后选择需要的 JSON 或 INI。')
    candidates.sort(key=lambda p: p.suffix.lower() != '.json')
    return candidates[0]


def load_package(path: Path) -> Package:
    path = path.resolve()
    temporary = None
    boundary = None
    try:
        if path.suffix.lower() == '.zip':
            temporary = tempfile.TemporaryDirectory(prefix='sifli-flash-')
            boundary = Path(temporary.name)
            seen, total = set(), 0
            with zipfile.ZipFile(path) as archive:
                for entry in archive.infolist():
                    name = entry.filename.replace('\\', '/')
                    parts = PurePosixPath(name).parts
                    key = name.rstrip('/').casefold()
                    mode = entry.external_attr >> 16
                    if (not parts or name.startswith('/') or '..' in parts or ':' in name
                            or stat.S_ISLNK(mode) or key in seen):
                        raise ValueError(f'ZIP 包含不安全或重复的路径：{entry.filename}')
                    seen.add(key)
                    total += entry.file_size
                    if entry.file_size > 512*1024*1024 or total > 1024*1024*1024 or len(seen) > 10000:
                        raise ValueError('ZIP 文件数量或解压大小超过限制。')
                    dest = boundary.joinpath(*parts)
                    if not dest.resolve().is_relative_to(boundary.resolve()):
                        raise ValueError('ZIP 路径超出解压目录。')
                    if entry.is_dir():
                        dest.mkdir(parents=True, exist_ok=True)
                    else:
                        dest.parent.mkdir(parents=True, exist_ok=True)
                        with archive.open(entry) as source, dest.open('wb') as output:
                            shutil.copyfileobj(source, output)
            manifest = find_manifest(boundary)
        elif path.is_dir():
            manifest = find_manifest(path)
        else:
            manifest = path
        package = load_manifest(manifest, boundary)
        # When both SDK formats are present, reject discrepancies rather than
        # silently burning the other format's image set.
        companion = next((p for p in manifest.parent.iterdir() if p.name.lower() == 'imgburnlist.ini'), None)
        if manifest.suffix.lower() == '.json' and companion and not package.usb_only:
            other = load_manifest(companion, boundary)
            if [(i.path, i.address) for i in package.images] != [(i.path, i.address) for i in other.images]:
                raise ValueError('JSON 与 INI 的烧录文件或地址不一致。')
        package.temporary = temporary
        package.origin = str(path)
        return package
    except Exception:
        if temporary:
            temporary.cleanup()
        raise


def find_sftool() -> Path | None:
    roots = [Path(getattr(sys, '_MEIPASS', Path(__file__).parent/'backend')),
             Path(__file__).parent/'backend', Path(sys.executable).parent, Path(__file__).parent]
    for root in roots:
        candidate = root/'sftool.exe'
        if candidate.is_file():
            return candidate
    # Do not silently select an installed/PATH copy of the affected old 0.2.5.
    # External tools can still be selected explicitly in the GUI.
    return None


def command(tool: Path, port: str, chip: str, memory: str, baud: str,
            before: str, after: str, verify: bool, images: list[Image]) -> list[str]:
    validate(images)
    if not tool.is_file():
        raise ValueError('请选择有效的 sftool.exe。')
    if not port or chip not in CHIPS or memory not in MEMORIES or before not in BEFORE or after not in AFTER:
        raise ValueError('请检查串口、芯片、存储类型和复位选项。')
    if not str(baud).isdigit() or not 9600 <= int(baud) <= 4000000:
        raise ValueError('波特率应为 9600–4000000。')
    args = [str(tool), '-p', port, '-c', chip, '-m', memory, '-b', str(baud),
            '--before', before, '--after', after, 'write_flash']
    if verify:
        args.append('--verify')
    return args + [f'{i.path}@0x{i.address:08X}' for i in images]


def run(args: list[str], emit, cancel: threading.Event, cwd: Path) -> int:
    """emit(kind,value) runs on worker thread; callers marshal UI updates."""
    flags = getattr(subprocess, 'CREATE_NO_WINDOW', 0)
    process = subprocess.Popen(args, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               stdin=subprocess.DEVNULL, creationflags=flags)
    stopped = threading.Event()
    def watch():
        while not stopped.wait(.1):
            if cancel.is_set():
                try:
                    process.terminate()
                except OSError:
                    pass
                return
    watcher = threading.Thread(target=watch, daemon=True)
    watcher.start()
    line = bytearray()
    try:
        while True:
            byte = process.stdout.read(1)
            if not byte:
                break
            if byte in (b'\r', b'\n'):
                if line:
                    value = line.decode('utf-8', errors='replace')
                    value = re.sub(r'\x1b\[[0-9;?]*[A-Za-z]', '', value)
                    emit('log', value)
                    match = re.search(r'(?<!\d)(\d{1,3})%', value)
                    if match:
                        emit('progress', min(100, int(match[1])))
                    line.clear()
            else:
                line.extend(byte)
        if line:
            emit('log', line.decode('utf-8', errors='replace'))
        return process.wait()
    finally:
        stopped.set()
        process.stdout.close()
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
        watcher.join(timeout=1)
