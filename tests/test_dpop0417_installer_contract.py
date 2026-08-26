from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417InstallerContractTests(unittest.TestCase):
    def test_installer_and_installed_smoke_are_exact_and_safe(self):
        iss = ROOT / "release" / "DPopCleaner_0.4.17.iss"
        smoke = ROOT / "tools" / "dpop0417_install_smoke.ps1"
        self.assertTrue(iss.is_file(), "release/DPopCleaner_0.4.17.iss is required")
        self.assertTrue(smoke.is_file(), "tools/dpop0417_install_smoke.ps1 is required")

        text = iss.read_text(encoding="utf-8")
        lowered = text.lower()
        self.assertIn('#define myappversion "0.4.17"', lowered)
        self.assertIn('appid={{b892e3d2-00cc-4d16-bb22-8b3943d42d15}', lowered)
        self.assertIn('versioninfoversion=0.4.17.0', lowered)
        self.assertIn('outputbasefilename=dpopcleaner_setup_0.4.17', lowered)
        self.assertIn('dpopcleaner.exe', lowered)
        self.assertIn('modules\\*', lowered)
        self.assertIn('languages\\*', lowered)
        self.assertIn('shell\\*', lowered)
        self.assertIn('documentation\\*', lowered)
        self.assertIn('resources\\*', lowered)
        self.assertIn('name: "{app}\\documentation"; permissions: users-modify', lowered)
        self.assertNotIn('downloads\\dpopcleaner_0.2.14_beta.exe', lowered)

        smoke_text = smoke.read_text(encoding="utf-8").lower()
        self.assertIn('/verysilent', smoke_text)
        self.assertIn('/norestart', smoke_text)
        self.assertIn('dpopcleaner_setup_0.4.17.exe', smoke_text)
        self.assertIn('efd0eff1f4962319282363fa85595c25e0cebe11', smoke_text)
        self.assertIn('modules/diskanalyzer.exe', smoke_text.replace('\\', '/'))
        self.assertIn('modules/restorecenter.exe', smoke_text.replace('\\', '/'))
        self.assertIn('dpop0417_disk_smoke.ps1', smoke_text)
        self.assertIn('dpop0417_restore_smoke.ps1', smoke_text)
        self.assertIn('get-acl', smoke_text)
        self.assertIn('s-1-5-32-545', smoke_text)
        self.assertIn('documentation_acl_modify', smoke_text)
        self.assertIn('unins000.exe', smoke_text)
        self.assertNotIn('invoke-expression', smoke_text)


if __name__ == "__main__":
    unittest.main()
