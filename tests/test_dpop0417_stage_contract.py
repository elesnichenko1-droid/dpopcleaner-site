from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417StageContractTests(unittest.TestCase):
    def test_stage_allowlist_is_exact_and_keeps_only_frozen_core_plus_approved_payloads(self):
        allowlist = (ROOT / "v0417" / "stage-allowlist.txt").read_text(encoding="utf-8")
        entries = [line.strip() for line in allowlist.splitlines() if line.strip()]
        self.assertEqual(entries, [
            "DPopCleaner.exe",
            "SimpleUpdate.exe",
            "LICENSE.txt",
            ".service/",
            "*.bat",
            "bin/",
            "lists/",
            "utils/",
            "Languages/",
            "Shell/",
            "Documentation/",
            "Modules/DPop.Common.dll",
            "Modules/DiskAnalyzer.exe",
            "Modules/RestoreCenter.exe",
            "Modules/ZapretScreenFix.exe",
            "Resources/",
        ])

        lowered = allowlist.lower()
        for forbidden in ["v035_overlay", "mainwindow.cpp", "pagebase.cpp", "fullcore.cpp", "cmake", "thirdparty/zapret"]:
            self.assertNotIn(forbidden, lowered)

    def test_staging_script_is_fail_closed_and_places_verified_zapret_beside_old_core(self):
        script = (ROOT / "tools" / "dpop0417_stage.ps1").read_text(encoding="utf-8")
        lowered = script.lower()
        normalized = lowered.replace("\\", "/")
        self.assertIn("v0417/contracts/core.json", lowered)
        self.assertIn("& git", lowered)
        self.assertIn("hash-object", lowered)
        self.assertIn("simpleupdate.exe", lowered)
        self.assertIn("v0417/src/simpleupdate/bin/release/net48/simpleupdate.exe", normalized)
        self.assertIn("dpop.common.dll", lowered)
        self.assertIn("diskanalyzer.exe", lowered)
        self.assertIn("restorecenter.exe", lowered)
        self.assertIn("zapretscreenfix.exe", lowered)
        self.assertIn("v0417/src/zapretscreenfix/bin/release/net48/zapretscreenfix.exe", normalized)
        self.assertIn("_release/0.4.17/third-party/zapret", normalized)
        self.assertIn("service.bat", lowered)
        self.assertIn("general.bat", lowered)
        self.assertIn("general*.bat", lowered)
        self.assertIn("bin/winws.exe", normalized)
        self.assertIn("bin/windivert.dll", normalized)
        self.assertIn("bin/windivert64.sys", normalized)
        self.assertIn(".service/version.txt", normalized)
        self.assertIn("1.10.2", lowered)
        self.assertIn("-requirecompanions", lowered)
        self.assertNotIn("v035_overlay", lowered)
        self.assertNotIn("mainwindow.cpp", lowered)
        self.assertNotIn("fullcore.cpp", lowered)
        self.assertNotIn("copy-item -path *", lowered)
        self.assertNotIn("copy-item -literalpath $pwd -recurse", lowered)


if __name__ == "__main__":
    unittest.main()
