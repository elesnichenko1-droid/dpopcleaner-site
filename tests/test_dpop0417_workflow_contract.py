from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417WorkflowContractTests(unittest.TestCase):
    def test_foundation_workflow_is_windows_only_read_only_and_non_publishing(self):
        path = ROOT / ".github" / "workflows" / "DPopCleaner_0.4.17_FOUNDATION.yml"
        text = path.read_text(encoding="utf-8")
        lowered = text.lower()

        self.assertIn("windows-2022", lowered)
        self.assertIn("contents: read", lowered)
        self.assertIn("fetch-depth: 0", lowered)
        self.assertIn("test_dpop0417_core_contract.py", lowered)
        self.assertIn("test_dpop0417_layout_contract.py", lowered)
        self.assertIn("test_dpop0417_stage_contract.py", lowered)
        self.assertIn("test_dpop0417_workflow_contract.py", lowered)
        self.assertIn("test_dpop0417_zapret_screen_fix_contract.py", lowered)
        self.assertIn("dotnet test", lowered)
        self.assertIn("v0417/tests/dpop.common.tests/dpop.common.tests.csproj", lowered)
        self.assertIn("v0417/tests/zapretscreenfix.tests/zapretscreenfix.tests.csproj", lowered)
        self.assertIn("dotnet build", lowered)
        self.assertIn("v0417/src/simpleupdate/simpleupdate.csproj", lowered)
        self.assertIn("v0417/src/zapretscreenfix/zapretscreenfix.csproj", lowered)
        self.assertIn("tools/dpop0417_stage.ps1", lowered)
        self.assertIn("modules/zapretscreenfix.exe", lowered.replace("\\", "/"))
        self.assertIn("efd0eff1f4962319282363fa85595c25e0cebe11", lowered)
        self.assertIn("actions/upload-artifact", lowered)

        for forbidden in [
            "cmake",
            "v035_overlay",
            "gh release",
            "deploy-pages",
            "pages: write",
            "contents: write",
        ]:
            self.assertNotIn(forbidden, lowered)


if __name__ == "__main__":
    unittest.main()
