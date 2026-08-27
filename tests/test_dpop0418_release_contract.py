from pathlib import Path
import json
import unittest

ROOT = Path(__file__).resolve().parents[1]
PINNED_ZAPRET_SHA256 = "5eaac9fb2e4b1abd693487452a3ff3f4dfe9578a45f9ddddfa4bc1f5a6bb62d5"


class DPop0418ReleaseContractTests(unittest.TestCase):
    def test_site_manifest_and_publisher_form_one_stable_0418_release(self):
        publisher = ROOT / '.github/workflows/publish-dpopcleaner-0.4.18.yml'
        notes = ROOT / 'release/RELEASE_NOTES_0.4.18.md'
        stable_manifest = ROOT / 'update/stable.json'
        legacy_publisher = ROOT / '.github/workflows/publish-dpopcleaner-0.4.17.yml'
        legacy_hotfix = ROOT / '.github/workflows/deploy-rev2-site-hotfix.yml'
        legacy_foundation = ROOT / '.github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml'
        self.assertTrue(publisher.is_file(), '0.4.18 publisher workflow is required')
        self.assertTrue(notes.is_file(), '0.4.18 release notes are required')
        self.assertTrue(stable_manifest.is_file(), 'stable update manifest is required')
        self.assertFalse(legacy_publisher.exists(), '0.4.17 publisher must be retired before 0.4.18 publication')
        self.assertFalse(legacy_hotfix.exists(), 'temporary rev.2 Pages hotfix must be removed before 0.4.18 publication')
        self.assertFalse(legacy_foundation.exists(), '0.4.17 Foundation must remain retired')

        version = json.loads((ROOT / 'version.json').read_text(encoding='utf-8'))
        self.assertEqual(version['product'], 'DPopCleaner')
        self.assertEqual(version['version'], '0.4.18')
        self.assertEqual(version['version_code'], 418)
        self.assertEqual(version['revision'], 2)
        self.assertEqual(version['channel'], 'stable')
        self.assertEqual(version['manifest'], './update/stable.json')

        stable = json.loads(stable_manifest.read_text(encoding='utf-8'))
        self.assertEqual(stable['product'], 'DPopCleaner')
        self.assertEqual(stable['version'], '0.4.18')
        self.assertEqual(stable['version_code'], 418)
        self.assertEqual(stable['revision'], 2)
        self.assertEqual(stable['channel'], 'stable')
        self.assertFalse(stable['available'], 'repository manifest must fail closed before production publishes real SHA/size')
        self.assertEqual(stable['download_url'], '')
        self.assertEqual(stable['sha256'], '')
        self.assertEqual(stable['size'], 0)

        manifest = (ROOT / 'release-manifest.js').read_text(encoding='utf-8').lower()
        self.assertIn("m.version === '0.4.18'", manifest)
        self.assertIn("m.channel === 'stable'", manifest)
        self.assertIn('number(m.revision) === 2', manifest)
        self.assertIn('v0\\.4\\.18', manifest)
        self.assertIn('dpopcleaner_setup_0\\.4\\.18\\.exe', manifest)

        index = (ROOT / 'index.html').read_text(encoding='utf-8').lower()
        for token in (
            'dpopcleaner 0.4.18', 'rev.2', 'мгновенное закрытие', 'автообновление',
            'проверить обновления сейчас', 'sha-256', 'dpopupdater.exe', 'анализатор диска',
            'центр восстановления', 'flowseal zapret 1.10.2', 'thirdparty\\zapret', 'windivert',
            'assets/dpopcleaner-0.4.18-overview.png', 'assets/dpopcleaner-0.4.18-zapret.png',
            'assets/dpopcleaner-0.4.18-settings.png',
        ):
            self.assertIn(token, index)
        self.assertNotIn('dpopcleaner-0.4.17-disk.png', index)
        self.assertNotIn('dpopcleaner-0.4.17-restore.png', index)
        self.assertNotIn('замороженное ядро', index)
        self.assertNotIn('byte-identical', index)
        self.assertNotIn('beta', index)

        script = (ROOT / 'script.js').read_text(encoding='utf-8').lower()
        self.assertIn('./update/stable.json', script)

        notes_text = notes.read_text(encoding='utf-8').lower()
        for token in (
            'мгновенн', 'автообнов', 'dpopupdater.exe', 'sha-256', '0.4.17',
            'flowseal zapret 1.10.2', 'thirdparty\\zapret', 'list-general-user.txt',
            'rev.2', 'икон', 'скриншот',
        ):
            self.assertIn(token, notes_text)
        self.assertNotIn('beta', notes_text)

        workflow = publisher.read_text(encoding='utf-8').lower()
        for token in (
            'v0.4.18', 'dpopcleaner_setup_0.4.18.exe', 'dpop0418_prepare_zapret.ps1',
            'test_dpop0418_zapret_bundle_contract.py', '1508077', PINNED_ZAPRET_SHA256,
            'dpop0418_stage.ps1 -requirecompanions', 'dpop0418_install_smoke.ps1',
            'dpop0418_close_smoke.ps1', 'dpop0418_icon_smoke.ps1',
            'dpop0418_capture_screenshots.ps1', 'dpopcleaner-0.4.18-overview.png',
            'dpopcleaner-0.4.18-zapret.png', 'dpopcleaner-0.4.18-settings.png',
            'test_dpop0418_release_contract.py', 'node --test tests/release-manifest.test.cjs',
            'ctest --test-dir build0418', 'zapretscreenfix.tests.csproj', 'version_code = 418',
            'revision = 2', 'get-filehash', 'gh release', 'update/stable.json',
            'actions/upload-pages-artifact', 'actions/deploy-pages', 'invoke-restmethod',
            'invoke-webrequest', 'sha256',
        ):
            self.assertIn(token, workflow)
        self.assertNotIn('assets/dpopcleaner-0.4.17-disk.png', workflow)
        self.assertNotIn('assets/dpopcleaner-0.4.17-restore.png', workflow)
        self.assertNotIn('--prerelease', workflow)
        self.assertNotIn('update/beta.json', workflow)
        self.assertNotIn('releases/latest', workflow)

    def test_windows_icon_resource_and_installer_contract(self):
        icon = ROOT / 'dpopcleaner.ico'
        resource_header = ROOT / 'v0418/resources/resource.h'
        resource_script = ROOT / 'v0418/resources/version.rc.in'
        cmake = ROOT / 'v0418/CMakeLists.txt'
        main = ROOT / 'v0418/core/main.cpp'
        installer = ROOT / 'release/DPopCleaner_0.4.18.iss'
        icon_smoke = ROOT / 'tools/dpop0418_icon_smoke.ps1'
        capture = ROOT / 'tools/dpop0418_capture_screenshots.ps1'

        self.assertTrue(icon.is_file(), 'canonical dpopcleaner.ico must exist')
        self.assertTrue(resource_header.is_file(), 'shared icon resource id header is required')
        self.assertTrue(icon_smoke.is_file(), 'compiled icon resource smoke is required')
        self.assertTrue(capture.is_file(), 'real UI screenshot capture script is required')

        header = resource_header.read_text(encoding='utf-8').lower()
        self.assertIn('#define idi_app_icon 101', header)

        rc = resource_script.read_text(encoding='utf-8').lower()
        self.assertIn('#include "resource.h"', rc)
        self.assertIn('idi_app_icon icon "dpopcleaner.ico"', rc)
        self.assertIn('fileversion 0,4,18,2', rc)
        self.assertIn('productversion 0,4,18,2', rc)

        cmake_text = cmake.read_text(encoding='utf-8').lower()
        self.assertIn('dpopcleaner.ico', cmake_text)
        self.assertIn('generated/dpopcleaner.ico', cmake_text)
        self.assertIn('generated/resource.h', cmake_text)

        main_text = main.read_text(encoding='utf-8').lower()
        self.assertIn('../resources/resource.h', main_text)
        self.assertIn('makeintresourcew(idi_app_icon)', main_text)
        self.assertIn('wm_seticon', main_text)
        self.assertIn('dpopcleaner0418mainwindow', main_text)

        iss = installer.read_text(encoding='utf-8').lower()
        self.assertIn('setupiconfile=..\\dpopcleaner.ico', iss)
        self.assertIn('versioninfoversion=0.4.18.2', iss)
        self.assertIn('versioninfoproductversion=0.4.18.2', iss)
        self.assertIn('setup revision 2', iss)
        self.assertIn('iconfilename: "{app}\\dpopcleaner.exe"', iss)


if __name__ == '__main__':
    unittest.main()
