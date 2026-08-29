using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using Microsoft.Win32;

namespace DPopCleaner.SimpleUpdate
{
    internal sealed class ZapretRuntimeFixHost : IDisposable
    {
        internal const int LegacyZapretDownloadButtonId = 1715;
        internal const int ZapretDownloadProxyButtonId = 1724;
        internal const int ZapretStatusProxyId = 1725;
        private const int ZapretStatusTextId = 1726;

        private const uint WS_CHILD = 0x40000000;
        private const uint WS_VISIBLE = 0x10000000;
        private const uint WS_TABSTOP = 0x00010000;
        private const uint WS_BORDER = 0x00800000;
        private const uint BS_PUSHBUTTON = 0x00000000;
        private const uint SS_LEFT = 0x00000000;
        private const uint SS_CENTERIMAGE = 0x00000200;
        private const uint WM_COMMAND = 0x0111;
        private const uint WM_ERASEBKGND = 0x0014;
        private const uint WM_SETFONT = 0x0030;
        private const uint WM_CTLCOLORSTATIC = 0x0138;
        private const string HostClassName = "DPopCleanerZapretRuntimeFixHost";
        private const string ZapretDescription = "Стратегии, winws.exe, службы и обновления Zapret в одном месте.";

        private static readonly object Sync = new object();
        private static readonly Dictionary<IntPtr, ZapretRuntimeFixHost> Hosts = new Dictionary<IntPtr, ZapretRuntimeFixHost>();
        private static readonly HostWndProc HostWndProcDelegate = StaticHostWndProc;
        private static readonly IntPtr BackgroundBrush = CreateSolidBrush(Rgb(12, 17, 23));
        private static bool _classRegistered;

        private readonly IntPtr _parent;
        private readonly string _applicationRoot;
        private IntPtr _legacyDownloadButton;
        private IntPtr _proxyHost;
        private IntPtr _proxyButton;
        private IntPtr _statusHost;
        private IntPtr _statusText;
        private DateTime _nextStatusRefreshUtc;
        private string _lastStatusText = string.Empty;
        private bool _disposed;

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

        private delegate IntPtr HostWndProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);

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

        [DllImport("gdi32.dll")]
        private static extern IntPtr CreateSolidBrush(uint colorRef);

        [DllImport("gdi32.dll")]
        private static extern uint SetTextColor(IntPtr hdc, uint colorRef);

        [DllImport("gdi32.dll")]
        private static extern uint SetBkColor(IntPtr hdc, uint colorRef);

        [DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]
        private static extern int SetWindowTheme(IntPtr hwnd, string subAppName, string subIdList);

        internal ZapretRuntimeFixHost(IntPtr parent, string applicationRoot)
        {
            if (parent == IntPtr.Zero) throw new ArgumentException("Parent window is required.", "parent");
            _parent = parent;
            _applicationRoot = Path.GetFullPath(applicationRoot ?? string.Empty);
            _legacyDownloadButton = FindLegacyDownloadButton();
            if (_legacyDownloadButton == IntPtr.Zero)
                throw new InvalidOperationException("Frozen Zapret download button was not found.");

            EnsureHostClass();
            CreateProxy();
            CreateStatusOverlay();
            Show();
        }

        internal void Show()
        {
            if (_disposed) return;

            var currentLegacy = FindLegacyDownloadButton();
            if (currentLegacy != IntPtr.Zero) _legacyDownloadButton = currentLegacy;
            if (_legacyDownloadButton == IntPtr.Zero) return;

            var bounds = NativeBridge.GetChildClientBounds(_parent, _legacyDownloadButton);
            if (bounds == null) return;

            // The frozen handler is the source of the "Модуль обновления Zapret не найден" dialog.
            // Keep the control at exactly the same visual position, but make the visible button owned
            // by this bridge process so its click never reaches the broken 0.2.14 handler.
            NativeBridge.ShowWindow(_legacyDownloadButton, NativeBridge.SW_HIDE);
            NativeBridge.PositionChildWindow(_proxyHost, bounds);
            SetWindowPos(_proxyButton, IntPtr.Zero, 0, 0, bounds.Width, bounds.Height, 0x0004 | 0x0010);
            NativeBridge.ShowWindow(_proxyHost, NativeBridge.SW_SHOW);

            PositionStatusOverlay();
            UpdateDisplayedVersion();
        }

