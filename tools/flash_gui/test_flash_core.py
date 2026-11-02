import json
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch
import zipfile

from flash_core import Image, address, command, find_sftool, load_package, read_text, run, validate


class Tests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root/'中文 image.bin').write_bytes(bytes(range(256)))
        self.data = {'chip': 'SF32LB52', 'memory': 'NOR', 'write_flash': {'verify': True, 'files': [{'path': '中文 image.bin', 'address': '0x12200000'}]}}
        self.manifest = self.root/'sftool_param.json'
        self.manifest.write_text(json.dumps(self.data), encoding='utf-8')

    def tearDown(self):
        self.tmp.cleanup()

    def ini(self):
        (self.root/'ImgBurnList.ini').write_text('[FILEINFO]\nFILE0=中文 image.bin\nADDR0=0x12200000\nNUM=1\n', encoding='gb18030')

    def test_json_ini_zip(self):
        self.ini()
        for path in (self.manifest, self.root/'ImgBurnList.ini', self.root):
            p = load_package(path)
            self.assertEqual(p.images[0].address, 0x12200000)
            self.assertTrue(p.verify)
            p.close()
        archive = self.root/'test.zip'
        with zipfile.ZipFile(archive, 'w') as z:
            for path in self.root.iterdir():
                if path != archive:
                    z.write(path, 'firmware/'+path.name)
        p = load_package(archive)
        extracted = p.images[0].path
        self.assertEqual(extracted.read_bytes(), bytes(range(256)))
        p.close()
        self.assertFalse(extracted.exists())

    def test_bundled_backend_takes_precedence(self):
        bundle = self.root/'bundle'
        bundle.mkdir()
        expected = bundle/'sftool.exe'
        expected.write_bytes(b'fixed backend')
        with patch.object(sys, '_MEIPASS', str(bundle), create=True):
            self.assertEqual(find_sftool(), expected)

    def test_old_installed_backend_is_not_selected_automatically(self):
        old = self.root/'.sifli/tools/sftool/0.2.5/sftool.exe'
        old.parent.mkdir(parents=True)
        old.write_bytes(b'old backend')
        with patch.object(sys, '_MEIPASS', str(self.root/'empty'), create=True), \
             patch.object(sys, 'executable', str(self.root/'python.exe')), \
             patch('flash_core.__file__', str(self.root/'src/flash_core.py')), \
             patch('flash_core.Path.home', return_value=self.root), \
             patch('flash_core.shutil.which', return_value=str(old)) as search:
            self.assertIsNone(find_sftool())
            search.assert_not_called()

    def test_command_and_validation(self):
        p = load_package(self.manifest)
        args = command(Path(sys.executable), 'COM34', p.chip, p.memory, '1000000', 'default_reset', 'soft_reset', True, p.images)
        self.assertIn('--verify', args)
        self.assertEqual(args[-1], str(p.images[0].path)+'@0x12200000')
        self.assertEqual(address('12200000'), 0x12200000)
        with self.assertRaises(ValueError):
            validate(p.images+[Image(p.images[0].path, 0x12200001)])
        with self.assertRaises(ValueError):
            validate([Image(p.images[0].path, 0xffffffff)])
        with self.assertRaises(ValueError):
            address(True)
        p.images[0].sha256 = '0'*64
        with self.assertRaises(ValueError):
            validate(p.images)

    def test_conflicting_manifests(self):
        self.ini()
        path = self.root/'ImgBurnList.ini'
        path.write_text(path.read_text(encoding='gb18030').replace('12200000', '12400000'), encoding='utf-8')
        with self.assertRaises(ValueError):
            load_package(self.root)

    def test_bad_zip_paths(self):
        for name in ('../escape.bin', '/absolute.bin', 'C:/escape.bin'):
            path = self.root/'bad.zip'
            with zipfile.ZipFile(path, 'w') as z:
                z.writestr(name, b'x')
            with self.assertRaises(ValueError):
                load_package(path)

    def test_zip_external_reference(self):
        self.data['write_flash']['files'][0]['path'] = '../outside.bin'
        path = self.root/'bad.zip'
        with zipfile.ZipFile(path, 'w') as z:
            z.writestr('sftool_param.json', json.dumps(self.data))
        with self.assertRaises(ValueError):
            load_package(path)

    def test_utf16_and_verify_false(self):
        self.data['write_flash']['verify'] = False
        self.manifest.write_text(json.dumps(self.data), encoding='utf-16')
        self.assertFalse(load_package(self.manifest).verify)

    def test_process_success_failure_cancel(self):
        events = []
        emit = lambda kind, value: events.append((kind, value))
        cancel = threading.Event()
        code = run([sys.executable, '-c', "print('0%');print('100%');print('tail',end='')"], emit, cancel, self.root)
        self.assertEqual(code, 0)
        self.assertIn(('progress', 100), events)
        self.assertIn(('log', 'tail'), events)
        self.assertEqual(run([sys.executable, '-c', 'raise SystemExit(7)'], emit, cancel, self.root), 7)
        timer = threading.Timer(.3, cancel.set)
        timer.start()
        start = time.monotonic()
        code = run([sys.executable, '-c', 'import time;time.sleep(20)'], emit, cancel, self.root)
        timer.join()
        self.assertNotEqual(code, 0)
        self.assertLess(time.monotonic()-start, 5)


if __name__ == '__main__':
    unittest.main()
