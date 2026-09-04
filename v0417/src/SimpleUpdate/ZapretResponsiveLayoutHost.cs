using System;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace DPopCleaner.SimpleUpdate
{
    internal sealed class ZapretResponsiveLayoutHost : IDisposable
    {
        internal const int ServiceActionsHeadingId = 1726;
        internal const int ServiceActionsHeadingHostId = 1727;

        private const int ResponsiveRowGap = 8;
        private const int ResponsiveColumnGap = 8;
        private const int ResponsiveButtonHeight = 34;
        private const int ResponsiveMaximumButtonHeight = 48;
        private const int ResponsiveMinimumButtonWidth = 76;
        private const int ResponsiveTextPadding = 28;
        private const int CompactIdleStatusDetailHeight = 116;
        private const int ExpandedStatusDetailHeight = 220;

        private const uint WS_CHILD = 0x40000000;
        private const uint WS_VISIBLE = 0x10000000;
        private const uint SS_LEFTNOWORDWRAP = 0x0000000C;

        private static readonly int[] ResponsiveZapretButtonIds =
        {
            1701, 1702, 1703, 1704, 1705, 1707, 1708, 1710, 1711,
            1713, 1714, 1716, 1717,
            1720, 1721, 1722, 1723, 1724, 1725
        };

        private static readonly int[] StrategyRowButtonIds = { 1701, 1713, 1714 };
        private static readonly int[] PrimaryUpdateButtonIds = { 1724, 1725 };
        private static readonly int[] CompactUpdateToggleButtonIds = { 1716, 1717 };
        private static readonly int[] BridgeActionButtonIds = { 1720, 1721, 1722, 1723 };
        private static readonly int[] PrimaryAdditionalRowButtonIds = { 1704, 1705, 1707, 1708 };
        private static readonly int[] ServiceActionButtonIds = { 1703, 1702, 1710, 1711 };

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

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
        private static extern IntPtr GetModuleHandle(string moduleName);

        [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr CreateWindowEx(uint exStyle, string className, string windowName, uint style,
            int x, int y, int width, int height, IntPtr parent, IntPtr menu, IntPtr instance, IntPtr param);

        [DllImport("user32.dll")]
        private static extern bool DestroyWindow(IntPtr hwnd);

        private readonly IntPtr _parent;
        private int _nativeButtonHeight;
        private int _nativeStatusDetailHeight;
        private IntPtr _serviceActionsHeadingHost;
        private IntPtr _serviceActionsHeading;
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
            if (_disposed) return;
            if (_serviceActionsHeadingHost != IntPtr.Zero)
                NativeBridge.ShowWindow(_serviceActionsHeadingHost, NativeBridge.SW_HIDE);
            else if (_serviceActionsHeading != IntPtr.Zero)
                NativeBridge.ShowWindow(_serviceActionsHeading, NativeBridge.SW_HIDE);
        }

        internal void ApplyResponsiveLayout()
        {
            if (_disposed) return;

            var marker = NativeBridge.FindChildById(_parent, NativeBridge.ZapretApplyButtonId);
            if (marker == IntPtr.Zero || !NativeBridge.IsWindowVisible(marker)) return;

            RECT clientRect;
            if (!GetClientRect(_parent, out clientRect)) return;
            var clientWidth = Math.Max(1, clientRect.Right - clientRect.Left);
            var clientHeight = Math.Max(1, clientRect.Bottom - clientRect.Top);

            var children = NativeBridge.GetChildren(_parent);
            var buttons = CaptureResponsiveButtons(children);
            if (buttons.Count != ResponsiveZapretButtonIds.Length) return;

            IntPtr statusSummary;
            IntPtr statusDetail;
            NativeBridge.ClientBounds statusSummaryBounds;
            NativeBridge.ClientBounds statusDetailBounds;
            if (!FindStatusEdits(children, out statusSummary, out statusDetail, out statusSummaryBounds, out statusDetailBounds)) return;

            var applyBounds = NativeBridge.GetChildClientBounds(_parent, buttons[NativeBridge.ZapretApplyButtonId]);
            if (applyBounds == null) return;
            if (_nativeButtonHeight <= 0) _nativeButtonHeight = Math.Max(1, applyBounds.Height);
            if (_nativeStatusDetailHeight <= 0) _nativeStatusDetailHeight = Math.Max(1, statusDetailBounds.Height);

            var nativeButtonHeight = Math.Max(ResponsiveButtonHeight, _nativeButtonHeight);
            var scale = Math.Max(0.75, Math.Min(2.50, (double)nativeButtonHeight / ResponsiveButtonHeight));
            var tallWindowExtra = Math.Max(0, clientHeight - 840);
            var buttonHeight = Math.Min(
                Scale(ResponsiveMaximumButtonHeight, scale),
                Math.Max(nativeButtonHeight, Scale(ResponsiveButtonHeight, scale) + tallWindowExtra / 8));
            var rowGap = Math.Min(Scale(16, scale), Scale(ResponsiveRowGap, scale) + tallWindowExtra / 28);
            var sectionGap = Math.Min(
                Scale(36, scale),
                Math.Max(rowGap, Scale(ResponsiveRowGap, scale) + Math.Max(0, clientHeight - 800) / 6));
            var columnGap = Scale(ResponsiveColumnGap, scale);
            var minimumButtonWidth = Scale(ResponsiveMinimumButtonWidth, scale);

            var contentLeft = Math.Max(0, Math.Min(statusSummaryBounds.Left, statusDetailBounds.Left));
            var rightMargin = Scale(24, scale);
            var bottomMargin = Scale(24, scale);
            var contentRight = Math.Max(contentLeft + 1, clientWidth - rightMargin);
            var contentBottom = Math.Max(1, clientHeight - bottomMargin);
            var supportButton = FindControlByCaption(children, "Поддержка", "Support", "Button");
            var supportBounds = NativeBridge.GetChildClientBounds(_parent, supportButton);
            if (supportBounds != null && supportBounds.Top > statusDetailBounds.Top)
                contentBottom = Math.Min(contentBottom, Math.Max(1, supportBounds.Top - rowGap));
            var contentWidth = contentRight - contentLeft;
            if (contentWidth < Scale(620, scale)) return;

            IntPtr strategyCombo;
            IntPtr filterCombo;
            if (!FindZapretCombos(children, out strategyCombo, out filterCombo)) return;

            var strategyComboBounds = NativeBridge.GetChildClientBounds(_parent, strategyCombo);
            var strategyLabel = FindStaticByCaption(children, "Стратегия", "Strategy");
            var strategyLabelBounds = NativeBridge.GetChildClientBounds(_parent, strategyLabel);
            if (strategyComboBounds == null || strategyLabelBounds == null) return;

            var strategyCaption = NativeBridge.ReadWindowText(strategyLabel) ?? string.Empty;
            var english = string.Equals(strategyCaption, "Strategy", StringComparison.OrdinalIgnoreCase);
            var serviceHeading = EnsureServiceActionsHeading(strategyLabel, english);
            if (serviceHeading == IntPtr.Zero) return;

            var updateHeading = FindStaticByCaption(children, "Обновление Zapret", "Zapret Update");
            var updateHeadingBounds = NativeBridge.GetChildClientBounds(_parent, updateHeading);
            var updateHeadingHeight = updateHeadingBounds == null
                ? Scale(23, scale)
                : Math.Max(1, updateHeadingBounds.Height);
            var updateHeadingWidth = updateHeadingBounds == null
                ? Scale(210, scale)
                : Math.Max(updateHeadingBounds.Width, Scale(180, scale));

            var additionalHeading = FindStaticByCaption(children, "Дополнительно", "Additional");
            var additionalHeadingBounds = NativeBridge.GetChildClientBounds(_parent, additionalHeading);
            var additionalHeadingHeight = additionalHeadingBounds == null
                ? Scale(23, scale)
                : Math.Max(1, additionalHeadingBounds.Height);

            var strategyButtons = ResolveButtons(buttons, StrategyRowButtonIds);
            var primaryUpdateButtons = ResolveButtons(buttons, PrimaryUpdateButtonIds);
            var compactUpdateButtons = ResolveButtons(buttons, CompactUpdateToggleButtonIds);
            var actionButtons = ResolveButtons(buttons, BridgeActionButtonIds);
            var additionalButtons = ResolveButtons(buttons, PrimaryAdditionalRowButtonIds);
            var serviceButtons = ResolveButtons(buttons, ServiceActionButtonIds);
            if (strategyButtons == null || primaryUpdateButtons == null || compactUpdateButtons == null ||
                actionButtons == null || additionalButtons == null || serviceButtons == null) return;

            var statusSummaryHeight = Math.Max(1, statusSummaryBounds.Height);
            var statusSummaryTop = statusSummaryBounds.Top;
            var statusSummaryBottom = statusSummaryTop + statusSummaryHeight;
            NativeBridge.PositionChildWindow(statusSummary, Bounds(
                contentLeft, statusSummaryTop, contentRight, statusSummaryBottom));

            var halfGap = Math.Max(2, rowGap / 2);
            var statusDetailTop = Math.Max(statusSummaryBottom + halfGap, statusDetailBounds.Top);
            var statusDetailHeight = ComputeStatusDetailHeight(
                NativeBridge.ReadWindowText(statusDetail), clientHeight, scale);
            var statusDetailBottom = Math.Min(
                statusDetailTop + statusDetailHeight,
                Math.Max(statusDetailTop + 1, contentBottom - Scale(280, scale)));
            NativeBridge.PositionChildWindow(statusDetail, Bounds(
                contentLeft, statusDetailTop, contentRight, statusDetailBottom));

            var strategyRowTop = statusDetailBottom + sectionGap;
            var labelWidth = Math.Max(strategyLabelBounds.Width, Scale(82, scale));
            var minimumStrategyWidth = labelWidth + Scale(120, scale);
            var desiredStrategyWidth = Math.Max(minimumStrategyWidth, (int)Math.Round(contentWidth * 0.42));
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
            LayoutUpdateRowWithCompactToggles(
                primaryUpdateButtons,
                compactUpdateButtons,
                contentLeft,
                updateRowTop,
                contentRight,
                buttonHeight,
                columnGap,
                minimumButtonWidth,
                scale);

            var actionRowTop = updateRowTop + buttonHeight + sectionGap;
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

            var additionalRowTop = actionRowTop + Math.Max(buttonHeight, additionalHeadingHeight) + sectionGap;
            var filterBounds = NativeBridge.GetChildClientBounds(_parent, filterCombo);
            if (filterBounds == null) return;
            var filterHeight = Math.Max(1, filterBounds.Height);
            var filterTop = additionalRowTop + Math.Max(0, (buttonHeight - filterHeight) / 2);
            var filterWidth = (int)Math.Round(contentWidth * 0.19);
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

            var serviceRowTop = additionalRowTop + buttonHeight + sectionGap;
            LayoutCompactServiceRow(
                serviceHeading,
                serviceButtons,
                contentLeft,
                serviceRowTop,
                contentRight,
                buttonHeight,
                columnGap,
                scale);
        }

        private int ComputeStatusDetailHeight(string statusText, int clientHeight, double scale)
        {
            var idleMinimum = Scale(96, scale);
            var idleCap = Scale(CompactIdleStatusDetailHeight, scale);
            var native = Math.Max(1, Math.Min(_nativeStatusDetailHeight, idleCap));
            var idleHeight = Math.Max(idleMinimum, native);

            var normalized = (statusText ?? string.Empty).Replace("\r\n", "\n");
            var lines = normalized.Length == 0 ? 1 : normalized.Split('\n').Length;
            var overflowLines = Math.Max(0, lines - 2);
            var overflowText = Math.Max(0, normalized.Length - 180);
            if (overflowLines == 0 && overflowText == 0) return idleHeight;

            var expanded = idleHeight + Scale(22 * overflowLines, scale) + Scale(18 * (overflowText / 90), scale);
            var maximum = Math.Min(Scale(ExpandedStatusDetailHeight, scale), Math.Max(idleHeight, clientHeight / 3));
            return Math.Max(idleHeight, Math.Min(maximum, expanded));
        }

        private IntPtr EnsureServiceActionsHeading(IntPtr fontAnchor, bool english)
        {
            var caption = english ? "Service actions" : "Сервисные действия";
            if (_serviceActionsHeadingHost == IntPtr.Zero)
            {
                _serviceActionsHeadingHost = CreateWindowEx(0, "Static", string.Empty,
                    WS_CHILD | WS_VISIBLE,
                    0, 0, 1, 1, _parent, new IntPtr(ServiceActionsHeadingHostId), GetModuleHandle(null), IntPtr.Zero);
                if (_serviceActionsHeadingHost == IntPtr.Zero) return IntPtr.Zero;
            }
            if (_serviceActionsHeading == IntPtr.Zero)
            {
                _serviceActionsHeading = CreateWindowEx(0, "Static", caption,
                    WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
                    0, 0, 1, 1, _serviceActionsHeadingHost, new IntPtr(ServiceActionsHeadingId), GetModuleHandle(null), IntPtr.Zero);
                if (_serviceActionsHeading == IntPtr.Zero)
                {
                    try { DestroyWindow(_serviceActionsHeadingHost); } catch { }
                    _serviceActionsHeadingHost = IntPtr.Zero;
                    return IntPtr.Zero;
                }
                var font = fontAnchor == IntPtr.Zero
                    ? IntPtr.Zero
                    : NativeBridge.SendMessage(fontAnchor, NativeBridge.WM_GETFONT, IntPtr.Zero, IntPtr.Zero);
                if (font != IntPtr.Zero)
                    NativeBridge.SendMessage(_serviceActionsHeading, 0x0030, font, new IntPtr(1));
            }
            NativeBridge.WriteWindowText(_serviceActionsHeading, caption);
            NativeBridge.ShowWindow(_serviceActionsHeadingHost, NativeBridge.SW_SHOW);
            NativeBridge.ShowWindow(_serviceActionsHeading, NativeBridge.SW_SHOW);
            return _serviceActionsHeading;
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

        private bool FindStatusEdits(NativeBridge.ChildInfo[] children, out IntPtr summary, out IntPtr detail,
            out NativeBridge.ClientBounds summaryBounds, out NativeBridge.ClientBounds detailBounds)
        {
            summary = IntPtr.Zero;
            detail = IntPtr.Zero;
            summaryBounds = null;
            detailBounds = null;
            foreach (var child in children)
            {
                if (!child.Visible || !string.Equals(child.ClassName, "Edit", StringComparison.OrdinalIgnoreCase)) continue;
                var bounds = NativeBridge.GetChildClientBounds(_parent, child.Handle);
                if (bounds == null) continue;
                if (summaryBounds == null || bounds.Top < summaryBounds.Top)
                {
                    detail = summary;
                    detailBounds = summaryBounds;
                    summary = child.Handle;
                    summaryBounds = bounds;
                }
                else if (detailBounds == null || bounds.Top < detailBounds.Top)
                {
                    detail = child.Handle;
                    detailBounds = bounds;
                }
            }
            return summary != IntPtr.Zero && detail != IntPtr.Zero && summaryBounds != null && detailBounds != null;
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
            return FindControlByCaption(children, russian, english, "Static");
        }

        private IntPtr FindControlByCaption(NativeBridge.ChildInfo[] children, string russian, string english, string className)
        {
            foreach (var child in children)
            {
                if (!child.Visible || !string.Equals(child.ClassName, className, StringComparison.OrdinalIgnoreCase)) continue;
                if (string.Equals(child.Text, russian, StringComparison.Ordinal) ||
                    string.Equals(child.Text, english, StringComparison.OrdinalIgnoreCase))
                    return child.Handle;
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

        private void LayoutUpdateRowWithCompactToggles(IntPtr[] primaryButtons, IntPtr[] toggleButtons,
            int left, int top, int right, int height, int gap, int minimumWidth, double scale)
        {
            if (primaryButtons == null || toggleButtons == null || right <= left) return;
            var compactWidths = ComputeCompactWidths(toggleButtons, right - left, gap, Scale(170, scale), Scale(240, scale), scale);
            var compactTotal = SumWidths(compactWidths) + gap * Math.Max(0, compactWidths.Length - 1);
            var primaryRight = Math.Max(left + 1, right - compactTotal - (compactWidths.Length > 0 ? gap : 0));
            LayoutZapretRow(primaryButtons, left, top, primaryRight, height, gap, minimumWidth, scale);
            var toggleLeft = Math.Min(right - 1, primaryRight + gap);
            PlaceButtonCells(toggleButtons, BuildCells(toggleLeft, top, height, gap, compactWidths));
        }

        private void LayoutCompactServiceRow(IntPtr heading, IntPtr[] buttons, int left, int top, int right,
            int height, int gap, double scale)
        {
            if (heading == IntPtr.Zero || buttons == null || right <= left) return;
            var caption = NativeBridge.ReadWindowText(heading) ?? string.Empty;
            var headingWidth = TextRenderer.MeasureText(caption, SystemFonts.MessageBoxFont).Width + Scale(24, scale);
            headingWidth = Math.Max(Scale(180, scale), Math.Min(Scale(240, scale), headingWidth));
            var headingHeight = Math.Min(height, Scale(26, scale));
            var headingTop = top + Math.Max(0, (height - headingHeight) / 2);
            if (_serviceActionsHeadingHost != IntPtr.Zero)
            {
                NativeBridge.PositionChildWindow(_serviceActionsHeadingHost,
                    Bounds(left, headingTop, left + headingWidth, headingTop + headingHeight));
                NativeBridge.PositionChildWindow(heading, Bounds(0, 0, headingWidth, headingHeight));
            }
            else
            {
                NativeBridge.PositionChildWindow(heading, Bounds(left, headingTop, left + headingWidth, headingTop + headingHeight));
            }

            var buttonsLeft = left + headingWidth + gap;
            var available = Math.Max(1, right - buttonsLeft);
            var widths = ComputeCompactWidths(buttons, available, gap, Scale(130, scale), Scale(220, scale), scale);
            PlaceButtonCells(buttons, BuildCells(buttonsLeft, top, height, gap, widths));
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
                desired[i] = Math.Max(minimumWidth, MeasurePreferredWidth(buttons[i], scale));
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

            return ShrinkWidths(desired, contentWidth, Math.Min(minimumWidth, Math.Max(1, contentWidth / buttons.Length)));
        }

        private int[] ComputeCompactWidths(IntPtr[] buttons, int availableWidth, int gap,
            int minimumWidth, int maximumWidth, double scale)
        {
            var widths = new int[buttons.Length];
            if (buttons.Length == 0) return widths;
            var contentWidth = Math.Max(buttons.Length, availableWidth - gap * (buttons.Length - 1));
            var desiredTotal = 0;
            for (var i = 0; i < buttons.Length; i++)
            {
                widths[i] = Math.Max(minimumWidth, Math.Min(maximumWidth, MeasurePreferredWidth(buttons[i], scale)));
                desiredTotal += widths[i];
            }
            if (desiredTotal <= contentWidth) return widths;
            return ShrinkWidths(widths, contentWidth, Math.Min(minimumWidth, Math.Max(1, contentWidth / buttons.Length)));
        }

        private static int[] ShrinkWidths(int[] desired, int contentWidth, int effectiveMinimum)
        {
            var widths = new int[desired.Length];
            if (desired.Length == 0) return widths;
            var minimumTotal = effectiveMinimum * desired.Length;
            var flexibleAvailable = Math.Max(0, contentWidth - minimumTotal);
            var flexibleDesired = 0;
            for (var i = 0; i < desired.Length; i++)
                flexibleDesired += Math.Max(0, desired[i] - effectiveMinimum);

            var assigned = 0;
            for (var i = 0; i < desired.Length; i++)
            {
                var flexible = Math.Max(0, desired[i] - effectiveMinimum);
                var share = i == desired.Length - 1
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

        private int MeasurePreferredWidth(IntPtr button, double scale)
        {
            var caption = NativeBridge.ReadWindowText(button) ?? string.Empty;
            var measured = TextRenderer.MeasureText(caption, SystemFonts.MessageBoxFont).Width;
            return measured + Scale(ResponsiveTextPadding, scale);
        }

        private static int SumWidths(int[] widths)
        {
            var total = 0;
            for (var i = 0; i < widths.Length; i++) total += widths[i];
            return total;
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
            if (_disposed) return;
            _disposed = true;
            if (_serviceActionsHeadingHost != IntPtr.Zero)
            {
                try { DestroyWindow(_serviceActionsHeadingHost); } catch { }
                _serviceActionsHeadingHost = IntPtr.Zero;
                _serviceActionsHeading = IntPtr.Zero;
            }
            else if (_serviceActionsHeading != IntPtr.Zero)
            {
                try { DestroyWindow(_serviceActionsHeading); } catch { }
                _serviceActionsHeading = IntPtr.Zero;
            }
        }
    }
}
