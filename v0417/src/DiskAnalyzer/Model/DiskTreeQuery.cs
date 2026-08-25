using System;
using System.Collections.Generic;
using System.Linq;

namespace DPop.DiskAnalyzer.Model
{
    public static class DiskTreeQuery
    {
        public static IReadOnlyList<DiskNode> LargestFiles(DiskNode root, int limit)
        {
            if (root == null) throw new ArgumentNullException(nameof(root));
            if (limit <= 0) return Array.Empty<DiskNode>();

            var files = new List<DiskNode>();
            CollectFiles(root, files);
            return files
                .OrderByDescending(x => x.LogicalBytes)
                .ThenBy(x => x.FullPath, StringComparer.OrdinalIgnoreCase)
                .Take(limit)
                .ToArray();
        }

        private static void CollectFiles(DiskNode node, ICollection<DiskNode> files)
        {
            if (!node.IsDirectory)
            {
                files.Add(node);
                return;
            }

            foreach (var child in node.Children)
                CollectFiles(child, files);
        }
    }
}
