using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace DPopCleaner.SimpleUpdate
{
    internal sealed class ZapretUpdateProxyHost : IDisposable
    {
        internal const int LegacyCheckVersionButtonId = 1724;
        internal const int LegacyDownloadButtonId = 1725;
        private const uint WS_CHILD = 0x40000000;
        private const uint WS_VISIBLE = 0x10000000;
        private const uint WS_TABSTOP = 0x00010000;
        private const uint WM_COMMAND = 0x0111;
        private const uint WM_ERASEBKGND = 0x0014;
        private const uint WM_SETFONT = 0x0030;
        private const string HostClassName = "DPopCleanerZapretUpdateProxyHost";

        private static readonly object Sync = new object();
        private static readonly Dictionary<IntPtr, ZapretUpdateProxyHost> Hosts = new Dictionary<IntPtr, ZapretUpdateProxyHost>();
        private static readonly HostWndProc ProcDelegate = StaticWndProc;
        private static bool _registered;

        private readonly IntPtr _parent;
        private readonly string _applicationRoot;
        private IntPtr _host;
        private IntPtr _legacyCheckVersionButton;
        private IntPtr _legacyDownloadButton;
        private bool _disposed;

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct WNDCLASSEX
        {
            public uint cbSize, style;
            public IntPtr lpfnWndProc;
            public int cbClsExtra, cbWndExtra;
            public IntPtr hInstance, hIcon, hCursor, hbrBackground;
            [MarshalAs(UnmanagedType.LPWStr)] public string lpszMenuName;
            [MarshalAs(UnmanagedType.LPWStr)] public string lpszClassName;
            public IntPtr hIconSm;
        }

        private delegate IntPtr HostWndProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] private static extern IntPtr GetModuleHandle(string moduleName);
        [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)] private static extern ushort RegisterClassEx(ref WNDCLASSEX wc);
        [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)] private static extern IntPtr CreateWindowEx(uint exStyle, string cls, string text, uint style, int x, int y, int w, int h, IntPtr parent, IntPtr menu, IntPtr instance, IntPtr param);
        [DllImport("user32.dll")] private static extern IntPtr DefWindowProc(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
        [DllImport("user32.dll")] private static extern bool DestroyWindow(IntPtr hwnd);
        [DllImport("user32.dll")] private static extern IntPtr LoadCursor(IntPtr instance, IntPtr cursorName);

        internal ZapretUpdateProxyHost(IntPtr parent, string applicationRoot)
        {
            _parent = parent;
            _applicationRoot = Path.GetFullPath(applicationRoot ?? string.Empty);
            EnsureClass();
            CreateLegacyUpdateProxy();
        }

        internal void Show()
        {
            if (_disposed) return;
            CreateLegacyUpdateProxy();
            RefreshDisplayedZapretVersion();
            if (_legacyCheckVersionButton != IntPtr.Zero) NativeBridge.ShowWindow(_legacyCheckVersionButton, NativeBridge.SW_HIDE);
            if (_legacyDownloadButton != IntPtr.Zero) NativeBridge.ShowWindow(_legacyDownloadButton, NativeBridge.SW_HIDE);
            if (_host != IntPtr.Zero) NativeBridge.ShowWindow(_host, NativeBridge.SW_SHOW);
        }

        internal void Hide()
        {
            if (!_disposed && _host != IntPtr.Zero) NativeBridge.ShowWindow(_host, NativeBridge.SW_HIDE);
        }

        private void CreateLegacyUpdateProxy()
        {
            _legacyCheckVersionButton = NativeBridge.FindChildByText(_parent, "Проверить версию", "Button", true);
            _legacyDownloadButton = NativeBridge.FindChildByText(_parent, "Скачать и установить", "Button", true);
            if (_legacyCheckVersionButton == IntPtr.Zero || _legacyDownloadButton == IntPtr.Zero) return;

            var a = NativeBridge.GetChildClientBounds(_parent, _legacyCheckVersionButton);
            var b = NativeBridge.GetChildClientBounds(_parent, _legacyDownloadButton);
            if (a == null || b == null) return;
            var left = Math.Min(a.Left, b.Left);
            var top = Math.Min(a.Top, b.Top);
            var right = Math.Max(a.Right, b.Right);
            var bottom = Math.Max(a.Bottom, b.Bottom);

            if (_host == IntPtr.Zero)
            {
                _host = CreateWindowEx(0, HostClassName, string.Empty, WS_CHILD | WS_VISIBLE,
                    left, top, right - left, bottom - top, _parent, IntPtr.Zero, GetModuleHandle(null), IntPtr.Zero);
                if (_host == IntPtr.Zero) throw new InvalidOperationException("Could not create Zapret update proxy.");
                lock (Sync) Hosts[_host] = this;

                var font = NativeBridge.SendMessage(_legacyCheckVersionButton, NativeBridge.WM_GETFONT, IntPtr.Zero, IntPtr.Zero);
                CreateButton("Проверить версию", LegacyCheckVersionButtonId, a.Left - left, a.Top - top, a.Width, a.Height, font);
                CreateButton("Скачать и установить", LegacyDownloadButtonId, b.Left - left, b.Top - top, b.Width, b.Height, font);
            }
            else
            {
                NativeBridge.PositionChildWindow(_host, new NativeBridge.ClientBounds { Left = left, Top = top, Right = right, Bottom = bottom });
            }

            NativeBridge.ShowWindow(_legacyCheckVersionButton, NativeBridge.SW_HIDE);
            NativeBridge.ShowWindow(_legacyDownloadButton, NativeBridge.SW_HIDE);
            RefreshDisplayedZapretVersion();
        }

        private void CreateButton(string text, int id, int x, int y, int width, int height, IntPtr font)
        {
            var button = CreateWindowEx(0, "Button", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                x, y, width, height, _host, new IntPtr(id), GetModuleHandle(null), IntPtr.Zero);
            if (button == IntPtr.Zero) throw new InvalidOperationException("Could not create Zapret update proxy button.");
            if (font != IntPtr.Zero) NativeBridge.SendMessage(button, WM_SETFONT, font, new IntPtr(1));
        }

        private void RefreshDisplayedZapretVersion()
        {
            var versionPath = Path.Combine(_applicationRoot, "Zapret", ".service", "version.txt");
            if (!File.Exists(versionPath)) return;
            var version = (File.ReadAllText(versionPath) ?? string.Empty).Trim();
            if (string.IsNullOrWhiteSpace(version)) return;

            foreach (var child in NativeBridge.GetChildren(_parent))
            {
                if (!child.Visible || !string.Equals(child.ClassName, "Static", StringComparison.OrdinalIgnoreCase)) continue;
                var text = (child.Text ?? string.Empty).Trim();
                if (!text.StartsWith("Zapret ", StringComparison.OrdinalIgnoreCase)) continue;
                var bullet = text.IndexOf('•');
                var suffix = bullet >= 0 ? "  " + text.Substring(bullet) : string.Empty;
                NativeBridge.WriteWindowText(child.Handle, "Zapret " + version + suffix);
                break;
            }
        }

        private IntPtr WindowProc(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp)
        {
            if (msg == WM_COMMAND)
            {
                var id = (int)(wp.ToInt64() & 0xffff);
                if (id == LegacyCheckVersionButtonId || id == LegacyDownloadButtonId)
                {
                    LegacyZapretUpdater.Run(_applicationRoot);
                    RefreshDisplayedZapretVersion();
                    return IntPtr.Zero;
                }
            }
            if (msg == WM_ERASEBKGND) return new IntPtr(1);
            return DefWindowProc(hwnd, msg, wp, lp);
        }

        private static void EnsureClass()
        {
            lock (Sync)
            {
                if (_registered) return;
                var wc = new WNDCLASSEX { cbSize = (uint)Marshal.SizeOf(typeof(WNDCLASSEX)), lpfnWndProc = Marshal.GetFunctionPointerForDelegate(ProcDelegate), hInstance = GetModuleHandle(null), hCursor = LoadCursor(IntPtr.Zero, new IntPtr(32512)), lpszClassName = HostClassName };
                var atom = RegisterClassEx(ref wc);
                if (atom == 0 && Marshal.GetLastWin32Error() != 1410) throw new InvalidOperationException("Could not register Zapret update proxy class.");
                _registered = true;
            }
        }

        private static IntPtr StaticWndProc(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp)
        {
            ZapretUpdateProxyHost host;
            lock (Sync) Hosts.TryGetValue(hwnd, out host);
            return host != null ? host.WindowProc(hwnd, msg, wp, lp) : DefWindowProc(hwnd, msg, wp, lp);
        }

        public void Dispose()
        {
            if (_disposed) return;
            _disposed = true;
            if (_host != IntPtr.Zero)
            {
                lock (Sync) Hosts.Remove(_host);
                try { DestroyWindow(_host); } catch { }
            }
            _host = IntPtr.Zero;
        }
    }
}
