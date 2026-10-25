import hashlib
import tempfile
import unittest
from pathlib import Path

from tools.check_assets import verify
from tools.inventory import collect
from tools.rom_analyze import parse


class ToolsTests(unittest.TestCase):
    def test_asset_missing_and_mismatch(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            profile = {'assets': [{'name': 'a.bin', 'bytes': 3, 'sha256': hashlib.sha256(b'abc').hexdigest()}]}
            self.assertFalse(verify(root, profile)[0]['valid'])
            (root / 'a.bin').write_bytes(b'abc')
            self.assertTrue(verify(root, profile)[0]['valid'])
            (root / 'a.bin').write_bytes(b'abd')
            self.assertFalse(verify(root, profile)[0]['valid'])

    def test_asset_path_traversal_rejected(self):
        with self.assertRaises(ValueError):
            verify(Path('.'), {'assets': [{'name': '../a.bin'}]})

    def test_inventory_is_file_read_only(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            device = root / '0000:01:00.0'
            device.mkdir()
            (device / 'vendor').write_text('0x10de\n')
            before = (device / 'vendor').read_bytes()
            result = collect(['0000:01:00.0'], root)
            self.assertEqual(result['devices']['0000:01:00.0']['vendor'], '0x10de')
            self.assertEqual(before, (device / 'vendor').read_bytes())
            self.assertIsNone(result['devices']['0000:01:00.0']['driver'])
        with self.assertRaises(ValueError):
            collect(['../../etc'])

    def test_bad_rom_refused(self):
        for data in (b'', bytes(511), bytes(1024), b'\x55\xaa' + bytes(510), bytes(1048577)):
            with self.assertRaises(ValueError):
                parse(data)

    def test_default_execution_gate_present(self):
        root = Path(__file__).resolve().parents[1]
        source = (root / 'kernel/tu102_execute_main.c').read_text()
        self.assertIn('#ifndef KOH_UNLOCK_REVIEWED_PROFILE', source)
        self.assertIn('return -EOPNOTSUPP;', source)
        self.assertIn('if (!execute || !target', source)
        probe = (root / 'kernel/tu102_diag.c').read_text()
        for forbidden in ('writel(', 'pci_write_', 'dma_alloc', 'request_firmware(', 'pcie_flr('):
            self.assertNotIn(forbidden, probe)


if __name__ == '__main__':
    unittest.main()
