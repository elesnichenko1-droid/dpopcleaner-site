from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECOVERY = ROOT / 'v034_overlay' / 'ui' / 'RecoveryControls.cpp'
APPLICATIONS = ROOT / 'v034_overlay' / 'ui' / 'pages' / 'ApplicationsPage.cpp'


class VisualSpacingRegressionTests(unittest.TestCase):
    def test_page_heading_wraps_to_actual_client_width(self):
        self.assertTrue(RECOVERY.is_file(), 'R2 must overlay RecoveryControls.cpp to own heading layout')
        source = RECOVERY.read_text(encoding='utf-8')
        match = re.search(r'void DrawPageHeading\([\s\S]*?\n}\n', source)
        self.assertIsNotNone(match, 'DrawPageHeading implementation missing')
        body = match.group(0)
        self.assertIn('WindowFromDC', body)
        self.assertIn('GetClientRect', body)
        self.assertIn('DT_WORDBREAK', body)
        self.assertNotIn('x + 980', body)
        self.assertNotIn('DT_SINGLELINE', body)

    def test_applications_search_controls_start_below_panel_title(self):
        self.assertTrue(APPLICATIONS.is_file(), 'ApplicationsPage overlay missing')
        source = APPLICATIONS.read_text(encoding='utf-8')
        self.assertIn('const int searchPanelH = 72;', source)
        self.assertRegex(source, r'MoveWindow\(search_,\s*margin \+ 12,\s*top \+ 34,')
        self.assertRegex(source, r'MoveWindow\(count_,\s*listRight - 130,\s*top \+ 36,')
        self.assertIn('RECT search{margin, top, listRight, top + searchPanelH};', source)
        self.assertIn('RECT list{margin, top + searchPanelH + 2, listRight, height - margin};', source)


if __name__ == '__main__':
    unittest.main()
