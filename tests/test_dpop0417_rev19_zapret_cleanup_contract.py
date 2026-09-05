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

    def test_tall_window_uses_virtual_compact_status_floor_without_reanchoring_native_edits(self):
        source = (ROOT / 'v0417/src/SimpleUpdate/ZapretResponsiveLayoutHost.cs').read_text(encoding='utf-8')
        self.assertIn('TallWindowMaximumRowShift', source)
        self.assertIn('_compactClientHeight', source)
        self.assertIn('_compactStatusSummaryTop', source)
        self.assertIn('if (clientHeight <= 840)', source)
        self.assertIn('_compactStatusSummaryTop = statusSummaryBounds.Top;', source)
        self.assertIn('var tallWindowHeightGrowth = Math.Max(0, clientHeight - _compactClientHeight);', source)
        self.assertIn('var tallWindowRowShift = Math.Min(', source)
        self.assertIn('tallWindowHeightGrowth / 3', source)
        self.assertIn('var virtualStatusSummaryTop = _compactStatusSummaryTop + tallWindowRowShift;', source)
        self.assertIn('var virtualStatusDetailBottom = virtualStatusDetailTop + statusDetailHeight;', source)
        self.assertIn('var strategyRowFloor = virtualStatusDetailBottom + sectionGap;', source)
        self.assertIn('var strategyRowTop = Math.Max(statusDetailBottom + sectionGap, strategyRowFloor);', source)
        self.assertIn('var statusSummaryTop = statusSummaryBounds.Top;', source)
        self.assertNotIn('Math.Max(statusSummaryBounds.Top', source)
        self.assertNotIn('tallWindowStatusFloor', source)

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

    def test_installed_wrapper_uses_strict_gate_without_diagnostic_probe_dependencies(self):
        wrapper = (ROOT / 'tools/dpop0417_rev19_installed_zapret_cleanup_smoke.ps1').read_text(encoding='utf-8')
        self.assertIn('dpop0417_rev19_zapret_cleanup_smoke.ps1', wrapper)
        self.assertIn('rev19-zapret-cleanup-report.json', wrapper)
        self.assertIn('dpop0417_rev19_button_style_probe.ps1', wrapper)
        self.assertIn('rev19-button-style-probe.json', wrapper)
        for diagnostic in (
            'dpop0417_rev19_1702_1024_capture_probe.ps1',
            'dpop0417_rev19_remove_services_probe.ps1',
            'dpop0417_rev19_subclass_probe.ps1',
            'dpop0417_rev19_layout_settle_probe.ps1',
            'rev19-remove-services-probe.json',
            'rev19-ownerdraw-probe.json',
        ):
            self.assertNotIn(diagnostic, wrapper)

    def test_primary_screenshot_composites_real_screen_remove_services(self):
        smoke = (ROOT / 'tools/dpop0417_rev19_zapret_cleanup_smoke.ps1').read_text(encoding='utf-8')
        self.assertIn('Capture-CompositeWindow', smoke)
        self.assertIn('Capture-ScreenChildBitmap', smoke)
        self.assertIn('CopyFromScreen', smoke)
        self.assertIn('$_.Id -eq 1702', smoke)
        self.assertIn('DrawImageUnscaled', smoke)
        self.assertIn('Ensure-CaptureWindowForeground', smoke)
        self.assertIn('BringWindowToTop', smoke)
        self.assertIn('SetForegroundWindow', smoke)
        self.assertIn('REV19_PRIMARY_COMPOSITE_OK', smoke)
        self.assertIn('Capture-CompositeWindow $window $shot $children $launcher.Id', smoke)

    def test_oversized_capture_window_keeps_tall_remove_services_inside_physical_desktop(self):
        smoke = (ROOT / 'tools/dpop0417_rev19_zapret_cleanup_smoke.ps1').read_text(encoding='utf-8')
        self.assertIn('$oversizedCaptureTopMargin = 32', smoke)
        self.assertIn('$screenBounds.Bottom-$size.Height-$oversizedCaptureTopMargin', smoke)
        self.assertIn('$captureResizeFlags = 0x0004 -bor 0x0010 -bor 0x0400', smoke)
        self.assertIn('SetWindowPos($window,[IntPtr]::Zero,0,$windowY,$size.Width,$size.Height,$captureResizeFlags)', smoke)

    def test_user_size_waits_for_stable_geometry_without_waiting_on_blank_space_threshold(self):
        smoke = (ROOT / 'tools/dpop0417_rev19_zapret_cleanup_smoke.ps1').read_text(encoding='utf-8')
        self.assertIn('function Get-ZapretGeometrySignature', smoke)
        self.assertIn('function Wait-StableZapretGeometry', smoke)
        self.assertIn('MinimumObservationMilliseconds', smoke)
        self.assertIn('StableSamples', smoke)
        self.assertIn('REV19_GEOMETRY_SETTLE', smoke)
        self.assertIn('Wait-StableZapretGeometry $window $targetIds', smoke)
        self.assertIn('$size.Width -eq 1908', smoke)
        self.assertIn('if($size.Width -eq 1908 -and $unusedBottom -gt 210)', smoke)
        settle_start = smoke.index('function Wait-StableZapretGeometry')
        settle_end = smoke.index('function Assert-TrayState', settle_start)
        settle = smoke[settle_start:settle_end]
        self.assertNotIn('unusedBottom', settle)
        self.assertNotIn('210', settle)

    def test_zapret_geometry_is_applied_before_bridge_painting(self):
        launcher = (ROOT / 'v0417/src/SimpleUpdate/LauncherContext.cs').read_text(encoding='utf-8')
        start = launcher.index('private void UpdateZapretEnhancements()')
        end = launcher.index('private void UpdateSettingsEnhancements()', start)
        method = launcher[start:end]
        layout_pos = method.index('new ZapretResponsiveLayoutHost')
        polish_pos = method.index('new ZapretVisualPolishHost')
        self.assertLess(layout_pos, polish_pos, 'responsive geometry must be applied before bridge visual painting')

    def test_nested_bridge_hosts_repaint_only_after_real_geometry_change(self):
        layout = (ROOT / 'v0417/src/SimpleUpdate/ZapretResponsiveLayoutHost.cs').read_text(encoding='utf-8')
        self.assertIn('private static bool PositionIfChanged', layout)
        self.assertIn('var groupChanged = PositionIfChanged', layout)
        self.assertIn('groupChanged |= PositionIfChanged', layout)
        self.assertIn('if (groupChanged)', layout)
        self.assertIn('RedrawWindow(parent', layout)
        self.assertIn('RDW_UPDATENOW', layout)

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

    def test_bridge_buttons_use_host_level_ownerdraw_not_per_button_subclass(self):
        source = (ROOT / 'v0417/src/SimpleUpdate/ZapretVisualPolishHost.cs').read_text(encoding='utf-8')
        self.assertIn('BS_OWNERDRAW', source)
        self.assertIn('WM_DRAWITEM', source)
        self.assertIn('BridgeHostSubclassDelegate', source)
        self.assertIn('DrawOwnerDrawButton', source)
        self.assertIn('SetOwnerDrawStyle', source)
        self.assertIn('CreateRoundRectRgn', source)
        self.assertIn('NativeBridge.WM_GETFONT', source)
        self.assertIn('SelectObject', source)
        self.assertIn('SetWindowTheme(button, string.Empty, string.Empty)', source)
        self.assertNotIn('SetWindowSubclass(button, ButtonSubclassDelegate', source)
        self.assertNotIn('StaticButtonSubclassProc', source)

    def test_bridge_ownerdraw_hosts_clip_children_from_parent_erase(self):
        source = (ROOT / 'v0417/src/SimpleUpdate/ZapretVisualPolishHost.cs').read_text(encoding='utf-8')
        self.assertIn('WS_CLIPCHILDREN', source)
        self.assertIn('SetClipChildrenStyle', source)
        self.assertIn('SetClipChildrenStyle(host)', source)
        self.assertIn('_originalBridgeHostStyles', source)

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
