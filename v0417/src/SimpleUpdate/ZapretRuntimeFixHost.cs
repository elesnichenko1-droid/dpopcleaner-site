using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace DPopCleaner.SimpleUpdate
{
    internal sealed class ZapretRuntimeFixHost : IDisposable
    {
        internal const int LegacyZapretDownloadButtonId = 1715;
        internal const int ZapretDownloadProxyButtonId = 1724;

        private const uint WS_CHILD = 0x40000000;
        private const uint WS_VISIBLE = 0x10000000;
        private const uint WS_TABSTOP = 0x00010000;
        private const uint BS_PUSHBUTTON = 0x00000000;
        private const uint WM_COMMAND = 0x0111;
        private const uint WM_ERASEBKGND = 0x0014;
        private const uint WM_SETFONT = 0x0030;
        private const string HostClassName = "DPopCleanerZapretRuntimeFixHost";

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
            UpdateDisplayedVersion();
        }

        internal void Hide()
        {
            if (_disposed) return;
            if (_proxyHost != IntPtr.Zero) NativeBridge.ShowWindow(_proxyHost, NativeBridge.SW_HIDE);
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

        private void UpdateDisplayedVersion()
        {
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
            if (string.IsNullOrWhiteSpace(version)) return;

            foreach (var child in NativeBridge.GetChildren(_parent))
            {
                if (!child.Visible || !string.Equals(child.ClassName, "Static", StringComparison.OrdinalIgnoreCase)) continue;
                var text = (child.Text ?? string.Empty).Trim();
                if (!text.StartsWith("Zapret ", StringComparison.OrdinalIgnoreCase)) continue;
                var separator = text.IndexOf('•');
                if (separator < 0) continue; // Do not touch the "Zapret Center" heading.

                var suffix = text.Substring(separator).TrimStart();
                NativeBridge.WriteWindowText(child.Handle, "Zapret " + version + "   •   " + suffix.TrimStart('•').TrimStart());
                return;
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
                    UpdateDisplayedVersion();
                    return IntPtr.Zero;
                }
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
            _proxyButton = IntPtr.Zero;
            _proxyHost = IntPtr.Zero;
        }
    }
}
