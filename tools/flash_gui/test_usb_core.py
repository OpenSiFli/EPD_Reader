import hashlib
import json
from pathlib import Path
import struct
import tempfile
import threading
import time
from types import SimpleNamespace
import unittest
import zipfile
import zlib

from flash_core import Image, load_package
from usb_core import ADDRESSES, Cancelled, CancellableRecovery, prepare_update, protocol, recovery_port, run_usb, select_targets


class FakeRecovery:
    def __init__(self, port, cancel):
        self.cancel = cancel
        self.calls = []
        self.closed = False
        self.fail = None
        self.cancel_after = None
        self.bad_offset = False

    def command(self, command, payload=b'', timeout=30):
        self.calls.append((command, payload, timeout))
        if command == self.fail:
            raise RuntimeError('Simulated device rejection')
        if command == self.cancel_after:
            self.cancel.set()
        if command == 3:
            return struct.unpack_from('<II', payload)[1] + len(payload)-8 + int(self.bad_offset)
        return 0x52454144 if command == 1 else 0

    def close(self):
        self.closed = True


class FakeSerial:
    """Exercise real frame, sequence, CRC and board checks without hardware."""
    def __init__(self, body_board=protocol.BOARD, corrupt=False, wrong_sequence=False, status=0):
        self.input = bytearray()
        self.board, self.corrupt, self.wrong_sequence, self.status = body_board, corrupt, wrong_sequence, status
        self.closed = False

    def write(self, frame):
        magic, op, seq, size, crc = struct.unpack_from('<IIIII', frame)
        assert magic == protocol.MAGIC and len(frame) == 20+size
        assert zlib.crc32(frame[20:], zlib.crc32(frame[:16])) == crc
        body = struct.pack('<iII', self.status, 0x52454144, self.board)
        self.input.extend(protocol.encode(op | 0x80000000, seq + int(self.wrong_sequence), body))
        if self.corrupt:
            self.input[-1] ^= 1
        return len(frame)

    def flush(self):
        pass

    def read(self, size):
        # Fragmented replies expose assumptions about serial read boundaries.
        size = min(size, 3)
        result = bytes(self.input[:size])
        del self.input[:size]
        return result

    def close(self):
        self.closed = True


