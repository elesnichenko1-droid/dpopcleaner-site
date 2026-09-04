using System;
using System.IO;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace SimpleUpdate.Tests
{
    [TestClass]
    public sealed class Rev16TrayAndZapretContractTests
    {
        private static string ReadSource(string fileName)
        {
            var directory = new DirectoryInfo(AppDomain.CurrentDomain.BaseDirectory);
            while (directory != null)
            {
                var candidate = Path.Combine(directory.FullName, "v0417", "src", "SimpleUpdate", fileName);
                if (File.Exists(candidate)) return File.ReadAllText(candidate);
                directory = directory.Parent;
            }

            Assert.Fail("Could not locate repository source file: " + fileName);
            return string.Empty;
        }

        private static string SliceMethod(string source, string signature, string nextSignature)
        {
            var start = source.IndexOf(signature, StringComparison.Ordinal);
            Assert.IsTrue(start >= 0, "Method not found: " + signature);
            var end = source.IndexOf(nextSignature, start + signature.Length, StringComparison.Ordinal);
            Assert.IsTrue(end > start, "Could not isolate method: " + signature);
            return source.Substring(start, end - start);
        }

        [TestMethod]
        public void Core_restart_keeps_the_same_tray_host_identity()
        {
            var launcher = ReadSource("LauncherContext.cs");
            var reset = SliceMethod(
                launcher,
                "private void ResetBridgeForRestartedCore()",
                "private void UpdateTrayRamBadge()");

            Assert.IsFalse(reset.Contains("_trayRamHost.Dispose()"),
                "The tray host owns the canonical Explorer notification identity and must survive core self-restart.");
            Assert.IsFalse(reset.Contains("_trayRamHost = null"),
                "Clearing the tray host would create a second HWND/uID identity on the successor core.");
            StringAssert.Contains(reset, "_trayRamHost.ReattachMainWindow");
        }

        [TestMethod]
        public void Tray_host_exposes_one_constant_icon_identity()
        {
            var source = ReadSource("TrayRamBadgeHost.cs");

            StringAssert.Contains(source, "private const uint TrayIconId = 1;");
            StringAssert.Contains(source, "internal IntPtr MessageWindowHandle");
            StringAssert.Contains(source, "internal uint IconId");
            StringAssert.Contains(source, "internal void ReattachMainWindow(IntPtr mainWindow)");
        }

        [TestMethod]
        public void Ghost_cleanup_is_scoped_to_the_exact_canonical_tray_identity()
        {
            var source = ReadSource("BridgeTrayGhostSuppressor.cs");

            StringAssert.Contains(source, "CleanupCurrentProcess(IntPtr keepWindow, uint keepIconId)");
            StringAssert.Contains(source, "tray.hwnd == keepWindow && tray.uID == keepIconId");
            Assert.IsFalse(source.Contains("FindKeepWindow(processId)"),
                "rev.16 must not rediscover the canonical tray HWND by window title when the exact HWND/uID is already owned by TrayRamBadgeHost.");
        }

        [TestMethod]
        public void Ghost_cleanup_reconciles_stale_dpopcleaner_rows_by_explorer_text_not_current_pid()
        {
            var source = ReadSource("BridgeTrayGhostSuppressor.cs");

            StringAssert.Contains(source, "TB_GETBUTTONTEXTW");
            StringAssert.Contains(source, "DPopCleaner");
            Assert.IsFalse(source.Contains("trayOwner != (uint)ownerProcessId"),
                "A killed prior launcher leaves Explorer rows whose HWND no longer belongs to the current launcher PID; filtering by the current PID cannot remove those ghosts.");
            Assert.IsFalse(source.Contains("Process.GetCurrentProcess().Id"),
                "Ghost reconciliation must classify the Explorer row itself, not trust a possibly stale/reused HWND owner PID.");
        }

        [TestMethod]
        public void Zapret_bridge_reads_factual_runtime_state()
        {
            var runtime = ReadSource("ZapretRuntimeState.cs");

            StringAssert.Contains(runtime, "internal static ZapretRuntimeState Read(string applicationRoot)");
            StringAssert.Contains(runtime, "internal bool BundledWinwsRunning");
            StringAssert.Contains(runtime, "internal bool ZapretServiceExists");
            StringAssert.Contains(runtime, "internal bool ZapretServiceRunning");
            StringAssert.Contains(runtime, "internal bool WinDivertServiceExists");
            StringAssert.Contains(runtime, "internal bool WinDivert14ServiceExists");
            StringAssert.Contains(runtime, "internal string BundledWinwsCommandLine");
            StringAssert.Contains(runtime, "QueryService(\"WinDivert\")");
            StringAssert.Contains(runtime, "QueryService(\"WinDivert14\")");
            StringAssert.Contains(ReadSource("ZapretEnhancementHost.cs"), "RefreshRuntimeStatus()");
        }

        [TestMethod]
        public void Proven_broken_install_action_is_replaced_by_a_same_bounds_bridge_proxy()
        {
            var source = ReadSource("ZapretEnhancementHost.cs");

            StringAssert.Contains(source, "InstallServiceProxyButtonId");
            StringAssert.Contains(source, "CreateInstallServiceProxy()");
            StringAssert.Contains(source, "InstallSelectedStrategyUsingUpstreamManager()");
            StringAssert.Contains(source, "ReadSelectedStrategy()");
            StringAssert.Contains(source, "service.bat");
            StringAssert.Contains(source, "RefreshRuntimeStatus()");
            StringAssert.Contains(source, "NativeBridge.ShowWindow(_legacyInstallServiceButton, NativeBridge.SW_HIDE)");
        }

        [TestMethod]
        public void Install_action_preserves_root_initialization_and_replaces_only_interactive_choices()
        {
            var source = ReadSource("ZapretEnhancementHost.cs");
            var install = SliceMethod(
                source,
                "private void InstallSelectedStrategyUsingUpstreamManager()",
                "private static int FindStrategyMenuIndex");

            StringAssert.Contains(install, "BuildDirectUpstreamInstallManager(service, menuIndex)");
            Assert.IsFalse(install.Contains("RedirectStandardInput = true"),
                "Flowseal child commands run before both prompts and can consume redirected stdin; rev.16 must make the temporary manager noninteractive instead.");
            Assert.IsFalse(install.Contains("process.StandardInput"),
                "The selected menu choices must be embedded only in the temporary service manager, not streamed through shared stdin.");
            StringAssert.Contains(install, "File.Delete(directManager)");

            StringAssert.Contains(source, "private static string BuildDirectUpstreamInstallManager(string service, int menuIndex)");
            StringAssert.Contains(source, "set /p menu_choice=   Select option (0-12): ");
            StringAssert.Contains(source, "set /p \\\"choice=Input option (0-!count!, default: 0): \\\"");
            StringAssert.Contains(source, "DPOP_INSTALL_ONCE");
            StringAssert.Contains(source, "set \\\"menu_choice=1\\\"");
            StringAssert.Contains(source, "var strategyReplacement");
            StringAssert.Contains(source, "menuIndex.ToString()");
            StringAssert.Contains(source, "Replace(strategyPrompt, strategyReplacement)");
            StringAssert.Contains(source, "service-dpop-install-");
            Assert.IsFalse(source.Contains("goto service_install"),
                "Jumping directly to :service_install skips Flowseal root-menu initialization, including GameFilterTCP/GameFilterUDP fallback values, and produces a service that exits with 1067.");
        }

        [TestMethod]
        public void Install_action_allows_a_safe_margin_for_real_flowseal_service_startup()
        {
            var source = ReadSource("ZapretEnhancementHost.cs");
            var install = SliceMethod(
                source,
                "private void InstallSelectedStrategyUsingUpstreamManager()",
                "private static string BuildDirectUpstreamInstallManager");

            StringAssert.Contains(install, "DateTime.UtcNow.AddSeconds(30)",
                "Real installed Flowseal 1.10.2 startup has measured near 10 seconds on current runners and exceeded the old 15-second budget on a slower runner; keep a deterministic safety margin.");
            Assert.IsFalse(install.Contains("DateTime.UtcNow.AddSeconds(15)"),
                "The old 15-second install deadline is too close to observed real startup time and caused an intermittent false timeout.");
        }

        [TestMethod]
        public void Proven_broken_remove_action_is_replaced_by_a_same_bounds_upstream_proxy()
        {
            var source = ReadSource("ZapretEnhancementHost.cs");

            StringAssert.Contains(source, "RemoveServicesProxyButtonId = 1702");
            StringAssert.Contains(source, "CreateRemoveServicesProxy()");
            StringAssert.Contains(source, "RemoveUsingUpstreamManager()");
            StringAssert.Contains(source, "BuildDirectUpstreamRemoveManager(service)");
            StringAssert.Contains(source, "DPOP_REMOVE_ONCE");
            StringAssert.Contains(source, "set \\\"menu_choice=2\\\"");
            StringAssert.Contains(source, "service-dpop-remove-");
            StringAssert.Contains(source, "NativeBridge.ShowWindow(_legacyRemoveServicesButton, NativeBridge.SW_HIDE)");

            var remove = SliceMethod(
                source,
                "private void RemoveUsingUpstreamManager()",
                "private static string BuildDirectUpstreamRemoveManager");
            Assert.IsFalse(remove.Contains("RedirectStandardInput = true"),
                "The upstream remove manager must be made noninteractive rather than sharing stdin with Flowseal child commands.");
            Assert.IsFalse(remove.Contains("process.StandardInput"),
                "Remove must embed root choice 2 in the temporary upstream manager.");
            StringAssert.Contains(remove, "ZapretRuntimeState.Read(_applicationRoot)");
            StringAssert.Contains(remove, "!state.ZapretServiceExists");
            StringAssert.Contains(remove, "!state.BundledWinwsRunning");
            StringAssert.Contains(remove, "!state.WinDivertServiceExists");
            StringAssert.Contains(remove, "!state.WinDivert14ServiceExists");
            StringAssert.Contains(remove, "File.Delete(removeManager)");
        }

        [TestMethod]
        public void Proven_broken_standalone_start_action_is_replaced_by_a_same_bounds_upstream_proxy()
        {
            var source = ReadSource("ZapretEnhancementHost.cs");

            StringAssert.Contains(source, "StartStandaloneProxyButtonId = 1713");
            StringAssert.Contains(source, "CreateStartStandaloneProxy()");
            StringAssert.Contains(source, "StartSelectedStrategyUsingUpstreamBatch()");
            StringAssert.Contains(source, "NativeBridge.ShowWindow(_legacyStartStandaloneButton, NativeBridge.SW_HIDE)");

            var start = SliceMethod(
                source,
                "private void StartSelectedStrategyUsingUpstreamBatch()",
                "private void InstallSelectedStrategyUsingUpstreamManager()");
            StringAssert.Contains(start, "ReadSelectedStrategy()");
            StringAssert.Contains(start, "Path.GetFullPath(Path.Combine(zapretRoot, selected))");
            StringAssert.Contains(start, "new ProcessStartInfo(\"cmd.exe\")");
            StringAssert.Contains(start, "WorkingDirectory = zapretRoot");
            StringAssert.Contains(start, "ZapretRuntimeState.Read(_applicationRoot)");
            StringAssert.Contains(start, "state.ZapretServiceRunning");
            StringAssert.Contains(start, "state.BundledWinwsRunning");
            StringAssert.Contains(start, "RefreshRuntimeStatus()");
            Assert.IsFalse(start.Contains("taskkill"),
                "Standalone start must execute only the selected bundled Flowseal strategy and must not use global process termination.");
        }

        [TestMethod]
        public void Zapret_visual_layer_follows_native_theme_and_styles_bridge_actions_consistently()
        {
            var visual = ReadSource("ZapretVisualPolishHost.cs");
            var bridge = ReadSource("NativeBridge.cs");
            var enhancement = ReadSource("ZapretEnhancementHost.cs");

            StringAssert.Contains(bridge, "internal static bool IsDarkThemeSelected(IntPtr parent)");
            StringAssert.Contains(bridge, "ReadSettingsThemeSelection(parent)");
            StringAssert.Contains(visual, "NativeBridge.IsDarkThemeSelected(_parent)");
            StringAssert.Contains(visual, "BridgeButtonIds");
            StringAssert.Contains(visual, "BS_OWNERDRAW");
            StringAssert.Contains(visual, "WM_DRAWITEM");
            StringAssert.Contains(visual, "BridgeHostSubclassDelegate");
            StringAssert.Contains(visual, "WS_CLIPCHILDREN");
            StringAssert.Contains(visual, "DarkButtonBrush");
            StringAssert.Contains(visual, "LightButtonBrush");
            StringAssert.Contains(visual, "ZapretEnhancementHost.InstallServiceProxyButtonId");
            StringAssert.Contains(visual, "ZapretEnhancementHost.RemoveServicesProxyButtonId");
            StringAssert.Contains(visual, "ZapretEnhancementHost.StartStandaloneProxyButtonId");
            Assert.IsFalse(visual.Contains("SetWindowSubclass(button, ButtonSubclassDelegate"),
                "Bridge button painting must be owned by the launcher host, not a fragile per-button WM_PAINT subclass.");
            Assert.IsFalse(visual.Contains("EnsureDarkBridgeButtons"),
                "Zapret presentation must not be permanently dark after the native theme changes.");
            Assert.IsFalse(enhancement.Contains("SetWindowTheme(button, \"DarkMode_Explorer\""),
                "Bridge controls must not force DarkMode_Explorer independently of the unified Zapret presentation layer.");
        }

        [TestMethod]
        public void Zapret_action_toolbar_uses_measured_text_and_available_page_width_instead_of_a_fixed_709px_strip()
        {
            var enhancement = ReadSource("ZapretEnhancementHost.cs");

            Assert.IsFalse(enhancement.Contains("ToolbarWidth = 709"),
                "A fixed 709px action strip clips or protrudes when DPI, language, or available page width changes.");
            StringAssert.Contains(enhancement, "LayoutActionButtons");
            StringAssert.Contains(enhancement, "TextRenderer.MeasureText");
            StringAssert.Contains(enhancement, "availableWidth");
            StringAssert.Contains(enhancement, "NativeBridge.PositionChildWindow(_actionToolbar");
        }

        [TestMethod]
        public void Zapret_visual_layer_hides_only_native_journal_while_zapret_is_active_and_restores_it()
        {
            var visual = ReadSource("ZapretVisualPolishHost.cs");
            var show = SliceMethod(visual, "internal void Show()", "internal void Hide()");
            var hide = SliceMethod(visual, "internal void Hide()", "private void EnsureBridgeButtonPainting()");

            StringAssert.Contains(show, "HideJournalForZapret()");
            StringAssert.Contains(hide, "RestoreJournal()");
            StringAssert.Contains(visual, "private void HideJournalForZapret()");
            StringAssert.Contains(visual, "private void RestoreJournal()");
            StringAssert.Contains(visual, "\"ListBox\"");
            StringAssert.Contains(visual, "NativeBridge.ShowWindow(_journalHeading, NativeBridge.SW_HIDE)");
            StringAssert.Contains(visual, "NativeBridge.ShowWindow(_journalList, NativeBridge.SW_HIDE)");
            StringAssert.Contains(visual, "NativeBridge.ShowWindow(_journalHeading, NativeBridge.SW_SHOW)");
            StringAssert.Contains(visual, "NativeBridge.ShowWindow(_journalList, NativeBridge.SW_SHOW)");
        }
    }
}