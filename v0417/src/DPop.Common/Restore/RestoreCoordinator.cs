using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using DPop.Common.History;

namespace DPop.Common.Restore
{
    public sealed class RestoreCoordinator
    {
        private readonly HistoryStore _history;
        private readonly BackupStore _backups;
        private readonly IReadOnlyList<IRestoreProvider> _providers;

        public RestoreCoordinator(HistoryStore history, BackupStore backups, IEnumerable<IRestoreProvider> providers)
        {
            _history = history ?? throw new ArgumentNullException(nameof(history));
            _backups = backups ?? throw new ArgumentNullException(nameof(backups));
            if (providers == null) throw new ArgumentNullException(nameof(providers));
            _providers = providers.Where(provider => provider != null).ToList().AsReadOnly();
        }

        public RestoreResult Restore(Guid historyRecordId)
        {
            var source = _history.Find(historyRecordId);
            if (source == null)
                return RestoreResult.Fail("history.not_found");

            if (!source.RollbackAvailable || string.IsNullOrWhiteSpace(source.BackupReference))
                return RestoreResult.Fail("rollback.unavailable");

            var provider = _providers.FirstOrDefault(candidate => candidate.CanHandle(source.OperationId));
            if (provider == null)
                return RestoreResult.Fail("provider.unavailable");

            string originalState;
            try
            {
                originalState = Encoding.UTF8.GetString(_backups.ReadBytes(source.BackupReference));
            }
            catch (Exception ex)
            {
                return RestoreResult.Fail("backup.invalid", ex.Message);
            }

            string currentState;
            string currentBackupReference;
            try
            {
                currentState = provider.Capture(source.Target);
                currentBackupReference = _backups.SaveBytes(
                    provider.BackupCategory,
                    Encoding.UTF8.GetBytes(currentState));

                var prestate = HistoryRecord.Create("restore.prestate", source.Target, rollbackAvailable: false);
                prestate.Description = source.Id.ToString("D");
                prestate.BeforeState = currentState;
                prestate.BackupReference = currentBackupReference;
                prestate.RollbackStatus = "captured";
                _history.Append(prestate);
            }
            catch (Exception ex)
            {
                return RestoreResult.Fail("prestate.capture_failed", ex.Message);
            }

            try
            {
                provider.Restore(source.Target, originalState);
                var verifiedState = provider.Capture(source.Target);
                if (!string.Equals(verifiedState, originalState, StringComparison.Ordinal))
                {
                    AppendOutcome("restore.failure", source, currentState, verifiedState, "verification_failed");
                    return RestoreResult.Fail("restore.verification_failed");
                }

                AppendOutcome("restore.success", source, currentState, verifiedState, "success");
                return RestoreResult.Ok();
            }
            catch (Exception ex)
            {
                AppendOutcome("restore.failure", source, currentState, null, "failed");
                return RestoreResult.Fail("restore.failed", ex.Message);
            }
        }

        private void AppendOutcome(
            string operationId,
            HistoryRecord source,
            string beforeState,
            string afterState,
            string status)
        {
            try
            {
                var outcome = HistoryRecord.Create(operationId, source.Target, rollbackAvailable: false);
                outcome.Description = source.Id.ToString("D");
                outcome.BeforeState = beforeState;
                outcome.AfterState = afterState;
                outcome.BackupReference = source.BackupReference;
                outcome.RollbackStatus = status;
                _history.Append(outcome);
            }
            catch
            {
                // The original backup is intentionally never deleted even if audit append fails.
            }
        }
    }
}
