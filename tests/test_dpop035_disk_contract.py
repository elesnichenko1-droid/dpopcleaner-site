from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "v035_overlay" / "modules" / "DiskAnalyzer.h"
SOURCE = ROOT / "v035_overlay" / "modules" / "DiskAnalyzer.cpp"
TREE_H = ROOT / "v035_overlay" / "ui" / "controls" / "DiskTreeList.h"
TREE_CPP = ROOT / "v035_overlay" / "ui" / "controls" / "DiskTreeList.cpp"
PAGE_H = ROOT / "v035_overlay" / "ui" / "pages" / "DiskPage.h"
PAGE_CPP = ROOT / "v035_overlay" / "ui" / "pages" / "DiskPage.cpp"


def test_disk_model_is_authoritative_and_loop_safe():
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    for marker in (
        "DiskNodeId",
        "logicalBytes",
        "allocatedBytes",
        "fileCount",
        "directoryCount",
        "DiskScanSnapshot",
        "ParentPercent",
        "std::stop_token",
    ):
        assert marker in header
    assert "FILE_ATTRIBUTE_REPARSE_POINT" in source
    assert "stop_requested" in source
    assert "skip_permission_denied" in source


def test_tree_list_has_treesize_columns_and_green_parent_bar():
    header = TREE_H.read_text(encoding="utf-8")
    source = TREE_CPP.read_text(encoding="utf-8")
    assert "DiskTreeRow" in header
    assert "parentPercent" in header
    for label in ("Имя", "Размер", "Занято", "Файлы", "Папки", "% родителя", "Изменено"):
        assert label in source
    assert "NM_CUSTOMDRAW" in source or "NMLVCUSTOMDRAW" in source
    assert "palette.accent" in source
    assert "Безопасность" not in source


def test_disk_page_is_an_async_analyzer_not_file_browser():
    header = PAGE_H.read_text(encoding="utf-8")
    source = PAGE_CPP.read_text(encoding="utf-8")
    for marker in (
        "StartScan",
        "BuildVisibleRows",
        "ScanDiskTree",
        "StartAsync",
        "QueueApply",
        "Останавливаем сканирование",
        "Крупные файлы",
        "Выбрать каталог",
        "Проводник",
    ):
        assert marker in header or marker in source
    assert "BrowserEntry" not in header
    assert "Содержимое папки" not in source
    assert "Безопасность" not in source


def test_disk_page_never_deletes_arbitrary_files():
    source = PAGE_CPP.read_text(encoding="utf-8")
    forbidden = ("DeleteFileW", "remove_all(", "MoveToRecycleBin", "SHFileOperation")
    for marker in forbidden:
        assert marker not in source
