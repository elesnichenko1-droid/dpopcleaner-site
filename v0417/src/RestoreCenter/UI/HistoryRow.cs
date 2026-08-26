using DPop.Common.History;

namespace DPop.RestoreCenter.UI
{
    internal sealed class HistoryRow
    {
        public HistoryRow(HistoryRecord record)
        {
            Record = record;
        }

        public HistoryRecord Record { get; }
    }
}
