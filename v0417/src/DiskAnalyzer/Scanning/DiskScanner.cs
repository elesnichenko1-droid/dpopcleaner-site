using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using DPop.DiskAnalyzer.Model;

namespace DPop.DiskAnalyzer.Scanning
{
    public sealed class DiskScanner
    {
        private readonly IAllocationSizeProvider _allocation;
        private readonly IFileSystemView _fileSystem;

        public DiskScanner(IAllocationSizeProvider allocation, IFileSystemView fileSystem = null)
        {
            _allocation = allocation ?? throw new ArgumentNullException(nameof(allocation));
            _fileSystem = fileSystem ?? new RealFileSystemView();
        }

        public Task<DiskNode> ScanAsync(string rootPath, CancellationToken token)
        {
            if (string.IsNullOrWhiteSpace(rootPath))
                throw new ArgumentException("A scan root is required.", nameof(rootPath));

            return Task.Run(() => ScanDirectory(rootPath, token), token);
        }

        private DiskNode ScanDirectory(string path, CancellationToken token)
        {
            token.ThrowIfCancellationRequested();

            var node = new DiskNode
            {
                Name = DisplayName(path),
                FullPath = path,
                IsDirectory = true,
                AllocatedComplete = true,
                AllocatedBytes = 0,
            };

            long allocatedTotal = 0;

            try
            {
                foreach (var entry in _fileSystem.Enumerate(path))
                {
                    token.ThrowIfCancellationRequested();

                    DiskNode child;
                    if (entry.IsDirectory)
                    {
                        if (entry.IsReparsePoint)
                        {
                            child = new DiskNode
                            {
                                Name = entry.Name,
                                FullPath = entry.FullPath,
                                IsDirectory = true,
                                LogicalBytes = 0,
                                AllocatedBytes = null,
                                AllocatedComplete = false,
                                FileCount = 0,
                                FolderCount = 0,
                                ModifiedUtc = entry.ModifiedUtc,
                            };
                        }
                        else
                        {
                            child = ScanDirectory(entry.FullPath, token);
                            child.Name = entry.Name;
                            child.ModifiedUtc = entry.ModifiedUtc;
                        }

                        node.FolderCount += 1 + child.FolderCount;
                    }
                    else
                    {
                        var allocated = _allocation.GetAllocatedBytes(entry.FullPath);
                        child = new DiskNode
                        {
                            Name = entry.Name,
                            FullPath = entry.FullPath,
                            IsDirectory = false,
                            LogicalBytes = entry.Length,
                            AllocatedBytes = allocated,
                            AllocatedComplete = allocated.HasValue,
                            FileCount = 1,
                            FolderCount = 0,
                            ModifiedUtc = entry.ModifiedUtc,
                        };
                    }

                    node.Children.Add(child);
                    node.LogicalBytes += child.LogicalBytes;
                    node.FileCount += child.FileCount;

                    if (node.AllocatedComplete && child.AllocatedComplete && child.AllocatedBytes.HasValue)
                    {
                        allocatedTotal += child.AllocatedBytes.Value;
                    }
                    else
                    {
                        node.AllocatedComplete = false;
                    }
                }
            }
            catch (UnauthorizedAccessException)
            {
                node.AllocatedComplete = false;
            }
            catch (IOException)
            {
                node.AllocatedComplete = false;
            }

            node.AllocatedBytes = node.AllocatedComplete ? (long?)allocatedTotal : null;
            return node;
        }

        private static string DisplayName(string path)
        {
            var trimmed = path.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            var name = Path.GetFileName(trimmed);
            return string.IsNullOrEmpty(name) ? path : name;
        }
    }
}
