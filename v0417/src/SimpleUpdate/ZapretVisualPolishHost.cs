using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;

namespace DPopCleaner.SimpleUpdate
{
    internal sealed class ZapretVisualPolishHost : IDisposable
    {
        private const string FallbackZapretVersion = "1.10.2";
        private const uint WM_DRAWITEM = 0x002B;
        private const uint WM_CTLCOLORSTATIC = 0x0138;
        private const uint ODT_BUTTON = 4;
        private const uint ODS_SELECTED = 0x0001;
        private const uint ODS_DISABLED = 0x0004;
        private const uint DT_CENTER = 0x0001;
        private const uint DT_VCENTER = 0x0004;
        private const uint DT_SINGLELINE = 0x0020;
        private const int TRANSPARENT = 1;
        private const int GWL_STYLE = -16;
        private const int WS_CLIPCHILDREN = 0x02000000;
        private const int BS_TYPEMASK = 0x0000000F;
        private const int BS_OWNERDRAW = 0x0000000B;
        private const uint BridgeHostSubclassId = 0xD512;
        private const uint ServiceHeadingSubclassId = 0xD511;
        private const uint RDW_INVALIDATE = 0x0001;
        private const uint RDW_ERASE = 0x0004;
        private const uint RDW_ALLCHILDREN = 0x0080;
        private const uint RDW_UPDATENOW = 0x0100;

        private static readonly HashSet<int> BridgeButtonIds = new HashSet<int>
        {
            ZapretEnhancementHost.InstallServiceProxyButtonId,
            ZapretEnhancementHost.RemoveServicesProxyButtonId,
            ZapretEnhancementHost.StartStandaloneProxyButtonId,
            ZapretEnhancementHost.RepairBroadcastButtonId,
            ZapretEnhancementHost.RepairConnectionButtonId,
            ZapretEnhancementHost.GameFilterButtonId,
            ZapretEnhancementHost.ManagerButtonId,
            ZapretEnhancementHost.LegacyCheckVersionButtonId,
            ZapretEnhancementHost.LegacyDownloadButtonId
        };

        private static readonly object Sync = new object();
        private static readonly Dictionary<IntPtr, ZapretVisualPolishHost> BridgeHostOwners = new Dictionary<IntPtr, ZapretVisualPolishHost>();
        private static readonly Dictionary<IntPtr, ZapretVisualPolishHost> ServiceHeadingOwners = new Dictionary<IntPtr, ZapretVisualPolishHost>();
        private static readonly SubclassProc BridgeHostSubclassDelegate = StaticBridgeHostSubclassProc;
        private static readonly SubclassProc ServiceHeadingSubclassDelegate = StaticServiceHeadingSubclassProc;

        private static readonly IntPtr DarkButtonBrush = CreateSolidBrush(Rgb(18, 27, 38));
        private static readonly IntPtr DarkPressedBrush = CreateSolidBrush(Rgb(27, 39, 53));
        private static readonly IntPtr DarkBorderBrush = CreateSolidBrush(Rgb(45, 61, 78));
        private static readonly IntPtr LightButtonBrush = CreateSolidBrush(Rgb(244, 246, 249));
        private static readonly IntPtr LightPressedBrush = CreateSolidBrush(Rgb(226, 231, 237));
        private static readonly IntPtr LightBorderBrush = CreateSolidBrush(Rgb(176, 184, 194));
        private static readonly IntPtr DarkPageBrush = CreateSolidBrush(Rgb(12, 17, 23));
        private static readonly IntPtr LightPageBrush = CreateSolidBrush(Rgb(247, 248, 250));

        private readonly IntPtr _parent;
        private readonly string _applicationRoot;
        private readonly HashSet<IntPtr> _bridgeHosts = new HashSet<IntPtr>();
        private readonly Dictionary<IntPtr, int> _originalBridgeHostStyles = new Dictionary<IntPtr, int>();
        private readonly Dictionary<IntPtr, int> _originalButtonStyles = new Dictionary<IntPtr, int>();
        private bool _darkTheme = true;
        private bool _themeKnown;
        private IntPtr _serviceHeadingHost;
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
        private static extern bool IsWindowEnabled(IntPtr hwnd);

