using System;
using System.IO;
using Microsoft.Win32;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using DPop.Common.Restore;

namespace DPop.Common.Tests
{
    [TestClass]
    public class RestoreProviderTests
    {
        [TestMethod]
        public void FileProviderRestoresBytesAndRejectsEscapeFromAllowedRoot()
        {
            var root = Path.Combine(Path.GetTempPath(), "dpop0417-file-provider-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(root);
            try
            {
                var provider = new FileStateProvider(root);
                var target = Path.Combine(root, "settings.json");
                File.WriteAllText(target, "before");

                var state = provider.Capture(target);
                File.WriteAllText(target, "after");
                provider.Restore(target, state);

                Assert.AreEqual("before", File.ReadAllText(target));

                var absent = Path.Combine(root, "absent.txt");
                var absentState = provider.Capture(absent);
                File.WriteAllText(absent, "created later");
                provider.Restore(absent, absentState);
                Assert.IsFalse(File.Exists(absent), "Rollback must restore the previous non-existent state.");

                var escaped = Path.GetFullPath(Path.Combine(root, "..", "outside.txt"));
                Assert.ThrowsException<InvalidDataException>(() => provider.Capture(escaped));
                Assert.ThrowsException<InvalidDataException>(() => provider.Restore(escaped, state));
            }
            finally
            {
                if (Directory.Exists(root)) Directory.Delete(root, true);
            }
        }

        [TestMethod]
        public void HkcuRegistryProviderRoundTripsWithoutAcceptingAnotherHive()
        {
            var subKey = @"Software\DPopCleaner\Tests\" + Guid.NewGuid().ToString("N");
            const string valueName = "RoundTrip";
            try
            {
                using (var key = Registry.CurrentUser.CreateSubKey(subKey))
                    key.SetValue(valueName, "before", RegistryValueKind.String);

                var provider = new HkcuRegistryValueProvider();
                var target = HkcuRegistryValueProvider.CreateTarget(subKey, valueName, RegistryValueKind.String);
                var state = provider.Capture(target);

                using (var key = Registry.CurrentUser.CreateSubKey(subKey))
                    key.SetValue(valueName, "after", RegistryValueKind.String);
                provider.Restore(target, state);

                using (var key = Registry.CurrentUser.OpenSubKey(subKey))
                    Assert.AreEqual("before", key.GetValue(valueName));

                var tampered = "{\"hive\":\"HKLM\",\"subKey\":\"" + subKey.Replace("\\", "\\\\") + "\",\"valueName\":\"" + valueName + "\",\"valueKind\":\"String\"}";
                Assert.ThrowsException<InvalidDataException>(() => provider.Capture(tampered));
            }
            finally
            {
                Registry.CurrentUser.DeleteSubKeyTree(subKey, false);
            }
        }
    }
}
