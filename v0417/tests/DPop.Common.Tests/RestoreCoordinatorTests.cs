using System;
using System.IO;
using System.Linq;
using System.Text;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using DPop.Common.History;
using DPop.Common.Restore;

namespace DPop.Common.Tests
{
    [TestClass]
    public class RestoreCoordinatorTests
    {
        [TestMethod]
        public void RestoreCapturesCurrentStateThenRestoresOriginalBackup()
        {
            var root = Path.Combine(Path.GetTempPath(), "dpop0417-restore-coordinator-" + Guid.NewGuid().ToString("N"));
            var settingsRoot = Path.Combine(root, "settings");
            var historyRoot = Path.Combine(root, "Documentation", "History");
            var backupsRoot = Path.Combine(root, "Documentation", "Backups");
            Directory.CreateDirectory(settingsRoot);
            try
            {
                var target = Path.Combine(settingsRoot, "settings.json");
                File.WriteAllText(target, "before");

                var history = new HistoryStore(historyRoot);
                var backups = new BackupStore(backupsRoot);
                var provider = new FileStateProvider(settingsRoot);
                var beforeState = provider.Capture(target);
                var beforeReference = backups.SaveBytes("Settings", Encoding.UTF8.GetBytes(beforeState));
                var original = HistoryRecord.Create("settings.file", target, rollbackAvailable: true);
                original.Description = "settings change";
                original.BackupReference = beforeReference;
                history.Append(original);

                File.WriteAllText(target, "after");
                var coordinator = new RestoreCoordinator(history, backups, new IRestoreProvider[] { provider });

                var result = coordinator.Restore(original.Id);

                Assert.IsTrue(result.Success, result.Code + ": " + result.Message);
                Assert.AreEqual("before", File.ReadAllText(target));
                Assert.IsTrue(backups.Exists(beforeReference), "Original backup must never be consumed by rollback.");
                var records = history.ReadAll();
                Assert.IsTrue(records.Any(x => x.OperationId == "restore.prestate"));
                Assert.IsTrue(records.Any(x => x.OperationId == "restore.success"));
                var prestate = records.First(x => x.OperationId == "restore.prestate");
                Assert.IsTrue(backups.Exists(prestate.BackupReference), "Pre-restore current state must be backed up.");
            }
            finally
            {
                if (Directory.Exists(root)) Directory.Delete(root, true);
            }
        }

        [TestMethod]
        public void NonReversibleRecordIsRejectedWithoutSideEffects()
        {
            var root = Path.Combine(Path.GetTempPath(), "dpop0417-restore-unavailable-" + Guid.NewGuid().ToString("N"));
            var settingsRoot = Path.Combine(root, "settings");
            Directory.CreateDirectory(settingsRoot);
            try
            {
                var target = Path.Combine(settingsRoot, "settings.json");
                File.WriteAllText(target, "unchanged");
                var history = new HistoryStore(Path.Combine(root, "Documentation", "History"));
                var backups = new BackupStore(Path.Combine(root, "Documentation", "Backups"));
                var record = HistoryRecord.Create("cleanup.temp", target, rollbackAvailable: false);
                history.Append(record);
                var coordinator = new RestoreCoordinator(
                    history,
                    backups,
                    new IRestoreProvider[] { new FileStateProvider(settingsRoot) });

                var result = coordinator.Restore(record.Id);

                Assert.IsFalse(result.Success);
                Assert.AreEqual("rollback.unavailable", result.Code);
                Assert.AreEqual("unchanged", File.ReadAllText(target));
                Assert.AreEqual(1, history.ReadAll().Count, "Rejected rollback must not create fake history records.");
            }
            finally
            {
                if (Directory.Exists(root)) Directory.Delete(root, true);
            }
        }
    }
}
