from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417Rev7FunctionalContractTests(unittest.TestCase):
    def test_launcher_does_not_treat_temporary_window_invisibility_as_exit(self):
        text = (ROOT / 'v0417/src/SimpleUpdate/LauncherContext.cs').read_text(encoding='utf-8')
        self.assertNotIn('else if (_mainWindowWasVisible)', text)
        self.assertIn('Temporary visibility changes are not exit signals', text)
        self.assertIn('if (_core.HasExited)', text)

    def test_existing_ram_threshold_combo_is_extended_in_place_to_5_through_95(self):
        native = (ROOT / 'v0417/src/SimpleUpdate/NativeBridge.cs').read_text(encoding='utf-8')
        launcher = (ROOT / 'v0417/src/SimpleUpdate/LauncherContext.cs').read_text(encoding='utf-8')
        self.assertIn('RamThresholdComboId = 1956', native)
        self.assertIn('EnsureRamThresholdRange', native)
        self.assertIn('5%', native)
        self.assertIn('95%', native)
        self.assertIn('EnsureRamThresholdRange(_mainWindow)', launcher)

    def test_settings_scroll_host_contains_existing_left_settings_and_application_updates(self):
        host = (ROOT / 'v0417/src/SimpleUpdate/AdditionalSettingsHost.cs').read_text(encoding='utf-8')
        bridge = (ROOT / 'v0417/src/SimpleUpdate/NativeBridge.cs').read_text(encoding='utf-8')
        for label in ('Включить автообновление приложения','Проверить обновления','Лицензия',
                      'Enable application auto-updates','Check for updates','License'):
            self.assertIn(label, host)
        self.assertIn('LegacySettingProxy', host)
        self.assertIn('FindSettingsCheckboxes', host)
        self.assertIn('ReadWindowText(setting.LegacyHandle)', host)
        self.assertIn('WriteWindowText(setting.ProxyHandle', host)
        self.assertNotIn('LegacySettingTexts', host)
        self.assertIn('FindSettingsCheckboxes', bridge)
        self.assertIn('IsSettingsPageVisible', bridge)
        self.assertIn('GetSettingsScrollBounds', bridge)

    def test_existing_zapret_center_gets_repairs_and_1102_actions_without_new_ui(self):
        path = ROOT / 'v0417/src/SimpleUpdate/ZapretEnhancementHost.cs'
        self.assertTrue(path.is_file())
        text = path.read_text(encoding='utf-8')
        for token in ('Починка трансляции','Починка подключения','Игровой фильтр 1.10.2','Менеджер 1.10.2','ZapretScreenFix.exe','game_filter.enabled','service.bat','status_zapret'):
            self.assertIn(token, text)
        launcher = (ROOT / 'v0417/src/SimpleUpdate/LauncherContext.cs').read_text(encoding='utf-8')
        self.assertIn('ZapretEnhancementHost', launcher)

    def test_existing_zapret_actions_use_one_compact_safe_toolbar(self):
        host = (ROOT / 'v0417/src/SimpleUpdate/ZapretEnhancementHost.cs').read_text(encoding='utf-8')
        self.assertIn('private IntPtr _actionToolbar;', host)
        for handle in ('_repairBroadcastButton', '_repairConnectionButton', '_gameFilterButton', '_managerButton'):
            self.assertIn(handle, host)
        self.assertIn('PositionActionToolbar', host)
        self.assertIn('LayoutActionButtons', host)
        self.assertIn('TextRenderer.MeasureText', host)
        self.assertIn('availableWidth', host)
        self.assertNotIn('ToolbarWidth = 709', host)
        self.assertIn('Дополнительно', host)
        self.assertIn('Тесты', host)
        smoke = (ROOT / 'tools/dpop0417_rev7_installed_ui_smoke.ps1').read_text(encoding='utf-8')
        self.assertIn('Zapret actions overlap', smoke)
        self.assertIn('Existing Zapret control overlaps compact action toolbar', smoke)

    def test_frozen_zapret_update_controls_are_bridge_owned(self):
        host = (ROOT / 'v0417/src/SimpleUpdate/ZapretEnhancementHost.cs').read_text(encoding='utf-8')
        program = (ROOT / 'v0417/src/SimpleUpdate/Program.cs').read_text(encoding='utf-8')
        self.assertIn('LegacyCheckVersionButtonId', host)
        self.assertIn('LegacyDownloadButtonId', host)
        self.assertIn('CreateLegacyUpdateProxy', host)
        self.assertIn('private IntPtr _updateToolbar;', host)
        self.assertIn('Проверить версию', host)
        self.assertIn('Скачать и установить', host)
        self.assertIn('NativeBridge.ShowWindow(_legacyCheckVersionButton, NativeBridge.SW_HIDE)', host)
        self.assertIn('NativeBridge.ShowWindow(_legacyDownloadButton, NativeBridge.SW_HIDE)', host)
        self.assertIn('LegacyZapretUpdater.Run(_applicationRoot)', host)
        self.assertIn('.service', host)
        self.assertIn('version.txt', host)
        self.assertNotIn('zapretProxyTimer', program)
        self.assertNotIn('ZapretUpdateProxyHost zapretUpdateProxy', program)

    def test_rev16_zapret_preserves_native_version_source_and_follows_selected_theme(self):
        visual_path = ROOT / 'v0417/src/SimpleUpdate/ZapretVisualPolishHost.cs'
        self.assertTrue(visual_path.is_file())
        visual = visual_path.read_text(encoding='utf-8')
        launcher = (ROOT / 'v0417/src/SimpleUpdate/LauncherContext.cs').read_text(encoding='utf-8')
        native = (ROOT / 'v0417/src/SimpleUpdate/NativeBridge.cs').read_text(encoding='utf-8')
        prepare = (ROOT / 'tools/dpop0417_prepare_zapret.ps1').read_text(encoding='utf-8')
        stage = (ROOT / 'tools/dpop0417_stage.ps1').read_text(encoding='utf-8')

        # The immutable core still owns the visible version row and reads
        # Zapret\\utils\\dpop_version.txt itself. Rev.16 only unifies presentation.
        for forbidden in (
            'VersionStatusProxyId', 'CreateVersionStatusProxy', 'AttachToExistingVersionStatus',
            'SelectExistingStatusEdit', 'RefreshExistingVersionStatus', 'RewriteVersionStatusText',
            '_versionStatus', 'WriteWindowText(_versionStatus', 'SetWindowSubclass(_versionStatus'
        ):
            self.assertNotIn(forbidden, visual)
        self.assertIn('EnsureUnifiedZapretButtons();', visual)
        self.assertIn('NativeBridge.IsDarkThemeSelected(_parent)', visual)
        self.assertIn('DarkButtonBrush', visual)
        self.assertIn('LightButtonBrush', visual)
        self.assertIn('BS_OWNERDRAW', visual)
        self.assertIn('WM_DRAWITEM', visual)
        self.assertIn('DrawOwnerButton', visual)
        self.assertIn('prefix + version', visual)
        self.assertIn('ReadSettingsThemeSelection', native)
        self.assertIn('ZapretVisualPolishHost', launcher)

        self.assertIn("utils/dpop_version.txt", prepare)
        self.assertIn("Copy-Item -LiteralPath $pinnedVersionFile", prepare)
        self.assertIn("Prepared frozen-core dpop_version.txt", prepare)
        self.assertIn("utils/dpop_version.txt", stage)
        self.assertIn("Frozen-core Zapret version source mismatch", stage)

    def test_frozen_zapret_download_button_has_compatibility_updater(self):
        program = (ROOT / 'v0417/src/SimpleUpdate/Program.cs').read_text(encoding='utf-8')
        updater = ROOT / 'v0417/src/SimpleUpdate/LegacyZapretUpdater.cs'
        installer = (ROOT / 'release/DPopCleaner_0.4.17.iss').read_text(encoding='utf-8')
        smoke = (ROOT / 'tools/dpop0417_rev7_installed_ui_smoke.ps1').read_text(encoding='utf-8')
        self.assertTrue(updater.is_file())
        updater_text = updater.read_text(encoding='utf-8')
        self.assertIn('DPopUpdate.exe', program)
        self.assertIn('LegacyZapretUpdater.Run', program)
        self.assertIn('Flowseal/zapret-discord-youtube', updater_text)
        self.assertIn('DestName: "DPopUpdate.exe"', installer)
        self.assertIn('Legacy Zapret updater compatibility module: PASS', smoke)

    def test_installed_rev7_smoke_reproduces_requested_behaviors(self):
        smoke_path = ROOT / 'tools/dpop0417_rev7_installed_ui_smoke.ps1'
        self.assertTrue(smoke_path.is_file())
        smoke = smoke_path.read_text(encoding='utf-8')
        for token in ('SW_HIDE','SW_SHOW','5%','95%','Починка трансляции','Починка подключения','Игровой фильтр 1.10.2','Менеджер 1.10.2','Автообновление приложения','WM_MOUSEWHEEL','v0.2.11 BETA'):
            self.assertIn(token, smoke)
        install_smoke = (ROOT / 'tools/dpop0417_install_smoke.ps1').read_text(encoding='utf-8')
        self.assertIn('dpop0417_rev7_installed_ui_smoke.ps1', install_smoke)


if __name__ == '__main__':
    unittest.main()
