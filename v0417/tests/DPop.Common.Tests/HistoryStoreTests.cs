using System;
using System.IO;
using System.Linq;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using DPop.Common.History;

namespace DPop.Common.Tests
{
    [TestClass]
    public class HistoryStoreTests
    {
        [TestMethod]
        public void HistoryIsAppendOnlyAndNewestFirst()
        {
            var root = Path.Combine(Path.GetTempPath(), "dpop0417-history-" + Guid.NewGuid().ToString("N"));
            try
            {
                var store = new HistoryStore(root);
                var first = new HistoryRecord
                {
                    Id = Guid.NewGuid(),
                    TimestampUtc = new DateTime(2026, 8, 25, 12, 0, 0, DateTimeKind.Utc),
                    OperationId = "one",
                    Description = "First operation",
                    Target = "A",
                    RollbackAvailable = true,
                    RollbackStatus = "available",
                };
                var second = new HistoryRecord
                {
                    Id = Guid.NewGuid(),
                    TimestampUtc = new DateTime(2026, 8, 25, 12, 0, 1, DateTimeKind.Utc),
                    OperationId = "two",
                    Description = "Second operation",
                    Target = "B",
                    RollbackAvailable = false,
                    RollbackStatus = "unavailable",
                };

                store.Append(first);
                store.Append(second);

                var records = store.ReadAll();
                Assert.AreEqual(2, records.Count);
                Assert.AreEqual("two", records[0].OperationId);
                Assert.AreEqual("one", records[1].OperationId);
                Assert.AreEqual(2, Directory.GetFiles(root, "*.json").Length);

                Assert.ThrowsException<InvalidOperationException>(() => store.Append(first));
                Assert.AreEqual(2, Directory.GetFiles(root, "*.json").Length, "Duplicate append must not overwrite or add history.");
                Assert.IsFalse(Directory.GetFiles(root, "*.tmp").Any(), "No temporary history file may remain visible.");
            }
            finally
            {
                if (Directory.Exists(root)) Directory.Delete(root, true);
            }
        }
    }
}
