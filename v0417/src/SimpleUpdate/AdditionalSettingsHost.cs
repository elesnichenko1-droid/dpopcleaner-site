using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace DPopCleaner.SimpleUpdate
{
    internal sealed class AdditionalSettingsHost : IDisposable
    {
        private const bool AutoScroll = true;
        private const int AutoScrollMinSize = 390;
        private const int ContentHeight = 410;
        private const int WheelStep = 48;

        private const uint WS_CHILD = 0x40000000;
        private const uint WS_VISIBLE = 0x10000000;
        private const uint WS_TABSTOP = 0x00010000;
        private const uint WS_CLIPSIBLINGS = 0x04000000;
        private const uint WS_CLIPCHILDREN = 0x02000000;
        private const uint WS_VSCROLL = 0x00200000;
        private const uint BS_AUTOCHECKBOX = 0x00000003;
        private const uint BS_PUSHBUTTON = 0x00000000;
        private const uint ES_AUTOHSCROLL = 0x0080;
        private const uint SS_LEFT = 0x00000000;

        private const uint WM_COMMAND = 0x0111;
        private const uint WM_VSCROLL = 0x0115;
        private const uint WM_MOUSEWHEEL = 0x020A;
        private const uint WM_ERASEBKGND = 0x0014;
        private const uint WM_CTLCOLORBTN = 0x0135;
        private const uint WM_CTLCOLORSTATIC = 0x0138;
        private const uint WM_CTLCOLOREDIT = 0x0133;
        private const uint WM_SETFONT = 0x0030;

        private const int SB_VERT = 1;
        private const int SB_LINEUP = 0;
        private const int SB_LINEDOWN = 1;
        private const int SB_PAGEUP = 2;
        private const int SB_PAGEDOWN = 3;
        private const int SB_THUMBPOSITION = 4;
        private const int SB_THUMBTRACK = 5;
        private const int SB_TOP = 6;
        private const int SB_BOTTOM = 7;
        private const uint SIF_RANGE = 0x0001;
        private const uint SIF_PAGE = 0x0002;
        private const uint SIF_POS = 0x0004;
        private const uint SIF_TRACKPOS = 0x0010;
        private const int TRANSPARENT = 1;

        private const string HostClassName = "DPopCleanerAdditionalSettingsHost";

        private static readonly string[] LegacySettingTexts =
        {
            "Фоновый контроль мусора каждые 30 минут",
            "Быстрый DPopGuard-скан при запуске",
            "Проверять кэш Windows Update при запуске",
            "Работать в трее и отслеживать новые установки",
            "Автозапуск DPopCleaner вместе с Windows",
            "Запускать приложение от имени администратора"
        };

        private sealed class ScrollItem
        {
            public IntPtr Handle;
            public int X;
            public int Y;
            public int Width;
            public int Height;
        }

        private sealed class LegacySettingProxy
        {
            public IntPtr LegacyHandle;
            public IntPtr ProxyHandle;
            public string Text;
            public int Id;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct WNDCLASSEX
        {
            public uint cbSize;
            public uint style;
            public IntPtr lpfnWndProc;
            public int cbClsExtra;
            public int cbWndExtra;
            public IntPtr hInstance;
            public IntPtr hIcon;
            public IntPtr hCursor;
            public IntPtr hbrBackground;
            [MarshalAs(UnmanagedType.LPWStr)] public string lpszMenuName;
            [MarshalAs(UnmanagedType.LPWStr)] public string lpszClassName;
            public IntPtr hIconSm;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct SCROLLINFO
        {
            public uint cbSize;
            public uint fMask;
            public int nMin;
            public int nMax;
            public uint nPage;
            public int nPos;
            public int nTrackPos;
        }

        private delegate IntPtr HostWndProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);
        private delegate IntPtr SubclassProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam, UIntPtr subclassId, UIntPtr refData);

        private static readonly object Sync = new object();
        private static readonly Dictionary<IntPtr, AdditionalSettingsHost> Hosts = new Dictionary<IntPtr, AdditionalSettingsHost>();
        private static readonly HostWndProc HostWndProcDelegate = StaticHostWndProc;
        private static readonly SubclassProc ChildSubclassDelegate = StaticChildSubclassProc;
        private static readonly IntPtr BackgroundBrush = CreateSolidBrush(Rgb(12, 17, 23));
        private static bool _classRegistered;

        private readonly List<ScrollItem> _items = new List<ScrollItem>();
        private readonly List<IntPtr> _wheelChildren = new List<IntPtr>();
        private readonly List<LegacySettingProxy> _legacySettings = new List<LegacySettingProxy>();
        private readonly Action<bool> _autoUpdateChanged;
        private readonly Action _checkUpdatesRequested;
        private readonly IntPtr _legacyKeyEdit;
        private readonly IntPtr _legacySaveButton;
        private readonly IntPtr _legacyBuyButton;
        private readonly IntPtr _font;

        private IntPtr _host;
        private IntPtr _autoUpdateCheckbox;
        private IntPtr _checkUpdatesButton;
        private IntPtr _licenseKeyEdit;
        private int _scrollPosition;
        private int _viewportHeight;
        private bool _disposed;

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
        private static extern IntPtr GetModuleHandle(string moduleName);

        [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern ushort RegisterClassEx(ref WNDCLASSEX windowClass);

        [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr CreateWindowEx(uint exStyle, string className, string windowName, uint style,
            int x, int y, int width, int height, IntPtr parent, IntPtr menu, IntPtr instance, IntPtr param);

        [DllImport("user32.dll")]
        private static extern IntPtr DefWindowProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);

        [DllImport("user32.dll")]
        private static extern IntPtr LoadCursor(IntPtr instance, IntPtr cursorName);

        [DllImport("user32.dll")]
        private static extern bool DestroyWindow(IntPtr hwnd);

        [DllImport("user32.dll")]
        private static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int width, int height, uint flags);

        [DllImport("user32.dll")]
        private static extern int SetScrollInfo(IntPtr hwnd, int bar, ref SCROLLINFO info, bool redraw);

        [DllImport("user32.dll")]
        private static extern bool GetScrollInfo(IntPtr hwnd, int bar, ref SCROLLINFO info);

        [DllImport("user32.dll")]
        private static extern int FillRect(IntPtr hdc, ref RECT rect, IntPtr brush);

        [DllImport("user32.dll")]
        private static extern bool GetClientRect(IntPtr hwnd, out RECT rect);

        [DllImport("gdi32.dll")]
        private static extern IntPtr CreateSolidBrush(uint colorRef);

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

        [DllImport("user32.dll")]
        private static extern int GetDlgCtrlID(IntPtr hwnd);

        [StructLayout(LayoutKind.Sequential)]
        private struct RECT { public int Left, Top, Right, Bottom; }

        internal AdditionalSettingsHost(
            IntPtr parent,
            NativeBridge.ClientBounds bounds,
            IntPtr adminAnchor,
            bool autoUpdateEnabled,
            Action<bool> autoUpdateChanged,
            Action checkUpdatesRequested,
            IntPtr legacyKeyEdit,
            IntPtr legacySaveButton,
            IntPtr legacyBuyButton)
        {
            if (!AutoScroll) throw new InvalidOperationException("Scrollable settings host must have AutoScroll = true.");
            if (parent == IntPtr.Zero) throw new ArgumentException("Parent window is required.", "parent");
            if (bounds == null) throw new ArgumentNullException("bounds");

            _autoUpdateChanged = autoUpdateChanged;
            _checkUpdatesRequested = checkUpdatesRequested;
            _legacyKeyEdit = legacyKeyEdit;
            _legacySaveButton = legacySaveButton;
            _legacyBuyButton = legacyBuyButton;
            _font = adminAnchor != IntPtr.Zero
                ? NativeBridge.SendMessage(adminAnchor, NativeBridge.WM_GETFONT, IntPtr.Zero, IntPtr.Zero)
                : IntPtr.Zero;

            for (var i = 0; i < LegacySettingTexts.Length; i++)
            {
                var legacy = NativeBridge.FindChildByText(parent, LegacySettingTexts[i], "Button", true);
                if (legacy == IntPtr.Zero)
                    throw new InvalidOperationException("Frozen Settings control was not found: " + LegacySettingTexts[i]);
                _legacySettings.Add(new LegacySettingProxy
                {
                    LegacyHandle = legacy,
                    Text = LegacySettingTexts[i],
                    Id = 1500 + i
                });
            }

            EnsureHostClass();
            _viewportHeight = bounds.Height;
            _host = CreateWindowEx(0, HostClassName, string.Empty,
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VSCROLL,
                bounds.Left, bounds.Top, bounds.Width, bounds.Height,
                parent, new IntPtr(NativeBridge.SettingsScrollHostId), GetModuleHandle(null), IntPtr.Zero);
            if (_host == IntPtr.Zero)
                throw new InvalidOperationException("Could not create scrollable Settings host.");

            lock (Sync) Hosts[_host] = this;
            CreateContents(autoUpdateEnabled);
            ApplyScroll(0);
        }

        internal IntPtr Handle { get { return _host; } }

        internal void Show(NativeBridge.ClientBounds bounds)
        {
            if (_disposed || _host == IntPtr.Zero || bounds == null) return;
            _viewportHeight = bounds.Height;
            SyncLegacySettingsFromCore();
            NativeBridge.PositionChildWindow(_host, bounds);
            UpdateScrollInfo();
            NativeBridge.ShowWindow(_host, NativeBridge.SW_SHOW);
        }

        internal void Hide()
        {
            if (_disposed || _host == IntPtr.Zero) return;
            NativeBridge.ShowWindow(_host, NativeBridge.SW_HIDE);
        }

        private void CreateContents(bool autoUpdateEnabled)
        {
            var y = 4;
            foreach (var setting in _legacySettings)
            {
                setting.ProxyHandle = CreateChild("Button", setting.Text,
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                    setting.Id, 10, y, 460, 24);
                NativeBridge.SetChecked(setting.ProxyHandle, NativeBridge.IsChecked(setting.LegacyHandle));
                y += 30;
            }

            _autoUpdateCheckbox = CreateChild("Button", "Включить автообновление приложения",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                NativeBridge.AutoUpdateCheckboxId, 10, y, 280, 26);
            NativeBridge.SetChecked(_autoUpdateCheckbox, autoUpdateEnabled);

            _checkUpdatesButton = CreateChild("Button", "Проверить обновления",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                NativeBridge.CheckNowButtonId, 300, y - 3, 170, 32);
            y += 46;

            CreateStatic("Лицензия", NativeBridge.LicenseHeadingProxyId, 10, y, 220, 26);
            y += 28;
            CreateStatic("Бесплатная BETA. Лицензионный сервер будет подключён позже.", 1498, 10, y, 460, 34);
            y += 40;

            _licenseKeyEdit = CreateChild("Edit", NativeBridge.ReadWindowText(_legacyKeyEdit),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                NativeBridge.LicenseKeyProxyId, 10, y, 300, 30);
            CreateChild("Button", "Сохранить ключ",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                NativeBridge.LicenseSaveProxyId, 320, y - 2, 140, 32);
            y += 42;
            CreateChild("Button", "Купить лицензию",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                NativeBridge.LicenseBuyProxyId, 10, y, 155, 32);
            CreateStatic("Покупка и проверка ключей будут подключены позже.", 1499, 178, y + 3, 282, 30);

            if (AutoScrollMinSize > ContentHeight)
                throw new InvalidOperationException("AutoScrollMinSize cannot exceed the content layout height.");
        }

        private IntPtr CreateStatic(string text, int id, int x, int y, int width, int height)
        {
            return CreateChild("Static", text, WS_CHILD | WS_VISIBLE | SS_LEFT, id, x, y, width, height);
        }

        private IntPtr CreateChild(string className, string text, uint style, int id, int x, int y, int width, int height)
        {
            var child = CreateWindowEx(0, className, text ?? string.Empty, style,
                x, y, width, height, _host, new IntPtr(id), GetModuleHandle(null), IntPtr.Zero);
            if (child == IntPtr.Zero) throw new InvalidOperationException("Could not create Settings control id=" + id + ".");

            if (_font != IntPtr.Zero)
                NativeBridge.SendMessage(child, WM_SETFONT, _font, new IntPtr(1));
            try { SetWindowTheme(child, "DarkMode_Explorer", null); } catch { }

            _items.Add(new ScrollItem { Handle = child, X = x, Y = y, Width = width, Height = height });
            var subclassId = new UIntPtr((uint)Math.Max(1, id));
            var refData = new UIntPtr(unchecked((ulong)_host.ToInt64()));
            if (SetWindowSubclass(child, ChildSubclassDelegate, subclassId, refData))
                _wheelChildren.Add(child);
            return child;
        }

        private IntPtr WindowProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam)
        {
            if (msg == WM_MOUSEWHEEL)
            {
                var delta = (short)((wParam.ToInt64() >> 16) & 0xffff);
                if (delta != 0) ApplyScroll(_scrollPosition - Math.Sign(delta) * WheelStep);
                return IntPtr.Zero;
            }

            if (msg == WM_VSCROLL)
            {
                HandleVerticalScroll((int)(wParam.ToInt64() & 0xffff));
                return IntPtr.Zero;
            }

            if (msg == WM_COMMAND)
            {
                var id = (int)(wParam.ToInt64() & 0xffff);
                foreach (var setting in _legacySettings)
                {
                    if (id == setting.Id)
                    {
                        SyncLegacySetting(setting);
                        return IntPtr.Zero;
                    }
                }

                if (id == NativeBridge.AutoUpdateCheckboxId)
                {
                    var enabled = NativeBridge.IsChecked(_autoUpdateCheckbox);
                    if (_autoUpdateChanged != null) _autoUpdateChanged(enabled);
                    return IntPtr.Zero;
                }
                if (id == NativeBridge.CheckNowButtonId)
                {
                    if (_checkUpdatesRequested != null) _checkUpdatesRequested();
                    return IntPtr.Zero;
                }
                if (id == NativeBridge.LicenseSaveProxyId)
                {
                    NativeBridge.WriteWindowText(_legacyKeyEdit, NativeBridge.ReadWindowText(_licenseKeyEdit));
                    NativeBridge.ClickButton(_legacySaveButton);
                    return IntPtr.Zero;
                }
                if (id == NativeBridge.LicenseBuyProxyId)
                {
                    NativeBridge.ClickButton(_legacyBuyButton);
                    return IntPtr.Zero;
                }
            }

            if (msg == WM_ERASEBKGND)
            {
                RECT rect;
                if (GetClientRect(hwnd, out rect)) FillRect(wParam, ref rect, BackgroundBrush);
                return new IntPtr(1);
            }

            if (msg == WM_CTLCOLORSTATIC || msg == WM_CTLCOLORBTN || msg == WM_CTLCOLOREDIT)
            {
                SetTextColor(wParam, Rgb(242, 246, 250));
                SetBkColor(wParam, Rgb(12, 17, 23));
                SetBkMode(wParam, TRANSPARENT);
                return BackgroundBrush;
            }

            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        private void SyncLegacySetting(LegacySettingProxy setting)
        {
            if (setting == null || setting.LegacyHandle == IntPtr.Zero || setting.ProxyHandle == IntPtr.Zero) return;
            var desired = NativeBridge.IsChecked(setting.ProxyHandle);
            var actual = NativeBridge.IsChecked(setting.LegacyHandle);
            if (desired != actual) NativeBridge.ClickButton(setting.LegacyHandle);
            NativeBridge.SetChecked(setting.ProxyHandle, NativeBridge.IsChecked(setting.LegacyHandle));
        }

        private void SyncLegacySettingsFromCore()
        {
            foreach (var setting in _legacySettings)
                if (setting.ProxyHandle != IntPtr.Zero)
                    NativeBridge.SetChecked(setting.ProxyHandle, NativeBridge.IsChecked(setting.LegacyHandle));
        }

        private void HandleVerticalScroll(int request)
        {
            var target = _scrollPosition;
            switch (request)
            {
                case SB_LINEUP: target -= 24; break;
                case SB_LINEDOWN: target += 24; break;
                case SB_PAGEUP: target -= Math.Max(48, _viewportHeight - 32); break;
                case SB_PAGEDOWN: target += Math.Max(48, _viewportHeight - 32); break;
                case SB_TOP: target = 0; break;
                case SB_BOTTOM: target = MaxScroll; break;
                case SB_THUMBPOSITION:
                case SB_THUMBTRACK:
                    var info = NewScrollInfo(SIF_TRACKPOS);
                    if (GetScrollInfo(_host, SB_VERT, ref info)) target = info.nTrackPos;
                    break;
            }
            ApplyScroll(target);
        }

        private int MaxScroll { get { return Math.Max(0, ContentHeight - Math.Max(1, _viewportHeight)); } }

        private void ApplyScroll(int target)
        {
            if (_host == IntPtr.Zero) return;
            _scrollPosition = Math.Max(0, Math.Min(MaxScroll, target));
            foreach (var item in _items)
                SetWindowPos(item.Handle, IntPtr.Zero, item.X, item.Y - _scrollPosition, item.Width, item.Height, 0x0004 | 0x0010);
            UpdateScrollInfo();
        }

        private void UpdateScrollInfo()
        {
            if (_host == IntPtr.Zero) return;
            var info = NewScrollInfo(SIF_RANGE | SIF_PAGE | SIF_POS);
            info.nMin = 0;
            info.nMax = Math.Max(0, ContentHeight - 1);
            info.nPage = (uint)Math.Max(1, _viewportHeight);
            info.nPos = _scrollPosition;
            SetScrollInfo(_host, SB_VERT, ref info, true);
        }

        private static SCROLLINFO NewScrollInfo(uint mask)
        {
            return new SCROLLINFO { cbSize = (uint)Marshal.SizeOf(typeof(SCROLLINFO)), fMask = mask };
        }

        private static void EnsureHostClass()
        {
            lock (Sync)
            {
                if (_classRegistered) return;
                var wc = new WNDCLASSEX
                {
                    cbSize = (uint)Marshal.SizeOf(typeof(WNDCLASSEX)),
                    lpfnWndProc = Marshal.GetFunctionPointerForDelegate(HostWndProcDelegate),
                    hInstance = GetModuleHandle(null),
                    hCursor = LoadCursor(IntPtr.Zero, new IntPtr(32512)),
                    hbrBackground = BackgroundBrush,
                    lpszClassName = HostClassName
                };
                var atom = RegisterClassEx(ref wc);
                if (atom == 0)
                {
                    var error = Marshal.GetLastWin32Error();
                    if (error != 1410) throw new InvalidOperationException("Could not register Settings host class. Win32=" + error);
                }
                _classRegistered = true;
            }
        }

        private static IntPtr StaticHostWndProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam)
        {
            AdditionalSettingsHost host;
            lock (Sync) Hosts.TryGetValue(hwnd, out host);
            return host != null ? host.WindowProc(hwnd, msg, wParam, lParam) : DefWindowProc(hwnd, msg, wParam, lParam);
        }

        private static IntPtr StaticChildSubclassProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam, UIntPtr subclassId, UIntPtr refData)
        {
            if (msg == WM_MOUSEWHEEL)
            {
                var host = new IntPtr(unchecked((long)refData.ToUInt64()));
                NativeBridge.SendMessage(host, msg, wParam, lParam);
                return IntPtr.Zero;
            }
            return DefSubclassProc(hwnd, msg, wParam, lParam);
        }

        private static uint Rgb(byte r, byte g, byte b)
        {
            return (uint)(r | (g << 8) | (b << 16));
        }

        public void Dispose()
        {
            if (_disposed) return;
            _disposed = true;
            foreach (var child in _wheelChildren)
            {
                var id = new UIntPtr((uint)Math.Max(1, GetDlgCtrlID(child)));
                try { RemoveWindowSubclass(child, ChildSubclassDelegate, id); } catch { }
            }
            _wheelChildren.Clear();
            if (_host != IntPtr.Zero)
            {
                lock (Sync) Hosts.Remove(_host);
                try { DestroyWindow(_host); } catch { }
                _host = IntPtr.Zero;
            }
        }
    }
}
