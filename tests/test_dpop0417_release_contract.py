from pathlib import Path
import hashlib
import json
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCREENSHOT_SHA256 = '9a98fc34d1442106f4d11037a5c4737c56c4f7d7e9407984d870655f56c5e078'
SCREENSHOT_SIZE = 16830
SCREENSHOT_PATH = 'assets/dpopcleaner-current-settings.webp'


class DPop0417ReleaseContractTests(unittest.TestCase):
    def test_exact_user_screenshot_is_bundled_for_every_pages_deploy(self):
        current_webp = ROOT / SCREENSHOT_PATH
        self.assertTrue(current_webp.is_file(), 'the exact current-program screenshot is required')
        self.assertEqual(current_webp.stat().st_size, SCREENSHOT_SIZE)
        self.assertEqual(hashlib.sha256(current_webp.read_bytes()).hexdigest(), SCREENSHOT_SHA256)

        publisher_text = (ROOT / '.github/workflows/publish-dpopcleaner-0.4.17.yml').read_text(encoding='utf-8').lower()
        static_text = (ROOT / '.github/workflows/static.yml').read_text(encoding='utf-8').lower()
        stage_site_text = (ROOT / 'scripts/Stage-Site.ps1').read_text(encoding='utf-8').lower()

        self.assertIn(SCREENSHOT_PATH, publisher_text)
        self.assertIn(SCREENSHOT_SHA256, publisher_text)
        self.assertIn('stage-site.ps1', static_text)
        self.assertIn(SCREENSHOT_PATH, stage_site_text)
        self.assertIn('assets/dpopcleaner-0.4.17-disk.png', stage_site_text)
        self.assertIn('assets/dpopcleaner-0.4.17-restore.png', stage_site_text)

    def test_site_manifest_and_publisher_are_one_stable_0417_rev5_release(self):
        publisher = ROOT / '.github/workflows/publish-dpopcleaner-0.4.17.yml'
        notes = ROOT / 'release/RELEASE_NOTES_0.4.17.md'
        stable_manifest = ROOT / 'update/stable.json'
        version = json.loads((ROOT / 'version.json').read_text(encoding='utf-8'))
        self.assertEqual(version['revision'], 5)
        stable = json.loads(stable_manifest.read_text(encoding='utf-8'))
        self.assertEqual(stable['revision'], 5)
        manifest = (ROOT / 'release-manifest.js').read_text(encoding='utf-8').lower()
        self.assertIn("number(m.revision) === 5", manifest)
        self.assertIn('v0\\.4\\.17-rev5', manifest)
        program = (ROOT / 'v0417/src/SimpleUpdate/Program.cs').read_text(encoding='utf-8').lower()
        launcher = (ROOT / 'v0417/src/SimpleUpdate/LauncherContext.cs').read_text(encoding='utf-8').lower()
        self.assertIn('currentrevision = 5', program)
        self.assertIn('dpopcleaner-simpleupdate/0.4.17-rev5', launcher)
        index = (ROOT / 'index.html').read_text(encoding='utf-8').lower()
        self.assertIn('0.4.17 rev.5', index)
        self.assertIn('flowseal zapret 1.10.2', index)
        self.assertIn(SCREENSHOT_PATH, index)
        notes_text = notes.read_text(encoding='utf-8').lower()
        self.assertIn('revision 5', notes_text)
        self.assertIn('flowseal zapret 1.10.2', notes_text)
        self.assertIn('service.bat', notes_text)
        self.assertIn('bin\\winws.exe', notes_text)
        workflow = publisher.read_text(encoding='utf-8').lower()
        for token in ('release_tag: v0.4.17-rev5','dpop0417_prepare_zapret.ps1','dpop0417_zapret_ui_smoke.ps1',SCREENSHOT_PATH,'revision=5','actions/deploy-pages','sha256'):
            self.assertIn(token, workflow)
        self.assertIn('$live.revision -ne 5', workflow)
        self.assertIn('v0\\.4\\.17-rev5', workflow)


if __name__ == '__main__':
    unittest.main()
