using System;

namespace DPop.Common.History
{
    public sealed class HistoryRecord
    {
        public Guid Id { get; set; }
        public DateTime TimestampUtc { get; set; }
        public string OperationId { get; set; }
        public string Description { get; set; }
        public string Target { get; set; }
        public string BeforeState { get; set; }
        public string AfterState { get; set; }
        public string BackupReference { get; set; }
        public bool RollbackAvailable { get; set; }
        public string RollbackStatus { get; set; }

        public static HistoryRecord Create(string operationId, string target, bool rollbackAvailable)
        {
            return new HistoryRecord
            {
                Id = Guid.NewGuid(),
                TimestampUtc = DateTime.UtcNow,
                OperationId = operationId,
                Target = target,
                RollbackAvailable = rollbackAvailable,
                RollbackStatus = rollbackAvailable ? "available" : "unavailable",
            };
        }
    }
}
