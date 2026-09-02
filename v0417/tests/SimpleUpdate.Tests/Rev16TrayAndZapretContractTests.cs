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
            StringAssert.Contains(runtime, "internal string BundledWinwsCommandLine");
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
            StringAssert.Contains(source, "set /p \"choice=Input option (0-!count!, default: 0): \"");
            StringAssert.Contains(source, "DPOP_INSTALL_ONCE");
            StringAssert.Contains(source, "set \"menu_choice=1\"");
            StringAssert.Contains(source, "set \"choice=\" + menuIndex.ToString()");
            StringAssert.Contains(source, "service-dpop-install-");
            Assert.IsFalse(source.Contains("goto service_install"),
                "Jumping directly to :service_install skips Flowseal root-menu initialization, including GameFilterTCP/GameFilterUDP fallback values, and produces a service that exits with 1067.");
        }
    }
}
