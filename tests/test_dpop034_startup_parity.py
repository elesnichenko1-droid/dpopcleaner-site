#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class StartupParityTests(unittest.TestCase):
    def test_manager_classifies_and_only_manages_safe_entries(self):
        header = (ROOT / 'v034_overlay/modules/StartupManager.h').read_text(encoding='utf-8')
        source = (ROOT / 'v034_overlay/modules/StartupManager.cpp').read_text(encoding='utf-8')
        for marker in (
            'category', 'recommendation', 'protectedEntry', 'manageable', 'enabled',
            'SetEnabled', 'Системный', 'Драйвер / vendor', 'Пользовательский',
            'Лучше не отключать', 'DisabledStartup',
        ):
            self.assertIn(marker, header + source)
        self.assertIn('HKEY_CURRENT_USER', source)
        self.assertIn('if (!entry.manageable)', source)

    def test_page_has_icons_classification_toggle_and_safe_adaptation(self):
        page = (ROOT / 'v034_overlay/ui/pages/StartupPage.cpp').read_text(encoding='utf-8')
        for marker in (
            'SHGFI_SYSICONINDEX', 'ListView_SetImageList', 'Состояние', 'Тип', 'Рекомендация',
            'Отключить выбранное', 'Включить выбранное', 'Адаптировать',
            'Системные компоненты, драйверы и HKLM-записи оставлены без изменений.',
            'ComputePageContentTop',
        ):
            self.assertIn(marker, page)
        self.assertNotIn('const int top = 68;', page)


if __name__ == '__main__':
    unittest.main()