class Tests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.blobs = [struct.pack('<II', 0x20010000, ADDRESSES[0]+9)+b'a'*5000, b'f'*5001, b'w'*3000]
        self.images = []
        for name, target, data in zip(protocol.NAMES, ADDRESSES, self.blobs):
            path = self.root/name
            path.write_bytes(data)
            self.images.append(Image(path, target))
        self.cancel = threading.Event()
        self.device = FakeRecovery('TEST', self.cancel)
        self.events = []

    def tearDown(self):
        self.tmp.cleanup()

    def emit(self, kind, value):
        self.events.append((kind, value))

    def update(self):
        return run_usb('TEST', 'update', prepare_update(self.images), self.emit, self.cancel,
                       lambda port, cancel: self.device)

    def metadata(self):
        records = [{'id': i, 'file': name, 'size': len(data), 'sha256': hashlib.sha256(data).hexdigest(), 'crc32': zlib.crc32(data)}
                   for i, (name, data) in enumerate(zip(protocol.NAMES, self.blobs))]
        path = self.root/'update.json'
        path.write_text(json.dumps({'board': protocol.BOARD, 'layout': 1, 'protocol': 1, 'images': records}), encoding='utf-8')
        return path

    def test_transaction_and_protected_targets(self):
        self.assertEqual(select_targets(self.images + [Image(self.images[0].path, 0x12010000)]), self.images)
        result = self.update()
        self.assertIn('升级完成', result)
        commands = [c[0] for c in self.device.calls]
        self.assertEqual(commands[:2], [1, 2])
        self.assertEqual(commands[-2:], [4, 6])
        self.assertTrue(self.device.closed)
        received = [bytearray(), bytearray(), bytearray()]
        for cmd, payload, _ in self.device.calls:
            if cmd == 3:
                index, offset = struct.unpack_from('<II', payload)
                self.assertEqual(offset, len(received[index]))
                received[index].extend(payload[8:])
        self.assertEqual([bytes(data) for data in received], self.blobs)
        manifest = self.device.calls[1][1]
        self.assertEqual(struct.unpack_from('<III', manifest), (protocol.BOARD, 1, 3))
        self.assertIn(('progress', 100), self.events)

    def test_reject_wrong_address_duplicate_or_missing(self):
        for images in (self.images[:2], self.images + [self.images[0]],
                       self.images[:2] + [Image(self.images[2].path, 0x12020000)]):
            with self.assertRaises(ValueError):
                prepare_update(images)

    def test_invalid_vectors_and_hash(self):
        self.images[0].path.write_bytes(b'bad')
        with self.assertRaises(ValueError):
            prepare_update(self.images)
        self.images[0].path.write_bytes(self.blobs[0])
        self.images[0].sha256 = '0'*64
        with self.assertRaises(ValueError):
            prepare_update(self.images)

    def test_metadata_zip_and_wrong_board(self):
        path = self.metadata()
        p = load_package(path)
        self.assertTrue(p.usb_only)
        self.assertEqual(prepare_update(p.images)[0], self.blobs)
        archive = self.root/'usb.zip'
        with zipfile.ZipFile(archive, 'w') as z:
            for name in (*protocol.NAMES, 'update.json'):
                z.write(self.root/name, 'usb/'+name)
        p = load_package(archive)
        try:
            self.assertTrue(p.usb_only)
            self.assertEqual(prepare_update(p.images)[0], self.blobs)
        finally:
            p.close()
        data = json.loads(path.read_text())
        data['board'] = 1
        path.write_text(json.dumps(data))
        with self.assertRaises(ValueError):
            prepare_update(self.images)

    def test_commit_failure_never_reboots(self):
        self.device.fail = 4
        with self.assertRaises(RuntimeError):
            self.update()
        self.assertNotIn(6, [c[0] for c in self.device.calls])
        self.assertTrue(self.device.closed)

    def test_offset_failure_never_commits(self):
        self.device.bad_offset = True
        with self.assertRaises(ValueError):
            self.update()
        self.assertNotIn(4, [c[0] for c in self.device.calls])
        self.assertTrue(self.device.closed)

    def test_cancel_never_commits_or_reboots(self):
        self.device.cancel_after = 3
        with self.assertRaises(Cancelled):
            self.update()
        self.assertNotIn(4, [c[0] for c in self.device.calls])
        self.assertNotIn(6, [c[0] for c in self.device.calls])
        self.assertTrue(self.device.closed)

    def test_info_is_read_only(self):
        result = run_usb('TEST', 'info', None, self.emit, self.cancel, lambda p,c: self.device)
        self.assertIn('设备已确认', result)
        self.assertEqual([c[0] for c in self.device.calls], [1])

    def test_real_wire_fragmented_crc_board_sequence_status(self):
        for kwargs, expected in [({}, None), ({'body_board': 1}, ValueError), ({'corrupt': True}, ValueError),
                                 ({'wrong_sequence': True}, ValueError), ({'status': -4}, RuntimeError)]:
            device = CancellableRecovery.__new__(CancellableRecovery)
            device.cancel, device.sequence, device.serial = self.cancel, 15, FakeSerial(**kwargs)
            if expected:
                with self.assertRaises(expected):
                    device.command(1)
            else:
                self.assertEqual(device.command(1), 0x52454144)
            device.close()
            self.assertTrue(device.serial.closed)

    def test_timeout_and_cancel_during_reply(self):
        device = CancellableRecovery.__new__(CancellableRecovery)
        device.cancel, device.serial = self.cancel, FakeSerial()
        with self.assertRaises(TimeoutError):
            device.exact(20, time.monotonic())
        self.cancel.set()
        with self.assertRaises(Cancelled):
            device.exact(20, time.monotonic()+10)

    def test_port_identity(self):
        port = SimpleNamespace(vid=0x38f4, pid=1, serial_number='EPD527-RECOVERY-1')
        self.assertTrue(recovery_port(port))
        port.serial_number = 'different-board'
        self.assertFalse(recovery_port(port))


if __name__ == '__main__':
    unittest.main()
