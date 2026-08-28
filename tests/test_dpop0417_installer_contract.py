from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417InstallerContractTests(unittest.TestCase):
    def test_installer_and_installed_smoke_are_exact_and_safe(self):
        iss = ROOT / "release" / "DPopCleaner_0.4.17.iss"
        smoke = ROOT / "tools" / "dpop0417_install_smoke.ps1"
        zapret_smoke = ROOT / "tools" / "dpop0417_zapret_ui_smoke.ps1"
        program = ROOT / "v0417" / "src" / "SimpleUpdate" / "Program.cs"
        self.assertTrue(iss.is_file())
        self.assertTrue(smoke.is_file())
        self.assertTrue(zapret_smoke.is_file())
        self.assertTrue(program.is_file())

        lowered = iss.read_text(encoding="utf-8").lower()
        self.assertIn('#define myappversion "0.4.17"', lowered)
        self.assertIn('appid={{b892e3d2-00cc-4d16-bb22-8b3943d42d15}', lowered)
        self.assertIn('versioninfoversion=0.4.17.0', lowered)
        self.assertIn('outputbasefilename=dpopcleaner_setup_0.4.17', lowered)
        self.assertIn('source: "{#stageroot}\\dpopcleaner.exe"; destdir: "{app}"; destname: "dpopcleaner.core.exe"', lowered)
        self.assertIn('source: "{#stageroot}\\simpleupdate.exe"; destdir: "{app}"; destname: "dpopcleaner.exe"', lowered)
        self.assertIn('source: "{#stageroot}\\simpleupdate.exe"; destdir: "{app}"; destname: "simpleupdate.exe"', lowered)
        self.assertIn('source: "{#stageroot}\\zapret\\*"; destdir: "{app}\\zapret"', lowered)
        self.assertNotIn('source: "{#stageroot}\\*.bat"; destdir: "{app}"', lowered)
        self.assertNotIn('source: "{#stageroot}\\bin\\*"; destdir: "{app}\\bin"', lowered)
        self.assertIn('name: "{autoprograms}\\dpopcleaner"; filename: "{app}\\dpopcleaner.exe"', lowered)
        self.assertIn('name: "{autodesktop}\\dpopcleaner"; filename: "{app}\\dpopcleaner.exe"', lowered)
        self.assertIn('filename: "{app}\\dpopcleaner.exe"; description: "запустить dpopcleaner"', lowered)
        self.assertIn('zapretscreenfix.exe', lowered)
        self.assertIn('name: "{app}\\documentation"; permissions: users-modify', lowered)
        self.assertNotIn('downloads\\dpopcleaner_0.2.14_beta.exe', lowered)

        program_text = program.read_text(encoding="utf-8").lower()
        self.assertIn('path.combine(basedirectory, "dpopcleaner.core.exe")', program_text)
        self.assertNotIn('path.combine(basedirectory, "dpopcleaner.exe")', program_text)

        smoke_text = smoke.read_text(encoding="utf-8").lower()
        normalized = smoke_text.replace('\\', '/')
        self.assertIn('/verysilent', smoke_text)
        self.assertIn('/norestart', smoke_text)
        self.assertIn('efd0eff1f4962319282363fa85595c25e0cebe11', smoke_text)
        self.assertIn("assert-file 'dpopcleaner.exe'", smoke_text)
        self.assertIn("assert-file 'dpopcleaner.core.exe'", smoke_text)
        self.assertIn("assert-file 'simpleupdate.exe'", smoke_text)
        self.assertIn("assert-file 'zapret/service.bat'", normalized)
        self.assertIn("assert-file 'zapret/general.bat'", normalized)
        self.assertIn("assert-file 'zapret/bin/winws.exe'", normalized)
        self.assertIn("assert-file 'zapret/bin/windivert64.sys'", normalized)
        self.assertIn("assert-file 'zapret/.service/version.txt'", normalized)
        self.assertIn('1.10.2', smoke_text)
        self.assertIn('zapret_runtime_present', smoke_text)
        self.assertIn('dpop0417_zapret_ui_smoke.ps1', smoke_text)
        self.assertIn('zapret_authentic_ui_smoke', smoke_text)
        self.assertIn('--no-update-check', smoke_text)
        self.assertIn('modules/diskanalyzer.exe', normalized)
        self.assertIn('modules/restorecenter.exe', normalized)
        self.assertIn('modules/zapretscreenfix.exe', normalized)
        self.assertIn('shell/commands/zapret-screen-fix.json', normalized)
        self.assertIn('get-acl', smoke_text)
        self.assertIn('s-1-5-32-545', smoke_text)
        self.assertIn('unins000.exe', smoke_text)
        self.assertNotIn('invoke-expression', smoke_text)
        self.assertIn('${zapretversion}: pass', smoke_text)
        self.assertNotIn('$zapretversion: pass', smoke_text)

        ui_text = zapret_smoke.read_text(encoding="utf-8").lower()
        self.assertIn("$zapretroot = join-path $rootpath 'zapret'", ui_text)
        self.assertIn("text -eq 'zapret'", ui_text)
        self.assertIn('combobox', ui_text)
        self.assertIn('general*.bat', ui_text)
        self.assertIn('стратегии не найдены', ui_text)
        self.assertIn('authentic_zapret_ui_smoke_ok', ui_text)


if __name__ == "__main__":
    unittest.main()
