from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "v035_overlay" / "modules" / "DiskAnalyzer.h"
SOURCE = ROOT / "v035_overlay" / "modules" / "DiskAnalyzer.cpp"
TREE_H = ROOT / "v035_overlay" / "ui" / "controls" / "DiskTreeList.h"
TREE_CPP = ROOT / "v035_overlay" / "ui" / "controls" / "DiskTreeList.cpp"
PAGE_H = ROOT / "v035_overlay" / "ui" / "pages" / "DiskPage.h"
PAGE_CPP = ROOT / "v035_overlay" / "ui" / "pages" / "DiskPage.cpp"


class DiskContractTests(unittest.TestCase):
    def test_disk_model_is_authoritative_progressive_and_loop_safe(self):
        header = HEADER.read_text(encoding="utf-8")
        source = SOURCE.read_text(encoding="utf-8")
        for marker in (
            "DiskNodeId",
            "logicalBytes",
            "allocatedBytes",
            "fileCount",
            "directoryCount",
            "DiskScanSnapshot",
            "DiskPartialCallback",
            "partialSnapshot",
            "ParentPercent",
            "std::stop_token",
        ):
            self.assertIn(marker, header)
        for marker in ("FILE_ATTRIBUTE_REPARSE_POINT", "stop_requested", "skip_permission_denied", "EmitPartial"):
            self.assertIn(marker, source)

    def test_tree_list_has_treesize_columns_and_green_parent_bar(self):
        header = TREE_H.read_text(encoding="utf-8")
        source = TREE_CPP.read_text(encoding="utf-8")
        self.assertIn("DiskTreeRow", header)
        self.assertIn("parentPercent", header)
        for label in ("Имя", "Размер", "Занято", "Файлы", "Папки", "% родителя", "Изменено"):
            self.assertIn(label, source)
        self.assertTrue("NM_CUSTOMDRAW" in source or "NMLVCUSTOMDRAW" in source)
        self.assertIn("palette.accent", source)
        self.assertNotIn("Безопасность", source)

    def test_disk_page_is_an_async_analyzer_not_file_browser(self):
        header = PAGE_H.read_text(encoding="utf-8")
        source = PAGE_CPP.read_text(encoding="utf-8")
        for marker in (
            "StartScan",
            "BuildVisibleRows",
            "ScanDiskTree",
            "StartAsync",
            "QueueApply",
            "partialSnapshot",
            "scanGeneration_",
            "Останавливаем сканирование",
            "Крупные файлы",
            "Выбрать каталог",
            "Проводник",
        ):
            self.assertTrue(marker in header or marker in source, marker)
        self.assertNotIn("BrowserEntry", header)
        self.assertNotIn("Содержимое папки", source)
        self.assertNotIn("Безопасность", source)

    def test_disk_page_never_deletes_arbitrary_files(self):
        source = PAGE_CPP.read_text(encoding="utf-8")
        for marker in ("DeleteFileW", "remove_all(", "MoveToRecycleBin", "SHFileOperation"):
            self.assertNotIn(marker, source)


if __name__ == "__main__":
    unittest.main()
