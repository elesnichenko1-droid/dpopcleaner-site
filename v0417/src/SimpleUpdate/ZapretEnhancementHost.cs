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
        private IntPtr _updateRow;
        private IntPtr _toolsRow;
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
            CreateRows();
        }

        internal void Show()
        {
            if (_disposed) return;
            PositionRows();
            NativeBridge.ShowWindow(_updateRow, NativeBridge.SW_SHOW);
            NativeBridge.ShowWindow(_toolsRow, NativeBridge.SW_SHOW);
        }

        internal void Hide()
        {
            if (_disposed) return;
            NativeBridge.ShowWindow(_updateRow, NativeBridge.SW_HIDE);
            NativeBridge.ShowWindow(_toolsRow, NativeBridge.SW_HIDE);
        }

        private void CreateRows()
        {
            var updateAnchor = NativeBridge.FindChildById(_parent, NativeBridge.ZapretCheckVersionButtonId);
            var toolsAnchor = NativeBridge.FindChildById(_parent, NativeBridge.ZapretApplyButtonId);
            var font = updateAnchor != IntPtr.Zero
                ? NativeBridge.SendMessage(updateAnchor, NativeBridge.WM_GETFONT, IntPtr.Zero, IntPtr.Zero)
                : IntPtr.Zero;

            _updateRow = CreateHost();
            _toolsRow = CreateHost();
            CreateButton(_updateRow, "Починка трансляции", RepairBroadcastButtonId, 0, 0, 168, 27, font);
            CreateButton(_updateRow, "Починка подключения", RepairConnectionButtonId, 176, 0, 177, 27, font);
            CreateButton(_toolsRow, "Игровой фильтр 1.10.2", GameFilterButtonId, 0, 0, 185, 27, font);
            CreateButton(_toolsRow, "Менеджер 1.10.2", ManagerButtonId, 193, 0, 160, 27, font);
            PositionRows();
        }

        private IntPtr CreateHost()
        {
            var handle = CreateWindowEx(0, HostClassName, string.Empty, WS_CHILD | WS_VISIBLE,
                0, 0, 353, 27, _parent, IntPtr.Zero, GetModuleHandle(null), IntPtr.Zero);
            if (handle == IntPtr.Zero) throw new InvalidOperationException("Could not create Zapret enhancement row.");
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

        private void PositionRows()
        {
            var update = NativeBridge.GetChildClientBounds(_parent, NativeBridge.FindChildById(_parent, NativeBridge.ZapretCheckVersionButtonId));
            var tools = NativeBridge.GetChildClientBounds(_parent, NativeBridge.FindChildById(_parent, NativeBridge.ZapretApplyButtonId));
            if (update != null)
            {
                NativeBridge.PositionChildWindow(_updateRow, new NativeBridge.ClientBounds
                {
                    Left = 245,
                    Top = Math.Max(0, update.Top - 31),
                    Right = 598,
                    Bottom = Math.Max(0, update.Top - 31) + 27
                });
            }
            if (tools != null)
            {
                NativeBridge.PositionChildWindow(_toolsRow, new NativeBridge.ClientBounds
                {
                    Left = 245,
                    Top = Math.Max(0, tools.Top - 31),
                    Right = 598,
                    Bottom = Math.Max(0, tools.Top - 31) + 27
                });
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

            // Upstream Flowseal 1.10.2 exposes status_zapret; connection repair keeps the same
            // safe TCP prerequisite but avoids touching unrelated/external winws processes.
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
            foreach (var handle in new[] { _updateRow, _toolsRow })
            {
                if (handle == IntPtr.Zero) continue;
                lock (Sync) Hosts.Remove(handle);
                try { DestroyWindow(handle); } catch { }
            }
            _updateRow = IntPtr.Zero;
            _toolsRow = IntPtr.Zero;
        }
    }
}
