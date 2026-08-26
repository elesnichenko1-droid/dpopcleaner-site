from pathlib import Path
import json
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417ZapretScreenFixContractTests(unittest.TestCase):
    def test_zapret_screen_fix_is_a_staged_shell_companion(self):
        project = ROOT / 'v0417/src/ZapretScreenFix/ZapretScreenFix.csproj'
        patcher = ROOT / 'v0417/src/ZapretScreenFix/ZapretStrategyPatcher.cs'
        command = ROOT / 'v0417/payload/Shell/commands/zapret-screen-fix.json'

        self.assertTrue(project.is_file(), 'ZapretScreenFix companion project is required')
        self.assertTrue(patcher.is_file(), 'ZapretStrategyPatcher implementation is required')
        self.assertTrue(command.is_file(), 'Zapret Screen Fix shell command is required')

        shell = json.loads((ROOT / 'v0417/payload/Shell/shell.json').read_text(encoding='utf-8'))
        self.assertIn('zapret-screen-fix', shell['commands'])

        command_data = json.loads(command.read_text(encoding='utf-8'))
        self.assertEqual(command_data['id'], 'zapret-screen-fix')
        self.assertEqual(command_data['executable'], r'Modules\ZapretScreenFix.exe')

        allowlist = (ROOT / 'v0417/stage-allowlist.txt').read_text(encoding='utf-8')
        self.assertIn('Modules/ZapretScreenFix.exe', allowlist)

        stage = (ROOT / 'tools/dpop0417_stage.ps1').read_text(encoding='utf-8')
        self.assertIn('ZapretScreenFix.exe', stage)

        installer = (ROOT / 'release/DPopCleaner_0.4.17.iss').read_text(encoding='utf-8')
        self.assertIn('ZapretScreenFix.exe', installer)

        install_smoke = (ROOT / 'tools/dpop0417_install_smoke.ps1').read_text(encoding='utf-8')
        self.assertIn('Modules\\ZapretScreenFix.exe', install_smoke)
        self.assertIn('Shell\\commands\\zapret-screen-fix.json', install_smoke)

    def test_patch_revision_is_two_and_publisher_builds_and_tests_companion(self):
        version = json.loads((ROOT / 'version.json').read_text(encoding='utf-8'))
        stable = json.loads((ROOT / 'update/stable.json').read_text(encoding='utf-8'))
        self.assertEqual(version['revision'], 2)
        self.assertEqual(stable['revision'], 2)

        workflow = (ROOT / '.github/workflows/publish-dpopcleaner-0.4.17.yml').read_text(encoding='utf-8').lower()
        self.assertIn('zapretscreenfix.tests.csproj', workflow)
        self.assertIn('zapretscreenfix.csproj', workflow)
        self.assertIn('revision = 2', workflow)
        self.assertIn('gh release edit', workflow)

    def test_release_notes_and_site_disclose_screen_share_fix(self):
        notes = (ROOT / 'release/RELEASE_NOTES_0.4.17.md').read_text(encoding='utf-8').lower()
        index = (ROOT / 'index.html').read_text(encoding='utf-8').lower()
        self.assertIn('zapret', notes)
        self.assertTrue('демонстрац' in notes or 'screen share' in notes)
        self.assertIn('zapret', index)


if __name__ == '__main__':
    unittest.main()
