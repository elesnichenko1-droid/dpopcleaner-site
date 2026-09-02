using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace DPopCleaner.SimpleUpdate
{
    internal sealed class ZapretVisualPolishHost : IDisposable
    {
        private const string FallbackZapretVersion = "1.10.2";
        private const int GWL_STYLE = -16;
        private const long BS_TYPEMASK = 0x0000000F;
        private const long BS_OWNERDRAW = 0x0000000B;
        private const uint WM_DRAWITEM = 0x002B;
        private const uint DT_CENTER = 0x0001;
        private const uint DT_VCENTER = 0x0004;
        private const uint DT_SINGLELINE = 0x0020;
        private const int TRANSPARENT = 1;
        private const uint ODS_SELECTED = 0x0001;
        private const uint ODS_DISABLED = 0x0004;
        private const uint ODT_BUTTON = 4;
        private const uint ToolbarSubclassId = 0xD510;

        private static readonly HashSet<int> UnifiedZapretButtonIds = new HashSet<int>
        {
            ZapretEnhancementHost.InstallServiceProxyButtonId,
            ZapretEnhancementHost.RemoveServicesProxyButtonId,
            1703,
            1704,
            1705,
            1707,
            1708,
            1710,
            1711,
            ZapretEnhancementHost.StartStandaloneProxyButtonId,
            1714,
            1716,
            1717,
            ZapretEnhancementHost.RepairBroadcastButtonId,
            ZapretEnhancementHost.RepairConnectionButtonId,
            ZapretEnhancementHost.GameFilterButtonId,
            ZapretEnhancementHost.ManagerButtonId,
            ZapretEnhancementHost.LegacyCheckVersionButtonId,
            ZapretEnhancementHost.LegacyDownloadButtonId
        };

        private static readonly object Sync = new object();
        private static readonly Dictionary<IntPtr, ZapretVisualPolishHost> ToolbarOwners = new Dictionary<IntPtr, ZapretVisualPolishHost>();
        private static readonly SubclassProc ToolbarSubclassDelegate = StaticToolbarSubclassProc;

        private static readonly IntPtr DarkButtonBrush = CreateSolidBrush(Rgb(18, 27, 38));
        private static readonly IntPtr DarkPressedBrush = CreateSolidBrush(Rgb(27, 39, 53));
        private static readonly IntPtr DarkBorderBrush = CreateSolidBrush(Rgb(45, 61, 78));
        private static readonly IntPtr LightButtonBrush = CreateSolidBrush(Rgb(244, 246, 249));
        private static readonly IntPtr LightPressedBrush = CreateSolidBrush(Rgb(226, 231, 237));
        private static readonly IntPtr LightBorderBrush = CreateSolidBrush(Rgb(176, 184, 194));

        private readonly IntPtr _parent;
        private readonly string _applicationRoot;
        private readonly HashSet<IntPtr> _buttons = new HashSet<IntPtr>();
        private readonly HashSet<IntPtr> _toolbarParents = new HashSet<IntPtr>();
        private readonly Dictionary<IntPtr, long> _originalButtonStyles = new Dictionary<IntPtr, long>();
        private bool _darkTheme = true;
        private bool _themeKnown;
        private IntPtr _journalHeading;
        private IntPtr _journalList;
        private bool _journalHeadingWasVisible;
        private bool _journalListWasVisible;
        private bool _journalCaptured;
        private bool _disposed;

        [StructLayout(LayoutKind.Sequential)]
        private struct RECT { public int Left, Top, Right, Bottom; }

        [StructLayout(LayoutKind.Sequential)]
        private struct DRAWITEMSTRUCT
        {
            public uint CtlType;
            public uint CtlID;
            public uint itemID;
            public uint itemAction;
            public uint itemState;
            public IntPtr hwndItem;
            public IntPtr hDC;
            public RECT rcItem;
            public UIntPtr itemData;
        }

        private delegate IntPtr SubclassProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam, UIntPtr subclassId, UIntPtr refData);

        [DllImport("user32.dll")]
        private static extern IntPtr GetParent(IntPtr hwnd);

        [DllImport("user32.dll")]
        private static extern bool InvalidateRect(IntPtr hwnd, IntPtr rect, bool erase);

        [DllImport("user32.dll")]
        private static extern int FillRect(IntPtr hdc, ref RECT rect, IntPtr brush);

        [DllImport("user32.dll")]
        private static extern int FrameRect(IntPtr hdc, ref RECT rect, IntPtr brush);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern int DrawText(IntPtr hdc, string text, int count, ref RECT rect, uint format);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int maxCount);

        [DllImport("gdi32.dll")]
        private static extern IntPtr CreateSolidBrush(uint colorRef);

        [DllImport("gdi32.dll")]
        private static extern uint SetTextColor(IntPtr hdc, uint colorRef);

        [DllImport("gdi32.dll")]
        private static extern int SetBkMode(IntPtr hdc, int mode);

        [DllImport("comctl32.dll", SetLastError = true)]
        private static extern bool SetWindowSubclass(IntPtr hwnd, SubclassProc proc, UIntPtr subclassId, UIntPtr refData);

        [DllImport("comctl32.dll")]
        private static extern IntPtr DefSubclassProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);

        [DllImport("comctl32.dll")]
        private static extern bool RemoveWindowSubclass(IntPtr hwnd, SubclassProc proc, UIntPtr subclassId);

        [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW")]
        private static extern IntPtr GetWindowLongPtr64(IntPtr hwnd, int index);

        [DllImport("user32.dll", EntryPoint = "SetWindowLongPtrW")]
        private static extern IntPtr SetWindowLongPtr64(IntPtr hwnd, int index, IntPtr value);

        [DllImport("user32.dll", EntryPoint = "GetWindowLongW")]
        private static extern int GetWindowLong32(IntPtr hwnd, int index);

        [DllImport("user32.dll", EntryPoint = "SetWindowLongW")]
        private static extern int SetWindowLong32(IntPtr hwnd, int index, int value);

        internal ZapretVisualPolishHost(IntPtr parent, string applicationRoot)
        {
            if (parent == IntPtr.Zero) throw new ArgumentException("Parent window is required.", "parent");
            _parent = parent;
            _applicationRoot = Path.GetFullPath(applicationRoot ?? string.Empty);
            Show();
        }

        internal void Show()
        {
            if (_disposed) return;
            EnsureUnifiedZapretButtons();
            RefreshVersionCaptions();
            HideJournalForZapret();
        }

        internal void Hide()
        {
            if (_disposed) return;
            RestoreJournal();
        }

        private void EnsureUnifiedZapretButtons()
        {
            var darkTheme = NativeBridge.IsDarkThemeSelected(_parent);
            var themeChanged = !_themeKnown || darkTheme != _darkTheme;
            _darkTheme = darkTheme;
            _themeKnown = true;

            var added = false;
            foreach (var child in NativeBridge.GetChildren(_parent))
            {
                if (!child.Visible || !string.Equals(child.ClassName, "Button", StringComparison.OrdinalIgnoreCase)) continue;
                if (!UnifiedZapretButtonIds.Contains(child.Id)) continue;
                var button = child.Handle;
                if (button == IntPtr.Zero || !_buttons.Add(button)) continue;

                var originalStyle = GetStyle(button);
                _originalButtonStyles[button] = originalStyle;
                SetOwnerDrawStyle(button, originalStyle);
                var toolbar = GetParent(button);
                if (toolbar != IntPtr.Zero && _toolbarParents.Add(toolbar))
                {
                    lock (Sync) ToolbarOwners[toolbar] = this;
                    SetWindowSubclass(toolbar, ToolbarSubclassDelegate, new UIntPtr(ToolbarSubclassId), UIntPtr.Zero);
                }
                added = true;
            }

            if (themeChanged || added)
            {
                foreach (var button in _buttons)
                    InvalidateRect(button, IntPtr.Zero, true);
            }
        }

        private void RefreshVersionCaptions()
        {
            var version = GetInstalledZapretVersion();
            foreach (var child in NativeBridge.GetChildren(_parent))
            {
                if (!child.Visible || !string.Equals(child.ClassName, "Button", StringComparison.OrdinalIgnoreCase)) continue;
                if (child.Id == ZapretEnhancementHost.GameFilterButtonId)
                {
                    var current = child.Text ?? string.Empty;
                    var prefix = current.StartsWith("Game", StringComparison.OrdinalIgnoreCase) ? "Game filter " : "Игровой фильтр ";
                    WriteCaptionIfDifferent(child.Handle, prefix + version);
                }
                else if (child.Id == ZapretEnhancementHost.ManagerButtonId)
                {
                    var current = child.Text ?? string.Empty;
                    var prefix = current.StartsWith("Manager", StringComparison.OrdinalIgnoreCase) ? "Manager " : "Менеджер ";
                    WriteCaptionIfDifferent(child.Handle, prefix + version);
                }
            }
        }

        private static void WriteCaptionIfDifferent(IntPtr button, string desired)
        {
            if (button == IntPtr.Zero) return;
            if (!string.Equals(NativeBridge.ReadWindowText(button), desired, StringComparison.Ordinal))
            {
                NativeBridge.WriteWindowText(button, desired);
                InvalidateRect(button, IntPtr.Zero, true);
            }
        }

        private void HideJournalForZapret()
        {
            CaptureJournalControls();
            if (!_journalCaptured) return;
            if (_journalHeading != IntPtr.Zero)
                NativeBridge.ShowWindow(_journalHeading, NativeBridge.SW_HIDE);
            if (_journalList != IntPtr.Zero)
                NativeBridge.ShowWindow(_journalList, NativeBridge.SW_HIDE);
        }

        private void RestoreJournal()
        {
            if (!_journalCaptured) return;
            if (_journalHeading != IntPtr.Zero && _journalHeadingWasVisible)
                NativeBridge.ShowWindow(_journalHeading, NativeBridge.SW_SHOW);
            if (_journalList != IntPtr.Zero && _journalListWasVisible)
                NativeBridge.ShowWindow(_journalList, NativeBridge.SW_SHOW);
        }

        private void CaptureJournalControls()
        {
            if (_journalCaptured) return;

            NativeBridge.ChildInfo listCandidate = null;
            NativeBridge.ClientBounds listBounds = null;
            var largestArea = 0;
            foreach (var child in NativeBridge.GetChildren(_parent))
            {
                if (!child.Visible || !string.Equals(child.ClassName, "ListBox", StringComparison.OrdinalIgnoreCase)) continue;
                var bounds = NativeBridge.GetChildClientBounds(_parent, child.Handle);
                if (bounds == null) continue;
                var area = bounds.Width * bounds.Height;
                if (area <= largestArea) continue;
                largestArea = area;
                listCandidate = child;
                listBounds = bounds;
            }
            if (listCandidate == null || listBounds == null) return;

            NativeBridge.ChildInfo headingCandidate = null;
            var nearestDistance = int.MaxValue;
            foreach (var child in NativeBridge.GetChildren(_parent))
            {
                if (!child.Visible || !string.Equals(child.ClassName, "Static", StringComparison.OrdinalIgnoreCase)) continue;
                var bounds = NativeBridge.GetChildClientBounds(_parent, child.Handle);
                if (bounds == null || bounds.Bottom > listBounds.Top + 8) continue;
                var overlaps = bounds.Left < listBounds.Right && bounds.Right > listBounds.Left;
                if (!overlaps) continue;
                var distance = Math.Max(0, listBounds.Top - bounds.Bottom);
                if (distance >= nearestDistance || distance > 60) continue;
                nearestDistance = distance;
                headingCandidate = child;
            }

            _journalList = listCandidate.Handle;
            _journalHeading = headingCandidate != null ? headingCandidate.Handle : IntPtr.Zero;
            _journalListWasVisible = NativeBridge.IsWindowVisible(_journalList);
            _journalHeadingWasVisible = _journalHeading != IntPtr.Zero && NativeBridge.IsWindowVisible(_journalHeading);
            _journalCaptured = _journalList != IntPtr.Zero;
        }

        private string GetInstalledZapretVersion()
        {
            try
            {
                var path = Path.Combine(_applicationRoot, "Zapret", ".service", "version.txt");
                if (File.Exists(path))
                {
                    var value = (File.ReadAllText(path) ?? string.Empty).Trim();
                    if (!string.IsNullOrWhiteSpace(value)) return value;
                }
            }
            catch { }
            return FallbackZapretVersion;
        }

        private static void SetOwnerDrawStyle(IntPtr button, long originalStyle)
        {
            var style = (originalStyle & ~BS_TYPEMASK) | BS_OWNERDRAW;
            if (style != originalStyle) SetStyle(button, style);
        }

        private static long GetStyle(IntPtr hwnd)
        {
            return IntPtr.Size == 8 ? GetWindowLongPtr64(hwnd, GWL_STYLE).ToInt64() : GetWindowLong32(hwnd, GWL_STYLE);
        }

        private static void SetStyle(IntPtr hwnd, long style)
        {
            if (IntPtr.Size == 8) SetWindowLongPtr64(hwnd, GWL_STYLE, new IntPtr(style));
            else SetWindowLong32(hwnd, GWL_STYLE, unchecked((int)style));
        }

        private bool OwnsButton(IntPtr hwnd)
        {
            return _buttons.Contains(hwnd);
        }

        private static IntPtr StaticToolbarSubclassProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam, UIntPtr subclassId, UIntPtr refData)
        {
            ZapretVisualPolishHost owner;
            lock (Sync) ToolbarOwners.TryGetValue(hwnd, out owner);
            if (owner != null && msg == WM_DRAWITEM && lParam != IntPtr.Zero)
            {
                var item = (DRAWITEMSTRUCT)Marshal.PtrToStructure(lParam, typeof(DRAWITEMSTRUCT));
                if (item.CtlType == ODT_BUTTON && owner.OwnsButton(item.hwndItem))
                {
                    owner.DrawOwnerButton(ref item);
                    return new IntPtr(1);
                }
            }
            return DefSubclassProc(hwnd, msg, wParam, lParam);
        }

        private void DrawOwnerButton(ref DRAWITEMSTRUCT item)
        {
            var selected = (item.itemState & ODS_SELECTED) != 0;
            var buttonBrush = _darkTheme
                ? (selected ? DarkPressedBrush : DarkButtonBrush)
                : (selected ? LightPressedBrush : LightButtonBrush);
            var borderBrush = _darkTheme ? DarkBorderBrush : LightBorderBrush;
            var rect = item.rcItem;
            FillRect(item.hDC, ref rect, buttonBrush);
            FrameRect(item.hDC, ref rect, borderBrush);

            var text = new StringBuilder(256);
            GetWindowText(item.hwndItem, text, text.Capacity);
            SetBkMode(item.hDC, TRANSPARENT);
            var disabled = (item.itemState & ODS_DISABLED) != 0;
            var textColor = disabled
                ? (_darkTheme ? Rgb(145, 154, 164) : Rgb(132, 139, 148))
                : (_darkTheme ? Rgb(242, 246, 250) : Rgb(28, 36, 46));
            SetTextColor(item.hDC, textColor);
            DrawText(item.hDC, text.ToString(), -1, ref rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        private static uint Rgb(byte r, byte g, byte b)
        {
            return (uint)(r | (g << 8) | (b << 16));
        }

        public void Dispose()
        {
            if (_disposed) return;
            RestoreJournal();
            _disposed = true;

            foreach (var toolbar in _toolbarParents)
            {
                try { RemoveWindowSubclass(toolbar, ToolbarSubclassDelegate, new UIntPtr(ToolbarSubclassId)); } catch { }
                lock (Sync) ToolbarOwners.Remove(toolbar);
            }
            foreach (var pair in _originalButtonStyles)
            {
                try { SetStyle(pair.Key, pair.Value); InvalidateRect(pair.Key, IntPtr.Zero, true); } catch { }
            }
            _toolbarParents.Clear();
            _buttons.Clear();
            _originalButtonStyles.Clear();
        }
    }
}
