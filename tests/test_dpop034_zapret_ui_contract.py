from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / 'v034_overlay' / 'ui' / 'pages' / 'ZapretPage.h'
SOURCE = ROOT / 'v034_overlay' / 'ui' / 'pages' / 'ZapretPage.cpp'


class ZapretUiContractTests(unittest.TestCase):
    def test_page_has_strategy_picker_six_actions_and_non_overlapping_layout(self):
        self.assertTrue(HEADER.is_file(), 'ZapretPage.h overlay missing')
        self.assertTrue(SOURCE.is_file(), 'ZapretPage.cpp overlay missing')
        header = HEADER.read_text(encoding='utf-8')
        source = SOURCE.read_text(encoding='utf-8')

        self.assertIn('std::array<HWND, 6> buttons_', header)
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
            'Запустить выбранную',
            'Остановить bundled winws',
        ):
            self.assertIn(marker, source)

        self.assertNotIn('const int top = 54', source)
        self.assertNotIn('std::array<std::wstring_view, 4>', source)


if __name__ == '__main__':
    unittest.main()
