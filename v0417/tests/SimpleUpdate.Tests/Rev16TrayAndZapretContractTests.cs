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
    }
}
