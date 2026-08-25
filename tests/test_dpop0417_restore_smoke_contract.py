from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417RestoreSmokeContractTests(unittest.TestCase):
    def test_restore_smoke_checks_reversible_and_nonreversible_paths(self):
        path = ROOT / "tools" / "dpop0417_restore_smoke.ps1"
        self.assertTrue(path.is_file(), "tools/dpop0417_restore_smoke.ps1 is required")
        text = path.read_text(encoding="utf-8")
        lowered = text.lower()
        self.assertIn("reversible-roundtrip", lowered)
        self.assertIn("nonreversible", lowered)
        self.assertIn("restore-smoke-report.json", lowered)
        self.assertIn("backup_preserved", lowered)
        self.assertIn("--smoke-create-file-record", lowered)
        self.assertIn("--smoke-restore-latest", lowered)
        self.assertNotIn("invoke-expression", lowered)
        self.assertNotIn("start-process cmd.exe", lowered)
        self.assertNotIn("start-process powershell", lowered)


if __name__ == "__main__":
    unittest.main()
