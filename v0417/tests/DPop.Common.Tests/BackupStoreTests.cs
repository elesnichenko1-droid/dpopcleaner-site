using System;
using System.IO;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using DPop.Common.History;

namespace DPop.Common.Tests
{
    [TestClass]
    public class BackupStoreTests
    {
        [TestMethod]
        public void BackupsAreImmutableAndTraversalSafe()
        {
            var root = Path.Combine(Path.GetTempPath(), "dpop0417-backups-" + Guid.NewGuid().ToString("N"));
            try
            {
                var store = new BackupStore(root);
                var firstBytes = new byte[] { 1, 2, 3, 4 };
                var secondBytes = new byte[] { 9, 8, 7 };

                var first = store.SaveBytes("Settings", firstBytes);
                var second = store.SaveBytes("Settings", secondBytes);

                Assert.AreNotEqual(first, second);
                CollectionAssert.AreEqual(firstBytes, store.ReadBytes(first));
                CollectionAssert.AreEqual(secondBytes, store.ReadBytes(second));
                CollectionAssert.AreEqual(firstBytes, store.ReadBytes(first), "A later backup must not overwrite an older backup.");

                Assert.ThrowsException<ArgumentException>(() => store.SaveBytes("../escape", firstBytes));
                Assert.ThrowsException<InvalidDataException>(() => store.ReadBytes("../escape.bin"));
                Assert.ThrowsException<InvalidDataException>(() => store.ReadBytes(Path.Combine(root, "outside.bin")));
            }
            finally
            {
                if (Directory.Exists(root)) Directory.Delete(root, true);
            }
        }
    }
}
