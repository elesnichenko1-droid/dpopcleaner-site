from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHELL = ROOT / "v035_overlay" / "ui" / "Shell.cpp"
TRAY_H = ROOT / "v035_overlay" / "ui" / "TrayIcon.h"
TRAY_CPP = ROOT / "v035_overlay" / "ui" / "TrayIcon.cpp"
SETTINGS = ROOT / "v035_overlay" / "modules" / "SettingsStore.h"


class RuntimeContractTests(unittest.TestCase):
    def test_shell_consumes_committed_settings(self):
        text = SHELL.read_text(encoding="utf-8")
        for marker in (
            "LoadAppSettings",
            "SetActiveSettings",
            "ApplyRuntimeSettings",
            "checkUpdatesAtStartup",
            "quickGuardAtStartup",
            "checkUpdateCacheAtStartup",
            "backgroundJunkMonitor",
            "memoryAutoTrimEnabled",
            "memoryAutoTrimIntervalMinutes",
            "memoryAutoTrimPercent",
            "MemoryScope::Advanced",
        ):
            self.assertIn(marker, text)

    def test_tray_is_real_shell_notify_icon(self):
        header = TRAY_H.read_text(encoding="utf-8")
        source = TRAY_CPP.read_text(encoding="utf-8")
        self.assertIn("class TrayIcon", header)
        self.assertIn("Shell_NotifyIconW", source)
        self.assertIn("NIM_ADD", source)
        self.assertIn("NIM_DELETE", source)
        self.assertIn("TrackPopupMenu", source)
        self.assertIn("Открыть DPopCleaner", source)
        self.assertIn("Выход", source)

    def test_close_behavior_is_not_decorative(self):
        text = SHELL.read_text(encoding="utf-8")
        self.assertIn("CloseBehavior::Exit", text)
        self.assertIn("CloseBehavior::MinimizeToTray", text)
        self.assertIn("CloseBehavior::Ask", text)
        self.assertIn("WM_CLOSE", text)
        self.assertIn("HideToTray", text)
        self.assertIn("RestoreFromTray", text)

    def test_active_settings_have_thread_safe_process_state(self):
        text = SETTINGS.read_text(encoding="utf-8")
        self.assertIn("ActiveSettings", text)
        self.assertIn("SetActiveSettings", text)
        self.assertIn("std::scoped_lock", text)


if __name__ == "__main__":
    unittest.main()
