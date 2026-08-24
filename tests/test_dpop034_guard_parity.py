#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class GuardParityTests(unittest.TestCase):
    def test_guard_backend_integrates_amsi_and_microsoft_defender(self):
        header = (ROOT / 'v034_overlay/modules/DPopGuard.h').read_text(encoding='utf-8')
        source = (ROOT / 'v034_overlay/modules/DPopGuard.cpp').read_text(encoding='utf-8')
        for marker in (
            'DefenderStatus', 'RunDefenderQuickScan', 'RunDefenderCustomScan',
            'MpCmdRun.exe', 'WinDefend', 'AmsiScanBuffer', 'CREATE_NO_WINDOW',
            'Protection history',
        ):
            self.assertIn(marker, header + source)

    def test_guard_page_exposes_real_providers_and_quarantine_actions(self):
        page = (ROOT / 'v034_overlay/ui/pages/GuardPage.cpp').read_text(encoding='utf-8')
        for marker in (
            'DPopGuard Quick Scan', 'Defender Quick Scan', 'Проверить файл', 'Проверить папку',
            'В карантин', 'Открыть карантин', 'Windows Security', 'CreateProgress',
            'RunDefenderCustomScan', 'ScanFolderWithAmsi', 'QuarantineFile', 'ComputePageContentTop',
        ):
            self.assertIn(marker, page)
        self.assertNotIn('const int top = 54;', page)


if __name__ == '__main__':
    unittest.main()
