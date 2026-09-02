from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417WorkflowContractTests(unittest.TestCase):
    def test_foundation_workflow_is_windows_only_read_only_and_non_publishing(self):
        path = ROOT / ".github" / "workflows" / "DPopCleaner_0.4.17_FOUNDATION.yml"
        lowered = path.read_text(encoding="utf-8").lower()
        normalized = lowered.replace("\\", "/")

        self.assertIn("windows-2022", lowered)
        self.assertIn("contents: read", lowered)
        self.assertIn("fetch-depth: 0", lowered)
        self.assertIn("test_dpop0417_core_contract.py", lowered)
        self.assertIn("test_dpop0417_layout_contract.py", lowered)
        self.assertIn("test_dpop0417_stage_contract.py", lowered)
        self.assertIn("test_dpop0417_workflow_contract.py", lowered)
        self.assertIn("test_dpop0417_zapret_screen_fix_contract.py", lowered)
        self.assertIn("test_dpop0417_zapret_bundle_contract.py", lowered)
        self.assertIn("v0417/tests/dpop.common.tests/dpop.common.tests.csproj", lowered)
        self.assertIn("v0417/tests/zapretscreenfix.tests/zapretscreenfix.tests.csproj", lowered)
        self.assertIn("v0417/src/simpleupdate/simpleupdate.csproj", lowered)
        self.assertIn("v0417/src/zapretscreenfix/zapretscreenfix.csproj", lowered)
        self.assertIn("tools/dpop0417_prepare_zapret.ps1", normalized)
        self.assertIn("tools/dpop0417_stage.ps1", normalized)
        self.assertIn("tools/dpop0417_zapret_ui_smoke.ps1", normalized)
        self.assertIn("stage/zapret/bin/winws.exe", normalized)
        self.assertIn("stage/zapret/bin/windivert64.sys", normalized)
        self.assertIn("stage/zapret/service.bat", normalized)
        self.assertIn("stage/zapret/.service/version.txt", normalized)
        self.assertNotIn("stage/bin/winws.exe", normalized)
        self.assertNotIn("stage/service.bat", normalized)
        self.assertIn("modules/zapretscreenfix.exe", normalized)
        self.assertIn("1.10.2", lowered)
        self.assertIn("efd0eff1f4962319282363fa85595c25e0cebe11", lowered)
        self.assertIn("actions/upload-artifact", lowered)

        for forbidden in ["cmake", "v035_overlay", "gh release", "deploy-pages", "pages: write", "contents: write"]:
            self.assertNotIn(forbidden, lowered)

    def test_publisher_verifies_the_same_legacy_zapret_subdirectory(self):
        path = ROOT / ".github" / "workflows" / "publish-dpopcleaner-0.4.17.yml"
        lowered = path.read_text(encoding="utf-8").lower()
        normalized = lowered.replace("\\", "/")

        self.assertIn("stage/zapret/service.bat", normalized)
        self.assertIn("stage/zapret/general.bat", normalized)
        self.assertIn("stage/zapret/bin/winws.exe", normalized)
        self.assertIn("stage/zapret/bin/windivert.dll", normalized)
        self.assertIn("stage/zapret/bin/windivert64.sys", normalized)
        self.assertIn("stage/zapret/.service/version.txt", normalized)
        self.assertNotIn("stage/service.bat", normalized)
        self.assertNotIn("stage/bin/winws.exe", normalized)
        self.assertIn("1.10.2", lowered)

    def test_rev16_zapret_functional_smoke_exits_zero_only_after_successful_cleanup(self):
        path = ROOT / "tools" / "dpop0417_rev16_zapret_functional_smoke.ps1"
        text = path.read_text(encoding="utf-8")

        failure_catch = text.find("catch {\n    $failure=$_.Exception.Message")
        finally_block = text.find("finally {", failure_catch)
        success_marker = text.rfind("REV16_ZAPRET_FUNCTIONAL_SMOKE_OK")
        explicit_success_exit = text.rfind("exit 0")
        rethrow = text.find("throw", failure_catch, finally_block)

        self.assertGreater(failure_catch, 0)
        self.assertGreater(rethrow, failure_catch)
        self.assertGreater(finally_block, rethrow)
        self.assertGreater(success_marker, finally_block)
        self.assertGreater(explicit_success_exit, success_marker)

    def test_rev16_installed_presentation_smoke_gates_theme_layout_and_journal_policy(self):
        smoke_path = ROOT / "tools" / "dpop0417_rev16_zapret_presentation_smoke.ps1"
        self.assertTrue(smoke_path.is_file())
        smoke = smoke_path.read_text(encoding="utf-8")
        for token in (
            "REV16_ZAPRET_LIGHT_THEME_OK",
            "REV16_ZAPRET_DARK_THEME_OK",
            "REV16_ZAPRET_BUTTON_LAYOUT_OK",
            "REV16_ZAPRET_JOURNAL_HIDDEN_OK",
            "REV16_OTHER_PAGE_LOG_UNCHANGED_OK",
            "BS_OWNERDRAW",
            "GetPixel",
            "ListBox",
        ):
            self.assertIn(token, smoke)

        foundation = (ROOT / ".github" / "workflows" / "DPopCleaner_0.4.17_FOUNDATION.yml").read_text(encoding="utf-8").replace("\\", "/")
        self.assertIn("tools/dpop0417_rev16_zapret_presentation_smoke.ps1", foundation)
        self.assertIn("rev16-zapret-presentation", foundation)


if __name__ == "__main__":
    unittest.main()
