from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417Rev18UserReportContractTests(unittest.TestCase):
    def test_zapret_layout_uses_current_client_height_not_only_width(self):
        source = (ROOT / 'v0417/src/SimpleUpdate/ZapretResponsiveLayoutHost.cs').read_text(encoding='utf-8')
        self.assertIn('clientHeight', source)
        self.assertIn('ComputeStatusDetailHeight', source)
        self.assertIn('sectionGap', source)
        self.assertIn('ResponsiveMaximumButtonHeight', source)
        self.assertIn('statusDetailBottom', source)

    def test_bridge_buttons_disable_native_windows_theme_without_cross_process_ownerdraw(self):
        source = (ROOT / 'v0417/src/SimpleUpdate/ZapretVisualPolishHost.cs').read_text(encoding='utf-8')
        self.assertIn('BridgeButtonIds', source)
        self.assertIn('SetWindowTheme', source)
        self.assertIn('SetWindowTheme(button, string.Empty, string.Empty)', source)
        self.assertIn('SetWindowTheme(button, null, null)', source)
        self.assertIn('ButtonSubclassDelegate', source)
        self.assertNotIn('UnifiedZapretButtonIds', source)
        self.assertNotIn('SetOwnerDrawStyle(button, originalStyle)', source)

    def test_single_tray_uses_separate_user_preference_and_keeps_core_tray_off(self):
        launcher = (ROOT / 'v0417/src/SimpleUpdate/LauncherContext.cs').read_text(encoding='utf-8')
        settings = (ROOT / 'v0417/src/SimpleUpdate/SettingsStore.cs').read_text(encoding='utf-8')
        host = (ROOT / 'v0417/src/SimpleUpdate/AdditionalSettingsHost.cs').read_text(encoding='utf-8')
        self.assertIn('LoadTrayIconEnabled', settings)
        self.assertIn('SaveTrayIconEnabled', settings)
        self.assertIn('_trayPreference', launcher)
        self.assertIn('CaptureTrayPreferenceFromProxy', launcher)
        self.assertIn('EnsureLegacyTrayDisabled', launcher)
        self.assertIn('ReapplyCanonicalTrayProxy', launcher)
        self.assertIn('OnTrayPreferenceChanged', launcher)
        self.assertIn('CanonicalTrayProxyId = 1503', host)
        self.assertIn('_trayPreferenceChanged', host)
        self.assertIn('_trayPreferenceChanged(NativeBridge.IsChecked(setting.ProxyHandle))', host)
        self.assertIn('if (setting.Id != CanonicalTrayProxyId)\n                    NativeBridge.SetChecked', host)

    def test_rev18_has_a_real_installed_gate_for_the_user_report(self):
        workflow_path = ROOT / '.github/workflows/DPopCleaner_0.4.17_REV18_USER_REPORT.yml'
        self.assertTrue(workflow_path.is_file(), 'rev.18 dedicated installed workflow is required')
        workflow = workflow_path.read_text(encoding='utf-8')
        self.assertIn('dpop0417_rev18_installed_user_report_smoke.ps1', workflow)
        self.assertIn('DPopCleaner_Setup_0.4.17.exe', workflow)
        smoke = (ROOT / 'tools/dpop0417_rev18_user_report_smoke.ps1').read_text(encoding='utf-8')
        self.assertIn('1908,950', smoke)
        self.assertIn('GetWindowTheme', smoke)
        self.assertIn("ExpectedCanonical", smoke)
        self.assertIn('Find-FrozenTrayCheckbox', smoke)


if __name__ == '__main__':
    unittest.main()
