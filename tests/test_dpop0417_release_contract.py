from pathlib import Path
import json
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417ReleaseContractTests(unittest.TestCase):
    def test_site_manifest_and_publisher_are_one_stable_0417_release(self):
        publisher = ROOT / '.github/workflows/publish-dpopcleaner-0.4.17.yml'
        notes = ROOT / 'release/RELEASE_NOTES_0.4.17.md'
        disk_png = ROOT / 'assets/dpopcleaner-0.4.17-disk.png'
        restore_png = ROOT / 'assets/dpopcleaner-0.4.17-restore.png'
        stable_manifest = ROOT / 'update/stable.json'
        self.assertTrue(publisher.is_file(), '0.4.17 publisher workflow is required')
        self.assertTrue(notes.is_file(), '0.4.17 release notes are required')
        self.assertTrue(disk_png.is_file(), 'real Disk Analyzer screenshot is required')
        self.assertTrue(restore_png.is_file(), 'real Restore Center screenshot is required')
        self.assertTrue(stable_manifest.is_file(), 'stable update manifest is required')

        version = json.loads((ROOT / 'version.json').read_text(encoding='utf-8'))
        self.assertEqual(version['version'], '0.4.17')
        self.assertEqual(version['version_code'], 417)
        self.assertEqual(version['revision'], 1)
        self.assertEqual(version['channel'], 'stable')
        self.assertEqual(version['manifest'], './update/stable.json')

        stable = json.loads(stable_manifest.read_text(encoding='utf-8'))
        self.assertEqual(stable['version'], '0.4.17')
        self.assertEqual(stable['version_code'], 417)
        self.assertEqual(stable['revision'], 1)
        self.assertEqual(stable['channel'], 'stable')

        manifest = (ROOT / 'release-manifest.js').read_text(encoding='utf-8').lower()
        self.assertIn("m.version === '0.4.17'", manifest)
        self.assertIn("m.channel === 'stable'", manifest)
        self.assertIn('v0\\.4\\.17', manifest)
        self.assertIn('dpopcleaner_setup_0\\.4\\.17\\.exe', manifest)

        index = (ROOT / 'index.html').read_text(encoding='utf-8').lower()
        self.assertIn('dpopcleaner 0.4.17', index)
        self.assertIn('assets/dpopcleaner-0.4.17-disk.png', index)
        self.assertIn('assets/dpopcleaner-0.4.17-restore.png', index)
        self.assertIn('ядро 0.2.14', index)
        self.assertIn('центр восстановления', index)
        self.assertIn('анализатор диска', index)
        self.assertNotIn('beta', index)

        script = (ROOT / 'script.js').read_text(encoding='utf-8').lower()
        self.assertIn('./update/stable.json', script)
        self.assertNotIn('beta', script)

        notes_text = notes.read_text(encoding='utf-8').lower()
        self.assertNotIn('beta', notes_text)

        workflow = publisher.read_text(encoding='utf-8').lower()
        for token in (
            'v0.4.17',
            'dpopcleaner_setup_0.4.17.exe',
            'dpop0417_stage.ps1 -requirecompanions',
            'dpop0417_install_smoke.ps1',
            'test_dpop0417_release_contract.py',
            'get-filehash',
            'gh release upload',
            'update/stable.json',
            'actions/deploy-pages',
            'invoke-restmethod',
            'sha256',
        ):
            self.assertIn(token, workflow)
        self.assertNotIn('--prerelease', workflow)
        self.assertNotIn('update/beta.json', workflow)
        self.assertNotIn('0.4.17 beta', workflow)

        old_clean = (ROOT / '.github/workflows/build-clean-0.2.14-r1.yml').read_text(encoding='utf-8').lower()
        static = (ROOT / '.github/workflows/static.yml').read_text(encoding='utf-8').lower()
        self.assertNotIn('  push:', old_clean, 'old 0.2.14 publisher must be manual-only')
        self.assertNotIn('  push:', static, 'generic Pages deploy must be manual-only during 0.4.17 publication')


if __name__ == '__main__':
    unittest.main()
