from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / 'tools' / 'dpop034_migrate.py'


def load_module():
    spec = importlib.util.spec_from_file_location('dpop034_migrate', MODULE_PATH)
    module = importlib.util.module_from_spec(spec)
    assert spec and spec.loader
    spec.loader.exec_module(module)
    return module


OLD = r'''#include "ui/Theme.h"

void WorkspacePage::LayoutChildren() noexcept {
    if (!hwnd_) return;
    RECT rc{}; GetClientRect(hwnd_, &rc);
    const int width = rc.right;
    const int height = rc.bottom;
    const int margin = 18;
    const int gap = 10;
    const int buttonHeight = 38;
    const int visibleButtons = static_cast<int>(std::count_if(buttons_.begin(), buttons_.end(), [](HWND h){ return h && IsWindowVisible(h); }));
    const int buttonWidth = visibleButtons > 0 ? std::max(110, (width - margin * 2 - gap * (visibleButtons - 1)) / visibleButtons) : 0;

    MoveWindow(heading_, margin, 12, std::max(0, width - margin * 2), 38, TRUE);
    MoveWindow(status_, margin, 52, std::max(0, width - margin * 2), 30, TRUE);
    const int listBottom = height - margin - (visibleButtons ? buttonHeight + gap : 0);
    MoveWindow(list_, margin, 86, std::max(0, width - margin * 2), std::max(80, listBottom - 86), TRUE);

    int x = margin;
    for (HWND button : buttons_) {
        if (!button || !IsWindowVisible(button)) continue;
        MoveWindow(button, x, height - margin - buttonHeight, buttonWidth, buttonHeight, TRUE);
        x += buttonWidth + gap;
    }
}
'''


class WorkspaceLayoutTransformTests(unittest.TestCase):
    def test_transform_replaces_fixed_overlap_geometry_with_shared_regions(self):
        mod = load_module()
        updated = mod.transform_workspace_layout_text(OLD)
        self.assertIn('#include "ui/PageLayout.h"', updated)
        self.assertIn('ComputePageRegions', updated)
        self.assertIn('GetDpiForWindow', updated)
        self.assertNotIn('MoveWindow(status_, margin, 52', updated)
        self.assertNotIn('MoveWindow(list_, margin, 86', updated)
        self.assertIn('regions.description', updated)
        self.assertIn('regions.content', updated)
        self.assertIn('regions.actions', updated)

    def test_transform_fails_closed_when_old_layout_drifted(self):
        mod = load_module()
        with self.assertRaises(ValueError):
            mod.transform_workspace_layout_text('void WorkspacePage::LayoutChildren() noexcept { /* drift */ }')


if __name__ == '__main__':
    unittest.main()
