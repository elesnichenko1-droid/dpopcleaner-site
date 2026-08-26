namespace DPop.DiskAnalyzer.Scanning
{
    public sealed class ScanProgress
    {
        public string CurrentPath { get; set; }
        public long FilesScanned { get; set; }
        public long FoldersScanned { get; set; }
        public long LogicalBytesFound { get; set; }
    }
}
