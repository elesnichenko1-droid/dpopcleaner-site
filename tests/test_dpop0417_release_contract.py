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
        self.assertTrue(materializer.is_file())
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

    def test_site_manifest_and_publisher_are_one_stable_0417_rev9_release(self):
        publisher = ROOT / '.github/workflows/publish-dpopcleaner-0.4.17.yml'
        notes = ROOT / 'release/RELEASE_NOTES_0.4.17.md'
        stable_manifest = ROOT / 'update/stable.json'
        version = json.loads((ROOT / 'version.json').read_text(encoding='utf-8'))
        self.assertEqual(version['revision'], 9)
        stable = json.loads(stable_manifest.read_text(encoding='utf-8'))
        self.assertEqual(stable['revision'], 9)
        manifest = (ROOT / 'release-manifest.js').read_text(encoding='utf-8').lower()
        self.assertIn("number(m.revision) === 9", manifest)
        self.assertIn('v0\\.4\\.17-rev9', manifest)
        program = (ROOT / 'v0417/src/SimpleUpdate/Program.cs').read_text(encoding='utf-8').lower()
        zapret_host = (ROOT / 'v0417/src/SimpleUpdate/ZapretEnhancementHost.cs').read_text(encoding='utf-8').lower()
        self.assertIn('currentrevision = 9', program)
        self.assertIn('dpopcleaner.core.exe', program)
        self.assertIn('dpopupdate.exe', program)
        self.assertIn('createlegacyupdateproxy', zapret_host)
        self.assertIn('legacydownloadbuttonid', zapret_host)

        index = (ROOT / 'index.html').read_text(encoding='utf-8').lower()
        for token in ('flowseal zapret 1.10.2', SCREENSHOT_PATH, 'dpopcleaner.core.exe', '5–95', 'починка трансляции', 'починка подключения', 'игровой фильтр 1.10.2', 'менеджер 1.10.2', 'автообновление приложения', 'прокрут'):
            self.assertIn(token, index)
        self.assertTrue('прежний интерфейс' in index or 'интерфейс сохран' in index)

        script = (ROOT / 'script.js').read_text(encoding='utf-8').lower()
        self.assertIn('m.revision', script)
        self.assertIn('applyrevision', script)
        self.assertIn('rev.${revision}', script)

        notes_text = notes.read_text(encoding='utf-8').lower()
        for token in ('revision 9', 'flowseal zapret 1.10.2', 'dpopcleaner.core.exe', 'dpopupdate.exe', 'модуль обновления zapret не найден', 'проверить версию', 'скачать и установить', '1.9.9d', 'компакт', '5–95', 'починка трансляции', 'починка подключения', 'игровой фильтр 1.10.2', 'менеджер 1.10.2'):
            self.assertIn(token, notes_text)
        self.assertTrue('прежний интерфейс' in notes_text or 'интерфейс сохран' in notes_text)

        workflow = publisher.read_text(encoding='utf-8').lower()
        for token in ('release_tag: v0.4.17-rev9', 'dpop0417_prepare_zapret.ps1', 'dpop0417_installed_settings_smoke.ps1', 'dpop0417_rev7_installed_ui_smoke.ps1', 'dpop0417_rev9_zapret_update_smoke.ps1', SCREENSHOT_PATH, 'revision=9', 'actions/deploy-pages', 'sha256', 'dpopcleaner-0.4.17-rev9-release-candidate'):
            self.assertIn(token, workflow)
        self.assertIn('$live.revision -ne 9', workflow)
        self.assertIn('v0\\.4\\.17-rev9', workflow)


if __name__ == '__main__':
    unittest.main()
