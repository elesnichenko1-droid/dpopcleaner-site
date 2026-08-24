from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / 'v034_overlay' / 'ui' / 'pages' / 'ZapretPage.h'
SOURCE = ROOT / 'v034_overlay' / 'ui' / 'pages' / 'ZapretPage.cpp'
LAYOUT_HEADER = ROOT / 'v034_overlay' / 'ui' / 'pages' / 'ZapretPageLayout.h'
MANAGER_HEADER = ROOT / 'v034_overlay' / 'modules' / 'ZapretManager.h'
MANAGER_SOURCE = ROOT / 'v034_overlay' / 'modules' / 'ZapretManager.cpp'


class ZapretUiContractTests(unittest.TestCase):
    def test_page_has_strategy_picker_eight_actions_rtc_repair_and_non_overlapping_layout(self):
        for path in (HEADER, SOURCE, LAYOUT_HEADER, MANAGER_HEADER, MANAGER_SOURCE):
            self.assertTrue(path.is_file(), f'{path.name} overlay missing')
        header = HEADER.read_text(encoding='utf-8')
        source = SOURCE.read_text(encoding='utf-8')
        layout_header = LAYOUT_HEADER.read_text(encoding='utf-8')
        manager_header = MANAGER_HEADER.read_text(encoding='utf-8')
        manager_source = MANAGER_SOURCE.read_text(encoding='utf-8')

        self.assertIn('std::array<HWND, 8> buttons_', header)
        self.assertIn('std::array<ZapretRect, 8> actions', layout_header)
        self.assertIn('HWND strategyCombo_', header)
        self.assertIn('std::vector<dpop::zapret::StrategyEntry> strategies_', header)

        for marker in (
            'CreateDropDown',
            'ComputeZapretPageLayout',
            'EnumerateStrategies()',
            'LaunchStrategy(',
            'StopBundledWinws(',
            'OpenServiceManager(',
            'LaunchDefaultStrategy(',
            'OpenBundledFolder(',
            'RepairRtc(',
            'OpenZapretUpdatePage(',
            'Запустить выбранную',
            'Остановить bundled winws',
            'Исправление трансляций',
            'Проверить обновление Zapret',
        ):
            self.assertIn(marker, source + manager_header + manager_source)

        self.assertIn('ipconfig.exe', manager_source)
        self.assertIn('/flushdns', manager_source)
        self.assertIn('serviceRunning', manager_source)
        self.assertIn('IsBundledWinwsPath', manager_source)
        self.assertNotIn('const int top = 54', source)
        self.assertNotIn('std::array<std::wstring_view, 4>', source)


if __name__ == '__main__':
    unittest.main()
