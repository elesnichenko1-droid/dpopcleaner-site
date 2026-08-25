using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using DPop.DiskAnalyzer.Scanning;
using DPop.DiskAnalyzer.Tests.TestFixtures;

namespace DPop.DiskAnalyzer.Tests
{
    [TestClass]
    public class DiskScannerTests
    {
        [TestMethod]
        public async Task FolderTotalsAreRecursiveAndUnknownAllocationPropagates()
        {
            const string root = @"C:\fixture";
            const string a = @"C:\fixture\a.bin";
            const string sub = @"C:\fixture\sub";
            const string b = @"C:\fixture\sub\b.bin";
            var modified = new DateTime(2026, 8, 25, 12, 0, 0, DateTimeKind.Utc);

            var fs = new FakeFileSystemView();
            fs.AddDirectory(root,
                new FileSystemEntry { FullPath = a, Name = "a.bin", Length = 100, ModifiedUtc = modified },
                new FileSystemEntry { FullPath = sub, Name = "sub", IsDirectory = true, ModifiedUtc = modified });
            fs.AddDirectory(sub,
                new FileSystemEntry { FullPath = b, Name = "b.bin", Length = 300, ModifiedUtc = modified });

            var allocation = new FakeAllocationProvider(new Dictionary<string, long?>
            {
                [a] = 4096,
                [b] = null,
            });
            var scanner = new DiskScanner(allocation, fs);

            var result = await scanner.ScanAsync(root, CancellationToken.None);

            Assert.AreEqual(400L, result.LogicalBytes);
            Assert.IsFalse(result.AllocatedComplete);
            Assert.IsNull(result.AllocatedBytes);
            Assert.AreEqual(2L, result.FileCount);
            Assert.AreEqual(1L, result.FolderCount);
            Assert.AreEqual(2, result.Children.Count);
            Assert.AreEqual(300L, result.Children.Single(x => x.Name == "sub").LogicalBytes);
        }

        [TestMethod]
        public async Task ReparseDirectoriesAreRepresentedButNotFollowedByDefault()
        {
            const string root = @"C:\fixture";
            const string link = @"C:\fixture\link";
            const string hidden = @"C:\fixture\link\hidden.bin";

            var fs = new FakeFileSystemView();
            fs.AddDirectory(root,
                new FileSystemEntry { FullPath = link, Name = "link", IsDirectory = true, IsReparsePoint = true });
            fs.AddDirectory(link,
                new FileSystemEntry { FullPath = hidden, Name = "hidden.bin", Length = 999 });

            var scanner = new DiskScanner(new FakeAllocationProvider(), fs);
            var result = await scanner.ScanAsync(root, CancellationToken.None);

            var linkNode = result.Children.Single(x => x.Name == "link");
            Assert.AreEqual(0L, linkNode.FileCount);
            Assert.AreEqual(0L, linkNode.FolderCount);
            Assert.AreEqual(0L, linkNode.LogicalBytes);
            Assert.IsFalse(linkNode.AllocatedComplete);
            Assert.IsNull(linkNode.AllocatedBytes);
        }

        [TestMethod]
        public async Task ScanReportsProgressAndHonorsCancellation()
        {
            const string root = @"C:\fixture";
            var entries = Enumerable.Range(1, 200)
                .Select(i => new FileSystemEntry
                {
                    FullPath = root + @"\file" + i + ".bin",
                    Name = "file" + i + ".bin",
                    Length = i,
                    ModifiedUtc = DateTime.UtcNow,
                })
                .ToArray();

            var fs = new FakeFileSystemView();
            fs.AddDirectory(root, entries);
            var scanner = new DiskScanner(new FakeAllocationProvider(), fs);
            var seen = new List<ScanProgress>();
            var cts = new CancellationTokenSource();

            await Assert.ThrowsExceptionAsync<OperationCanceledException>(async () =>
                await scanner.ScanAsync(root, cts.Token, progress =>
                {
                    seen.Add(progress);
                    if (progress.FilesScanned >= 10)
                        cts.Cancel();
                }));

            Assert.IsTrue(seen.Count > 0);
            Assert.IsTrue(seen.Any(x => x.FilesScanned >= 10));
            Assert.IsTrue(seen.All(x => !string.IsNullOrWhiteSpace(x.CurrentPath)));
        }
    }
}
