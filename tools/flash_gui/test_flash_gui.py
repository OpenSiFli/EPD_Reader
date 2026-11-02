"""Windows Tk dispatch tests. All subprocess and serial access is mocked."""
import sys
import time
import unittest
from unittest.mock import patch


@unittest.skipUnless(sys.platform == 'win32', 'Windows Tk UI tests')
class GuiTests(unittest.TestCase):
    def setUp(self):
        import tkinter as tk
        from tkinter import ttk
        import flash_gui
        self.tk, self.ttk, self.module = tk, ttk, flash_gui
        self.ports = patch.object(flash_gui.list_ports, 'comports', return_value=[])
        self.ports.start()
        self.app = flash_gui.App()
        self.app.withdraw()
        self.app.tool.set(sys.executable)
        self.app.port_names = {'MOCK UART': 'COM_TEST'}
        self.app.port.set('MOCK UART')
        self.backend = patch.object(flash_gui, 'run', return_value=0)
        self.run = self.backend.start()
        self.errors = patch.object(flash_gui.messagebox, 'showerror')
        self.showerror = self.errors.start()
        self.confirm = patch.object(flash_gui.messagebox, 'askyesno', return_value=True)
        self.ask = self.confirm.start()

    def tearDown(self):
        self.app.update_idletasks()
        self.app.close()
        self.confirm.stop()
        self.errors.stop()
        self.backend.stop()
        self.ports.stop()

    def widgets(self, root, cls):
        result = []
        for child in root.winfo_children():
            if isinstance(child, cls):
                result.append(child)
            result.extend(self.widgets(child, cls))
        return result

    def dialog(self):
        self.app.erase_dialog()
        return self.widgets(self.app, self.tk.Toplevel)[0]

    def execute(self, dialog):
        next(w for w in self.widgets(dialog, self.ttk.Button) if w.cget('text') == '执行擦除').invoke()
        deadline = time.monotonic()+5
        while self.app.mode != 'idle' and time.monotonic() < deadline:
            self.app.update()
            time.sleep(.01)
        self.assertEqual(self.app.mode, 'idle')

    def test_region_dispatch_and_result_without_firmware_files(self):
        for rc, expected in [(0, '擦除完成'), (7, '擦除失败')]:
            with self.subTest(rc=rc):
                self.run.return_value = rc
                dialog = self.dialog()
                entries = self.widgets(dialog, self.ttk.Entry)
                self.app.setvar(entries[0].cget('textvariable'), '0x12FFF000')
                self.app.setvar(entries[1].cget('textvariable'), '0x1000')
                self.execute(dialog)
                self.assertEqual(self.run.call_args.args[0][-2:], ['erase_region', '0x12FFF000:0x1000'])
                self.assertIn(expected, self.app.status.get())
                self.assertNotIn('写后校验', self.app.status.get())
                self.assertIn('COM_TEST', self.ask.call_args.args[1])
                self.assertIn('0x12FFFFFF', self.ask.call_args.args[1])

    def test_whole_flash_dispatch(self):
        dialog = self.dialog()
        next(w for w in self.widgets(dialog, self.ttk.Radiobutton) if w.cget('value') == 'all').invoke()
        self.assertTrue(self.widgets(dialog, self.ttk.Entry)[1].instate(['disabled']))
        self.execute(dialog)
        self.assertEqual(self.run.call_args.args[0][-2:], ['erase_flash', '0x12000000'])
        self.assertIn('整片', self.ask.call_args.args[1])

    def test_confirmation_cancel_never_runs(self):
        self.ask.return_value = False
        dialog = self.dialog()
        self.execute(dialog)
        self.run.assert_not_called()
        self.assertTrue(dialog.winfo_exists())

    def test_invalid_range_is_rejected_before_confirmation(self):
        dialog = self.dialog()
        self.app.setvar(self.widgets(dialog, self.ttk.Entry)[0].cget('textvariable'), '0x12000001')
        self.execute(dialog)
        self.run.assert_not_called()
        self.ask.assert_not_called()
        self.showerror.assert_called_once()

    def test_usb_cannot_erase_even_after_busy_state(self):
        self.app.transport.set('usb')
        self.app.change_transport()
        self.app.busy('loading')
        self.app.busy('idle')
        self.assertTrue(self.app.erase_button.instate(['disabled']))
        self.app.erase_dialog()
        self.assertFalse(self.widgets(self.app, self.tk.Toplevel))
        self.run.assert_not_called()

    def test_cancelled_host_wait_is_not_reported_as_erase_success(self):
        def cancel(args, emit, cancel, cwd):
            cancel.set()
            return -1
        self.run.side_effect = cancel
        self.execute(self.dialog())
        self.assertIn('已停止等待', self.app.status.get())
        self.assertIn('擦除可能仍在执行', self.app.status.get())
        self.assertNotIn('擦除完成', self.app.status.get())


if __name__ == '__main__':
    unittest.main()
