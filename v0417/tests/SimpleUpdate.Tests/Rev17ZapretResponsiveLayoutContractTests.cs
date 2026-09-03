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
            var source = ReadSource("ZapretResponsiveLayoutHost.cs");

            StringAssert.Contains(source, "ApplyResponsiveLayout()");
            StringAssert.Contains(source, "LayoutZapretRow");
            StringAssert.Contains(source, "ResponsiveZapretButtonIds");
            StringAssert.Contains(source, "NativeBridge.ZapretApplyButtonId");
            foreach (var id in new[] { "1701", "1702", "1703", "1704", "1705", "1707", "1708", "1710", "1711", "1713", "1714", "1716", "1717", "1720", "1721", "1722", "1723", "1724", "1725" })
                StringAssert.Contains(source, id);
        }

        [TestMethod]
        public void Responsive_layout_is_recomputed_from_current_client_width_and_native_dpi_height()
        {
            var source = ReadSource("ZapretResponsiveLayoutHost.cs");

            StringAssert.Contains(source, "GetClientRect(_parent");
            StringAssert.Contains(source, "TextRenderer.MeasureText");
            StringAssert.Contains(source, "clientWidth");
            StringAssert.Contains(source, "nativeButtonHeight");
            StringAssert.Contains(source, "scale");
            StringAssert.Contains(source, "Math.Max");
            StringAssert.Contains(source, "Math.Min");
        }

        [TestMethod]
        public void Responsive_layout_has_explicit_row_spacing_and_minimum_button_width_rules()
        {
            var source = ReadSource("ZapretResponsiveLayoutHost.cs");

            StringAssert.Contains(source, "ResponsiveRowGap");
            StringAssert.Contains(source, "ResponsiveColumnGap");
            StringAssert.Contains(source, "ResponsiveButtonHeight");
            StringAssert.Contains(source, "ResponsiveMinimumButtonWidth");
        }

        [TestMethod]
        public void Vertical_grid_is_anchored_to_live_status_region_not_previous_responsive_positions()
        {
            var source = ReadSource("ZapretResponsiveLayoutHost.cs");

            StringAssert.Contains(source, "FindStatusRegionBounds");
            StringAssert.Contains(source, "statusBounds.Bottom + rowGap");
            StringAssert.Contains(source, "updateHeadingTop");
            Assert.IsFalse(source.Contains("legacyStrategyTop"),
                "Strategy Y must not be inherited from the previous responsive tick; maximize/restore must be reversible.");
            Assert.IsFalse(source.Contains("updateCurrentTop"),
                "Update-row Y must not be inherited from the previous responsive tick; maximize/restore must be reversible.");
        }

        [TestMethod]
        public void Update_heading_is_bound_to_exact_native_caption_not_a_fuzzy_Zapret_update_match()
        {
            var source = ReadSource("ZapretResponsiveLayoutHost.cs");

            StringAssert.Contains(source, "FindStaticByCaption(children, \"Обновление Zapret\", \"Zapret Update\")");
            Assert.IsFalse(source.Contains("mentionsZapret"),
                "A fuzzy Zapret/update match can select another visible status label and leave the real heading on its frozen Y.");
            Assert.IsFalse(source.Contains("mentionsUpdate"),
                "The update heading must be identified by its exact native caption before it is repositioned.");
        }

        [TestMethod]
        public void Launcher_applies_responsive_layout_after_existing_Zapret_bridge_and_visual_polish()
        {
            var launcher = ReadSource("LauncherContext.cs");
            StringAssert.Contains(launcher, "private ZapretResponsiveLayoutHost _zapretResponsiveHost;");
            StringAssert.Contains(launcher, "new ZapretResponsiveLayoutHost(_mainWindow)");
            StringAssert.Contains(launcher, "_zapretResponsiveHost.Show();");
            StringAssert.Contains(launcher, "_zapretResponsiveHost.Dispose();");

            var bridgeIndex = launcher.IndexOf("_zapretHost.Show();", StringComparison.Ordinal);
            var visualIndex = launcher.IndexOf("_zapretVisualHost.Show();", StringComparison.Ordinal);
            var responsiveIndex = launcher.IndexOf("_zapretResponsiveHost.Show();", StringComparison.Ordinal);
            Assert.IsTrue(bridgeIndex >= 0 && visualIndex > bridgeIndex && responsiveIndex > visualIndex,
                "Responsive geometry must run last so legacy/proxy positioning cannot overwrite rev.17 layout in the same 100ms tick.");
        }
    }
}
