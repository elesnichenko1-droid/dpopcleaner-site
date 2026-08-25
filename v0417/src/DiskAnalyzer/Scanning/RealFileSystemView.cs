using System;
using System.Collections.Generic;
using System.IO;

namespace DPop.DiskAnalyzer.Scanning
{
    public sealed class RealFileSystemView : IFileSystemView
    {
        public IEnumerable<FileSystemEntry> Enumerate(string directory)
        {
            foreach (var path in Directory.EnumerateFileSystemEntries(directory))
            {
                var attributes = File.GetAttributes(path);
                var isDirectory = (attributes & FileAttributes.Directory) != 0;
                var isReparse = (attributes & FileAttributes.ReparsePoint) != 0;

                if (isDirectory)
                {
                    var info = new DirectoryInfo(path);
                    yield return new FileSystemEntry
                    {
                        FullPath = info.FullName,
                        Name = info.Name,
                        IsDirectory = true,
                        IsReparsePoint = isReparse,
                        Length = 0,
                        ModifiedUtc = info.LastWriteTimeUtc,
                    };
                }
                else
                {
                    var info = new FileInfo(path);
                    yield return new FileSystemEntry
                    {
                        FullPath = info.FullName,
                        Name = info.Name,
                        IsDirectory = false,
                        IsReparsePoint = isReparse,
                        Length = info.Length,
                        ModifiedUtc = info.LastWriteTimeUtc,
                    };
                }
            }
        }
    }
}
