using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace DPopCleaner.SimpleUpdate
{
    internal sealed class ZapretEnhancementHost : IDisposable
    {
        internal const int RepairBroadcastButtonId = 1720;
        internal const int RepairConnectionButtonId = 1721;
        internal const int GameFilterButtonId = 1722;
        internal const int ManagerButtonId = 1723;
        internal const int LegacyCheckVersionButtonId = 1724;
        internal const int LegacyDownloadButtonId = 1725;

        private const int ToolbarButtonCount = 4;
        private const int ButtonGap = 8;
        private const int ToolbarHeight = 27;
        private const int ToolbarWidth = 709;
        private const string UpstreamStatusCommand = "status_zapret";
        private const uint WS_CHILD = 0x40000000;
        private const uint WS_VISIBLE = 0x10000000;
        private const uint WS_TABSTOP = 0x00010000;
        private const uint BS_PUSHBUTTON = 0x00000000;
        private const uint WM_COMMAND = 0x0111;
        private const uint WM_ERASEBKGND = 0x0014;
        private const uint WM_SETFONT = 0x0030;
        private const string HostClassName = "DPopCleanerZapretEnhancementHost";

        private static readonly object Sync = new object();
        private static readonly Dictionary<IntPtr, ZapretEnhancementHost> Hosts = new Dictionary<IntPtr, ZapretEnhancementHost>();
        private static readonly HostWndProc HostWndProcDelegate = StaticHostWndProc;
        private static readonly IntPtr BackgroundBrush = CreateSolidBrush(Rgb(12, 17, 23));
        private static bool _classRegistered;

        private readonly IntPtr _parent;
        private readonly string _applicationRoot;
        private IntPtr _actionToolbar;
        private IntPtr _updateToolbar;
        private IntPtr _legacyCheckVersionButton;
        private IntPtr _legacyDownloadButton;
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

        [DllImport("gdi32.dll")]
        private static extern IntPtr CreateSolidBrush(uint colorRef);

        [DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]
        private static extern int SetWindowTheme(IntPtr hwnd, string subAppName, string subIdList);

        internal ZapretEnhancementHost(IntPtr parent, string applicationRoot)
        {
            if (parent == IntPtr.Zero) throw new ArgumentException("Parent window is required.", "parent");
            _parent = parent;
            _applicationRoot = Path.GetFullPath(applicationRoot ?? string.Empty);
            EnsureHostClass();
            CreateToolbar();
            CreateLegacyUpdateProxy();
            RefreshDisplayedZapretVersion();
        }

        internal void Show()
        {
            if (_disposed) return;
            PositionActionToolbar();
            PositionUpdateToolbar();
            RefreshDisplayedZapretVersion();
            if (_legacyCheckVersionButton != IntPtr.Zero)
                NativeBridge.ShowWindow(_legacyCheckVersionButton, NativeBridge.SW_HIDE);
            if (_legacyDownloadButton != IntPtr.Zero)
                NativeBridge.ShowWindow(_legacyDownloadButton, NativeBridge.SW_HIDE);
            if (_actionToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_actionToolbar, NativeBridge.SW_SHOW);
            if (_updateToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_updateToolbar, NativeBridge.SW_SHOW);
        }

        internal void Hide()
        {
            if (_disposed) return;
            if (_actionToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_actionToolbar, NativeBridge.SW_HIDE);
            if (_updateToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_updateToolbar, NativeBridge.SW_HIDE);
        }

        private void CreateToolbar()
        {
            var fontAnchor = NativeBridge.FindChildById(_parent, NativeBridge.ZapretCheckVersionButtonId);
            var font = fontAnchor != IntPtr.Zero
                ? NativeBridge.SendMessage(fontAnchor, NativeBridge.WM_GETFONT, IntPtr.Zero, IntPtr.Zero)
                : IntPtr.Zero;

            _actionToolbar = CreateHost(ToolbarWidth, ToolbarHeight);

            // One safe row beside the frozen "Дополнительно" heading. Widths deliberately
            // preserve the full Russian labels while keeping all four actions inside the old page.
            var x = 0;
            CreateButton(_actionToolbar, "Починка трансляции", RepairBroadcastButtonId, x, 0, 165, ToolbarHeight, font);
            x += 165 + ButtonGap;
            CreateButton(_actionToolbar, "Починка подключения", RepairConnectionButtonId, x, 0, 175, ToolbarHeight, font);
            x += 175 + ButtonGap;
            CreateButton(_actionToolbar, "Игровой фильтр 1.10.2", GameFilterButtonId, x, 0, 185, ToolbarHeight, font);
            x += 185 + ButtonGap;
            CreateButton(_actionToolbar, "Менеджер 1.10.2", ManagerButtonId, x, 0, 160, ToolbarHeight, font);

            GC.KeepAlive(ToolbarButtonCount);
            PositionActionToolbar();
        }

        private void CreateLegacyUpdateProxy()
        {
            _legacyCheckVersionButton = NativeBridge.FindChildByText(_parent, "Проверить версию", "Button", true);
            _legacyDownloadButton = NativeBridge.FindChildByText(_parent, "Скачать и установить", "Button", true);
            if (_legacyCheckVersionButton == IntPtr.Zero || _legacyDownloadButton == IntPtr.Zero) return;

            var check = NativeBridge.GetChildClientBounds(_parent, _legacyCheckVersionButton);
            var download = NativeBridge.GetChildClientBounds(_parent, _legacyDownloadButton);
            if (check == null || download == null) return;

            var left = Math.Min(check.Left, download.Left);
            var top = Math.Min(check.Top, download.Top);
            var right = Math.Max(check.Right, download.Right);
            var bottom = Math.Max(check.Bottom, download.Bottom);
            var width = Math.Max(1, right - left);
            var height = Math.Max(1, bottom - top);
            var font = NativeBridge.SendMessage(_legacyCheckVersionButton, NativeBridge.WM_GETFONT, IntPtr.Zero, IntPtr.Zero);

            _updateToolbar = CreateHost(width, height);
            CreateButton(_updateToolbar, "Проверить версию", LegacyCheckVersionButtonId,
                check.Left - left, check.Top - top, check.Width, check.Height, font);
            CreateButton(_updateToolbar, "Скачать и установить", LegacyDownloadButtonId,
                download.Left - left, download.Top - top, download.Width, download.Height, font);

            NativeBridge.ShowWindow(_legacyCheckVersionButton, NativeBridge.SW_HIDE);
            NativeBridge.ShowWindow(_legacyDownloadButton, NativeBridge.SW_HIDE);
            PositionUpdateToolbar();
        }

        private IntPtr CreateHost(int width, int height)
        {
            var handle = CreateWindowEx(0, HostClassName, string.Empty, WS_CHILD | WS_VISIBLE,
                0, 0, width, height, _parent, IntPtr.Zero, GetModuleHandle(null), IntPtr.Zero);
            if (handle == IntPtr.Zero) throw new InvalidOperationException("Could not create Zapret bridge toolbar.");
            lock (Sync) Hosts[handle] = this;
            return handle;
        }

        private static void CreateButton(IntPtr host, string text, int id, int x, int y, int width, int height, IntPtr font)
        {
            var button = CreateWindowEx(0, "Button", text,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                x, y, width, height, host, new IntPtr(id), GetModuleHandle(null), IntPtr.Zero);
            if (button == IntPtr.Zero) throw new InvalidOperationException("Could not create Zapret button id=" + id + ".");
            if (font != IntPtr.Zero) NativeBridge.SendMessage(button, WM_SETFONT, font, new IntPtr(1));
            try { SetWindowTheme(button, "DarkMode_Explorer", null); } catch { }
        }

        private void PositionActionToolbar()
        {
            var additional = NativeBridge.GetChildClientBounds(
                _parent,
                NativeBridge.FindChildByText(_parent, "Дополнительно", "Static", true));
            var tests = NativeBridge.GetChildClientBounds(
                _parent,
                NativeBridge.FindChildByText(_parent, "Тесты", "Button", true));
            if (additional == null || tests == null) return;

            var right = tests.Right;
            var left = right - ToolbarWidth;
            var minimumLeft = additional.Right + 12;
            if (left < minimumLeft) left = minimumLeft;
            var top = Math.Max(0, additional.Top - 4);

            NativeBridge.PositionChildWindow(_actionToolbar, new NativeBridge.ClientBounds
            {
                Left = left,
                Top = top,
                Right = left + ToolbarWidth,
                Bottom = top + ToolbarHeight
            });
        }

        private void PositionUpdateToolbar()
        {
            if (_updateToolbar == IntPtr.Zero || _legacyCheckVersionButton == IntPtr.Zero || _legacyDownloadButton == IntPtr.Zero) return;
            var check = NativeBridge.GetChildClientBounds(_parent, _legacyCheckVersionButton);
            var download = NativeBridge.GetChildClientBounds(_parent, _legacyDownloadButton);
            if (check == null || download == null) return;
            var left = Math.Min(check.Left, download.Left);
            var top = Math.Min(check.Top, download.Top);
            var right = Math.Max(check.Right, download.Right);
            var bottom = Math.Max(check.Bottom, download.Bottom);
            NativeBridge.PositionChildWindow(_updateToolbar, new NativeBridge.ClientBounds
            {
                Left = left,
                Top = top,
                Right = right,
                Bottom = bottom
            });
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

        private IntPtr WindowProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam)
        {
            if (msg == WM_COMMAND)
            {
                var id = (int)(wParam.ToInt64() & 0xffff);
                try
                {
                    if (id == RepairBroadcastButtonId) RepairBroadcast();
                    else if (id == RepairConnectionButtonId) RepairConnection();
                    else if (id == GameFilterButtonId) CycleGameFilter();
                    else if (id == ManagerButtonId) OpenOfficialManager();
                    else if (id == LegacyCheckVersionButtonId || id == LegacyDownloadButtonId)
                    {
                        LegacyZapretUpdater.Run(_applicationRoot);
                        RefreshDisplayedZapretVersion();
                    }
                }
                catch (Exception ex)
                {
                    MessageBox.Show("Операция Zapret не выполнена.\r\n\r\n" + ex.Message,
                        "DPopCleaner — Zapret", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
                return IntPtr.Zero;
            }
            if (msg == WM_ERASEBKGND) return new IntPtr(1);
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        private void RepairBroadcast()
        {
            var module = Path.Combine(_applicationRoot, "Modules", "ZapretScreenFix.exe");
            if (!File.Exists(module)) throw new FileNotFoundException("ZapretScreenFix.exe не найден.", module);
            StartVisible(module, string.Empty, Path.GetDirectoryName(module));
        }

        private void RepairConnection()
        {
            var zapretRoot = GetZapretRoot();
            var service = Path.Combine(zapretRoot, "service.bat");
            var winws = Path.Combine(zapretRoot, "bin", "winws.exe");
            if (!File.Exists(service) || !File.Exists(winws))
                throw new InvalidOperationException("Bundled Zapret 1.10.2 повреждён или не установлен полностью.");

            var statusCommand = UpstreamStatusCommand;
            RunHidden("cmd.exe", "/d /c \"\"" + service + "\" load_user_lists\"", zapretRoot, 8000);
            RunHidden("netsh.exe", "interface tcp set global timestamps=enabled", zapretRoot, 8000);
            RunHidden("ipconfig.exe", "/flushdns", zapretRoot, 8000);
            GC.KeepAlive(statusCommand);
            MessageBox.Show("Подключение Zapret восстановлено: пользовательские списки проверены, TCP timestamps включены, DNS-кэш очищен.\r\n\r\nВнешние экземпляры winws не изменялись.",
                "DPopCleaner — Zapret", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        private void CycleGameFilter()
        {
            var zapretRoot = GetZapretRoot();
            var utils = Path.Combine(zapretRoot, "utils");
            Directory.CreateDirectory(utils);
            var flag = Path.Combine(utils, "game_filter.enabled");
            string current = null;
            if (File.Exists(flag)) current = File.ReadAllText(flag).Trim().ToLowerInvariant();

            string next;
            string label;
            if (string.IsNullOrEmpty(current)) { next = "all"; label = "TCP + UDP"; }
            else if (current == "all") { next = "tcp"; label = "только TCP"; }
            else if (current == "tcp") { next = "udp"; label = "только UDP"; }
            else { next = null; label = "выключен"; }

            if (next == null)
            {
                if (File.Exists(flag)) File.Delete(flag);
            }
            else
            {
                File.WriteAllText(flag, next + Environment.NewLine);
            }

            MessageBox.Show("Игровой фильтр Flowseal Zapret 1.10.2: " + label + ".\r\n\r\nИзменение применяется штатной логикой service.bat при следующем запуске/переустановке стратегии.",
                "DPopCleaner — Zapret", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        private void OpenOfficialManager()
        {
            var zapretRoot = GetZapretRoot();
            var service = Path.Combine(zapretRoot, "service.bat");
            if (!File.Exists(service)) throw new FileNotFoundException("service.bat Zapret 1.10.2 не найден.", service);
            StartVisible(service, string.Empty, zapretRoot);
        }

        private string GetZapretRoot()
        {
            return Path.Combine(_applicationRoot, "Zapret");
        }

        private static void StartVisible(string path, string arguments, string workingDirectory)
        {
            var info = new ProcessStartInfo(path)
            {
                UseShellExecute = true,
                Arguments = arguments ?? string.Empty,
                WorkingDirectory = workingDirectory ?? string.Empty
            };
            if (Process.Start(info) == null) throw new InvalidOperationException("Не удалось запустить: " + path);
        }

        private static void RunHidden(string file, string arguments, string workingDirectory, int timeoutMs)
        {
            var info = new ProcessStartInfo(file)
            {
                UseShellExecute = false,
                CreateNoWindow = true,
                WindowStyle = ProcessWindowStyle.Hidden,
                Arguments = arguments ?? string.Empty,
                WorkingDirectory = workingDirectory ?? string.Empty
            };
            using (var process = Process.Start(info))
            {
                if (process == null) throw new InvalidOperationException("Не удалось запустить: " + file);
                if (!process.WaitForExit(timeoutMs))
                {
                    try { process.Kill(); } catch { }
                    throw new TimeoutException("Команда восстановления не завершилась вовремя: " + file);
                }
                if (process.ExitCode != 0)
                    throw new InvalidOperationException("Команда восстановления завершилась с кодом " + process.ExitCode + ": " + file);
            }
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
                    if (error != 1410) throw new InvalidOperationException("Could not register Zapret bridge class. Win32=" + error);
                }
                _classRegistered = true;
            }
        }

        private static IntPtr StaticHostWndProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam)
        {
            ZapretEnhancementHost host;
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
            foreach (var handle in new[] { _actionToolbar, _updateToolbar })
            {
                if (handle == IntPtr.Zero) continue;
                lock (Sync) Hosts.Remove(handle);
                try { DestroyWindow(handle); } catch { }
            }
            _actionToolbar = IntPtr.Zero;
            _updateToolbar = IntPtr.Zero;
        }
    }
}
