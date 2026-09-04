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
        self.assertIn('Scale(170, scale)', source)

    def test_compact_service_row_measures_actual_button_font_and_never_ellipsizes(self):
        layout = (ROOT / 'v0417/src/SimpleUpdate/ZapretResponsiveLayoutHost.cs').read_text(encoding='utf-8')
        polish = (ROOT / 'v0417/src/SimpleUpdate/ZapretVisualPolishHost.cs').read_text(encoding='utf-8')
        self.assertIn('NativeBridge.WM_GETFONT', layout)
        self.assertIn('Font.FromHfont', layout)
        self.assertIn('MeasurePreferredWidth', layout)
        self.assertNotIn('DT_END_ELLIPSIS', polish)

    def test_remove_services_probe_rejects_blank_first_paint(self):
        probe = (ROOT / 'tools/dpop0417_rev19_remove_services_probe.ps1').read_text(encoding='utf-8')
        self.assertIn('Assert-FirstPaint', probe)
        self.assertIn('REV19_FIRST_PAINT_OK', probe)
        self.assertIn('rev19-proxy-{0}-button.png', probe)
        self.assertIn('REV19_FIRST_PAINT_DIAG', probe)
        self.assertIn('Assert-FirstPaint $buttonPath', probe)

    def test_primary_screenshot_composites_launcher_owned_proxy_buttons(self):
        smoke = (ROOT / 'tools/dpop0417_rev19_zapret_cleanup_smoke.ps1').read_text(encoding='utf-8')
        self.assertIn('Capture-CompositeWindow', smoke)
        self.assertIn('Capture-ProxyBitmap', smoke)
        self.assertIn('$ProxyButtonIds', smoke)
        self.assertIn('$child.OwnerPid -eq $LauncherPid', smoke)
        self.assertIn('DrawImageUnscaled', smoke)
        self.assertIn('REV19_PRIMARY_COMPOSITE_OK', smoke)
        self.assertIn('Capture-CompositeWindow $window $shot $children $launcher.Id', smoke)

    def test_zapret_geometry_is_applied_before_bridge_painting(self):
        launcher = (ROOT / 'v0417/src/SimpleUpdate/LauncherContext.cs').read_text(encoding='utf-8')
        start = launcher.index('private void UpdateZapretEnhancements()')
        end = launcher.index('private void UpdateSettingsEnhancements()', start)
        method = launcher[start:end]
        layout_pos = method.index('new ZapretResponsiveLayoutHost')
        polish_pos = method.index('new ZapretVisualPolishHost')
        self.assertLess(layout_pos, polish_pos, 'responsive geometry must be applied before bridge visual painting')

    def test_service_heading_uses_same_process_host_and_current_theme(self):
        layout = (ROOT / 'v0417/src/SimpleUpdate/ZapretResponsiveLayoutHost.cs').read_text(encoding='utf-8')
        polish = (ROOT / 'v0417/src/SimpleUpdate/ZapretVisualPolishHost.cs').read_text(encoding='utf-8')
        self.assertIn('ServiceActionsHeadingHostId', layout)
        self.assertIn('_serviceActionsHeadingHost', layout)
        self.assertIn('SS_LEFTNOWORDWRAP', layout)
        self.assertIn('CreateWindowEx(0, "Static", string.Empty', layout)
        self.assertIn('_serviceActionsHeadingHost, new IntPtr(ServiceActionsHeadingId)', layout)
        self.assertIn('Scale(180, scale)', layout)
        self.assertIn('Scale(130, scale)', layout)
        self.assertIn('WM_CTLCOLORSTATIC', polish)
        self.assertIn('EnsureServiceHeadingThemeHost', polish)
        self.assertIn('GetParent(heading)', polish)
        self.assertIn('DrawServiceHeading', polish)
        self.assertIn('DarkPageBrush', polish)
        self.assertIn('LightPageBrush', polish)

    def test_compact_rows_replace_legacy_ellipsis_captions_with_full_labels(self):
        source = (ROOT / 'v0417/src/SimpleUpdate/ZapretVisualPolishHost.cs').read_text(encoding='utf-8')
        for token in ('Автообновление', 'Автозапуск Zapret', 'Auto-update', 'Zapret autostart', 'Удалить сервисы', 'Remove services', 'Диагностика', 'Diagnostics'):
            self.assertIn(token, source)

    def test_bridge_buttons_use_same_process_direct_painting(self):
        source = (ROOT / 'v0417/src/SimpleUpdate/ZapretVisualPolishHost.cs').read_text(encoding='utf-8')
        self.assertIn('BridgeButtonIds', source)
        for token in (
            'ZapretEnhancementHost.InstallServiceProxyButtonId',
            'ZapretEnhancementHost.RemoveServicesProxyButtonId',
            'ZapretEnhancementHost.StartStandaloneProxyButtonId',
            'ZapretEnhancementHost.RepairBroadcastButtonId',
            'ZapretEnhancementHost.RepairConnectionButtonId',
            'ZapretEnhancementHost.GameFilterButtonId',
            'ZapretEnhancementHost.ManagerButtonId',
            'ZapretEnhancementHost.LegacyCheckVersionButtonId',
            'ZapretEnhancementHost.LegacyDownloadButtonId',
        ):
            self.assertIn(token, source)
        self.assertIn('ButtonSubclassDelegate', source)
        self.assertIn('SetWindowSubclass(button, ButtonSubclassDelegate', source)
        self.assertIn('WM_PAINT', source)
        self.assertIn('WM_PRINTCLIENT', source)
        self.assertIn('DrawBridgeButton', source)
        self.assertIn('CreateRoundRectRgn', source)
        self.assertIn('WM_GETFONT', source)
        self.assertIn('SelectObject', source)
        self.assertNotIn('SetOwnerDrawStyle(button, originalStyle)', source)
        self.assertNotIn('UnifiedZapretButtonIds', source)

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
