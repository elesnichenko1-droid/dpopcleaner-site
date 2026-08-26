using Microsoft.VisualStudio.TestTools.UnitTesting;
using DPop.DiskAnalyzer.Model;
using DPop.DiskAnalyzer.UI;

namespace DPop.DiskAnalyzer.Tests
{
    [TestClass]
    public class SizeFormatterTests
    {
        [TestMethod]
        public void UnknownAllocatedSizeIsDash()
        {
            var node = new DiskNode
            {
                LogicalBytes = 123456,
                AllocatedComplete = false,
                AllocatedBytes = null,
            };

            Assert.AreEqual("—", SizeFormatter.AllocatedText(node));
        }

        [TestMethod]
        public void MissingAllocatedValueIsDashEvenWhenCompletenessFlagWasTrue()
        {
            var node = new DiskNode
            {
                LogicalBytes = 123456,
                AllocatedComplete = true,
                AllocatedBytes = null,
            };

            Assert.AreEqual("—", SizeFormatter.AllocatedText(node));
        }
    }
}
