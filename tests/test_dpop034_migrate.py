from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
import sys

ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / 'tools' / 'dpop034_migrate.py'


def load_module():
    spec = importlib.util.spec_from_file_location('dpop034_migrate', MODULE_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError('cannot load dpop034_migrate')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def make_donor(root: Path) -> None:
    (root / 'CMakeLists.txt').write_text('project(DPopCleaner VERSION 0.3.3 LANGUAGES CXX)\n', encoding='utf-8')
    (root / 'Version.h').write_text(
        'inline constexpr wchar_t kVersion[] = L"0.3.3";\n'
        'inline constexpr wchar_t kDisplayVersion[] = L"0.3.3 BETA R1";\n'
        'inline constexpr int kVersionCode = 3031;\n'
        'inline constexpr int kRevision = 1;\n', encoding='utf-8')
    (root / 'version.rc.in').write_text(
        'FILEVERSION 0,3,3,1\nPRODUCTVERSION 0,3,3,1\n'
        'VALUE "FileVersion", "0.3.3.1\\0"\n'
        'VALUE "ProductVersion", "0.3.3 BETA R1\\0"\n', encoding='utf-8')


class VersionTransformTests(unittest.TestCase):
    def test_transform_rewrites_033_identity_to_034_r2(self):
        mod = load_module()
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            make_donor(root)
            (root / 'Shell.cpp').write_text('auto v=L"DPopCleaner 0.3.3 BETA R1";\n', encoding='utf-8')
            report = mod.transform_v034_overlay(root)
            self.assertEqual(report['display_version'], '0.3.4 BETA R2')
            self.assertEqual(report['version_code'], '3042')
            self.assertEqual(report['revision'], '2')
            header = (root / 'Version.h').read_text(encoding='utf-8')
            self.assertIn('L"0.3.4 BETA R2"', header)
            self.assertIn('kVersionCode = 3042', header)
            self.assertIn('kRevision = 2', header)
            resource = (root / 'version.rc.in').read_text(encoding='utf-8')
            self.assertIn('FILEVERSION 0,3,4,2', resource)
            self.assertIn('0.3.4.2\\0', resource)
            self.assertIn('DPopCleaner 0.3.4 BETA R2', (root / 'Shell.cpp').read_text(encoding='utf-8'))

    def test_transform_fails_closed_when_donor_drifted(self):
        mod = load_module()
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            (root / 'CMakeLists.txt').write_text('project(DPopCleaner VERSION 9.9.9)\n', encoding='utf-8')
            (root / 'Version.h').write_text('bad\n', encoding='utf-8')
            (root / 'version.rc.in').write_text('bad\n', encoding='utf-8')
            with self.assertRaises(ValueError):
                mod.transform_v034_overlay(root)


class OverlaySafetyTests(unittest.TestCase):
    def test_overlay_only_writes_relative_files(self):
        mod = load_module()
        with tempfile.TemporaryDirectory() as td:
            base = Path(td); overlay = base / 'overlay'; target = base / 'target'
            overlay.mkdir(); target.mkdir()
            (overlay / 'x.txt').write_text('new', encoding='utf-8')
            (target / 'keep.txt').write_text('keep', encoding='utf-8')
            self.assertEqual(mod.apply_overlay(overlay, target), ['x.txt'])
            self.assertEqual((target / 'x.txt').read_text(encoding='utf-8'), 'new')
            self.assertEqual((target / 'keep.txt').read_text(encoding='utf-8'), 'keep')

    def test_overlay_rejects_symlinks(self):
        mod = load_module()
        with tempfile.TemporaryDirectory() as td:
            base = Path(td); overlay = base / 'overlay'; target = base / 'target'
            overlay.mkdir(); target.mkdir(); outside = base / 'outside'; outside.write_text('x', encoding='utf-8')
            link = overlay / 'link'
            try:
                link.symlink_to(outside)
            except (OSError, NotImplementedError):
                self.skipTest('symlinks unavailable')
            with self.assertRaises(ValueError):
                mod.apply_overlay(overlay, target)


class DonorIsolationTests(unittest.TestCase):
    def test_prepare_copies_donor_without_mutating_v033(self):
        mod = load_module()
        with tempfile.TemporaryDirectory() as td:
            base = Path(td); donor = base / 'v033'; overlay = base / 'overlay'; target = base / 'v034'
            donor.mkdir(); overlay.mkdir(); make_donor(donor)
            report = mod.prepare_v034_from_donor(donor, overlay, target)
            self.assertEqual(report['version']['display_version'], '0.3.4 BETA R2')
            self.assertIn('0.3.3 BETA R1', (donor / 'Version.h').read_text(encoding='utf-8'))
            self.assertIn('0.3.4 BETA R2', (target / 'Version.h').read_text(encoding='utf-8'))


class DonorEntrypointTests(unittest.TestCase):
    def test_repaired_033_entrypoint_is_used(self):
        mod = load_module(); calls = []
        repaired = SimpleNamespace(migrate=lambda *a, **k: calls.append((a, k)) or {'fixed': True})
        old = sys.modules.get('dpop033_migrate')
        sys.modules['dpop033_migrate'] = repaired
        try:
            result = mod._run_donor_migration(Path('repo'), Path('out'), Path('work'))
        finally:
            if old is None: sys.modules.pop('dpop033_migrate', None)
            else: sys.modules['dpop033_migrate'] = old
        self.assertEqual(result, {'fixed': True})
        self.assertEqual(len(calls), 1)


class CliContractTests(unittest.TestCase):
    def test_parser_supports_no_build(self):
        mod = load_module()
        args = mod.build_parser().parse_args(['--repository','repo','--output','out','--workspace','work','--no-build'])
        self.assertTrue(args.no_build)
        self.assertFalse(args.build)


if __name__ == '__main__':
    unittest.main()
