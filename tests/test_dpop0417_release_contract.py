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

    def test_site_manifest_and_publisher_are_one_stable_0417_rev16_release(self):
        publisher = ROOT / '.github/workflows/publish-dpopcleaner-0.4.17.yml'
        notes = ROOT / 'release/RELEASE_NOTES_0.4.17.md'
        stable_manifest = ROOT / 'update/stable.json'
        version = json.loads((ROOT / 'version.json').read_text(encoding='utf-8'))
        self.assertEqual(version['revision'], 16)
        stable = json.loads(stable_manifest.read_text(encoding='utf-8'))
        self.assertEqual(stable['revision'], 16)
        manifest = (ROOT / 'release-manifest.js').read_text(encoding='utf-8').lower()
        self.assertIn("number(m.revision) === 16", manifest)
        self.assertIn('v0\\.4\\.17-rev16', manifest)

        program = (ROOT / 'v0417/src/SimpleUpdate/Program.cs').read_text(encoding='utf-8').lower()
        zapret_host = (ROOT / 'v0417/src/SimpleUpdate/ZapretEnhancementHost.cs').read_text(encoding='utf-8').lower()
        visual_host = (ROOT / 'v0417/src/SimpleUpdate/ZapretVisualPolishHost.cs').read_text(encoding='utf-8').lower()
        settings_host = (ROOT / 'v0417/src/SimpleUpdate/AdditionalSettingsHost.cs').read_text(encoding='utf-8').lower()
        launcher = (ROOT / 'v0417/src/SimpleUpdate/LauncherContext.cs').read_text(encoding='utf-8').lower()
        prepare = (ROOT / 'tools/dpop0417_prepare_zapret.ps1').read_text(encoding='utf-8').lower()
        stage = (ROOT / 'tools/dpop0417_stage.ps1').read_text(encoding='utf-8').lower()
        rev12_smoke = (ROOT / 'tools/dpop0417_rev12_native_version_smoke.ps1').read_text(encoding='utf-8').lower()
        installed_settings_smoke = (ROOT / 'tools/dpop0417_installed_settings_smoke.ps1').read_text(encoding='utf-8').lower()
        restart_smoke = (ROOT / 'tools/dpop0417_rev15_restart_recovery_smoke.ps1').read_text(encoding='utf-8').lower()
        installed_restart_smoke = (ROOT / 'tools/dpop0417_rev15_installed_restart_smoke.ps1').read_text(encoding='utf-8').lower()

        self.assertIn('currentrevision = 16', program)
        self.assertIn('dpopcleaner.core.exe', program)
        self.assertIn('dpopupdate.exe', program)
        self.assertIn('createlegacyupdateproxy', zapret_host)
        self.assertIn('legacydownloadbuttonid', zapret_host)
        self.assertNotIn('versionstatusproxyid', visual_host)
        self.assertNotIn('createversionstatusproxy', visual_host)
        self.assertNotIn('attachtoexistingversionstatus', visual_host)
        self.assertNotIn('rewriteversionstatustext', visual_host)
        self.assertNotIn('writewindowtext(_versionstatus', visual_host)
        self.assertIn('bs_ownerdraw', visual_host)
        self.assertIn('utils/dpop_version.txt', prepare)
        self.assertIn('utils/dpop_version.txt', stage)
        self.assertIn('rev12-zapret-native-version.png', rev12_smoke)
        self.assertIn('printwindow', rev12_smoke)
        self.assertIn('begindeferwindowpos', settings_host)
        self.assertIn('redrawsettingshost', settings_host)
        self.assertIn('_settingshostbounds', launcher)
        self.assertIn('dpopcleaner-simpleupdate/0.4.17-rev16', launcher)
        self.assertIn('tryattachrestartedcore', launcher)
        self.assertIn('resetbridgeforrestartedcore', launcher)
        self.assertIn('installed_settings_language_switch_smoke_ok', installed_settings_smoke)
        self.assertIn('rev15_language_restart_bridge_smoke_ok', restart_smoke)
        self.assertIn('rev15_language_restart_ram_tray_smoke_ok', restart_smoke)
        self.assertIn('rev15_installed_language_restart_bridge_smoke_ok', installed_restart_smoke)
        self.assertIn('rev15_installed_language_restart_ram_tray_smoke_ok', installed_restart_smoke)

        index = (ROOT / 'index.html').read_text(encoding='utf-8').lower()
        for token in ('flowseal zapret 1.10.2', SCREENSHOT_PATH, 'dpopcleaner.core.exe', '5–95', 'починка трансляции', 'починка подключения', 'игровой фильтр 1.10.2', 'менеджер 1.10.2', 'автообновление приложения', 'rev.16', '1.9.9d', 'utils\\dpop_version.txt', 'родн', 'перерис', 'смен', 'язык', 'tray'):
            self.assertIn(token, index)
        self.assertTrue('прежний интерфейс' in index or 'интерфейс сохран' in index or 'программа остаётся узнаваемой' in index)
        self.assertIn('не переписывает родную версию через hwnd', index)

        script = (ROOT / 'script.js').read_text(encoding='utf-8').lower()
        self.assertIn('m.revision', script)
        self.assertIn('applyrevision', script)
        self.assertIn('rev.${revision}', script)

        notes_text = notes.read_text(encoding='utf-8').lower()
        for token in ('revision 16', 'flowseal zapret 1.10.2', 'dpopcleaner.core.exe', 'dpopupdate.exe', 'frozen-updater', 'utils\\dpop_version.txt', 'перерис', 'code 740', 'uac', 'shell_notifyicon', 'ghost-записи explorer', 'озу', 'смен', 'язык', 'settings', 'перезапуск', 'tray'):
            self.assertIn(token, notes_text)
        self.assertTrue('прежний интерфейс' in notes_text or 'интерфейс' in notes_text)
        self.assertIn('ghost-записи explorer', notes_text)

        workflow = publisher.read_text(encoding='utf-8').lower()
        for token in ('release_tag: v0.4.17-rev16', 'dpop0417_prepare_zapret.ps1', 'dpop0417_install_smoke.ps1', 'dpop0417_rev15_installed_restart_smoke.ps1', 'dpop0417_rev9_zapret_update_smoke.ps1', 'dpop0417_rev12_native_version_smoke.ps1', SCREENSHOT_PATH, 'revision=16', 'actions/deploy-pages', 'sha256', 'dpopcleaner-0.4.17-rev16-release-candidate'):
            self.assertIn(token, workflow)
        self.assertIn('$live.revision -ne 16', workflow)
        self.assertIn('v0\\.4\\.17-rev16', workflow)


if __name__ == '__main__':
    unittest.main()
