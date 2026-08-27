using System;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using DPopCleaner.SimpleUpdate;

namespace SimpleUpdate.Tests
{
    [TestClass]
    public sealed class UpdateClientTests
    {
        [TestMethod]
        public async Task CheckForNewerAsync_ReturnsOnlyValidatedNewerStableBuild()
        {
            var json = "{\"product\":\"DPopCleaner\",\"channel\":\"stable\",\"version\":\"0.4.18\",\"version_code\":418,\"revision\":1,\"available\":true,\"download_url\":\"https://example.test/DPopCleaner_Setup_0.4.18.exe\",\"sha256\":\"" + new string('a', 64) + "\",\"size\":4,\"install_args\":\"/SILENT /NORESTART\"}";
            using (var http = new HttpClient(new StubHandler(_ => new HttpResponseMessage(HttpStatusCode.OK)
            {
                Content = new StringContent(json, Encoding.UTF8, "application/json")
            })))
            {
                var client = new UpdateClient(http);
                var manifest = await client.CheckForNewerAsync("https://example.test/stable.json", 417, 3, CancellationToken.None);
                Assert.IsNotNull(manifest);
                Assert.AreEqual(418, manifest.VersionCode);
            }
        }

        [TestMethod]
        public async Task DownloadVerifiedPackageAsync_WritesVerifiedBytes()
        {
            var bytes = new byte[] { 1, 2, 3, 4 };
            var sha = Sha256(bytes);
            UpdateManifest manifest;
            Assert.IsTrue(UpdateManifest.TryParseValidated("{\"product\":\"DPopCleaner\",\"channel\":\"stable\",\"version\":\"0.4.18\",\"version_code\":418,\"revision\":1,\"available\":true,\"download_url\":\"https://example.test/setup.exe\",\"sha256\":\"" + sha + "\",\"size\":4}", out manifest));

            var root = Path.Combine(Path.GetTempPath(), "DPopSimpleUpdateTests", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(root);
            var path = Path.Combine(root, "setup.exe");
            try
            {
                using (var http = new HttpClient(new StubHandler(_ => new HttpResponseMessage(HttpStatusCode.OK)
                {
                    Content = new ByteArrayContent(bytes)
                })))
                {
                    var client = new UpdateClient(http);
                    var result = await client.DownloadVerifiedPackageAsync(manifest, path, CancellationToken.None);
                    Assert.AreEqual(path, result);
                    CollectionAssert.AreEqual(bytes, File.ReadAllBytes(path));
                }
            }
            finally
            {
                if (Directory.Exists(root)) Directory.Delete(root, true);
            }
        }

        [TestMethod]
        public async Task DownloadVerifiedPackageAsync_DeletesTamperedPackage()
        {
            var bytes = new byte[] { 1, 2, 3, 4 };
            UpdateManifest manifest;
            Assert.IsTrue(UpdateManifest.TryParseValidated("{\"product\":\"DPopCleaner\",\"channel\":\"stable\",\"version\":\"0.4.18\",\"version_code\":418,\"revision\":1,\"available\":true,\"download_url\":\"https://example.test/setup.exe\",\"sha256\":\"" + new string('0', 64) + "\",\"size\":4}", out manifest));

            var root = Path.Combine(Path.GetTempPath(), "DPopSimpleUpdateTests", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(root);
            var path = Path.Combine(root, "setup.exe");
            try
            {
                using (var http = new HttpClient(new StubHandler(_ => new HttpResponseMessage(HttpStatusCode.OK)
                {
                    Content = new ByteArrayContent(bytes)
                })))
                {
                    var client = new UpdateClient(http);
                    await Assert.ThrowsExceptionAsync<InvalidDataException>(() => client.DownloadVerifiedPackageAsync(manifest, path, CancellationToken.None));
                    Assert.IsFalse(File.Exists(path));
                }
            }
            finally
            {
                if (Directory.Exists(root)) Directory.Delete(root, true);
            }
        }

        private static string Sha256(byte[] bytes)
        {
            using (var sha = SHA256.Create())
            {
                var hash = sha.ComputeHash(bytes);
                var sb = new StringBuilder(hash.Length * 2);
                foreach (var b in hash) sb.Append(b.ToString("x2"));
                return sb.ToString();
            }
        }

        private sealed class StubHandler : HttpMessageHandler
        {
            private readonly Func<HttpRequestMessage, HttpResponseMessage> _reply;
            internal StubHandler(Func<HttpRequestMessage, HttpResponseMessage> reply) { _reply = reply; }
            protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
            {
                return Task.FromResult(_reply(request));
            }
        }
    }
}
