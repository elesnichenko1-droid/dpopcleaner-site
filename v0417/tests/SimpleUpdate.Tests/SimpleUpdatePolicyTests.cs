using System;
using System.IO;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using DPopCleaner.SimpleUpdate;

namespace SimpleUpdate.Tests
{
    [TestClass]
    public sealed class SimpleUpdatePolicyTests
    {
        [TestMethod]
        public void Settings_DefaultToAutoUpdateEnabled_AndRoundTrip()
        {
            var root = Path.Combine(Path.GetTempPath(), "DPopSimpleUpdateTests", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(root);
            try
            {
                var path = Path.Combine(root, "SimpleUpdate.ini");
                var store = new SettingsStore(path);
                Assert.IsTrue(store.LoadAutoUpdateEnabled(), "Auto-update should default to enabled when no setting exists.");

                store.SaveAutoUpdateEnabled(false);
                Assert.IsFalse(new SettingsStore(path).LoadAutoUpdateEnabled());

                store.SaveAutoUpdateEnabled(true);
                Assert.IsTrue(new SettingsStore(path).LoadAutoUpdateEnabled());
            }
            finally
            {
                if (Directory.Exists(root)) Directory.Delete(root, true);
            }
        }

        [TestMethod]
        public void UpdatePolicy_UsesVersionCodeThenRevision()
        {
            Assert.IsFalse(UpdatePolicy.IsNewer(417, 3, 417, 3));
            Assert.IsFalse(UpdatePolicy.IsNewer(417, 3, 417, 2));
            Assert.IsTrue(UpdatePolicy.IsNewer(417, 3, 417, 4));
            Assert.IsTrue(UpdatePolicy.IsNewer(417, 3, 418, 1));
            Assert.IsFalse(UpdatePolicy.IsNewer(417, 3, 416, 99));
        }

        [TestMethod]
        public void PackageVerifier_RequiresExactSizeAndSha256()
        {
            var root = Path.Combine(Path.GetTempPath(), "DPopSimpleUpdateTests", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(root);
            try
            {
                var path = Path.Combine(root, "package.bin");
                File.WriteAllBytes(path, new byte[] { 1, 2, 3, 4 });
                const string sha = "9f64a747e1b97f131fabb6b447296c9b6f0201e79fb3c5356e6c77e89b6a806a";

                Assert.IsTrue(PackageVerifier.Verify(path, 4, sha));
                Assert.IsFalse(PackageVerifier.Verify(path, 5, sha));
                Assert.IsFalse(PackageVerifier.Verify(path, 4, new string('0', 64)));
            }
            finally
            {
                if (Directory.Exists(root)) Directory.Delete(root, true);
            }
        }
    }
}
