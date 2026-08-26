using System;

namespace DPop.DiskAnalyzer.Scanning
{
    public sealed class FileSystemEntry
    {
        public string FullPath { get; set; }
        public string Name { get; set; }
        public bool IsDirectory { get; set; }
        public bool IsReparsePoint { get; set; }
        public long Length { get; set; }
        public DateTime ModifiedUtc { get; set; }
    }
}
