from pathlib import Path
import json
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417CoreContractTests(unittest.TestCase):
    def test_original_0214_core_is_frozen(self):
        contract = json.loads((ROOT / "v0417/contracts/core.json").read_text(encoding="utf-8"))
        core = ROOT / contract["path"]
        self.assertEqual(core.stat().st_size, 389632)
        blob = subprocess.check_output(
            ["git", "hash-object", str(core)], cwd=ROOT, text=True
        ).strip()
        self.assertEqual(blob, "efd0eff1f4962319282363fa85595c25e0cebe11")
        self.assertEqual(contract["git_blob_sha1"], blob)
        self.assertEqual(contract["staged_name"], "DPopCleaner.exe")


if __name__ == "__main__":
    unittest.main()
