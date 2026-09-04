from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DPop0417Rev19ZapretCleanupContractTests(unittest.TestCase):
    def test_status_detail_is_content_aware_and_capped_in_idle_state(self):
        source = (ROOT / 'v0417/src/SimpleUpdate/ZapretResponsiveLayoutHost.cs').read_text(encoding='utf-8')
        self.assertIn('CompactIdleStatusDetailHeight', source)
        self.assertIn('ExpandedStatusDetailHeight', source)
        self.assertIn('ComputeStatusDetailHeight', source)
        self.assertIn('NativeBridge.ReadWindowText(statusDetail)', source)

    def test_secondary_commands_are_removed_from_primary_rows(self):
        source = (ROOT / 'v0417/src/SimpleUpdate/ZapretResponsiveLayoutHost.cs').read_text(encoding='utf-8')
        self.assertIn('StrategyRowButtonIds = { 1701, 1713, 1714 }', source)
        self.assertIn('PrimaryUpdateButtonIds = { 1724, 1725 }', source)
        self.assertIn('CompactUpdateToggleButtonIds = { 1716, 1717 }', source)
        self.assertIn('PrimaryAdditionalRowButtonIds = { 1704, 1705, 1707, 1708 }', source)
        self.assertIn('ServiceActionButtonIds = { 1703, 1702, 1710, 1711 }', source)

    def test_service_actions_have_their_own_compact_heading_and_row(self):
        layout = (ROOT / 'v0417/src/SimpleUpdate/ZapretResponsiveLayoutHost.cs').read_text(encoding='utf-8')
        self.assertIn('ServiceActionsHeadingId', layout)
        self.assertIn('Сервисные действия', layout)
        self.assertIn('Service actions', layout)
        self.assertIn('EnsureServiceActionsHeading', layout)
        self.assertIn('LayoutCompactServiceRow', layout)

    def test_auto_controls_use_text_sized_compact_cells_not_equal_flex_widths(self):
        source = (ROOT / 'v0417/src/SimpleUpdate/ZapretResponsiveLayoutHost.cs').read_text(encoding='utf-8')
        self.assertIn('LayoutUpdateRowWithCompactToggles', source)
        self.assertIn('MeasurePreferredWidth', source)
        self.assertIn('CompactUpdateToggleButtonIds', source)

    def test_rev19_has_real_installed_multisize_visual_and_tray_gate(self):
        workflow_path = ROOT / '.github/workflows/DPopCleaner_0.4.17_REV19_ZAPRET_CLEANUP.yml'
        self.assertTrue(workflow_path.is_file(), 'rev.19 dedicated installed workflow is required')
        workflow = workflow_path.read_text(encoding='utf-8')
        self.assertIn('dpop0417_rev19_installed_zapret_cleanup_smoke.ps1', workflow)
        self.assertIn('DPopCleaner_Setup_0.4.17.exe', workflow)
        smoke = (ROOT / 'tools/dpop0417_rev19_zapret_cleanup_smoke.ps1').read_text(encoding='utf-8')
        for token in ('1024', '1366', '1680', '1908', 'Service actions', 'Сервисные действия', 'GetWindowTheme', 'Assert-TrayState', 'rev19-zapret-cleanup-report.json'):
            self.assertIn(token, smoke)


if __name__ == '__main__':
    unittest.main()
