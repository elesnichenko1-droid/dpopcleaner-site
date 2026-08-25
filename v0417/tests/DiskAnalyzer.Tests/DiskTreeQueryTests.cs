using System.Collections.Generic;
using System.Linq;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using DPop.DiskAnalyzer.Model;
using DPop.DiskAnalyzer.UI;

namespace DPop.DiskAnalyzer.Tests
{
    [TestClass]
    public class DiskTreeQueryTests
    {
        [TestMethod]
        public void LargestFilesSortsNumericallyNotLexically()
        {
            var root = new DiskNode { Name = "root", IsDirectory = true };
            root.Children.Add(File("900.bin", 900));
            root.Children.Add(File("12000.bin", 12000));
            root.Children.Add(File("1000.bin", 1000));

            var files = DiskTreeQuery.LargestFiles(root, 3);

            CollectionAssert.AreEqual(
                new long[] { 12000, 1000, 900 },
                files.Select(x => x.LogicalBytes).ToArray());
        }

        [TestMethod]
        public void GridSizeSorterUsesRawBytes()
        {
            var rows = new List<DiskGridRow>
            {
                new DiskGridRow(File("900.bin", 900), 0, 0),
                new DiskGridRow(File("12000.bin", 12000), 0, 0),
                new DiskGridRow(File("1000.bin", 1000), 0, 0),
            };

            var sorted = DiskGridSorter.Sort(rows, "size", ascending: false).ToArray();

            CollectionAssert.AreEqual(
                new long[] { 12000, 1000, 900 },
                sorted.Select(x => x.Node.LogicalBytes).ToArray());
        }

        private static DiskNode File(string name, long bytes)
        {
            return new DiskNode
            {
                Name = name,
                FullPath = name,
                IsDirectory = false,
                LogicalBytes = bytes,
                AllocatedBytes = bytes,
                AllocatedComplete = true,
                FileCount = 1,
            };
        }
    }
}