        [DllImport("user32.dll")]
        private static extern bool InvalidateRect(IntPtr hwnd, IntPtr rect, bool erase);

        [DllImport("user32.dll")]
        private static extern bool RedrawWindow(IntPtr hwnd, IntPtr updateRect, IntPtr updateRegion, uint flags);

        [DllImport("user32.dll")]
        private static extern int FillRect(IntPtr hdc, ref RECT rect, IntPtr brush);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern int DrawText(IntPtr hdc, string text, int count, ref RECT rect, uint format);

        [DllImport("user32.dll", EntryPoint = "GetWindowLongW", SetLastError = true)]
        private static extern int GetWindowLong(IntPtr hwnd, int index);

        [DllImport("user32.dll", EntryPoint = "SetWindowLongW", SetLastError = true)]
        private static extern int SetWindowLong(IntPtr hwnd, int index, int value);

        [DllImport("gdi32.dll")]
        private static extern IntPtr CreateSolidBrush(uint colorRef);

        [DllImport("gdi32.dll")]
        private static extern IntPtr CreateRoundRectRgn(int left, int top, int right, int bottom, int widthEllipse, int heightEllipse);

        [DllImport("gdi32.dll")]
        private static extern bool FillRgn(IntPtr hdc, IntPtr region, IntPtr brush);

        [DllImport("gdi32.dll")]
        private static extern bool FrameRgn(IntPtr hdc, IntPtr region, IntPtr brush, int width, int height);

        [DllImport("gdi32.dll")]
        private static extern bool DeleteObject(IntPtr obj);

        [DllImport("gdi32.dll")]
        private static extern IntPtr SelectObject(IntPtr hdc, IntPtr obj);

        [DllImport("gdi32.dll")]
        private static extern uint SetTextColor(IntPtr hdc, uint colorRef);

        [DllImport("gdi32.dll")]
        private static extern uint SetBkColor(IntPtr hdc, uint colorRef);

        [DllImport("gdi32.dll")]
        private static extern int SetBkMode(IntPtr hdc, int mode);

        [DllImport("comctl32.dll", SetLastError = true)]
        private static extern bool SetWindowSubclass(IntPtr hwnd, SubclassProc proc, UIntPtr subclassId, UIntPtr refData);

        [DllImport("comctl32.dll")]
        private static extern IntPtr DefSubclassProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);

        [DllImport("comctl32.dll")]
        private static extern bool RemoveWindowSubclass(IntPtr hwnd, SubclassProc proc, UIntPtr subclassId);

