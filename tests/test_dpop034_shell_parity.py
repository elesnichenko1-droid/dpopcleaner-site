from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / 'tools' / 'dpop034_migrate.py'


def load_module():
    spec = importlib.util.spec_from_file_location('dpop034_migrate_shell_parity', MODULE_PATH)
    module = importlib.util.module_from_spec(spec)
    assert spec and spec.loader
    spec.loader.exec_module(module)
    return module


SHELL_CMAKE = r'''add_executable(DPopCleaner WIN32
  src/ui/Shell.cpp
  src/ui/pages/ApplicationsPage.cpp
  src/ui/pages/WindowsPage.cpp
  src/ui/pages/SettingsPage.cpp
  src/modules/StartupManager.cpp
  src/update/UpdateClient.cpp
)
'''


class ShellParityTests(unittest.TestCase):
    def test_overlay_restores_old_sidebar_and_missing_sections(self):
        shell_model = (ROOT / 'v034_overlay/ui/ShellModel.cpp').read_text(encoding='utf-8')
        layout = (ROOT / 'v034_overlay/ui/Layout.h').read_text(encoding='utf-8')
        shell = (ROOT / 'v034_overlay/ui/Shell.cpp').read_text(encoding='utf-8')
        for label in ('Автозагрузка', 'Обновления', 'Настройки'):
            self.assertIn(label, shell_model)
        self.assertIn('std::array<TabDescriptor, 13>', shell_model)
        self.assertIn('std::array<Box, 13> navButtons', layout)
        self.assertIn('StartupPage', shell)
        self.assertIn('UpdatesPage', shell)
        self.assertIn('std::array<HWND, 13> navButtons_', shell)
        self.assertNotIn('tabGap', shell)
        self.assertNotIn('tabWidth', shell)

    def test_startup_and_updates_pages_are_real_overlay_sources(self):
        startup = (ROOT / 'v034_overlay/ui/pages/StartupPage.cpp').read_text(encoding='utf-8')
        updates = (ROOT / 'v034_overlay/ui/pages/UpdatesPage.cpp').read_text(encoding='utf-8')
        self.assertIn('EnumerateAll', startup)
        self.assertIn('ms-settings:startupapps', startup)
        self.assertIn('CheckForUpdates', updates)
        self.assertIn('DownloadPackage', updates)
        self.assertIn('PrepareAndLaunchUpdater', updates)

    def test_cmake_transform_registers_both_pages_idempotently(self):
        mod = load_module()
        updated = mod.transform_cmake_for_shell_parity(SHELL_CMAKE)
        self.assertIn('src/ui/pages/StartupPage.cpp', updated)
        self.assertIn('src/ui/pages/UpdatesPage.cpp', updated)
        self.assertEqual(updated, mod.transform_cmake_for_shell_parity(updated))


if __name__ == '__main__':
    unittest.main()
