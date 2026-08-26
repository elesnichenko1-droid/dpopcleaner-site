using System;
using DPop.DiskAnalyzer.Model;

namespace DPop.DiskAnalyzer.UI
{
    public sealed class DiskGridRow
    {
        public DiskGridRow(DiskNode node, int depth, double parentPercent)
        {
            Node = node ?? throw new ArgumentNullException(nameof(node));
            Depth = Math.Max(0, depth);
            ParentPercent = Math.Max(0.0, Math.Min(100.0, parentPercent));
        }

        public DiskNode Node { get; }
        public int Depth { get; }
        public double ParentPercent { get; }
        public string NameText => new string(' ', Depth * 2) + Node.Name;
    }
}
