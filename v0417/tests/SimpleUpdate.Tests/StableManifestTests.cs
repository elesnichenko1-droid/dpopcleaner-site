using Microsoft.VisualStudio.TestTools.UnitTesting;
using DPopCleaner.SimpleUpdate;

namespace SimpleUpdate.Tests
{
    [TestClass]
    public sealed class StableManifestTests
    {
        private const string Good = "{\"product\":\"DPopCleaner\",\"channel\":\"stable\",\"version\":\"0.4.18\",\"version_code\":418,\"revision\":1,\"available\":true,\"download_url\":\"https://example.test/DPopCleaner_Setup_0.4.18.exe\",\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\",\"size\":12345,\"install_args\":\"/SILENT /NORESTART\"}";

        [TestMethod]
        public void Parse_AcceptsStrictStableManifest()
        {
            UpdateManifest manifest;
            Assert.IsTrue(UpdateManifest.TryParseValidated(Good, out manifest));
            Assert.AreEqual("0.4.18", manifest.Version);
            Assert.AreEqual(418, manifest.VersionCode);
            Assert.AreEqual(1, manifest.Revision);
            Assert.AreEqual(12345L, manifest.Size);
        }

        [TestMethod]
        public void Parse_RejectsUnavailableHttpAndBadSha()
        {
            UpdateManifest manifest;
            Assert.IsFalse(UpdateManifest.TryParseValidated(Good.Replace("\"available\":true", "\"available\":false"), out manifest));
            Assert.IsFalse(UpdateManifest.TryParseValidated(Good.Replace("https://", "http://"), out manifest));
            Assert.IsFalse(UpdateManifest.TryParseValidated(Good.Replace("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", "abcd"), out manifest));
        }

        [TestMethod]
        public void Parse_RejectsWrongProductChannelAndNonPositiveSize()
        {
            UpdateManifest manifest;
            Assert.IsFalse(UpdateManifest.TryParseValidated(Good.Replace("DPopCleaner", "OtherCleaner"), out manifest));
            Assert.IsFalse(UpdateManifest.TryParseValidated(Good.Replace("\"stable\"", "\"beta\""), out manifest));
            Assert.IsFalse(UpdateManifest.TryParseValidated(Good.Replace("12345", "0"), out manifest));
        }
    }
}