        [DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]
        private static extern int SetWindowTheme(IntPtr hwnd, string subAppName, string subIdList);

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
            EnsureBridgeButtonPainting();
            RefreshVersionCaptions();
            EnsureServiceHeadingThemeHost();
            HideJournalForZapret();
        }

        internal void Hide()
        {
            if (_disposed) return;
            RestoreJournal();
        }

        private void EnsureBridgeButtonPainting()
        {
            var darkTheme = NativeBridge.IsDarkThemeSelected(_parent);
            var themeChanged = !_themeKnown || darkTheme != _darkTheme;
            _darkTheme = darkTheme;
            _themeKnown = true;

            var changed = false;
            foreach (var child in NativeBridge.GetChildren(_parent))
            {
                if (!child.Visible || !string.Equals(child.ClassName, "Button", StringComparison.OrdinalIgnoreCase)) continue;
                if (!BridgeButtonIds.Contains(child.Id)) continue;

                var button = child.Handle;
                if (button == IntPtr.Zero) continue;
                var host = GetParent(button);
                if (host == IntPtr.Zero || host == _parent) continue;

                // Install the renderer on the launcher-owned parent first. Only after the host can
                // answer WM_DRAWITEM do we switch its child Button to BS_OWNERDRAW. The host also
                // clips child rectangles so its own WM_ERASEBKGND cannot paint over the owner-draw
                // button after it was rendered.
                if (!EnsureBridgeHostSubclass(host)) continue;
                changed |= SetOwnerDrawStyle(button);
                SetWindowTheme(button, string.Empty, string.Empty);
                if (themeChanged || changed) InvalidateRect(button, IntPtr.Zero, true);
            }

            if (themeChanged || changed)
            {
                foreach (var host in _bridgeHosts)
                    RedrawWindow(host, IntPtr.Zero, IntPtr.Zero,
                        RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);

                var serviceHeading = NativeBridge.FindChildById(_parent, ZapretResponsiveLayoutHost.ServiceActionsHeadingId);
                if (serviceHeading != IntPtr.Zero) InvalidateRect(serviceHeading, IntPtr.Zero, true);
            }
        }

        private bool EnsureBridgeHostSubclass(IntPtr host)
        {
            if (_bridgeHosts.Contains(host)) return true;
            SetClipChildrenStyle(host);
            lock (Sync) BridgeHostOwners[host] = this;
            if (!SetWindowSubclass(host, BridgeHostSubclassDelegate, new UIntPtr(BridgeHostSubclassId), UIntPtr.Zero))
            {
                lock (Sync) BridgeHostOwners.Remove(host);
                int original;
                if (_originalBridgeHostStyles.TryGetValue(host, out original))
                {
                    try { SetWindowLong(host, GWL_STYLE, original); } catch { }
                    _originalBridgeHostStyles.Remove(host);
                }
                return false;
            }
            _bridgeHosts.Add(host);
            return true;
        }

        private void SetClipChildrenStyle(IntPtr host)
        {
            var current = GetWindowLong(host, GWL_STYLE);
            if (!_originalBridgeHostStyles.ContainsKey(host)) _originalBridgeHostStyles[host] = current;
            var desired = current | WS_CLIPCHILDREN;
            if (current != desired) SetWindowLong(host, GWL_STYLE, desired);
        }

        private bool SetOwnerDrawStyle(IntPtr button)
        {
            var current = GetWindowLong(button, GWL_STYLE);
            if (!_originalButtonStyles.ContainsKey(button)) _originalButtonStyles[button] = current;
            var desired = (current & ~BS_TYPEMASK) | BS_OWNERDRAW;
            if (current == desired) return false;
            SetWindowLong(button, GWL_STYLE, desired);
            InvalidateRect(button, IntPtr.Zero, true);
            return true;
        }

        private void RefreshVersionCaptions()
        {
            var version = GetInstalledZapretVersion();
            var english = IsEnglishZapretUi();
            foreach (var child in NativeBridge.GetChildren(_parent))
            {
                if (!child.Visible || !string.Equals(child.ClassName, "Button", StringComparison.OrdinalIgnoreCase)) continue;
                if (child.Id == ZapretEnhancementHost.GameFilterButtonId)
                {
                    WriteCaptionIfDifferent(child.Handle, (english ? "Game filter " : "Игровой фильтр ") + version);
                }
                else if (child.Id == ZapretEnhancementHost.ManagerButtonId)
                {
                    WriteCaptionIfDifferent(child.Handle, (english ? "Manager " : "Менеджер ") + version);
                }
                else if (child.Id == 1716)
                {
                    WriteCaptionIfDifferent(child.Handle, english ? "Auto-update" : "Автообновление");
                }
                else if (child.Id == 1717)
                {
                    WriteCaptionIfDifferent(child.Handle, english ? "Zapret autostart" : "Автозапуск Zapret");
                }
                else if (child.Id == ZapretEnhancementHost.RemoveServicesProxyButtonId)
                {
                    WriteCaptionIfDifferent(child.Handle, english ? "Remove services" : "Удалить сервисы");
                }
                else if (child.Id == 1710)
                {
                    WriteCaptionIfDifferent(child.Handle, english ? "Diagnostics" : "Диагностика");
                }
            }
        }

        private bool IsEnglishZapretUi()
        {
            foreach (var child in NativeBridge.GetChildren(_parent))
            {
                if (!child.Visible || !string.Equals(child.ClassName, "Static", StringComparison.OrdinalIgnoreCase)) continue;
                if (string.Equals(child.Text, "Strategy", StringComparison.OrdinalIgnoreCase) ||
                    string.Equals(child.Text, "Zapret Update", StringComparison.OrdinalIgnoreCase) ||
                    string.Equals(child.Text, "Additional", StringComparison.OrdinalIgnoreCase))
                    return true;
            }
            return false;
        }

        private void EnsureServiceHeadingThemeHost()
        {
            var heading = NativeBridge.FindChildById(_parent, ZapretResponsiveLayoutHost.ServiceActionsHeadingId);
            if (heading == IntPtr.Zero) return;
            var host = GetParent(heading);
            if (host == IntPtr.Zero || host == _parent) return;
            if (host == _serviceHeadingHost) return;

            if (_serviceHeadingHost != IntPtr.Zero)
            {
                try { RemoveWindowSubclass(_serviceHeadingHost, ServiceHeadingSubclassDelegate, new UIntPtr(ServiceHeadingSubclassId)); } catch { }
                lock (Sync) ServiceHeadingOwners.Remove(_serviceHeadingHost);
                _serviceHeadingHost = IntPtr.Zero;
            }

            lock (Sync) ServiceHeadingOwners[host] = this;
            if (!SetWindowSubclass(host, ServiceHeadingSubclassDelegate, new UIntPtr(ServiceHeadingSubclassId), UIntPtr.Zero))
            {
                lock (Sync) ServiceHeadingOwners.Remove(host);
                return;
            }
            _serviceHeadingHost = host;
            InvalidateRect(heading, IntPtr.Zero, true);
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

            // The frozen core owns page visibility. If Zapret is no longer active, do not
            // resurrect its journal over another page; simply forget these page-specific HWNDs.
            var zapretMarker = NativeBridge.FindChildById(_parent, NativeBridge.ZapretApplyButtonId);
            var zapretStillVisible = zapretMarker != IntPtr.Zero && NativeBridge.IsWindowVisible(zapretMarker);
            if (zapretStillVisible)
            {
                if (_journalHeading != IntPtr.Zero && _journalHeadingWasVisible)
                    NativeBridge.ShowWindow(_journalHeading, NativeBridge.SW_SHOW);
                if (_journalList != IntPtr.Zero && _journalListWasVisible)
                    NativeBridge.ShowWindow(_journalList, NativeBridge.SW_SHOW);
            }

            _journalHeading = IntPtr.Zero;
            _journalList = IntPtr.Zero;
            _journalHeadingWasVisible = false;
            _journalListWasVisible = false;
            _journalCaptured = false;
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

        private static IntPtr StaticBridgeHostSubclassProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam, UIntPtr subclassId, UIntPtr refData)
        {
            ZapretVisualPolishHost owner;
            lock (Sync) BridgeHostOwners.TryGetValue(hwnd, out owner);
            if (owner != null && msg == WM_DRAWITEM && lParam != IntPtr.Zero)
            {
                var draw = (DRAWITEMSTRUCT)Marshal.PtrToStructure(lParam, typeof(DRAWITEMSTRUCT));
                if (draw.CtlType == ODT_BUTTON && BridgeButtonIds.Contains(unchecked((int)draw.CtlID)) &&
                    draw.hwndItem != IntPtr.Zero && owner._originalButtonStyles.ContainsKey(draw.hwndItem))
                {
                    owner.DrawOwnerDrawButton(ref draw);
                    return new IntPtr(1);
                }
            }
            return DefSubclassProc(hwnd, msg, wParam, lParam);
        }

        private static IntPtr StaticServiceHeadingSubclassProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam, UIntPtr subclassId, UIntPtr refData)
        {
            ZapretVisualPolishHost owner;
            lock (Sync) ServiceHeadingOwners.TryGetValue(hwnd, out owner);
            if (owner != null && msg == WM_CTLCOLORSTATIC && lParam != IntPtr.Zero)
            {
                var brush = owner.DrawServiceHeading(wParam, lParam);
                if (brush != IntPtr.Zero) return brush;
            }
            return DefSubclassProc(hwnd, msg, wParam, lParam);
        }

        private IntPtr DrawServiceHeading(IntPtr hdc, IntPtr control)
        {
            var heading = NativeBridge.FindChildById(_parent, ZapretResponsiveLayoutHost.ServiceActionsHeadingId);
            if (heading == IntPtr.Zero || heading != control || GetParent(heading) != _serviceHeadingHost) return IntPtr.Zero;

            var textColor = _darkTheme ? Rgb(242, 246, 250) : Rgb(28, 36, 46);
            var pageColor = _darkTheme ? Rgb(12, 17, 23) : Rgb(247, 248, 250);
            SetTextColor(hdc, textColor);
            SetBkColor(hdc, pageColor);
            SetBkMode(hdc, TRANSPARENT);
            return _darkTheme ? DarkPageBrush : LightPageBrush;
        }

        private void DrawOwnerDrawButton(ref DRAWITEMSTRUCT draw)
        {
            var button = draw.hwndItem;
            var hdc = draw.hDC;
            var rect = draw.rcItem;
            if (button == IntPtr.Zero || hdc == IntPtr.Zero) return;

            var pageBrush = _darkTheme ? DarkPageBrush : LightPageBrush;
            FillRect(hdc, ref rect, pageBrush);

            var pushed = (draw.itemState & ODS_SELECTED) != 0;
            var buttonBrush = _darkTheme
                ? (pushed ? DarkPressedBrush : DarkButtonBrush)
                : (pushed ? LightPressedBrush : LightButtonBrush);
            var borderBrush = _darkTheme ? DarkBorderBrush : LightBorderBrush;
            var height = Math.Max(1, rect.Bottom - rect.Top);
            var radius = Math.Max(6, Math.Min(12, height / 3));
            var region = CreateRoundRectRgn(rect.Left, rect.Top, rect.Right + 1, rect.Bottom + 1, radius, radius);
            if (region != IntPtr.Zero)
            {
                try
                {
                    FillRgn(hdc, region, buttonBrush);
                    FrameRgn(hdc, region, borderBrush, 1, 1);
                }
                finally
                {
                    DeleteObject(region);
                }
            }

            var font = NativeBridge.SendMessage(button, NativeBridge.WM_GETFONT, IntPtr.Zero, IntPtr.Zero);
            var oldFont = IntPtr.Zero;
            if (font != IntPtr.Zero) oldFont = SelectObject(hdc, font);
            try
            {
                SetBkMode(hdc, TRANSPARENT);
                var disabled = (draw.itemState & ODS_DISABLED) != 0 || !IsWindowEnabled(button);
                var textColor = disabled
                    ? (_darkTheme ? Rgb(145, 154, 164) : Rgb(132, 139, 148))
                    : (_darkTheme ? Rgb(242, 246, 250) : Rgb(28, 36, 46));
                SetTextColor(hdc, textColor);
                var textRect = rect;
                if (pushed) { textRect.Top += 1; textRect.Bottom += 1; }
                DrawText(hdc, NativeBridge.ReadWindowText(button) ?? string.Empty, -1, ref textRect,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            finally
            {
                if (oldFont != IntPtr.Zero) SelectObject(hdc, oldFont);
            }
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

            if (_serviceHeadingHost != IntPtr.Zero)
            {
                try { RemoveWindowSubclass(_serviceHeadingHost, ServiceHeadingSubclassDelegate, new UIntPtr(ServiceHeadingSubclassId)); } catch { }
                lock (Sync) ServiceHeadingOwners.Remove(_serviceHeadingHost);
                _serviceHeadingHost = IntPtr.Zero;
            }

            foreach (var host in _bridgeHosts)
            {
                try { RemoveWindowSubclass(host, BridgeHostSubclassDelegate, new UIntPtr(BridgeHostSubclassId)); } catch { }
                lock (Sync) BridgeHostOwners.Remove(host);
            }
            _bridgeHosts.Clear();

            foreach (var pair in _originalBridgeHostStyles)
            {
                try
                {
                    SetWindowLong(pair.Key, GWL_STYLE, pair.Value);
                    RedrawWindow(pair.Key, IntPtr.Zero, IntPtr.Zero,
                        RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
                }
                catch { }
            }
            _originalBridgeHostStyles.Clear();

            foreach (var pair in _originalButtonStyles)
            {
                try
                {
                    SetWindowLong(pair.Key, GWL_STYLE, pair.Value);
                    SetWindowTheme(pair.Key, null, null);
                    InvalidateRect(pair.Key, IntPtr.Zero, true);
                }
                catch { }
            }
            _originalButtonStyles.Clear();
        }
    }
}
