"""Protected SF32LB527 USB CDC recovery transport, shared wire protocol with CLI."""
from __future__ import annotations

from pathlib import Path
import struct
import sys
import time
import zlib

# Direct source execution lives one directory below the existing CLI module.
# PyInstaller resolves that module with build.ps1's --paths argument.
if not getattr(sys, 'frozen', False):
    sys.path.append(str(Path(__file__).resolve().parent.parent))
import usb_dfu_527 as protocol

ADDRESSES = (0x12200000, 0x12400000, 0x12EC0000)
USB_VID, USB_PID, USB_SERIAL = 0x38F4, 1, 'EPD527-RECOVERY-1'


class Cancelled(Exception):
    pass


def recovery_port(port):
    return (port.vid, port.pid, port.serial_number) == (USB_VID, USB_PID, USB_SERIAL)


def select_targets(images):
    """Only select the three update slots; never send raw addresses over USB."""
    return [item for target in ADDRESSES for item in images if item.address == target]


def prepare_update(images):
    if len(images) != 3 or sorted(item.address for item in images) != sorted(ADDRESSES):
        raise ValueError('USB 升级必须包含应用、字体和波形三个分区，地址分别为 0x12200000 / 0x12400000 / 0x12EC0000。')
    ordered = select_targets(images)
    blobs = []
    for item, limit in zip(ordered, protocol.LIMITS):
        if not item.path.is_file() or not 0 < item.path.stat().st_size <= limit:
            raise ValueError(f'USB 升级文件不存在、为空或超出分区：{item.path.name}')
        data = item.path.read_bytes()
        if item.sha256:
            import hashlib
            if hashlib.sha256(data).hexdigest() != item.sha256.lower():
                raise ValueError(f'文件 SHA-256 校验失败：{item.path.name}')
        blobs.append(data)
    if len(blobs[0]) < 8:
        raise ValueError('应用镜像缺少向量表。')
    sp, pc = struct.unpack_from('<II', blobs[0])
    if sp % 8 or not 0x20000000 <= sp <= 0x2007fc00 or not pc & 1 or not ADDRESSES[0] <= pc < ADDRESSES[0] + len(blobs[0]):
        raise ValueError('应用向量表不适用于 SF32LB527 当前分区布局。')
    # A factory / update package binds bytes, board, layout and protocol before
    # BEGIN can invalidate the installed application. Manual mode uses the same
    # fixed board/layout and validates lengths/vectors above.
    parents = {item.path.parent.resolve() for item in ordered}
    if len(parents) == 1 and (ordered[0].path.parent/'update.json').is_file():
        checked, manifest = protocol.load_package(ordered[0].path.parent)
        if checked != blobs:
            raise ValueError('当前文件与 update.json 不一致，请重新加载升级包。')
    else:
        manifest = struct.pack('<III', protocol.BOARD, 1, 3) + b''.join(
            struct.pack('<II', len(data), zlib.crc32(data)) for data in blobs)
    return blobs, manifest


def load_update_manifest(path):
    """Import a standalone update.json, including its SHA-256/vector checks."""
    from flash_core import Image, Package
    blobs, _ = protocol.load_package(path.parent)
    import hashlib
    return Package([Image(path.parent/name, target, hashlib.sha256(data).hexdigest())
                    for name, target, data in zip(protocol.NAMES, ADDRESSES, blobs)],
                   origin=str(path), usb_only=True)


class CancellableRecovery(protocol.Recovery):
    def __init__(self, port, cancel):
        self.cancel = cancel
        self.check_cancel()
        try:
            super().__init__(port)
        except Exception:
            if getattr(self, 'serial', None):
                self.serial.close()
            raise

    def check_cancel(self):
        if self.cancel.is_set():
            raise Cancelled('USB 升级已停止，设备保留在恢复模式；重新完整升级即可恢复。')

    def exact(self, count, deadline):
        result = bytearray()
        while len(result) < count and time.monotonic() < deadline:
            self.check_cancel()
            result.extend(self.serial.read(count - len(result)))
        self.check_cancel()
        if len(result) != count:
            raise TimeoutError('USB 恢复端口响应超时，请保持恢复模式并重新完整升级。')
        return bytes(result)

    def command(self, command, payload=b'', timeout=30):
        self.check_cancel()
        return super().command(command, payload, timeout)


def run_usb(port, action, prepared, emit, cancel, factory=CancellableRecovery):
    """Return only after acknowledged board checks / readback / reboot.

    factory is injectable for deterministic host tests; production uses the
    original Recovery frame/CRC/sequence/board checks on a native CDC port.
    """
    if action not in ('update', 'info', 'reboot') or not port:
        raise ValueError('请选择原生 USB 恢复端口和有效操作。')
    if action == 'update' and prepared is None:
        raise ValueError('请先加载 USB 升级文件。')
    def check():
        if cancel.is_set():
            raise Cancelled('USB 操作已停止；设备保留在恢复模式，可重新升级。')
    check()
    device = factory(port, cancel)
    try:
        state = device.command(1)
        emit('log', f'已连接 SF32LB527 USB 恢复程序，状态 0x{state:08X}')
        if action == 'info':
            return f'USB 设备已确认 · SF32LB527 · 状态 0x{state:08X}'
        if action == 'update':
            blobs, manifest = prepared
            check()
            emit('phase', '开始升级事务；引导与恢复分区不在写入范围')
            device.command(2, manifest)
            total, written, progress = sum(map(len, blobs)), 0, -1
            for index, data in enumerate(blobs):
                emit('phase', f'正在写入 {protocol.NAMES[index]}（{index+1}/3）')
                for offset in range(0, len(data), 2048):
                    check()
                    block = data[offset:offset+2048]
                    received = device.command(3, struct.pack('<II', index, offset) + block)
                    if received != offset + len(block):
                        raise ValueError('设备返回的写入偏移不符，请重新完整升级。')
                    written += len(block)
                    percent = written * 90 // total
                    if percent != progress:
                        progress = percent
                        emit('progress', percent)
                emit('log', f'{protocol.NAMES[index]} 写入完成：{len(data):,} 字节')
            check()
            emit('phase', '正在读回校验三个镜像，请保持 USB 连接…')
            device.command(4, timeout=120)
            emit('log', '三个分区的设备端 Flash 读回 CRC 校验全部通过')
            emit('progress', 98)
        check()
        emit('phase', '正在校验启动条件并重启阅读器…')
        device.command(6, timeout=120)
        emit('progress', 100)
        return ('USB 升级完成 · 设备端读回校验通过，已请求重启' if action == 'update'
                else '设备已确认启动条件，已请求退出 USB 升级模式')
    finally:
        device.close()
