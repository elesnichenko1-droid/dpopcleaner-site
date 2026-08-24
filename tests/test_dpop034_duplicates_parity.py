#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class DuplicatesParityTests(unittest.TestCase):
    def test_duplicates_page_has_reference_copy_roles_and_safe_deletion(self):
        page = (ROOT / 'v034_overlay/ui/pages/DuplicatesPage.cpp').read_text(encoding='utf-8')
        for marker in (
            'Эталон группы (оставить)', 'Дубликат ', 'Безопасные копии', 'referenceCopy',
            'protectedPath', 'IsPathExcluded', 'Системный/исключённый путь',
            'MoveToRecycleBin', 'SHGFI_SYSICONINDEX', 'ComputePageContentTop',
        ):
            self.assertIn(marker, page)
        self.assertIn('Эталон группы нельзя отправить в Корзину', page)
        self.assertNotIn('const int top = 54;', page)


if __name__ == '__main__':
    unittest.main()
