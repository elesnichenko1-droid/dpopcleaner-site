#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / 'tools' / 'dpop034_migrate.py'
SPEC = importlib.util.spec_from_file_location('dpop034_migrate_page_layout_test', MODULE_PATH)
assert SPEC and SPEC.loader
MOD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MOD)


SAMPLE_PAGE = '''#include "ui/pages/MemoryPage.h"\n\n#include "ui/Controls.h"\n#include "ui/Theme.h"\n\nvoid Layout(int width, int height) {\n    const int top = 54;\n    (void)width; (void)height; (void)top;\n}\nvoid Paint() {\n    const int top = 54;\n    (void)top;\n}\n'''


class SharedPageContentTopTests(unittest.TestCase):
    def test_transform_replaces_legacy_top_with_dpi_aware_shared_helper(self):
        updated = MOD.transform_page_content_top(SAMPLE_PAGE)
        self.assertIn('#include "ui/PageLayout.h"', updated)
        self.assertNotIn('const int top = 54;', updated)
        self.assertEqual(updated.count('ComputePageContentTop('), 2)
        self.assertIn('GetDpiForWindow(Hwnd())', updated)

    def test_transform_is_idempotent(self):
        once = MOD.transform_page_content_top(SAMPLE_PAGE)
        twice = MOD.transform_page_content_top(once)
        self.assertEqual(once, twice)

    def test_page_layout_overlay_exports_safe_content_top_helper(self):
        header = (ROOT / 'v034_overlay/ui/PageLayout.h').read_text(encoding='utf-8')
        source = (ROOT / 'v034_overlay/ui/PageLayout.cpp').read_text(encoding='utf-8')
        self.assertIn('ComputePageContentTop', header)
        self.assertIn('ComputePageContentTop', source)
        self.assertIn('descriptionHeight', source)


if __name__ == '__main__':
    unittest.main()
