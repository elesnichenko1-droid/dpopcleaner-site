from pathlib import Path
import json
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417LayoutContractTests(unittest.TestCase):
    def test_shell_and_documentation_layout_is_real_and_safe(self):
        base = ROOT / "v0417" / "payload"
        shell = json.loads((base / "Shell" / "shell.json").read_text(encoding="utf-8"))
        self.assertEqual(shell["version"], "0.4.17")
        self.assertEqual(shell["commands"], ["disk-analyzer", "restore-center", "zapret-screen-fix"])

        expected_commands = {
            "disk-analyzer.json": ("disk-analyzer", r"Modules\DiskAnalyzer.exe"),
            "restore-center.json": ("restore-center", r"Modules\RestoreCenter.exe"),
            "zapret-screen-fix.json": ("zapret-screen-fix", r"Modules\ZapretScreenFix.exe"),
        }
        for filename, (command_id, executable) in expected_commands.items():
            command = json.loads((base / "Shell" / "commands" / filename).read_text(encoding="utf-8"))
            self.assertEqual(command["id"], command_id)
            self.assertEqual(command["executable"], executable)
            self.assertEqual(command["arguments"], [])
            self.assertNotIn("command", command)
            serialized = json.dumps(command).lower()
            self.assertNotIn("cmd.exe", serialized)
            self.assertNotIn("powershell", serialized)

        required = [
            "Documentation/README.txt",
            "Documentation/History/.gitkeep",
            "Documentation/Backups/Settings/.gitkeep",
            "Documentation/Backups/Registry/.gitkeep",
            "Documentation/Backups/System/.gitkeep",
            "Documentation/Logs/.gitkeep",
        ]
        for rel in required:
            self.assertTrue((base / rel).is_file(), rel)


if __name__ == "__main__":
    unittest.main()
