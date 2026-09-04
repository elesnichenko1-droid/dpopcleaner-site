using System;
using System.IO;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace SimpleUpdate.Tests
{
    [TestClass]
    public sealed class SettingsLanguageContractTests
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

        [TestMethod]
        public void SettingsProxyLocalization_MustSearchInsideBridgeHostToAvoidFrozenIdCollisions()
        {
            var source = ReadSource("SettingsProxyLocalization.cs");
            StringAssert.Contains(source, "SettingsScrollHostId");
            StringAssert.Contains(source, "FindDescendantById");
            Assert.IsFalse(source.Contains("FindChildById(parent, 1500"),
                "Proxy ids 1500-1505 collide with descendants in the frozen core; localization must search from bridge host id=1492, not the main window.");
        }

        [TestMethod]
        public void Launcher_MustRecoverBridgeWhenFrozenCoreSelfRestartsForLanguageChange()
        {
            var launcher = ReadSource("LauncherContext.cs");

            Assert.IsFalse(
                launcher.Contains("private readonly Process _core;"),
                "A language change can replace the frozen core process; the launcher must be able to adopt the successor process.");
            StringAssert.Contains(launcher, "TryAttachRestartedCore");
            StringAssert.Contains(launcher, "ResetBridgeForRestartedCore");
            Assert.IsFalse(
                launcher.Contains("if (!_updateInstallInProgress) ExitThread();\n                    return;"),
                "The launcher must not immediately exit when the frozen core exits if that exit produced a successor process for a language restart.");
        }

        [TestMethod]
        public void RestartRecovery_MustPreserveRamBadgeIdentityAndDiscardOldWindowHosts()
        {
            var launcher = ReadSource("LauncherContext.cs");

            var resetStart = launcher.IndexOf("private void ResetBridgeForRestartedCore()", StringComparison.Ordinal);
            Assert.IsTrue(resetStart >= 0, "ResetBridgeForRestartedCore was not found.");
            var resetEnd = launcher.IndexOf("private void UpdateTrayRamBadge()", resetStart, StringComparison.Ordinal);
            Assert.IsTrue(resetEnd > resetStart, "Could not isolate ResetBridgeForRestartedCore.");
            var reset = launcher.Substring(resetStart, resetEnd - resetStart);

            Assert.IsFalse(reset.Contains("_trayRamHost.Dispose()"),
                "The canonical tray message HWND/uID must survive frozen-core restarts.");
            Assert.IsFalse(reset.Contains("_trayRamHost = null"),
                "A language restart must not create a second bridge tray identity.");
            StringAssert.Contains(reset, "_trayRamHost.ReattachMainWindow");
            Assert.IsFalse(reset.Contains("_trayPreference = null"),
                "rev.18 keeps the persisted canonical tray preference across frozen-core restart.");
            StringAssert.Contains(reset, "_mainWindow = IntPtr.Zero;");
            StringAssert.Contains(reset, "_settingsHost = null;");
            StringAssert.Contains(reset, "_zapretHost = null;");
            StringAssert.Contains(reset, "_zapretVisualHost = null;");
        }
    }
}
