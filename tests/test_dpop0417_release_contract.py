from pathlib import Path
import base64
import hashlib
import json
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCREENSHOT_SHA256 = 'ad8dd8dfd5d07312d9ff588f2afcae6d655e1a84cb64e17cb1666dc22dd7a572'
SCREENSHOT_SIZE = 74050
SCREENSHOT_PATH = 'assets/dpopcleaner-current-settings.png'


class DPop0417ReleaseContractTests(unittest.TestCase):
    def test_exact_user_screenshot_can_be_reconstructed_before_every_pages_deploy(self):
        source_dir = ROOT / 'assets/current-settings-source'
        parts = sorted(source_dir.glob('part*.b64'))
        self.assertEqual([part.name for part in parts], [f'part{i:02d}.b64' for i in range(13)])
        encoded = ''.join(part.read_text(encoding='ascii').strip() for part in parts)
        payload = base64.b64decode(encoded, validate=True)
        self.assertEqual(len(payload), SCREENSHOT_SIZE)
        self.assertEqual(hashlib.sha256(payload).hexdigest(), SCREENSHOT_SHA256)

        materializer = ROOT / 'tools/dpop0417_materialize_current_screenshot.ps1'
        self.assertTrue(materializer.is_file(), 'exact screenshot materializer is required')
        materializer_text = materializer.read_text(encoding='utf-8').lower()
        self.assertIn('current-settings-source', materializer_text)
        self.assertIn('dpopcleaner-current-settings.png', materializer_text)
        self.assertIn(SCREENSHOT_SHA256, materializer_text)
        self.assertIn(str(SCREENSHOT_SIZE), materializer_text)

        foundation_text = (ROOT / '.github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml').read_text(encoding='utf-8').lower()
        publisher_text = (ROOT / '.github/workflows/publish-dpopcleaner-0.4.17.yml').read_text(encoding='utf-8').lower()
        static_text = (ROOT / '.github/workflows/static.yml').read_text(encoding='utf-8').lower()
        stage_site_text = (ROOT / 'scripts/Stage-Site.ps1').read_text(encoding='utf-8').lower()
        materialize_token = 'dpop0417_materialize_current_screenshot.ps1'
        self.assertIn(materialize_token, foundation_text)
        self.assertIn(materialize_token, publisher_text)
        self.assertIn(materialize_token, static_text)
        self.assertIn(SCREENSHOT_PATH, stage_site_text)
        self.assertIn('assets/dpopcleaner-0.4.17-disk.png', stage_site_text)
        self.assertIn('assets/dpopcleaner-0.4.17-restore.png', stage_site_text)

    def test_site_manifest_and_publisher_are_one_stable_0417_rev6_release(self):
        publisher = ROOT / '.github/workflows/publish-dpopcleaner-0.4.17.yml'
        notes = ROOT / 'release/RELEASE_NOTES_0.4.17.md'
        stable_manifest = ROOT / 'update/stable.json'
        version = json.loads((ROOT / 'version.json').read_text(encoding='utf-8'))
        self.assertEqual(version['revision'], 6)
        stable = json.loads(stable_manifest.read_text(encoding='utf-8'))
        self.assertEqual(stable['revision'], 6)
        manifest = (ROOT / 'release-manifest.js').read_text(encoding='utf-8').lower()
        self.assertIn("number(m.revision) === 6", manifest)
        self.assertIn('v0\\.4\\.17-rev6', manifest)
        program = (ROOT / 'v0417/src/SimpleUpdate/Program.cs').read_text(encoding='utf-8').lower()
        launcher = (ROOT / 'v0417/src/SimpleUpdate/LauncherContext.cs').read_text(encoding='utf-8').lower()
        self.assertIn('currentrevision = 6', program)
        self.assertIn('dpopcleaner.core.exe', program)
        self.assertIn('dpopcleaner-simpleupdate/0.4.17-rev6', launcher)
        index = (ROOT / 'index.html').read_text(encoding='utf-8').lower()
        self.assertIn('0.4.17 rev.6', index)
        self.assertIn('flowseal zapret 1.10.2', index)
        self.assertIn(SCREENSHOT_PATH, index)
        self.assertIn('zapret\\service.bat', index)
        self.assertIn('dpopcleaner.core.exe', index)
        self.assertIn('v0.2.11 beta', index)
        self.assertIn('автообнов', index)
        notes_text = notes.read_text(encoding='utf-8').lower()
        self.assertIn('revision 6', notes_text)
        self.assertIn('flowseal zapret 1.10.2', notes_text)
        self.assertIn('dpopcleaner.core.exe', notes_text)
        self.assertIn('dpopcleaner.exe', notes_text)
        self.assertIn('v0.2.11 beta', notes_text)
        self.assertIn('автообнов', notes_text)
        self.assertIn('zapret\\service.bat', notes_text)
        self.assertIn('zapret\\general.bat', notes_text)
        self.assertIn('zapret\\bin\\winws.exe', notes_text)
        workflow = publisher.read_text(encoding='utf-8').lower()
        for token in ('release_tag: v0.4.17-rev6','dpop0417_prepare_zapret.ps1','dpop0417_installed_settings_smoke.ps1',SCREENSHOT_PATH,'revision=6','actions/deploy-pages','sha256'):
            self.assertIn(token, workflow)
        self.assertIn('$live.revision -ne 6', workflow)
        self.assertIn('v0\\.4\\.17-rev6', workflow)


if __name__ == '__main__':
    unittest.main()
