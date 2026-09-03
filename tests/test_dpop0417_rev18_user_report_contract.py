from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417Rev18UserReportContractTests(unittest.TestCase):
    def test_zapret_layout_uses_current_client_height_not_only_width(self):
        source = (ROOT / 'v0417/src/SimpleUpdate/ZapretResponsiveLayoutHost.cs').read_text(encoding='utf-8')
        self.assertIn('clientHeight', source)
        self.assertIn('availableVerticalSpace', source)
        self.assertIn('ResponsiveMaximumButtonHeight', source)
        self.assertIn('statusDetailBottom', source)

    def test_ownerdraw_zapret_buttons_disable_native_windows_theme(self):
        source = (ROOT / 'v0417/src/SimpleUpdate/ZapretVisualPolishHost.cs').read_text(encoding='utf-8')
        self.assertIn('SetWindowTheme', source)
        self.assertIn('SetWindowTheme(button, string.Empty, string.Empty)', source)
        self.assertIn('SetWindowTheme(pair.Key, null, null)', source)

    def test_single_tray_uses_separate_user_preference_and_keeps_core_tray_off(self):
        launcher = (ROOT / 'v0417/src/SimpleUpdate/LauncherContext.cs').read_text(encoding='utf-8')
        settings = (ROOT / 'v0417/src/SimpleUpdate/SettingsStore.cs').read_text(encoding='utf-8')
        host = (ROOT / 'v0417/src/SimpleUpdate/AdditionalSettingsHost.cs').read_text(encoding='utf-8')
        self.assertIn('LoadTrayIconEnabled', settings)
        self.assertIn('SaveTrayIconEnabled', settings)
        self.assertIn('OnTraySettingChanged', launcher)
        self.assertIn('EnsureLegacyTrayDisabled', launcher)
        self.assertIn('trayPreferenceEnabled', host)
        self.assertIn('trayPreferenceChanged', host)


if __name__ == '__main__':
    unittest.main()
