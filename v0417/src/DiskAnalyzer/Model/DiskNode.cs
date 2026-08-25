using System;
using System.Collections.Generic;

namespace DPop.DiskAnalyzer.Model
{
    public sealed class DiskNode
    {
        public string Name { get; set; }
        public string FullPath { get; set; }
        public bool IsDirectory { get; set; }
        public long LogicalBytes { get; set; }
        public long? AllocatedBytes { get; set; }
        public bool AllocatedComplete { get; set; } = true;
        public long FileCount { get; set; }
        public long FolderCount { get; set; }
        public DateTime ModifiedUtc { get; set; }
        public List<DiskNode> Children { get; } = new List<DiskNode>();
    }
}