        internal void Hide()
        {
            if (_disposed) return;
            if (_proxyHost != IntPtr.Zero) NativeBridge.ShowWindow(_proxyHost, NativeBridge.SW_HIDE);
            if (_statusHost != IntPtr.Zero) NativeBridge.ShowWindow(_statusHost, NativeBridge.SW_HIDE);
        }

        private IntPtr FindLegacyDownloadButton()
        {
            var byId = NativeBridge.FindChildById(_parent, LegacyZapretDownloadButtonId);
            if (byId != IntPtr.Zero) return byId;
            return NativeBridge.FindChildByText(_parent, "Скачать и установить", "Button", false);
        }

        private void CreateProxy()
        {
            var bounds = NativeBridge.GetChildClientBounds(_parent, _legacyDownloadButton);
            var width = bounds != null ? Math.Max(1, bounds.Width) : 205;
            var height = bounds != null ? Math.Max(1, bounds.Height) : 40;
            var font = NativeBridge.SendMessage(_legacyDownloadButton, NativeBridge.WM_GETFONT, IntPtr.Zero, IntPtr.Zero);

            _proxyHost = CreateWindowEx(0, HostClassName, string.Empty, WS_CHILD | WS_VISIBLE,
                0, 0, width, height, _parent, IntPtr.Zero, GetModuleHandle(null), IntPtr.Zero);
            if (_proxyHost == IntPtr.Zero)
                throw new InvalidOperationException("Could not create Zapret updater proxy host.");
            lock (Sync) Hosts[_proxyHost] = this;

            _proxyButton = CreateWindowEx(0, "Button", "Скачать и установить",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                0, 0, width, height, _proxyHost, new IntPtr(ZapretDownloadProxyButtonId), GetModuleHandle(null), IntPtr.Zero);
            if (_proxyButton == IntPtr.Zero)
                throw new InvalidOperationException("Could not create Zapret updater proxy button.");
            if (font != IntPtr.Zero) NativeBridge.SendMessage(_proxyButton, WM_SETFONT, font, new IntPtr(1));
            try { SetWindowTheme(_proxyButton, "DarkMode_Explorer", null); } catch { }
        }

        private void CreateStatusOverlay()
        {
            var fontAnchor = NativeBridge.FindChildById(_parent, NativeBridge.ZapretCheckVersionButtonId);
            var font = fontAnchor != IntPtr.Zero
                ? NativeBridge.SendMessage(fontAnchor, NativeBridge.WM_GETFONT, IntPtr.Zero, IntPtr.Zero)
                : IntPtr.Zero;

            _statusHost = CreateWindowEx(0, HostClassName, string.Empty,
                WS_CHILD | WS_VISIBLE | WS_BORDER,
                0, 0, 400, 30, _parent, new IntPtr(ZapretStatusProxyId), GetModuleHandle(null), IntPtr.Zero);
            if (_statusHost == IntPtr.Zero)
                throw new InvalidOperationException("Could not create Zapret status overlay.");
            lock (Sync) Hosts[_statusHost] = this;

            _statusText = CreateWindowEx(0, "Static", string.Empty,
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                5, 0, 390, 28, _statusHost, new IntPtr(ZapretStatusTextId), GetModuleHandle(null), IntPtr.Zero);
            if (_statusText == IntPtr.Zero)
                throw new InvalidOperationException("Could not create Zapret status text.");
            if (font != IntPtr.Zero) NativeBridge.SendMessage(_statusText, WM_SETFONT, font, new IntPtr(1));
        }

        private void PositionStatusOverlay()
        {
            if (_statusHost == IntPtr.Zero) return;
            var description = NativeBridge.GetChildClientBounds(
                _parent,
                NativeBridge.FindChildByText(_parent, ZapretDescription, "Static", true));
            var statusButton = NativeBridge.GetChildClientBounds(
                _parent,
                NativeBridge.FindChildById(_parent, 1703));
            var installButton = NativeBridge.GetChildClientBounds(
                _parent,
                NativeBridge.FindChildById(_parent, 1701));
            if (description == null || statusButton == null || installButton == null) return;

            var top = description.Bottom + 7;
            var bottomLimit = installButton.Top - 44;
            if (bottomLimit > top + 24) top = Math.Min(top, bottomLimit - 30);
            var height = 30;
            var width = Math.Max(200, statusButton.Right - description.Left);
            NativeBridge.PositionChildWindow(_statusHost, new NativeBridge.ClientBounds
            {
                Left = description.Left,
                Top = top,
                Right = description.Left + width,
                Bottom = top + height
            });
            SetWindowPos(_statusText, IntPtr.Zero, 5, 0, Math.Max(1, width - 10), Math.Max(1, height - 2), 0x0004 | 0x0010);
            NativeBridge.ShowWindow(_statusHost, NativeBridge.SW_SHOW);
        }

