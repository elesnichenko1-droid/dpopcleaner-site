from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / 'tools' / 'dpop034_migrate.py'


def load_module():
    if not MODULE_PATH.is_file():
        raise FileNotFoundError(f'production module missing: {MODULE_PATH}')
    spec = importlib.util.spec_from_file_location('dpop034_migrate', MODULE_PATH)
    module = importlib.util.module_from_spec(spec)
    assert spec and spec.loader
    spec.loader.exec_module(module)
    return module


class VersionTransformTests(unittest.TestCase):
    def test_transform_rewrites_exact_033_identity_to_034(self):
        mod = load_module()
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            (root / 'CMakeLists.txt').write_text(
                'project(DPopCleaner VERSION 0.3.3 LANGUAGES CXX)\n', encoding='utf-8'
            )
            (root / 'Version.h').write_text(
                '#pragma once\n'
                'inline constexpr wchar_t kVersion[] = L"0.3.3";\n'
                'inline constexpr wchar_t kDisplayVersion[] = L"0.3.3 BETA R1";\n'
                'inline constexpr int kVersionCode = 3031;\n'
                'inline constexpr int kRevision = 1;\n',
                encoding='utf-8',
            )
            (root / 'version.rc.in').write_text(
                'FILEVERSION 0,3,3,1\n'
                'PRODUCTVERSION 0,3,3,1\n'
                'VALUE "FileVersion", "0.3.3.1\\0"\n'
                'VALUE "ProductVersion", "0.3.3 BETA R1\\0"\n',
                encoding='utf-8',
            )
            (root / 'Shell.cpp').write_text(
                'auto a=L"DPopCleaner 0.3.3 BETA R1";\n'
                'auto b=L"v0.3.3 BETA";\n'
                'auto internal=L"DPopCleaner033ShellWindow";\n',
                encoding='utf-8',
            )

            report = mod.transform_v034_overlay(root)

            self.assertEqual(report['version'], '0.3.4')
            self.assertEqual(report['version_code'], '3041')
            self.assertIn('VERSION 0.3.4', (root / 'CMakeLists.txt').read_text(encoding='utf-8'))
            header = (root / 'Version.h').read_text(encoding='utf-8')
            self.assertIn('L"0.3.4"', header)
            self.assertIn('L"0.3.4 BETA R1"', header)
            self.assertIn('kVersionCode = 3041', header)
            resource = (root / 'version.rc.in').read_text(encoding='utf-8')
            self.assertIn('FILEVERSION 0,3,4,1', resource)
            shell = (root / 'Shell.cpp').read_text(encoding='utf-8')
            self.assertIn('DPopCleaner 0.3.4 BETA R1', shell)
            self.assertIn('v0.3.4 BETA', shell)
            self.assertIn('DPopCleaner033ShellWindow', shell)

    def test_transform_fails_closed_if_donor_identity_drifted(self):
        mod = load_module()
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            (root / 'CMakeLists.txt').write_text('project(DPopCleaner VERSION 9.9.9)\n', encoding='utf-8')
            (root / 'Version.h').write_text('bad\n', encoding='utf-8')
            (root / 'version.rc.in').write_text('bad\n', encoding='utf-8')
            with self.assertRaises(ValueError):
                mod.transform_v034_overlay(root)


class OverlaySafetyTests(unittest.TestCase):
    def test_overlay_replaces_only_relative_files_under_target(self):
        mod = load_module()
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            overlay = base / 'overlay'
            target = base / 'target'
            overlay.joinpath('ui').mkdir(parents=True)
            target.joinpath('ui').mkdir(parents=True)
            overlay.joinpath('ui', 'PageLayout.h').write_text('new', encoding='utf-8')
            target.joinpath('ui', 'PageLayout.h').write_text('old', encoding='utf-8')
            target.joinpath('keep.txt').write_text('keep', encoding='utf-8')

            changed = mod.apply_overlay(overlay, target)

            self.assertEqual(changed, ['ui/PageLayout.h'])
            self.assertEqual((target / 'ui' / 'PageLayout.h').read_text(encoding='utf-8'), 'new')
            self.assertEqual((target / 'keep.txt').read_text(encoding='utf-8'), 'keep')

    def test_overlay_rejects_symlinks(self):
        mod = load_module()
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            overlay = base / 'overlay'
            target = base / 'target'
            overlay.mkdir(); target.mkdir()
            outside = base / 'outside.txt'; outside.write_text('secret', encoding='utf-8')
            link = overlay / 'escape.txt'
            try:
                link.symlink_to(outside)
            except (OSError, NotImplementedError):
                self.skipTest('symlink creation unavailable')
            with self.assertRaises(ValueError):
                mod.apply_overlay(overlay, target)


