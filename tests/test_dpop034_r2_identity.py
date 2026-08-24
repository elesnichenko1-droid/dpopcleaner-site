from __future__ import annotations

import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / 'tools' / 'dpop034_core.py'
SHELL = ROOT / 'v034_overlay' / 'ui' / 'Shell.h'
SHELL_TEST = ROOT / 'v034_overlay' / 'tests' / 'ShellContractTests.cpp'
INSTALLER = ROOT / 'release' / 'DPopCleaner_0.3.4_R2.iss'
WORKFLOW = ROOT / '.github' / 'workflows' / 'publish-dpopcleaner-0.3.4.yml'
MANIFEST = ROOT / 'update' / 'beta.json'
SITE_MANIFEST = ROOT / 'release-manifest.js'
INDEX = ROOT / 'index.html'


class R2IdentityTests(unittest.TestCase):
    def test_all_release_surfaces_agree_on_r2(self):
        for path in (CORE, SHELL, SHELL_TEST, INSTALLER, WORKFLOW, MANIFEST, SITE_MANIFEST, INDEX):
            self.assertTrue(path.is_file(), f'missing R2 release surface: {path}')

        core = CORE.read_text(encoding='utf-8')
        shell = SHELL.read_text(encoding='utf-8')
        shell_test = SHELL_TEST.read_text(encoding='utf-8')
        installer = INSTALLER.read_text(encoding='utf-8')
        workflow = WORKFLOW.read_text(encoding='utf-8')
        site_manifest = SITE_MANIFEST.read_text(encoding='utf-8')
        index = INDEX.read_text(encoding='utf-8')
        manifest = json.loads(MANIFEST.read_text(encoding='utf-8-sig'))

        for marker in (
            "TARGET_DISPLAY_VERSION = '0.3.4 BETA R2'",
            "TARGET_VERSION_CODE = '3042'",
            "TARGET_REVISION = '2'",
            "TARGET_RESOURCE_VERSION = '0.3.4.2'",
        ):
            self.assertIn(marker, core)

        self.assertIn('L"DPopCleaner 0.3.4 BETA R2"', shell)
        self.assertNotIn('L"DPopCleaner 0.3.4 BETA R1"', shell)
        self.assertIn('identity.windowTitle == L"DPopCleaner 0.3.4 BETA R2"', shell_test)

        for marker in (
            '#define MyAppVersion "0.3.4 BETA R2"',
            'VersionInfoVersion=0.3.4.2',
            'OutputBaseFilename=DPopCleaner_Setup_0.3.4_BETA_R2',
        ):
            self.assertIn(marker, installer)

        for marker in (
            'Build, release and deploy DPopCleaner 0.3.4 R2',
            'RELEASE_TAG: v0.3.4-beta-r2',
            'RELEASE_ASSET: DPopCleaner_Setup_0.3.4_BETA_R2.exe',
            'version_code = 3042',
            'revision = 2',
        ):
            self.assertIn(marker, workflow)

        self.assertEqual(manifest['version'], '0.3.4')
        self.assertEqual(manifest['version_code'], 3042)
        self.assertEqual(manifest['revision'], 2)
        self.assertFalse(manifest['available'])

        for marker in ('v0\\.3\\.4-beta-r2', '3042', 'revision)===2'):
            self.assertIn(marker, site_manifest)

        self.assertIn('0.3.4 BETA R2', index)
        self.assertNotIn('0.3.4 BETA R1', index)


if __name__ == '__main__':
    unittest.main()
