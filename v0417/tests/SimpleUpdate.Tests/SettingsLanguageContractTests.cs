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
        public void SettingsPageDetection_MustUseRightSideControlsThatBridgeNeverHides()
        {
            var launcher = ReadSource("LauncherContext.cs");
            var locator = ReadSource("SettingsPageLocator.cs");

            StringAssert.Contains(launcher, "SettingsPageLocator.IsVisible(_mainWindow)");
            StringAssert.Contains(locator, "SaveSettingsButtonId");
            StringAssert.Contains(locator, "AddFileButtonId");
            Assert.IsFalse(
                locator.Contains("AdminCheckboxId"),
                "The admin checkbox is hidden by the left Settings overlay and must never be used to decide whether Settings is still visible.");
            Assert.IsFalse(
                launcher.Contains("FindChildByText(_mainWindow, \"Настройки\""),
                "Changing the frozen core language changes 'Настройки' to 'Settings'; locale text must not decide whether the bridge stays visible.");
        }

        [TestMethod]
        public void SettingsBounds_MustNotDependOnRussianCheckboxOrReadyText()
        {
            var native = ReadSource("NativeBridge.cs");
            StringAssert.Contains(native, "FindSettingsCheckboxes");
            Assert.IsFalse(
                native.Contains("FindChildByText(parent, \"Фоновый контроль мусора каждые 30 минут\""),
                "The Settings host must also be creatable when DPopCleaner starts with English already selected.");
            Assert.IsFalse(
                native.Contains("FindChildByText(parent, \"Готово.\""),
                "Settings bounds must not disappear when the status label is localized to 'Ready.'.");
        }

        [TestMethod]
        public void SettingsProxyCaptions_MustFollowNativeLanguageComboSelection()
        {
            var launcher = ReadSource("LauncherContext.cs");
            var localization = ReadSource("SettingsProxyLocalization.cs");

            StringAssert.Contains(launcher, "SettingsProxyLocalization.Apply(_mainWindow)");
            StringAssert.Contains(localization, "CB_GETCURSEL");
            StringAssert.Contains(localization, "English");
            StringAssert.Contains(localization, "Always run application as administrator");
            StringAssert.Contains(localization, "Enable application auto-updates");
            Assert.IsFalse(
                localization.Contains("AdminCheckboxId"),
                "Language detection must read the native Language ComboBox, not a hidden left-side checkbox caption.");
        }

        [TestMethod]
        public void SettingsProxyLocalization_MustScopeBridgeOwnedIdsToSettingsHost()
        {
            var localization = ReadSource("SettingsProxyLocalization.cs");

            StringAssert.Contains(localization, "NativeBridge.SettingsScrollHostId");
            StringAssert.Contains(localization, "FindChildById(parent, NativeBridge.SettingsScrollHostId)");
            StringAssert.Contains(localization, "WriteIfDifferent(host,");
            Assert.IsFalse(
                localization.Contains("WriteIfDifferent(parent, FirstSettingProxyId"),
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
    }
}
