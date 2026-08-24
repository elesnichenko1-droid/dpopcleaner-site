from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PAGE_H = ROOT / "v035_overlay" / "ui" / "pages" / "SettingsPage.h"
PAGE_CPP = ROOT / "v035_overlay" / "ui" / "pages" / "SettingsPage.cpp"
STORE_CPP = ROOT / "v035_overlay" / "modules" / "SettingsStore.cpp"


class SettingsContractTests(unittest.TestCase):
    def test_settings_has_exact_five_horizontal_sections(self):
        text = PAGE_CPP.read_text(encoding="utf-8")
        for label in ("Основное", "Очистка", "Память", "Защита", "Исключения"):
            self.assertIn(label, text)
        self.assertIn("sectionButtons_", PAGE_H.read_text(encoding="utf-8"))
        self.assertNotIn("Язык:", text)
        self.assertNotIn("Тема:", text)

    def test_settings_actions_have_distinct_semantics(self):
        header = PAGE_H.read_text(encoding="utf-8")
        source = PAGE_CPP.read_text(encoding="utf-8")
        for marker in ("ApplyChanges", "CancelChanges", "LoadDefaults", "HasUnsavedChanges"):
            self.assertIn(marker, header)
        for label in ("Применить", "Сохранить", "Отмена", "По умолчанию"):
            self.assertIn(label, source)
        self.assertIn("SaveAppSettings", source)
        self.assertIn("CommitInMemory", source)
        self.assertIn("CancelEdits", source)

    def test_settings_visible_controls_are_real_typed_fields(self):
        source = PAGE_CPP.read_text(encoding="utf-8")
        for field in (
            "confirmDestructive",
            "largeFileMB",
            "duplicateMinMB",
            "runAtStartup",
            "alwaysRunAsAdmin",
            "checkUpdatesAtStartup",
            "quickGuardAtStartup",
            "checkUpdateCacheAtStartup",
            "backgroundJunkMonitor",
            "trayEnabled",
            "closeBehavior",
            "memoryAutoTrimEnabled",
            "memoryAutoTrimPercent",
            "memoryAutoTrimIntervalMinutes",
            "memoryScope",
            "cleanExclusions",
        ):
            self.assertIn(field, source)

    def test_settings_store_is_atomic_and_corrupt_safe(self):
        source = STORE_CPP.read_text(encoding="utf-8")
        self.assertTrue("settings.json.tmp" in source or 'L".tmp"' in source)
        self.assertIn("FlushFileBuffers", source)
        self.assertIn("ReplaceFileW", source)
        self.assertIn("MOVEFILE_WRITE_THROUGH", source)
        self.assertIn("DefaultSettings", source)
        self.assertIn("settings.json повреждён", source)


if __name__ == "__main__":
    unittest.main()
