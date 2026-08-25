using Microsoft.VisualStudio.TestTools.UnitTesting;
using DPop.Common;
using System.IO;

namespace DPop.Common.Tests
{
    [TestClass]
    public class AppPathsTests
    {
        [TestMethod]
        public void IdentityAndFoldersAreDeterministic()
        {
            var root = Path.Combine("C:\\Program Files", "DPopCleaner");
            var paths = new AppPaths(root);

            Assert.AreEqual("0.4.17", AppIdentity.Version);
            Assert.AreEqual("DPopCleaner", AppIdentity.ProductName);
            Assert.AreEqual(Path.Combine(root, "Languages"), paths.LanguagesDirectory);
            Assert.AreEqual(Path.Combine(root, "Shell"), paths.ShellDirectory);
            Assert.AreEqual(Path.Combine(root, "Documentation"), paths.DocumentationDirectory);
            Assert.AreEqual(Path.Combine(root, "Modules"), paths.ModulesDirectory);
            Assert.AreEqual(Path.Combine(root, "Resources"), paths.ResourcesDirectory);
        }
    }
}
