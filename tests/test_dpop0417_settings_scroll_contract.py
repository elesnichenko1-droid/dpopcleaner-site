from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417SettingsScrollContractTests(unittest.TestCase):
    def test_scroll_host_exists_and_owns_overflow_settings(self):
        host = ROOT / "v0417" / "src" / "SimpleUpdate" / "AdditionalSettingsHost.cs"
        self.assertTrue(host.is_file(), "AdditionalSettingsHost.cs is required")
        text = host.read_text(encoding="utf-8").lower()
        self.assertIn("autoscroll = true", text)
        self.assertIn("autoscrollminsize", text)
        self.assertIn("settingsscrollhostid", text)
        self.assertIn("autoupdatecheckboxid", text)
        self.assertIn("checknowbuttonid", text)
        self.assertIn("mousewheel", text)
        self.assertIn("лицензия", text)
        self.assertIn("включить автообновление", text)
        self.assertIn("проверить обновления", text)

    def test_launcher_uses_scroll_host_and_hides_legacy_version_badge(self):
        launcher = (ROOT / "v0417" / "src" / "SimpleUpdate" / "LauncherContext.cs").read_text(encoding="utf-8").lower()
        native = (ROOT / "v0417" / "src" / "SimpleUpdate" / "NativeBridge.cs").read_text(encoding="utf-8").lower()
        self.assertIn("additionalsettingshost", launcher)
        self.assertIn("hidelegacyversionbadge", launcher)
        self.assertIn("hidelegacyversionbadge", native)
        self.assertNotIn("makeroomforautoupdate(_mainwindow)", launcher)

    def test_authentic_ui_smoke_proves_wheel_scroll_and_hidden_version(self):
        smoke = (ROOT / "tools" / "dpop0417_simpleupdate_smoke.ps1").read_text(encoding="utf-8").lower()
        self.assertIn("1492", smoke)
        self.assertIn("wm_mousewheel", smoke)
        self.assertIn("v0.2.11 beta", smoke)
        self.assertIn("scroll", smoke)


if __name__ == "__main__":
    unittest.main()
