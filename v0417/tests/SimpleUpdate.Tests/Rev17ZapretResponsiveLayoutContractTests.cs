using System;
using System.IO;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace SimpleUpdate.Tests
{
    [TestClass]
    public sealed class Rev17ZapretResponsiveLayoutContractTests
    {
        private static string ReadSource(string fileName)
        {
            var directory = new DirectoryInfo(AppDomain.CurrentDomain.BaseDirectory);
            while (directory != null)
            {
                var candidate = Path.Combine(directory.FullName, "v0417", "src", "SimpleUpdate", fileName);
                if (File.Exists(candidate)) return File.ReadAllText(candidate);
                directory = directory.Parent;
            }
            Assert.Fail("Could not locate repository source file: " + fileName);
            return string.Empty;
        }

        [TestMethod]
        public void Zapret_page_uses_one_responsive_layout_pass_for_all_action_rows()
        {
            var source = ReadSource("ZapretEnhancementHost.cs");

            StringAssert.Contains(source, "ApplyResponsiveLayout()");
            StringAssert.Contains(source, "LayoutZapretRow");
            StringAssert.Contains(source, "ResponsiveZapretButtonIds");
            StringAssert.Contains(source, "NativeBridge.ZapretApplyButtonId");
            foreach (var id in new[] { "1701", "1702", "1703", "1704", "1705", "1707", "1708", "1710", "1711", "1713", "1714", "1716", "1717", "1720", "1721", "1722", "1723", "1724", "1725" })
                StringAssert.Contains(source, id);
        }

        [TestMethod]
        public void Responsive_layout_is_recomputed_from_current_client_width_and_not_legacy_button_bounds()
        {
            var source = ReadSource("ZapretEnhancementHost.cs");

            StringAssert.Contains(source, "GetClientRect(_parent");
            StringAssert.Contains(source, "TextRenderer.MeasureText");
            StringAssert.Contains(source, "clientWidth");
            StringAssert.Contains(source, "Math.Max");
            StringAssert.Contains(source, "Math.Min");
            Assert.IsFalse(source.Contains("PositionInstallServiceProxy()"), "Install-service proxy must participate in the common responsive grid, not shadow frozen bounds.");
            Assert.IsFalse(source.Contains("PositionRemoveServicesProxy()"), "Remove-services proxy must participate in the common responsive grid, not shadow frozen bounds.");
            Assert.IsFalse(source.Contains("PositionStartStandaloneProxy()"), "Standalone-start proxy must participate in the common responsive grid, not shadow frozen bounds.");
        }

        [TestMethod]
        public void Responsive_layout_has_explicit_row_spacing_and_minimum_button_width_rules()
        {
            var source = ReadSource("ZapretEnhancementHost.cs");

            StringAssert.Contains(source, "ResponsiveRowGap");
            StringAssert.Contains(source, "ResponsiveColumnGap");
            StringAssert.Contains(source, "ResponsiveButtonHeight");
            StringAssert.Contains(source, "ResponsiveMinimumButtonWidth");
        }
    }
}
