from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PAGE_H = ROOT / "v035_overlay" / "ui" / "pages" / "SettingsPage.h"
PAGE_CPP = ROOT / "v035_overlay" / "ui" / "pages" / "SettingsPage.cpp"
STORE_CPP = ROOT / "v035_overlay" / "modules" / "SettingsStore.cpp"


def test_settings_has_exact_five_horizontal_sections():
    text = PAGE_CPP.read_text(encoding="utf-8")
    for label in ("Основное", "Очистка", "Память", "Защита", "Исключения"):
        assert label in text
    assert "sectionButtons_" in PAGE_H.read_text(encoding="utf-8")
    assert "Язык:" not in text
    assert "Тема:" not in text


def test_settings_actions_have_distinct_semantics():
    header = PAGE_H.read_text(encoding="utf-8")
    source = PAGE_CPP.read_text(encoding="utf-8")
    for marker in ("ApplyChanges", "CancelChanges", "LoadDefaults", "HasUnsavedChanges"):
        assert marker in header
    for label in ("Применить", "Сохранить", "Отмена", "По умолчанию"):
        assert label in source
    assert "SaveAppSettings" in source
    assert "CommitInMemory" in source
    assert "CancelEdits" in source


def test_settings_visible_controls_are_real_typed_fields():
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
        assert field in source


def test_settings_store_is_atomic_and_corrupt_safe():
    source = STORE_CPP.read_text(encoding="utf-8")
    assert "settings.json.tmp" in source or 'L".tmp"' in source
    assert "FlushFileBuffers" in source
    assert "ReplaceFileW" in source
    assert "MOVEFILE_WRITE_THROUGH" in source
    assert "DefaultSettings" in source
    assert "settings.json повреждён" in source
