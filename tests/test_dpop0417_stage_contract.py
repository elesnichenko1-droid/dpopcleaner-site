from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417StageContractTests(unittest.TestCase):
    def test_stage_allowlist_is_exact_and_excludes_reconstructed_runtime(self):
        allowlist = (ROOT / "v0417" / "stage-allowlist.txt").read_text(encoding="utf-8")
        entries = [line.strip() for line in allowlist.splitlines() if line.strip()]
        self.assertEqual(entries, [
            "DPopCleaner.exe",
            "Languages/",
            "Shell/",
            "Documentation/",
            "Modules/DPop.Common.dll",
            "Modules/DiskAnalyzer.exe",
            "Modules/RestoreCenter.exe",
            "Resources/",
        ])

        lowered = allowlist.lower()
        for forbidden in ["v035_overlay", "mainwindow.cpp", "pagebase.cpp", "fullcore.cpp", "cmake"]:
            self.assertNotIn(forbidden, lowered)

    def test_staging_script_is_fail_closed_and_copies_only_named_inputs(self):
        script = (ROOT / "tools" / "dpop0417_stage.ps1").read_text(encoding="utf-8")
        lowered = script.lower()
        self.assertIn("v0417/contracts/core.json", lowered)
        self.assertIn("& git", lowered)
        self.assertIn("hash-object", lowered)
        self.assertIn("dpop.common.dll", lowered)
        self.assertIn("diskanalyzer.exe", lowered)
        self.assertIn("restorecenter.exe", lowered)
        self.assertIn("-requirecompanions", lowered)
        self.assertNotIn("v035_overlay", lowered)
        self.assertNotIn("mainwindow.cpp", lowered)
        self.assertNotIn("fullcore.cpp", lowered)
        self.assertNotIn("copy-item -path *", lowered)
        self.assertNotIn("copy-item -literalpath $pwd -recurse", lowered)


if __name__ == "__main__":
    unittest.main()
