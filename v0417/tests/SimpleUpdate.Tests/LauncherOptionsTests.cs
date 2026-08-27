using Microsoft.VisualStudio.TestTools.UnitTesting;
using DPopCleaner.SimpleUpdate;

namespace SimpleUpdate.Tests
{
    [TestClass]
    public sealed class LauncherOptionsTests
    {
        private const string StableManifestUrl = "https://elesnichenko1-droid.github.io/dpopcleaner-site/update/stable.json";

        [TestMethod]
        public void Parse_DefaultsToEnabledStableAutoCheck()
        {
            var options = LauncherOptions.Parse(new string[0]);
            Assert.IsTrue(options.UpdateCheckEnabled);
            Assert.AreEqual(StableManifestUrl, options.ManifestUrl);
            Assert.IsNull(options.SettingsPathOverride);
        }

        [TestMethod]
        public void Parse_NoUpdateCheck_DisablesNetworkCheck()
        {
            var options = LauncherOptions.Parse(new[] { "--no-update-check" });
            Assert.IsFalse(options.UpdateCheckEnabled);
        }

        [TestMethod]
        public void Parse_OverridesManifestAndSettingsPath()
        {
            var options = LauncherOptions.Parse(new[]
            {
                "--manifest-url", "https://127.0.0.1:4443/stable.json",
                "--settings-path", "C:\\Temp\\SimpleUpdate.ini"
            });
            Assert.AreEqual("https://127.0.0.1:4443/stable.json", options.ManifestUrl);
            Assert.AreEqual("C:\\Temp\\SimpleUpdate.ini", options.SettingsPathOverride);
        }
    }
}
