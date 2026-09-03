using System;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace DPopCleaner.SimpleUpdate
{
    internal sealed class ZapretResponsiveLayoutHost : IDisposable
    {
        private const int ResponsiveRowGap = 8;
        private const int ResponsiveColumnGap = 8;
        private const int ResponsiveButtonHeight = 34;
        private const int ResponsiveMinimumButtonWidth = 76;
        private const int ResponsiveTextPadding = 28;

        private static readonly int[] ResponsiveZapretButtonIds =
        {
            1701, 1702, 1703, 1704, 1705, 1707, 1708, 1710, 1711,
            1713, 1714, 1716, 1717,
            1720, 1721, 1722, 1723, 1724, 1725
        };

        private static readonly int[] StrategyRowButtonIds = { 1701, 1713, 1714, 1703 };
        private static readonly int[] UpdateRowButtonIds = { 1724, 1725, 1716, 1717, 1702 };
        private static readonly int[] BridgeActionButtonIds = { 1720, 1721, 1722, 1723 };
        private static readonly int[] AdditionalRowButtonIds = { 1704, 1705, 1707, 1708, 1710, 1711 };

        [StructLayout(LayoutKind.Sequential)]
        private struct RECT
        {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
        }

        [DllImport("user32.dll")]
        private static extern bool GetClientRect(IntPtr hwnd, out RECT rect);

        [DllImport("user32.dll")]
        private static extern IntPtr GetParent(IntPtr hwnd);

        private readonly IntPtr _parent;
        private bool _disposed;

        internal ZapretResponsiveLayoutHost(IntPtr parent)
        {
            if (parent == IntPtr.Zero) throw new ArgumentException("Parent window is required.", "parent");
            _parent = parent;
            Show();
        }

        internal void Show()
        {
            if (_disposed) return;
            ApplyResponsiveLayout();
        }

        internal void Hide()
        {
            // Geometry is page-local and the frozen core owns visibility. Nothing is created here.
        }

        internal void ApplyResponsiveLayout()
        {
            if (_disposed) return;

            var marker = NativeBridge.FindChildById(_parent, NativeBridge.ZapretApplyButtonId);
            if (marker == IntPtr.Zero || !NativeBridge.IsWindowVisible(marker)) return;

            RECT clientRect;
            if (!GetClientRect(_parent, out clientRect)) return;
            var clientWidth = Math.Max(1, clientRect.Right - clientRect.Left);

            var children = NativeBridge.GetChildren(_parent);
            var buttons = CaptureResponsiveButtons(children);
            if (buttons.Count != ResponsiveZapretButtonIds.Length) return;

            // The frozen page has two visible Edit status blocks. Treat them as one protected
            // upper region because their heights change independently when the window is widened.
            var statusBounds = FindStatusRegionBounds(children);
            if (statusBounds == null || statusBounds.Width < 400) return;

            var applyBounds = NativeBridge.GetChildClientBounds(_parent, buttons[NativeBridge.ZapretApplyButtonId]);
            if (applyBounds == null) return;
            var nativeButtonHeight = Math.Max(ResponsiveButtonHeight, applyBounds.Height);
            var scale = Math.Max(0.75, Math.Min(2.50, (double)nativeButtonHeight / ResponsiveButtonHeight));
            var buttonHeight = Math.Max(nativeButtonHeight, Scale(ResponsiveButtonHeight, scale));
            var rowGap = Scale(ResponsiveRowGap, scale);
            var columnGap = Scale(ResponsiveColumnGap, scale);
            var minimumButtonWidth = Scale(ResponsiveMinimumButtonWidth, scale);

            var contentLeft = Math.Max(0, statusBounds.Left);
            var rightMargin = Scale(24, scale);
            var contentRight = Math.Max(contentLeft + 1, clientWidth - rightMargin);
            var contentWidth = contentRight - contentLeft;
            if (contentWidth < Scale(620, scale)) return;

            IntPtr strategyCombo;
            IntPtr filterCombo;
            if (!FindZapretCombos(children, out strategyCombo, out filterCombo)) return;

            var strategyComboBounds = NativeBridge.GetChildClientBounds(_parent, strategyCombo);
            var strategyLabel = FindStaticByCaption(children, "Стратегия", "Strategy");
            var strategyLabelBounds = NativeBridge.GetChildClientBounds(_parent, strategyLabel);
            if (strategyComboBounds == null || strategyLabelBounds == null) return;

            var strategyButtons = ResolveButtons(buttons, StrategyRowButtonIds);
            var updateButtons = ResolveButtons(buttons, UpdateRowButtonIds);
            var actionButtons = ResolveButtons(buttons, BridgeActionButtonIds);
            var additionalButtons = ResolveButtons(buttons, AdditionalRowButtonIds);
            if (strategyButtons == null || updateButtons == null || actionButtons == null || additionalButtons == null) return;

            // Every tick is a pure function of the current status/client geometry. Do not inherit Y
            // from a previous responsive pass: maximize -> restore must return to the compact grid.
            var strategyRowTop = statusBounds.Bottom + rowGap;
            var labelWidth = Math.Max(strategyLabelBounds.Width, Scale(82, scale));
            var minimumStrategyWidth = labelWidth + Scale(120, scale);
            var desiredStrategyWidth = Math.Max(minimumStrategyWidth, (int)Math.Round(contentWidth * 0.40));
            var strategyButtonsMinimum = minimumButtonWidth * strategyButtons.Length + columnGap * (strategyButtons.Length - 1);
            var maximumStrategyWidth = contentWidth - strategyButtonsMinimum - columnGap;
            if (maximumStrategyWidth < minimumStrategyWidth) return;
            var strategyWidth = Math.Min(desiredStrategyWidth, maximumStrategyWidth);

            var labelHeight = Math.Max(1, strategyLabelBounds.Height);
            var comboHeight = Math.Max(1, strategyComboBounds.Height);
            var labelTop = strategyRowTop + Math.Max(0, (buttonHeight - labelHeight) / 2);
            var comboTop = strategyRowTop + Math.Max(0, (buttonHeight - comboHeight) / 2);

            NativeBridge.PositionChildWindow(strategyLabel, Bounds(
                contentLeft, labelTop, contentLeft + labelWidth, labelTop + labelHeight));
            NativeBridge.PositionChildWindow(strategyCombo, Bounds(
                contentLeft + labelWidth + columnGap,
                comboTop,
                contentLeft + strategyWidth,
                comboTop + comboHeight));

            LayoutZapretRow(strategyButtons,
                contentLeft + strategyWidth + columnGap,
                strategyRowTop,
                contentRight,
                buttonHeight,
                columnGap,
                minimumButtonWidth,
                scale);

            // "Обновление Zapret" is an explicit responsive row separator, not a frozen Y anchor.
            var updateHeading = FindUpdateHeading(children);
            var updateHeadingBounds = NativeBridge.GetChildClientBounds(_parent, updateHeading);
            var updateHeadingHeight = updateHeadingBounds == null
                ? Scale(23, scale)
                : Math.Max(1, updateHeadingBounds.Height);
            var updateHeadingWidth = updateHeadingBounds == null
                ? Scale(210, scale)
                : Math.Max(updateHeadingBounds.Width, Scale(180, scale));
            var halfGap = Math.Max(2, rowGap / 2);
            var updateHeadingTop = strategyRowTop + buttonHeight + halfGap;
            if (updateHeading != IntPtr.Zero)
            {
                NativeBridge.PositionChildWindow(updateHeading, Bounds(
                    contentLeft,
                    updateHeadingTop,
                    Math.Min(contentRight, contentLeft + updateHeadingWidth),
                    updateHeadingTop + updateHeadingHeight));
            }

            var updateRowTop = Math.Max(
                updateHeadingTop + updateHeadingHeight + halfGap,
                strategyRowTop + buttonHeight + rowGap);
            LayoutZapretRow(updateButtons, contentLeft, updateRowTop, contentRight,
                buttonHeight, columnGap, minimumButtonWidth, scale);

            var actionRowTop = updateRowTop + buttonHeight + rowGap;
            var additionalHeading = FindStaticByCaption(children, "Дополнительно", "Additional");
            var additionalHeadingBounds = NativeBridge.GetChildClientBounds(_parent, additionalHeading);
            var additionalHeadingHeight = additionalHeadingBounds == null
                ? Scale(23, scale)
                : Math.Max(1, additionalHeadingBounds.Height);
            var headingWidth = additionalHeadingBounds == null
                ? Scale(180, scale)
                : Math.Max(additionalHeadingBounds.Width, Scale(150, scale));
            headingWidth = Math.Min(headingWidth, Math.Max(Scale(150, scale), contentWidth / 4));
            var additionalHeadingTop = actionRowTop + Math.Max(0, (buttonHeight - additionalHeadingHeight) / 2);
            if (additionalHeading != IntPtr.Zero)
            {
                NativeBridge.PositionChildWindow(additionalHeading, Bounds(
                    contentLeft,
                    additionalHeadingTop,
                    Math.Min(contentRight, contentLeft + headingWidth),
                    additionalHeadingTop + additionalHeadingHeight));
            }

            var actionLeft = Math.Min(contentRight - 1, contentLeft + headingWidth + columnGap);
            LayoutZapretRow(actionButtons, actionLeft, actionRowTop, contentRight,
                buttonHeight, columnGap, minimumButtonWidth, scale);

            var additionalRowTop = actionRowTop + Math.Max(buttonHeight, additionalHeadingHeight) + rowGap;
            var filterBounds = NativeBridge.GetChildClientBounds(_parent, filterCombo);
            if (filterBounds == null) return;
            var filterHeight = Math.Max(1, filterBounds.Height);
            var filterTop = additionalRowTop + Math.Max(0, (buttonHeight - filterHeight) / 2);
            var filterWidth = (int)Math.Round(contentWidth * 0.18);
            filterWidth = Math.Max(Scale(145, scale), filterWidth);
            filterWidth = Math.Min(filterWidth, Math.Max(1, contentWidth / 3));

            NativeBridge.PositionChildWindow(filterCombo, Bounds(
                contentLeft,
                filterTop,
                contentLeft + filterWidth,
                filterTop + filterHeight));

            LayoutZapretRow(additionalButtons,
                contentLeft + filterWidth + columnGap,
                additionalRowTop,
                contentRight,
                buttonHeight,
                columnGap,
                minimumButtonWidth,
                scale);
        }

        private Dictionary<int, IntPtr> CaptureResponsiveButtons(NativeBridge.ChildInfo[] children)
        {
            var result = new Dictionary<int, IntPtr>();
            foreach (var id in ResponsiveZapretButtonIds)
            {
                foreach (var child in children)
                {
                    if (!child.Visible || child.Id != id ||
                        !string.Equals(child.ClassName, "Button", StringComparison.OrdinalIgnoreCase)) continue;
                    result[id] = child.Handle;
                    break;
                }
            }
            return result;
        }

        private NativeBridge.ClientBounds FindStatusRegionBounds(NativeBridge.ChildInfo[] children)
        {
            NativeBridge.ClientBounds region = null;
            foreach (var child in children)
            {
                if (!child.Visible || !string.Equals(child.ClassName, "Edit", StringComparison.OrdinalIgnoreCase)) continue;
                var bounds = NativeBridge.GetChildClientBounds(_parent, child.Handle);
                if (bounds == null) continue;
                if (region == null)
                {
                    region = Bounds(bounds.Left, bounds.Top, bounds.Right, bounds.Bottom);
                    continue;
                }
                region.Left = Math.Min(region.Left, bounds.Left);
                region.Top = Math.Min(region.Top, bounds.Top);
                region.Right = Math.Max(region.Right, bounds.Right);
                region.Bottom = Math.Max(region.Bottom, bounds.Bottom);
            }
            return region;
        }

        private bool FindZapretCombos(NativeBridge.ChildInfo[] children, out IntPtr strategyCombo, out IntPtr filterCombo)
        {
            strategyCombo = IntPtr.Zero;
            filterCombo = IntPtr.Zero;
            NativeBridge.ClientBounds strategyBounds = null;
            NativeBridge.ClientBounds filterBounds = null;

            foreach (var child in children)
            {
                if (!child.Visible || !string.Equals(child.ClassName, "ComboBox", StringComparison.OrdinalIgnoreCase)) continue;
                var bounds = NativeBridge.GetChildClientBounds(_parent, child.Handle);
                if (bounds == null) continue;
                if (strategyBounds == null || bounds.Top < strategyBounds.Top)
                {
                    filterCombo = strategyCombo;
                    filterBounds = strategyBounds;
                    strategyCombo = child.Handle;
                    strategyBounds = bounds;
                }
                else if (filterBounds == null || bounds.Top < filterBounds.Top)
                {
                    filterCombo = child.Handle;
                    filterBounds = bounds;
                }
            }
            return strategyCombo != IntPtr.Zero && filterCombo != IntPtr.Zero;
        }

        private IntPtr FindStaticByCaption(NativeBridge.ChildInfo[] children, string russian, string english)
        {
            foreach (var child in children)
            {
                if (!child.Visible || !string.Equals(child.ClassName, "Static", StringComparison.OrdinalIgnoreCase)) continue;
                if (string.Equals(child.Text, russian, StringComparison.Ordinal) ||
                    string.Equals(child.Text, english, StringComparison.OrdinalIgnoreCase))
                    return child.Handle;
            }
            return IntPtr.Zero;
        }

        private IntPtr FindUpdateHeading(NativeBridge.ChildInfo[] children)
        {
            foreach (var child in children)
            {
                if (!child.Visible || !string.Equals(child.ClassName, "Static", StringComparison.OrdinalIgnoreCase)) continue;
                var text = child.Text ?? string.Empty;
                var mentionsZapret = text.IndexOf("Zapret", StringComparison.OrdinalIgnoreCase) >= 0;
                var mentionsUpdate = text.IndexOf("Обнов", StringComparison.OrdinalIgnoreCase) >= 0 ||
                    text.IndexOf("Update", StringComparison.OrdinalIgnoreCase) >= 0;
                if (mentionsZapret && mentionsUpdate) return child.Handle;
            }
            return IntPtr.Zero;
        }

        private IntPtr[] ResolveButtons(Dictionary<int, IntPtr> buttons, int[] ids)
        {
            var result = new IntPtr[ids.Length];
            for (var i = 0; i < ids.Length; i++)
            {
                IntPtr handle;
                if (!buttons.TryGetValue(ids[i], out handle) || handle == IntPtr.Zero) return null;
                result[i] = handle;
            }
            return result;
        }

        private void LayoutZapretRow(IntPtr[] buttons, int left, int top, int right, int height,
            int gap, int minimumWidth, double scale)
        {
            if (buttons == null || buttons.Length == 0 || right <= left) return;
            var widths = ComputeWidths(buttons, right - left, gap, minimumWidth, scale);
            var cells = BuildCells(left, top, height, gap, widths);
            PlaceButtonCells(buttons, cells);
        }

        private int[] ComputeWidths(IntPtr[] buttons, int availableWidth, int gap, int minimumWidth, double scale)
        {
            var widths = new int[buttons.Length];
            if (buttons.Length == 0) return widths;
            var contentWidth = Math.Max(buttons.Length, availableWidth - gap * (buttons.Length - 1));
            var desired = new int[buttons.Length];
            var desiredTotal = 0;
            for (var i = 0; i < buttons.Length; i++)
            {
                desired[i] = Math.Max(minimumWidth, MeasureDesiredWidth(buttons[i], scale));
                desiredTotal += desired[i];
            }

            if (desiredTotal <= contentWidth)
            {
                var extra = contentWidth - desiredTotal;
                for (var i = 0; i < buttons.Length; i++)
                {
                    var share = extra / (buttons.Length - i);
                    widths[i] = desired[i] + share;
                    extra -= share;
                }
                return widths;
            }

            var effectiveMinimum = Math.Min(minimumWidth, Math.Max(1, contentWidth / buttons.Length));
            var minimumTotal = effectiveMinimum * buttons.Length;
            var flexibleAvailable = Math.Max(0, contentWidth - minimumTotal);
            var flexibleDesired = 0;
            for (var i = 0; i < buttons.Length; i++)
                flexibleDesired += Math.Max(0, desired[i] - effectiveMinimum);

            var assigned = 0;
            for (var i = 0; i < buttons.Length; i++)
            {
                var flexible = Math.Max(0, desired[i] - effectiveMinimum);
                var share = i == buttons.Length - 1
                    ? contentWidth - assigned - effectiveMinimum
                    : (flexibleDesired <= 0 ? 0 : (int)Math.Floor((double)flexibleAvailable * flexible / flexibleDesired));
                widths[i] = effectiveMinimum + Math.Max(0, share);
                assigned += widths[i];
            }

            var delta = contentWidth;
            for (var i = 0; i < widths.Length; i++) delta -= widths[i];
            if (widths.Length > 0) widths[widths.Length - 1] = Math.Max(1, widths[widths.Length - 1] + delta);
            return widths;
        }

        private int MeasureDesiredWidth(IntPtr button, double scale)
        {
            var caption = NativeBridge.ReadWindowText(button) ?? string.Empty;
            var measured = TextRenderer.MeasureText(caption, SystemFonts.MessageBoxFont).Width;
            return measured + Scale(ResponsiveTextPadding, scale);
        }

        private NativeBridge.ClientBounds[] BuildCells(int left, int top, int height, int gap, int[] widths)
        {
            var cells = new NativeBridge.ClientBounds[widths.Length];
            var x = left;
            for (var i = 0; i < widths.Length; i++)
            {
                cells[i] = Bounds(x, top, x + widths[i], top + height);
                x += widths[i] + (i + 1 < widths.Length ? gap : 0);
            }
            return cells;
        }

        private void PlaceButtonCells(IntPtr[] buttons, NativeBridge.ClientBounds[] cells)
        {
            if (buttons == null || cells == null || buttons.Length != cells.Length) return;

            var placed = new bool[buttons.Length];
            for (var i = 0; i < buttons.Length; i++)
            {
                if (placed[i] || buttons[i] == IntPtr.Zero || cells[i] == null) continue;

                var parent = GetParent(buttons[i]);
                if (parent == IntPtr.Zero || parent == _parent)
                {
                    NativeBridge.PositionChildWindow(buttons[i], cells[i]);
                    placed[i] = true;
                    continue;
                }

                var groupLeft = cells[i].Left;
                var groupTop = cells[i].Top;
                var groupRight = cells[i].Right;
                var groupBottom = cells[i].Bottom;
                for (var j = i + 1; j < buttons.Length; j++)
                {
                    if (placed[j] || buttons[j] == IntPtr.Zero || cells[j] == null) continue;
                    if (GetParent(buttons[j]) != parent) continue;
                    groupLeft = Math.Min(groupLeft, cells[j].Left);
                    groupTop = Math.Min(groupTop, cells[j].Top);
                    groupRight = Math.Max(groupRight, cells[j].Right);
                    groupBottom = Math.Max(groupBottom, cells[j].Bottom);
                }

                NativeBridge.PositionChildWindow(parent, Bounds(groupLeft, groupTop, groupRight, groupBottom));
                for (var j = i; j < buttons.Length; j++)
                {
                    if (placed[j] || buttons[j] == IntPtr.Zero || cells[j] == null) continue;
                    if (GetParent(buttons[j]) != parent) continue;
                    NativeBridge.PositionChildWindow(buttons[j], Bounds(
                        cells[j].Left - groupLeft,
                        cells[j].Top - groupTop,
                        cells[j].Right - groupLeft,
                        cells[j].Bottom - groupTop));
                    placed[j] = true;
                }
            }
        }

        private static NativeBridge.ClientBounds Bounds(int left, int top, int right, int bottom)
        {
            return new NativeBridge.ClientBounds
            {
                Left = left,
                Top = top,
                Right = Math.Max(left + 1, right),
                Bottom = Math.Max(top + 1, bottom)
            };
        }

        private static int Scale(int value, double scale)
        {
            return Math.Max(1, (int)Math.Round(value * scale));
        }

        public void Dispose()
        {
            _disposed = true;
        }
    }
}
