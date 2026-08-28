from pathlib import Path
import json
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417ZapretScreenFixContractTests(unittest.TestCase):
    def test_rev5_preserves_zapret_screen_fix_as_a_staged_shell_companion(self):
        project = ROOT / 'v0417/src/ZapretScreenFix/ZapretScreenFix.csproj'
        patcher = ROOT / 'v0417/src/ZapretScreenFix/ZapretStrategyPatcher.cs'
        command = ROOT / 'v0417/payload/Shell/commands/zapret-screen-fix.json'

        self.assertTrue(project.is_file(), 'ZapretScreenFix companion project is required in rev.5')
        self.assertTrue(patcher.is_file(), 'ZapretStrategyPatcher implementation is required in rev.5')
        self.assertTrue(command.is_file(), 'Zapret Screen Fix shell command is required in rev.5')

        shell = json.loads((ROOT / 'v0417/payload/Shell/shell.json').read_text(encoding='utf-8'))
        self.assertIn('zapret-screen-fix', shell['commands'])

        command_data = json.loads(command.read_text(encoding='utf-8'))
        self.assertEqual(command_data['id'], 'zapret-screen-fix')
        self.assertEqual(command_data['executable'], r'Modules\ZapretScreenFix.exe')

        allowlist = (ROOT / 'v0417/stage-allowlist.txt').read_text(encoding='utf-8')
        self.assertIn('SimpleUpdate.exe', allowlist)
        self.assertIn('Modules/ZapretScreenFix.exe', allowlist)

        stage = (ROOT / 'tools/dpop0417_stage.ps1').read_text(encoding='utf-8')
        self.assertIn('SimpleUpdate.exe', stage)
        self.assertIn('ZapretScreenFix.exe', stage)

        installer = (ROOT / 'release/DPopCleaner_0.4.17.iss').read_text(encoding='utf-8')
        self.assertIn('SimpleUpdate.exe', installer)
        self.assertIn('ZapretScreenFix.exe', installer)

        install_smoke = (ROOT / 'tools/dpop0417_install_smoke.ps1').read_text(encoding='utf-8')
        self.assertIn('SimpleUpdate.exe', install_smoke)
        self.assertIn('Modules\\ZapretScreenFix.exe', install_smoke)
        self.assertIn('Shell\\commands\\zapret-screen-fix.json', install_smoke)

    def test_rev5_builds_and_tests_the_preserved_rev2_companion(self):
        version = json.loads((ROOT / 'version.json').read_text(encoding='utf-8'))
        stable = json.loads((ROOT / 'update/stable.json').read_text(encoding='utf-8'))
        self.assertEqual(version['revision'], 5)
        self.assertEqual(stable['revision'], 5)

        workflow = (ROOT / '.github/workflows/publish-dpopcleaner-0.4.17.yml').read_text(encoding='utf-8').lower()
        self.assertIn('zapretscreenfix.tests.csproj', workflow)
        self.assertIn('zapretscreenfix.csproj', workflow)
        self.assertIn('simpleupdate.csproj', workflow)
        self.assertIn('revision=5', workflow)
        self.assertIn('v0.4.17-rev5', workflow)

    def test_rev5_release_notes_and_site_disclose_preserved_screen_share_fix(self):
        notes = (ROOT / 'release/RELEASE_NOTES_0.4.17.md').read_text(encoding='utf-8').lower()
        index = (ROOT / 'index.html').read_text(encoding='utf-8').lower()
        self.assertIn('zapret', notes)
        self.assertTrue('демонстрац' in notes or 'screen share' in notes)
        self.assertIn('zapret', index)
        self.assertTrue('демонстрац' in index or 'screen share' in index)
        self.assertIn('автообнов', notes)
        self.assertIn('автообнов', index)


if __name__ == '__main__':
    unittest.main()