        private void UpdateDisplayedVersion()
        {
            if (_statusHost == IntPtr.Zero || _statusText == IntPtr.Zero) return;
            if (DateTime.UtcNow < _nextStatusRefreshUtc) return;
            _nextStatusRefreshUtc = DateTime.UtcNow.AddSeconds(1);

            var versionPath = Path.Combine(_applicationRoot, "Zapret", ".service", "version.txt");
            string version;
            try
            {
                version = File.Exists(versionPath) ? (File.ReadAllText(versionPath) ?? string.Empty).Trim() : string.Empty;
            }
            catch
            {
                version = string.Empty;
            }
            if (string.IsNullOrWhiteSpace(version)) version = "не определена";

            var serviceInstalled = IsZapretServiceInstalled();
            var winwsRunning = IsWinwsRunning();
            var status = "Zapret " + version +
                         "   •   сервис: " + (serviceInstalled ? "установлен" : "не установлен") +
                         "   •   winws: " + (winwsRunning ? "ON" : "OFF");
            if (string.Equals(status, _lastStatusText, StringComparison.Ordinal)) return;

            _lastStatusText = status;
            NativeBridge.WriteWindowText(_statusHost, status);
            NativeBridge.WriteWindowText(_statusText, status);
        }

        private static bool IsZapretServiceInstalled()
        {
            try
            {
                using (var key = Registry.LocalMachine.OpenSubKey(@"SYSTEM\CurrentControlSet\Services\zapret", false))
                    return key != null;
            }
            catch
            {
                return false;
            }
        }

        private static bool IsWinwsRunning()
        {
            try
            {
                var processes = Process.GetProcessesByName("winws");
                try { return processes.Length > 0; }
                finally
                {
                    foreach (var process in processes)
                    {
                        try { process.Dispose(); } catch { }
                    }
                }
            }
            catch
            {
                return false;
            }
        }

        private IntPtr WindowProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam)
        {
            if (msg == WM_COMMAND)
            {
                var id = (int)(wParam.ToInt64() & 0xffff);
                if (id == ZapretDownloadProxyButtonId)
                {
                    LegacyZapretUpdater.Run(_applicationRoot);
                    _nextStatusRefreshUtc = DateTime.MinValue;
                    UpdateDisplayedVersion();
                    return IntPtr.Zero;
                }
            }
            if (msg == WM_CTLCOLORSTATIC)
            {
                try
                {
                    SetTextColor(wParam, Rgb(238, 244, 250));
                    SetBkColor(wParam, Rgb(12, 17, 23));
                }
                catch { }
                return BackgroundBrush;
            }
            if (msg == WM_ERASEBKGND) return new IntPtr(1);
            return DefWindowProc(hwnd, msg, wParam, lParam);
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
                    if (error != 1410)
                        throw new InvalidOperationException("Could not register Zapret runtime fix class. Win32=" + error);
                }
                _classRegistered = true;
            }
        }

        private static IntPtr StaticHostWndProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam)
        {
            ZapretRuntimeFixHost host;
            lock (Sync) Hosts.TryGetValue(hwnd, out host);
            return host != null ? host.WindowProc(hwnd, msg, wParam, lParam) : DefWindowProc(hwnd, msg, wParam, lParam);
        }

        private static uint Rgb(byte r, byte g, byte b)
        {
            return (uint)(r | (g << 8) | (b << 16));
        }

        public void Dispose()
        {
            if (_disposed) return;
            _disposed = true;

            if (_legacyDownloadButton != IntPtr.Zero)
            {
                try { NativeBridge.ShowWindow(_legacyDownloadButton, NativeBridge.SW_SHOW); } catch { }
            }
            if (_proxyHost != IntPtr.Zero)
            {
                lock (Sync) Hosts.Remove(_proxyHost);
                try { DestroyWindow(_proxyHost); } catch { }
            }
            if (_statusHost != IntPtr.Zero)
            {
                lock (Sync) Hosts.Remove(_statusHost);
                try { DestroyWindow(_statusHost); } catch { }
            }
            _statusText = IntPtr.Zero;
            _statusHost = IntPtr.Zero;
            _proxyButton = IntPtr.Zero;
            _proxyHost = IntPtr.Zero;
        }
    }
}
