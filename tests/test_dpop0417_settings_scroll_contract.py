from pathlib import Path
import re
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

    def test_virtual_content_extent_is_at_least_autoscroll_minimum(self):
        text = (ROOT / "v0417" / "src" / "SimpleUpdate" / "AdditionalSettingsHost.cs").read_text(encoding="utf-8")
        minimum = re.search(r"AutoScrollMinSize\s*=\s*(\d+)", text)
        content = re.search(r"ContentHeight\s*=\s*(\d+)", text)
        self.assertIsNotNone(minimum, "AutoScrollMinSize constant is required")
        self.assertIsNotNone(content, "ContentHeight constant is required")
        self.assertGreaterEqual(int(content.group(1)), int(minimum.group(1)))

    def test_launcher_uses_scroll_host_and_hides_legacy_version_badge(self):
        launcher = (ROOT / "v0417" / "src" / "SimpleUpdate" / "LauncherContext.cs").read_text(encoding="utf-8").lower()
        native = (ROOT / "v0417" / "src" / "SimpleUpdate" / "NativeBridge.cs").read_text(encoding="utf-8").lower()
        self.assertIn("additionalsettingshost", launcher)
        self.assertIn("hidelegacyversionbadge", launcher)
        self.assertIn("hidelegacyversionbadge", native)
        self.assertNotIn("makeroomforautoupdate(_mainwindow)", launcher)

    def test_simpleupdate_self_elevates_to_admin_integrity_for_admin_core_ui_bridge(self):
        project = (ROOT / "v0417" / "src" / "SimpleUpdate" / "SimpleUpdate.csproj").read_text(encoding="utf-8").lower()
        manifest = ROOT / "v0417" / "src" / "SimpleUpdate" / "app.manifest"
        program = (ROOT / "v0417" / "src" / "SimpleUpdate" / "Program.cs").read_text(encoding="utf-8").lower()
        elevation = (ROOT / "v0417" / "src" / "SimpleUpdate" / "ElevationBootstrap.cs").read_text(encoding="utf-8").lower()
        self.assertTrue(manifest.is_file())
        self.assertIn("<applicationmanifest>app.manifest</applicationmanifest>", project)
        manifest_text = manifest.read_text(encoding="utf-8").lower()
        self.assertIn('requestedexecutionlevel level="asinvoker"', manifest_text)
        self.assertNotIn('requireadministrator', manifest_text)
        self.assertIn('elevationbootstrap.ensureadministrator(args)', program)
        self.assertIn('windowsprincipal', elevation)
        self.assertIn('windowsbuiltinrole.administrator', elevation)
        self.assertIn('verb = "runas"', elevation)
        self.assertIn('useshellexecute = true', elevation)

    def test_authentic_ui_smoke_proves_wheel_scroll_and_hidden_version(self):
        smoke = (ROOT / "tools" / "dpop0417_simpleupdate_smoke.ps1").read_text(encoding="utf-8").lower()
        self.assertIn("1492", smoke)
        self.assertIn("wm_mousewheel", smoke)
        self.assertIn("v0.2.11 beta", smoke)
        self.assertIn("scroll", smoke)

    def test_rev10_settings_host_uses_fixed_bounds_instead_of_recursive_proxy_feedback(self):
        launcher = (ROOT / "v0417" / "src" / "SimpleUpdate" / "LauncherContext.cs").read_text(encoding="utf-8")
        capture = "_settingsHostBounds = NativeBridge.GetSettingsScrollBounds(_mainWindow);"
        self.assertIn("private NativeBridge.ClientBounds _settingsHostBounds;", launcher)
        self.assertEqual(launcher.count(capture), 1)
        self.assertEqual(launcher.count("NativeBridge.GetSettingsScrollBounds(_mainWindow)"), 1)
        self.assertIn("_settingsHost.Show(_settingsHostBounds);", launcher)
        self.assertNotIn("var hostBounds = NativeBridge.GetSettingsScrollBounds(_mainWindow);", launcher)
        self.assertIn("HideLegacyOverflowControls(_mainWindow, _settingsHost.Handle, _settingsHostBounds)", launcher)

    def test_rev12_runtime_smoke_uses_native_version_source_and_captures_real_zapret_page(self):
        smoke = (ROOT / "tools" / "dpop0417_rev12_native_version_smoke.ps1").read_text(encoding="utf-8")
        for token in (
            "REV12_NATIVE_ZAPRET_VERSION_SMOKE_OK",
            "Zapret\\utils\\dpop_version.txt",
            "Zapret\\.service\\version.txt",
            "nativeVersion -ne '1.10.2'",
            "version_metadata_byte_identical",
            "PrintWindow",
            "rev12-zapret-native-version.png",
            "Assert-NoForbiddenVersionProxy1726",
            "Сервисные действия",
            "Service actions",
            "ClassName -eq 'Static'",
            "Forbidden rev.10 version proxy id=1726 exists",
            "bridge_buttons_owner_draw",
            "Click-Id $window 905",
            "Wait-Visible $window 1703",
        ):
            self.assertIn(token, smoke)
        self.assertNotIn("Wait-NativeZapretStatus", smoke)
        self.assertNotIn("WriteWindowText", smoke)

    def test_rev11_direct_frozen_core_diagnostic_records_native_status_click_effect(self):
        diagnostic = (ROOT / "tools" / "dpop0417_rev7_ui_diagnostic.ps1").read_text(encoding="utf-8")
        for token in (
            "REV11_NATIVE_STATUS_BEFORE",
            "REV11_NATIVE_STATUS_AFTER",
            "Get-ZapretEdits",
            "Click-ControlId $window 1703",
            "upper=",
            "lower=",
        ):
            self.assertIn(token, diagnostic)

    def test_rev11_settings_scroll_moves_children_atomically_and_repaints_once(self):
        host = (ROOT / "v0417" / "src" / "SimpleUpdate" / "AdditionalSettingsHost.cs").read_text(encoding="utf-8")
        for token in (
            "BeginDeferWindowPos",
            "DeferWindowPos",
            "EndDeferWindowPos",
            "RedrawWindow",
            "RDW_INVALIDATE",
            "RDW_ERASE",
            "RDW_ALLCHILDREN",
            "RDW_UPDATENOW",
            "if (clamped == _scrollPosition)",
        ):
            self.assertIn(token, host)
        self.assertIn("RedrawSettingsHost", host)


if __name__ == "__main__":
    unittest.main()