using Microsoft.VisualStudio.TestTools.UnitTesting;
using DPop.Common.Localization;
using DPop.Common.Tests.TestFixtures;

namespace DPop.Common.Tests
{
    [TestClass]
    public class LanguageCatalogTests
    {
        [TestMethod]
        public void MissingEnglishKeyFallsBackToRussian()
        {
            var dir = TestLanguageFixture.Create(
                ru: "{\"common.ok\":\"ОК\",\"disk.scan\":\"Сканировать\"}",
                en: "{\"common.ok\":\"OK\"}");

            var catalog = LanguageCatalog.Load(dir, "en");

            Assert.AreEqual("OK", catalog.Get("common.ok"));
            Assert.AreEqual("Сканировать", catalog.Get("disk.scan"));
        }

        [TestMethod]
        public void UnknownLanguageFallsBackToRussianPack()
        {
            var dir = TestLanguageFixture.Create(
                ru: "{\"common.ok\":\"ОК\"}",
                en: "{}");

            var catalog = LanguageCatalog.Load(dir, "de");

            Assert.AreEqual("ОК", catalog.Get("common.ok"));
        }

        [TestMethod]
        public void MissingKeyIsVisibleInsteadOfBlank()
        {
            var dir = TestLanguageFixture.Create(ru: "{}", en: "{}");
            var catalog = LanguageCatalog.Load(dir, "en");

            Assert.AreEqual("[missing.key]", catalog.Get("missing.key"));
        }
    }
}
