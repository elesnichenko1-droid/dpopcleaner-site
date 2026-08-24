#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class DiskAndApplicationsParityTests(unittest.TestCase):
    def test_disk_page_is_a_real_icon_aware_browser_with_system_warnings(self):
        page = (ROOT / 'v034_overlay/ui/pages/DiskPage.cpp').read_text(encoding='utf-8')
        for marker in (
            'SHGFI_SYSICONINDEX', 'ListView_SetImageList', 'Navigate(', 'Browse()', 'NM_DBLCLK',
            'Крупные файлы', 'Системный путь', 'ScanLargeFiles', 'OpenPathInExplorer',
            'ComputePageContentTop',
        ):
            self.assertIn(marker, page)
        self.assertNotIn('const int top = 54;', page)

    def test_applications_page_has_icons_uninstall_leftovers_and_real_update_check(self):
        page = (ROOT / 'v034_overlay/ui/pages/ApplicationsPage.cpp').read_text(encoding='utf-8')
        for marker in (
            'SHGFI_SYSICONINDEX', 'DisplayIconPath', 'Проверить обновление', 'winget.exe',
            'upgrade --name', 'LaunchUninstaller', 'FindLeftovers', 'MoveLeftoversToRecycleBin',
            'OpenPathInExplorer', 'ComputePageContentTop',
        ):
            self.assertIn(marker, page)
        self.assertIn('HIGH', page)
        self.assertIn('review', page)
        self.assertNotIn('const int top = 54;', page)


if __name__ == '__main__':
    unittest.main()
