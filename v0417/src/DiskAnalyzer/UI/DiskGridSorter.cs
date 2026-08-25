using System;
using System.Collections.Generic;
using System.Linq;

namespace DPop.DiskAnalyzer.UI
{
    public static class DiskGridSorter
    {
        public static IEnumerable<DiskGridRow> Sort(
            IEnumerable<DiskGridRow> rows,
            string columnName,
            bool ascending)
        {
            if (rows == null) throw new ArgumentNullException(nameof(rows));
            return rows.OrderBy(x => x, new RowComparer(columnName, ascending));
        }

        private sealed class RowComparer : IComparer<DiskGridRow>
        {
            private readonly string _columnName;
            private readonly bool _ascending;

            public RowComparer(string columnName, bool ascending)
            {
                _columnName = columnName ?? "name";
                _ascending = ascending;
            }

            public int Compare(DiskGridRow x, DiskGridRow y)
            {
                if (ReferenceEquals(x, y)) return 0;
                if (x == null) return 1;
                if (y == null) return -1;

                if (string.Equals(_columnName, "allocated", StringComparison.OrdinalIgnoreCase))
                {
                    var xValue = x.Node.AllocatedComplete ? x.Node.AllocatedBytes : null;
                    var yValue = y.Node.AllocatedComplete ? y.Node.AllocatedBytes : null;
                    if (!xValue.HasValue && yValue.HasValue) return 1;
                    if (xValue.HasValue && !yValue.HasValue) return -1;
                    if (xValue.HasValue && yValue.HasValue)
                        return WithDirection(xValue.Value.CompareTo(yValue.Value), x, y);
                }

                int result;
                switch (_columnName)
                {
                    case "size":
                        result = x.Node.LogicalBytes.CompareTo(y.Node.LogicalBytes);
                        break;
                    case "files":
                        result = x.Node.FileCount.CompareTo(y.Node.FileCount);
                        break;
                    case "folders":
                        result = x.Node.FolderCount.CompareTo(y.Node.FolderCount);
                        break;
                    case "parentPercent":
                        result = x.ParentPercent.CompareTo(y.ParentPercent);
                        break;
                    case "modified":
                        result = x.Node.ModifiedUtc.CompareTo(y.Node.ModifiedUtc);
                        break;
                    default:
                        result = string.Compare(x.Node.Name, y.Node.Name, StringComparison.CurrentCultureIgnoreCase);
                        break;
                }

                return WithDirection(result, x, y);
            }

            private int WithDirection(int result, DiskGridRow x, DiskGridRow y)
            {
                if (result == 0)
                    result = string.Compare(x.Node.Name, y.Node.Name, StringComparison.CurrentCultureIgnoreCase);
                return _ascending ? result : -result;
            }
        }
    }
}
