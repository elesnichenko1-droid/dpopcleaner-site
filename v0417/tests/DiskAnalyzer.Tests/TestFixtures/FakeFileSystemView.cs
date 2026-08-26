using System;
using System.Collections.Generic;
using DPop.DiskAnalyzer.Scanning;

namespace DPop.DiskAnalyzer.Tests.TestFixtures
{
    internal sealed class FakeFileSystemView : IFileSystemView
    {
        private readonly Dictionary<string, List<FileSystemEntry>> _entries =
            new Dictionary<string, List<FileSystemEntry>>(StringComparer.OrdinalIgnoreCase);

        public void AddDirectory(string path, params FileSystemEntry[] entries)
        {
            _entries[path] = new List<FileSystemEntry>(entries);
        }

        public IEnumerable<FileSystemEntry> Enumerate(string directory)
        {
            List<FileSystemEntry> entries;
            return _entries.TryGetValue(directory, out entries)
                ? entries
                : (IEnumerable<FileSystemEntry>)Array.Empty<FileSystemEntry>();
        }
    }
}
