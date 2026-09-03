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

        materializer = (ROOT / 'tools/dpop0417_materialize_current_screenshot.ps1').read_text(encoding='utf-8').lower()
        self.assertIn('current-settings-source', materializer)
        self.assertIn('dpopcleaner-current-settings.png', materializer)
        self.assertIn(SCREENSHOT_SHA256, materializer)
        self.assertIn(str(SCREENSHOT_SIZE), materializer)

        for path in (
            '.github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml',
            '.github/workflows/publish-dpopcleaner-0.4.17.yml',
            '.github/workflows/static.yml',
        ):
            self.assertIn('dpop0417_materialize_current_screenshot.ps1', (ROOT / path).read_text(encoding='utf-8').lower())

        stage_site = (ROOT / 'scripts/Stage-Site.ps1').read_text(encoding='utf-8').lower()
        self.assertIn(SCREENSHOT_PATH, stage_site)
        self.assertIn('assets/dpopcleaner-0.4.17-disk.png', stage_site)
        self.assertIn('assets/dpopcleaner-0.4.17-restore.png', stage_site)

    def test_site_manifest_and_publisher_are_one_stable_0417_rev17_release(self):
        version = json.loads((ROOT / 'version.json').read_text(encoding='utf-8'))
        stable = json.loads((ROOT / 'update/stable.json').read_text(encoding='utf-8'))
        self.assertEqual(version['version'], '0.4.17')
        self.assertEqual(version['revision'], 17)
        self.assertEqual(stable['version'], '0.4.17')
        self.assertEqual(stable['revision'], 17)
        self.assertEqual(stable['channel'], 'stable')

        program = (ROOT / 'v0417/src/SimpleUpdate/Program.cs').read_text(encoding='utf-8').lower()
        self.assertIn('currentrevision = 17', program)
        self.assertIn('dpopcleaner.core.exe', program)
        self.assertIn('dpopupdate.exe', program)

        manifest = (ROOT / 'release-manifest.js').read_text(encoding='utf-8').lower()
        self.assertIn('number(m.revision) === 17', manifest)
        self.assertIn('v0\\.4\\.17-rev17', manifest)

        zapret_host = (ROOT / 'v0417/src/SimpleUpdate/ZapretEnhancementHost.cs').read_text(encoding='utf-8').lower()
        visual_host = (ROOT / 'v0417/src/SimpleUpdate/ZapretVisualPolishHost.cs').read_text(encoding='utf-8').lower()
        responsive_host = (ROOT / 'v0417/src/SimpleUpdate/ZapretResponsiveLayoutHost.cs').read_text(encoding='utf-8').lower()
        settings_host = (ROOT / 'v0417/src/SimpleUpdate/AdditionalSettingsHost.cs').read_text(encoding='utf-8').lower()
        launcher = (ROOT / 'v0417/src/SimpleUpdate/LauncherContext.cs').read_text(encoding='utf-8').lower()
        self.assertIn('createlegacyupdateproxy', zapret_host)
        self.assertIn('legacydownloadbuttonid', zapret_host)
        self.assertIn('bs_ownerdraw', visual_host)
        self.assertIn('responsivezapretbuttonids', responsive_host)
        self.assertIn('getclientrect', responsive_host)
        self.assertIn('layoutzapretrow', responsive_host)
        self.assertIn('begindeferwindowpos', settings_host)
        self.assertIn('redrawsettingshost', settings_host)
        self.assertIn('_settingshostbounds', launcher)
        self.assertIn('tryattachrestartedcore', launcher)
        self.assertIn('resetbridgeforrestartedcore', launcher)

        prepare = (ROOT / 'tools/dpop0417_prepare_zapret.ps1').read_text(encoding='utf-8').lower()
        stage = (ROOT / 'tools/dpop0417_stage.ps1').read_text(encoding='utf-8').lower()
        self.assertIn('utils/dpop_version.txt', prepare)
        self.assertIn('utils/dpop_version.txt', stage)

        index = (ROOT / 'index.html').read_text(encoding='utf-8').lower()
        for token in ('flowseal zapret 1.10.2', SCREENSHOT_PATH, '5–95', 'починка трансляции', 'починка подключения', 'игровой фильтр 1.10.2', 'менеджер 1.10.2', 'автообновление приложения', 'tray'):
            self.assertIn(token, index)
        self.assertTrue('программа остаётся узнаваемой' in index or 'интерфейс сохран' in index)

        script = (ROOT / 'script.js').read_text(encoding='utf-8').lower()
        self.assertIn('m.revision', script)
        self.assertIn('applyrevision', script)
        self.assertIn('rev.${revision}', script)

        notes = (ROOT / 'release/RELEASE_NOTES_0.4.17.md').read_text(encoding='utf-8').lower()
        for token in ('revision 17', 'responsive', '1680', 'flowseal zapret 1.10.2', 'dpopcleaner.core.exe', 'dpopupdate.exe', 'code 740', 'озу', 'смен', 'язык', 'tray'):
            self.assertIn(token, notes)

        workflow = (ROOT / '.github/workflows/publish-dpopcleaner-0.4.17.yml').read_text(encoding='utf-8').lower()
        for token in (
            'release_tag: v0.4.17-rev17',
            'dpop0417_prepare_zapret.ps1',
            'dpop0417_install_smoke.ps1',
            'dpop0417_rev15_installed_restart_smoke.ps1',
            'dpop0417_rev16_single_tray_smoke.ps1',
            'dpop0417_rev16_zapret_functional_smoke.ps1',
            'dpop0417_rev16_zapret_presentation_smoke.ps1',
            'dpop0417_rev9_zapret_update_smoke.ps1',
            'dpop0417_rev12_native_version_smoke.ps1',
            SCREENSHOT_PATH,
            'revision=17',
            'actions/deploy-pages',
            'sha256',
            'dpopcleaner-0.4.17-rev17-release-candidate',
        ):
            self.assertIn(token, workflow)
        self.assertIn('$live.revision -ne 17', workflow)
        self.assertIn('v0\\.4\\.17-rev17', workflow)


if __name__ == '__main__':
    unittest.main()
