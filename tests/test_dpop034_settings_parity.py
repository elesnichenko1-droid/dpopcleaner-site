#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class SettingsParityTests(unittest.TestCase):
    def test_fullcore_persists_real_legacy_preferences(self):
        header = (ROOT / 'v034_overlay/modules/FullCore.h').read_text(encoding='utf-8')
        source = (ROOT / 'v034_overlay/modules/FullCore.cpp').read_text(encoding='utf-8')
        for marker in (
            'alwaysRunAsAdmin',
            'checkUpdatesAtStartup',
            'quickGuardAtStartup',
            'checkUpdateCacheAtStartup',
            'minimizeToTray',
            'monitorInstallations',
            'backgroundJunkMonitor',
            'memoryAutoTrimPercent',
            'cleanExclusions',
            'SetAlwaysRunAsAdmin',
            'IsPathExcluded',
        ):
            self.assertIn(marker, header + source)
        self.assertIn('clean_exclusions', source)
        self.assertIn('RUNASADMIN', source)

    def test_settings_page_restores_legacy_controls_and_exclusions(self):
        page = (ROOT / 'v034_overlay/ui/pages/SettingsPage.cpp').read_text(encoding='utf-8')
        for label in (
            'Фоновый контроль мусора каждые 30 минут',
            'Quick DPopGuard-скан при запуске',
            'Проверять кэш Windows Update при запуске',
            'Проверять обновления DPopCleaner при запуске',
            'Работать в трее и отслеживать новые установки',
            'Запускать DPopCleaner вместе с Windows',
            'Всегда запускать DPopCleaner от администратора',
            'Добавить файл',
            'Добавить папку',
            'Удалить из списка',
            'Сохранить и применить',
        ):
            self.assertIn(label, page)
        self.assertIn('ComputePageContentTop', page)
        self.assertNotIn('const int top = 54;', page)


if __name__ == '__main__':
    unittest.main()
