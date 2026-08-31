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
        public void SettingsPageDetection_MustNotDependOnRussianHeadingText()
        {
            var launcher = ReadSource("LauncherContext.cs");
            StringAssert.Contains(launcher, "NativeBridge.IsSettingsPageVisible(_mainWindow)");
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
        public void SettingsProxyCaptions_MustFollowFrozenCoreLanguage()
        {
            var host = ReadSource("AdditionalSettingsHost.cs");
            StringAssert.Contains(host, "NativeBridge.FindSettingsCheckboxes");
            StringAssert.Contains(host, "NativeBridge.ReadWindowText(setting.LegacyHandle)");
            StringAssert.Contains(host, "NativeBridge.WriteWindowText(setting.ProxyHandle");
        }
    }
}
