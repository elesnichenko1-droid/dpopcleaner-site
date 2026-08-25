using System.Collections.Generic;

namespace DPop.DiskAnalyzer.Scanning
{
    public interface IFileSystemView
    {
        IEnumerable<FileSystemEntry> Enumerate(string directory);
    }
}
