using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using DPopCleaner.SimpleUpdate;

namespace SimpleUpdate.Tests
{
    [TestClass]
    public sealed class UpdateCoordinatorTests
    {
        [TestMethod]
        public async Task CheckAndInstallAsync_VerifiesBeforeCloseAndLaunch()
        {
            UpdateManifest manifest;
            Assert.IsTrue(UpdateManifest.TryParseValidated("{\"product\":\"DPopCleaner\",\"channel\":\"stable\",\"version\":\"0.4.18\",\"version_code\":418,\"revision\":1,\"available\":true,\"download_url\":\"https://example.test/setup.exe\",\"sha256\":\"" + new string('a', 64) + "\",\"size\":4,\"install_args\":\"/SILENT /NORESTART\"}", out manifest));

            var calls = new List<string>();
            var source = new FakeSource(manifest, calls);
            var coordinator = new UpdateCoordinator(
                source,
                m => { calls.Add("prompt"); return true; },
                m => "C:\\Temp\\DPopCleaner_Setup_" + m.Version + ".exe",
                () => { calls.Add("close"); return Task.CompletedTask; },
                (path, args) => calls.Add("launch:" + path + ":" + args));

            var installed = await coordinator.CheckAndInstallAsync("https://example.test/stable.json", 417, 3, CancellationToken.None);

            Assert.IsTrue(installed);
            CollectionAssert.AreEqual(new[]
            {
                "check",
                "prompt",
                "download:C:\\Temp\\DPopCleaner_Setup_0.4.18.exe",
                "close",
                "launch:C:\\Temp\\DPopCleaner_Setup_0.4.18.exe:/SILENT /NORESTART"
            }, calls);
        }

        [TestMethod]
        public async Task CheckAndInstallAsync_DoesNothingWhenUserDeclines()
        {
            UpdateManifest manifest;
            Assert.IsTrue(UpdateManifest.TryParseValidated("{\"product\":\"DPopCleaner\",\"channel\":\"stable\",\"version\":\"0.4.18\",\"version_code\":418,\"revision\":1,\"available\":true,\"download_url\":\"https://example.test/setup.exe\",\"sha256\":\"" + new string('a', 64) + "\",\"size\":4}", out manifest));

            var calls = new List<string>();
            var source = new FakeSource(manifest, calls);
            var coordinator = new UpdateCoordinator(
                source,
                m => { calls.Add("prompt"); return false; },
                m => "unused.exe",
                () => { calls.Add("close"); return Task.CompletedTask; },
                (path, args) => calls.Add("launch"));

            var installed = await coordinator.CheckAndInstallAsync("https://example.test/stable.json", 417, 3, CancellationToken.None);

            Assert.IsFalse(installed);
            CollectionAssert.AreEqual(new[] { "check", "prompt" }, calls);
        }

        private sealed class FakeSource : IUpdateSource
        {
            private readonly UpdateManifest _manifest;
            private readonly List<string> _calls;
            internal FakeSource(UpdateManifest manifest, List<string> calls) { _manifest = manifest; _calls = calls; }

            public Task<UpdateManifest> CheckForNewerAsync(string manifestUrl, int currentVersionCode, int currentRevision, CancellationToken cancellationToken)
            {
                _calls.Add("check");
                return Task.FromResult(_manifest);
            }

            public Task<string> DownloadVerifiedPackageAsync(UpdateManifest manifest, string destinationPath, CancellationToken cancellationToken)
            {
                _calls.Add("download:" + destinationPath);
                return Task.FromResult(destinationPath);
            }
        }
    }
}
