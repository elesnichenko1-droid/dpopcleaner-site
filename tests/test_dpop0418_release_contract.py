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
        self.assertTrue(publisher.is_file(), '0.4.18 publisher workflow is required')
        self.assertTrue(notes.is_file(), '0.4.18 release notes are required')
        self.assertTrue(stable_manifest.is_file(), 'stable update manifest is required')

        version = json.loads((ROOT / 'version.json').read_text(encoding='utf-8'))
        self.assertEqual(version['product'], 'DPopCleaner')
        self.assertEqual(version['version'], '0.4.18')
        self.assertEqual(version['version_code'], 418)
        self.assertEqual(version['revision'], 1)
        self.assertEqual(version['channel'], 'stable')
        self.assertEqual(version['manifest'], './update/stable.json')

        stable = json.loads(stable_manifest.read_text(encoding='utf-8'))
        self.assertEqual(stable['product'], 'DPopCleaner')
        self.assertEqual(stable['version'], '0.4.18')
        self.assertEqual(stable['version_code'], 418)
        self.assertEqual(stable['revision'], 1)
        self.assertEqual(stable['channel'], 'stable')
        self.assertFalse(stable['available'], 'repository manifest must fail closed before production publishes real SHA/size')
        self.assertEqual(stable['download_url'], '')
        self.assertEqual(stable['sha256'], '')
        self.assertEqual(stable['size'], 0)

        manifest = (ROOT / 'release-manifest.js').read_text(encoding='utf-8').lower()
        self.assertIn("m.version === '0.4.18'", manifest)
        self.assertIn("m.channel === 'stable'", manifest)
        self.assertIn('number(m.revision) === 1', manifest)
        self.assertIn('v0\\.4\\.18', manifest)
        self.assertIn('dpopcleaner_setup_0\\.4\\.18\\.exe', manifest)

        index = (ROOT / 'index.html').read_text(encoding='utf-8').lower()
        for token in (
            'dpopcleaner 0.4.18',
            'мгновенное закрытие',
            'автообновление',
            'проверить обновления сейчас',
            'sha-256',
            'dpopupdater.exe',
            'анализатор диска',
            'центр восстановления',
            'flowseal zapret 1.10.2',
            'thirdparty\\zapret',
            'windivert',
        ):
            self.assertIn(token, index)
        self.assertNotIn('замороженное ядро', index)
        self.assertNotIn('byte-identical', index)
        self.assertNotIn('beta', index)

        script = (ROOT / 'script.js').read_text(encoding='utf-8').lower()
        self.assertIn('./update/stable.json', script)

        notes_text = notes.read_text(encoding='utf-8').lower()
        for token in (
            'мгновенн',
            'автообнов',
            'dpopupdater.exe',
            'sha-256',
            '0.4.17',
            'flowseal zapret 1.10.2',
            'thirdparty\\zapret',
            'list-general-user.txt',
        ):
            self.assertIn(token, notes_text)
        self.assertNotIn('beta', notes_text)

        workflow = publisher.read_text(encoding='utf-8').lower()
        for token in (
            'v0.4.18',
            'dpopcleaner_setup_0.4.18.exe',
            'dpop0418_prepare_zapret.ps1',
            'test_dpop0418_zapret_bundle_contract.py',
            '1508077',
            PINNED_ZAPRET_SHA256,
            'dpop0418_stage.ps1 -requirecompanions',
            'dpop0418_install_smoke.ps1',
            'dpop0418_close_smoke.ps1',
            'test_dpop0418_release_contract.py',
            'node --test tests/release-manifest.test.cjs',
            'ctest --test-dir build0418',
            'zapretscreenfix.tests.csproj',
            'version_code = 418',
            'revision = 1',
            'get-filehash',
            'gh release',
            'update/stable.json',
            'actions/upload-pages-artifact',
            'actions/deploy-pages',
            'invoke-restmethod',
            'invoke-webrequest',
            'sha256',
        ):
            self.assertIn(token, workflow)
        self.assertNotIn('--prerelease', workflow)
        self.assertNotIn('update/beta.json', workflow)
        self.assertNotIn('releases/latest', workflow)


if __name__ == '__main__':
    unittest.main()
