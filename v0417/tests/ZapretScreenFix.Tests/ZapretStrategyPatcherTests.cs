using System;
using System.IO;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace DPop.ZapretScreenFix.Tests
{
    [TestClass]
    public sealed class ZapretStrategyPatcherTests
    {
        private const string DiscordLine = "start \"zapret\" /min bin\\winws.exe --filter-tcp=2053,2083,2087,2096,8443 --hostlist-domains=discord.media --dpi-desync=fake";

        [TestMethod]
        public void PatchText_Adds443OnlyToDiscordMediaFilter()
        {
            var other = "start \"zapret\" /min bin\\winws.exe --filter-tcp=80,443 --hostlist=list-general.txt";
            var input = other + "\r\n" + DiscordLine + "\r\n";

            var result = ZapretStrategyPatcher.PatchText(input);

            Assert.IsTrue(result.Changed);
            Assert.AreEqual(1, result.PatchedLines);
            StringAssert.Contains(result.Text, other);
            StringAssert.Contains(result.Text, "--filter-tcp=443,2053,2083,2087,2096,8443 --hostlist-domains=discord.media");
        }

        [TestMethod]
        public void PatchText_IsIdempotentWhen443AlreadyPresent()
        {
            var input = DiscordLine.Replace("--filter-tcp=2053", "--filter-tcp=443,2053") + "\r\n";

            var result = ZapretStrategyPatcher.PatchText(input);

            Assert.IsFalse(result.Changed);
            Assert.AreEqual(0, result.PatchedLines);
            Assert.AreEqual(input, result.Text);
        }

        [TestMethod]
        public void PatchFile_CreatesBackupAndRestoreReturnsOriginalBytes()
        {
            var root = Path.Combine(Path.GetTempPath(), "dpop0417-zapret-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(root);
            try
            {
                var path = Path.Combine(root, "general.bat");
                File.WriteAllText(path, "@echo off\r\n" + DiscordLine + "\r\n");
                var original = File.ReadAllBytes(path);

                Assert.IsTrue(ZapretStrategyPatcher.PatchFile(path));
                Assert.IsTrue(File.Exists(path + ZapretStrategyPatcher.BackupSuffix));
                CollectionAssert.AreEqual(original, File.ReadAllBytes(path + ZapretStrategyPatcher.BackupSuffix));
                StringAssert.Contains(File.ReadAllText(path), "--filter-tcp=443,2053,2083,2087,2096,8443");

                Assert.IsTrue(ZapretStrategyPatcher.RestoreFile(path));
                CollectionAssert.AreEqual(original, File.ReadAllBytes(path));
            }
            finally
            {
                if (Directory.Exists(root)) Directory.Delete(root, true);
            }
        }

        [TestMethod]
        public void FindCandidates_ReturnsOnlyBatchFilesThatNeedTheFix()
        {
            var root = Path.Combine(Path.GetTempPath(), "dpop0417-zapret-scan-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(root);
            try
            {
                File.WriteAllText(Path.Combine(root, "general.bat"), DiscordLine);
                File.WriteAllText(Path.Combine(root, "already-fixed.bat"), DiscordLine.Replace("--filter-tcp=2053", "--filter-tcp=443,2053"));
                File.WriteAllText(Path.Combine(root, "unrelated.bat"), "--filter-tcp=80,443 --hostlist=list-general.txt");

                var candidates = ZapretStrategyPatcher.FindCandidates(root);

                Assert.AreEqual(1, candidates.Count);
                Assert.AreEqual("general.bat", Path.GetFileName(candidates[0]));
            }
            finally
            {
                if (Directory.Exists(root)) Directory.Delete(root, true);
            }
        }
    }
}
