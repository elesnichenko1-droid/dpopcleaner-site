from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHELL = ROOT / 'v034_overlay' / 'ui' / 'Shell.cpp'
GUARD = ROOT / 'v034_overlay' / 'ui' / 'pages' / 'GuardPage.h'
WINDOWS_H = ROOT / 'v034_overlay' / 'ui' / 'pages' / 'WindowsPage.h'
WINDOWS_CPP = ROOT / 'v034_overlay' / 'ui' / 'pages' / 'WindowsPage.cpp'
UPDATES_H = ROOT / 'v034_overlay' / 'ui' / 'pages' / 'UpdatesPage.h'


class StartupHooksTests(unittest.TestCase):
    def test_saved_startup_settings_execute_real_non_destructive_actions(self):
        for path in (SHELL, GUARD, WINDOWS_H, WINDOWS_CPP, UPDATES_H):
            self.assertTrue(path.is_file(), f'{path.name} missing')
        shell = SHELL.read_text(encoding='utf-8')
        guard = GUARD.read_text(encoding='utf-8')
        windows_h = WINDOWS_H.read_text(encoding='utf-8')
        windows_cpp = WINDOWS_CPP.read_text(encoding='utf-8')
        updates_h = UPDATES_H.read_text(encoding='utf-8')

        self.assertIn('RunQuickScanAtStartup', guard)
        self.assertIn('CheckUpdateCacheAtStartup', windows_h)
        self.assertIn('EstimateUpdateCacheBytes', windows_cpp)
        self.assertIn('Автоочистка не выполнялась', windows_cpp)
        self.assertIn('CheckAtStartup', updates_h)

        for marker in (
            'checkUpdatesAtStartup',
            'quickGuardAtStartup',
            'checkUpdateCacheAtStartup',
            'guardPage_.RunQuickScanAtStartup()',
            'windowsPage_.CheckUpdateCacheAtStartup()',
            'updatesPage_.CheckAtStartup()',
        ):
            self.assertIn(marker, shell)

        # Startup hooks must never silently trigger destructive maintenance.
        startup_block = shell[shell.index('case kRunStartupActionsMessage'):shell.index('case kOverviewLogChangedMessage')]
        self.assertNotIn('ClearUpdateCache', startup_block)
        self.assertNotIn('CleanSelected', startup_block)


if __name__ == '__main__':
    unittest.main()
