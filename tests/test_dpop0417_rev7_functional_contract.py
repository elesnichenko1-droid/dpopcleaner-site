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
        for label in (
            'Фоновый контроль мусора каждые 30 минут',
            'Быстрый DPopGuard-скан при запуске',
            'Проверять кэш Windows Update при запуске',
            'Работать в трее и отслеживать новые установки',
            'Автозапуск DPopCleaner вместе с Windows',
            'Запускать приложение от имени администратора',
            'Включить автообновление приложения',
            'Проверить обновления',
            'Лицензия',
        ):
            self.assertIn(label, host)
        self.assertIn('LegacySettingProxy', host)
        self.assertIn('FindChildByText', bridge)
        self.assertIn('GetSettingsScrollBounds', bridge)

    def test_existing_zapret_center_gets_repairs_and_1102_actions_without_new_ui(self):
        path = ROOT / 'v0417/src/SimpleUpdate/ZapretEnhancementHost.cs'
        self.assertTrue(path.is_file(), 'ZapretEnhancementHost.cs must extend the frozen Zapret page')
        text = path.read_text(encoding='utf-8')
        for token in (
            'Починка трансляции',
            'Починка подключения',
            'Игровой фильтр 1.10.2',
            'Менеджер 1.10.2',
            'ZapretScreenFix.exe',
            'game_filter.enabled',
            'service.bat',
            'status_zapret',
        ):
            self.assertIn(token, text)
        launcher = (ROOT / 'v0417/src/SimpleUpdate/LauncherContext.cs').read_text(encoding='utf-8')
        self.assertIn('ZapretEnhancementHost', launcher)

    def test_existing_zapret_actions_use_one_compact_two_by_two_host(self):
        host = (ROOT / 'v0417/src/SimpleUpdate/ZapretEnhancementHost.cs').read_text(encoding='utf-8')
        self.assertIn('private IntPtr _actionGrid;', host)
        self.assertNotIn('private IntPtr _updateRow;', host)
        self.assertNotIn('private IntPtr _toolsRow;', host)
        self.assertIn('CompactGridColumns = 2', host)
        self.assertIn('CompactGridRows = 2', host)
        self.assertIn('ButtonGap = 8', host)
        self.assertIn('PositionActionGrid', host)
        self.assertEqual(host.count('CreateHost();'), 1)

        smoke = (ROOT / 'tools/dpop0417_rev7_installed_ui_smoke.ps1').read_text(encoding='utf-8')
        self.assertIn('Zapret actions overlap', smoke)
        self.assertIn('Existing Zapret control overlaps compact action grid', smoke)

    def test_installed_rev7_smoke_reproduces_requested_behaviors(self):
        smoke_path = ROOT / 'tools/dpop0417_rev7_installed_ui_smoke.ps1'
        self.assertTrue(smoke_path.is_file(), 'rev.7 installed UI behavior smoke is required')
        smoke = smoke_path.read_text(encoding='utf-8')
        for token in (
            'SW_HIDE',
            'SW_SHOW',
            '5%',
            '95%',
            'Починка трансляции',
            'Починка подключения',
            'Игровой фильтр 1.10.2',
            'Менеджер 1.10.2',
            'Автообновление приложения',
            'WM_MOUSEWHEEL',
            'v0.2.11 BETA',
        ):
            self.assertIn(token, smoke)
        install_smoke = (ROOT / 'tools/dpop0417_install_smoke.ps1').read_text(encoding='utf-8')
        self.assertIn('dpop0417_rev7_installed_ui_smoke.ps1', install_smoke)


if __name__ == '__main__':
    unittest.main()