class DonorIsolationTests(unittest.TestCase):
    def test_prepare_v034_copies_donor_without_mutating_v033(self):
        mod = load_module()
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            donor = base / 'v033'
            overlay = base / 'overlay'
            target = base / 'v034'
            donor.mkdir(); overlay.joinpath('ui').mkdir(parents=True)
            donor.joinpath('CMakeLists.txt').write_text('project(DPopCleaner VERSION 0.3.3 LANGUAGES CXX)\n', encoding='utf-8')
            donor.joinpath('Version.h').write_text(
                'inline constexpr wchar_t kVersion[] = L"0.3.3";\n'
                'inline constexpr wchar_t kDisplayVersion[] = L"0.3.3 BETA R1";\n'
                'inline constexpr int kVersionCode = 3031;\n'
                'inline constexpr int kRevision = 1;\n', encoding='utf-8')
            donor.joinpath('version.rc.in').write_text(
                'FILEVERSION 0,3,3,1\nPRODUCTVERSION 0,3,3,1\n'
                'VALUE "FileVersion", "0.3.3.1\\0"\n'
                'VALUE "ProductVersion", "0.3.3 BETA R1\\0"\n', encoding='utf-8')
            donor.joinpath('keep.cpp').write_text('auto v=L"DPopCleaner 0.3.3";\n', encoding='utf-8')
            donor.joinpath('ui').mkdir()
            donor.joinpath('ui','WorkspacePage.cpp').write_text(
                '#include "ui/Theme.h"\n'
                'void WorkspacePage::LayoutChildren() noexcept {\n'
                '  const int margin = 18; const int buttonHeight = 38;\n'
                '  MoveWindow(status_, margin, 52, 10, 30, TRUE);\n'
                '  MoveWindow(list_, margin, 86, 10, 80, TRUE);\n'
                '}\n', encoding='utf-8')
            overlay.joinpath('ui','PageLayout.h').write_text('layout', encoding='utf-8')
            overlay.joinpath('ui','Panel.cpp').write_text('auto v=L"DPopCleaner 0.3.3 BETA R1";\n', encoding='utf-8')

            report = mod.prepare_v034_from_donor(donor, overlay, target)

            self.assertEqual(report['version']['version'], '0.3.4')
            self.assertEqual(report['overlay_files'], ['ui/PageLayout.h', 'ui/Panel.cpp'])
            self.assertIn('0.3.3', donor.joinpath('Version.h').read_text(encoding='utf-8'))
            self.assertIn('0.3.4', target.joinpath('Version.h').read_text(encoding='utf-8'))
            self.assertTrue(target.joinpath('ui','PageLayout.h').is_file())
            self.assertIn('0.3.4 BETA R1', target.joinpath('ui','Panel.cpp').read_text(encoding='utf-8'))
            self.assertIn('ComputePageRegions', target.joinpath('ui','WorkspacePage.cpp').read_text(encoding='utf-8'))


class OrchestratorTests(unittest.TestCase):
    def test_migrate_034_exports_v034_from_recovered_donor(self):
        mod = load_module()
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            repo = base / 'repo'; output = base / 'out'; workspace = base / 'work'
            repo.mkdir(); repo.joinpath('v034_overlay').mkdir()

            def fake_donor(repository, donor_output, donor_workspace):
                donor = donor_workspace / 'repo-stage' / 'v033'
                donor.mkdir(parents=True)
                donor.joinpath('CMakeLists.txt').write_text('project(DPopCleaner VERSION 0.3.3 LANGUAGES CXX)\n', encoding='utf-8')
                donor.joinpath('Version.h').write_text(
                    'inline constexpr wchar_t kVersion[] = L"0.3.3";\n'
                    'inline constexpr wchar_t kDisplayVersion[] = L"0.3.3 BETA R1";\n'
                    'inline constexpr int kVersionCode = 3031;\n'
                    'inline constexpr int kRevision = 1;\n', encoding='utf-8')
                donor.joinpath('version.rc.in').write_text(
                    'FILEVERSION 0,3,3,1\nPRODUCTVERSION 0,3,3,1\n'
                    'VALUE "FileVersion", "0.3.3.1\\0"\n'
                    'VALUE "ProductVersion", "0.3.3 BETA R1\\0"\n', encoding='utf-8')
                return {'target_version': '0.3.3'}

            mod._run_donor_migration = fake_donor
            report = mod.migrate_034(repo, output, workspace, build=False)

            exported = output / 'source-overlay' / 'v034'
            self.assertTrue(exported.is_dir())
            self.assertIn('0.3.4', exported.joinpath('Version.h').read_text(encoding='utf-8'))
            self.assertEqual(report['target_version'], '0.3.4')
            self.assertEqual(report['donor']['target_version'], '0.3.3')


class CliContractTests(unittest.TestCase):
    def test_parser_supports_repository_output_workspace_and_no_build(self):
        mod = load_module()
        args = mod.build_parser().parse_args([
            '--repository', 'repo', '--output', 'out', '--workspace', 'work', '--no-build'
        ])
        self.assertEqual(args.repository, Path('repo'))
        self.assertEqual(args.output, Path('out'))
        self.assertEqual(args.workspace, Path('work'))
        self.assertTrue(args.no_build)
        self.assertFalse(args.build)


if __name__ == '__main__':
    unittest.main()
