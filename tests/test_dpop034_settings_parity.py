#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class SettingsParityTests(unittest.TestCase):
    def test_migrator_generates_real_legacy_settings_backend(self):
        source = (ROOT / 'tools/dpop034_migrate.py').read_text(encoding='utf-8')
        for marker in (
            'transform_fullcore_header',
            'transform_fullcore_source',
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
            'clean_exclusions',
            'RUNASADMIN',
        ):
            self.assertIn(marker, source)

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
        self.assertIn('SetAlwaysRunAsAdmin', page)
        self.assertIn('SetRunAtStartup', page)
        self.assertNotIn('const int top = 54;', page)


if __name__ == '__main__':
    unittest.main()
