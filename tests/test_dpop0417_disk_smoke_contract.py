from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417DiskSmokeContractTests(unittest.TestCase):
    def test_disk_smoke_requires_controlled_fixture_and_no_delete_action(self):
        script = (ROOT / "tools" / "dpop0417_disk_smoke.ps1").read_text(encoding="utf-8")
        lowered = script.lower()

        self.assertIn("dpop0417-disk-fixture", lowered)
        self.assertIn("disk-smoke-report.json", lowered)
        self.assertIn("--root", lowered)
        self.assertIn("--smoke-report", lowered)
        self.assertIn("printwindow", lowered)
        self.assertIn("remove-item $fixture", lowered)

        for forbidden in [
            "deletefile",
            "deletefilew",
            "remove-item $fixture\\*",
            "cmd.exe /c del",
            "powershell -command remove-item",
        ]:
            self.assertNotIn(forbidden, lowered)


if __name__ == "__main__":
    unittest.main()
