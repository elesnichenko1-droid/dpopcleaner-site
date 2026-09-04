using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
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
        internal const int InstallServiceProxyButtonId = 1701;
        internal const int RemoveServicesProxyButtonId = 1702;
        internal const int StartStandaloneProxyButtonId = 1713;

        private const int ButtonGap = 8;
        private const int ToolbarHeight = 27;
        private const string UpstreamStatusCommand = "status_zapret";
        private const uint WS_CHILD = 0x40000000;
        private const uint WS_VISIBLE = 0x10000000;
        private const uint WS_TABSTOP = 0x00010000;
        private const uint BS_PUSHBUTTON = 0x00000000;
        private const uint WM_COMMAND = 0x0111;
        private const uint WM_ERASEBKGND = 0x0014;
        private const uint WM_SETFONT = 0x0030;
        private const uint CB_GETCOUNT = 0x0146;
        private const uint CB_GETCURSEL = 0x0147;
        private const uint CB_GETLBTEXT = 0x0148;
        private const uint CB_GETLBTEXTLEN = 0x0149;
        private const string HostClassName = "DPopCleanerZapretEnhancementHost";

        private static readonly object Sync = new object();
        private static readonly Dictionary<IntPtr, ZapretEnhancementHost> Hosts = new Dictionary<IntPtr, ZapretEnhancementHost>();
        private static readonly HostWndProc HostWndProcDelegate = StaticHostWndProc;
        private static readonly IntPtr DarkBackgroundBrush = CreateSolidBrush(Rgb(12, 17, 23));
        private static readonly IntPtr LightBackgroundBrush = CreateSolidBrush(Rgb(247, 248, 250));
        private static bool _classRegistered;

        private readonly IntPtr _parent;
        private readonly string _applicationRoot;
        private IntPtr _actionToolbar;
        private IntPtr _updateToolbar;
        private IntPtr _installServiceToolbar;
        private IntPtr _removeServicesToolbar;
        private IntPtr _startStandaloneToolbar;
        private IntPtr _repairBroadcastButton;
        private IntPtr _repairConnectionButton;
        private IntPtr _gameFilterButton;
        private IntPtr _managerButton;
        private IntPtr _legacyCheckVersionButton;
        private IntPtr _legacyDownloadButton;
        private IntPtr _legacyInstallServiceButton;
        private IntPtr _legacyRemoveServicesButton;
        private IntPtr _legacyStartStandaloneButton;
        private DateTime _nextRuntimeRefreshUtc = DateTime.MinValue;
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

        [StructLayout(LayoutKind.Sequential)]
        private struct RECT { public int Left, Top, Right, Bottom; }

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
        private static extern bool GetClientRect(IntPtr hwnd, out RECT rect);

        [DllImport("user32.dll")]
        private static extern int FillRect(IntPtr hdc, ref RECT rect, IntPtr brush);

        [DllImport("gdi32.dll")]
        private static extern IntPtr CreateSolidBrush(uint colorRef);

        [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")]
        private static extern IntPtr SendMessageBuffer(IntPtr hwnd, uint msg, IntPtr wParam, StringBuilder lParam);

        internal ZapretEnhancementHost(IntPtr parent, string applicationRoot)
        {
            if (parent == IntPtr.Zero) throw new ArgumentException("Parent window is required.", "parent");
            _parent = parent;
            _applicationRoot = Path.GetFullPath(applicationRoot ?? string.Empty);
            EnsureHostClass();
            CreateToolbar();
            CreateLegacyUpdateProxy();
            CreateInstallServiceProxy();
            CreateRemoveServicesProxy();
            CreateStartStandaloneProxy();
            RefreshDisplayedZapretVersion();
            RefreshRuntimeStatus();
        }

        internal void Show()
        {
            if (_disposed) return;
            // rev.17 has exactly one geometry owner: ZapretResponsiveLayoutHost. The proxy hosts
            // receive their initial legacy placement from the Create* methods, but Show() must not
            // copy hidden frozen-control coordinates every 100 ms. Doing so creates a second writer
            // for the same HWNDs and exposes transient legacy positions during resize/maximize.
            RefreshDisplayedZapretVersion();
            if (DateTime.UtcNow >= _nextRuntimeRefreshUtc) RefreshRuntimeStatus();
            if (_legacyCheckVersionButton != IntPtr.Zero)
                NativeBridge.ShowWindow(_legacyCheckVersionButton, NativeBridge.SW_HIDE);
            if (_legacyDownloadButton != IntPtr.Zero)
                NativeBridge.ShowWindow(_legacyDownloadButton, NativeBridge.SW_HIDE);
            if (_legacyInstallServiceButton != IntPtr.Zero)
                NativeBridge.ShowWindow(_legacyInstallServiceButton, NativeBridge.SW_HIDE);
            if (_legacyRemoveServicesButton != IntPtr.Zero)
                NativeBridge.ShowWindow(_legacyRemoveServicesButton, NativeBridge.SW_HIDE);
            if (_legacyStartStandaloneButton != IntPtr.Zero)
                NativeBridge.ShowWindow(_legacyStartStandaloneButton, NativeBridge.SW_HIDE);
            if (_actionToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_actionToolbar, NativeBridge.SW_SHOW);
            if (_updateToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_updateToolbar, NativeBridge.SW_SHOW);
            if (_installServiceToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_installServiceToolbar, NativeBridge.SW_SHOW);
            if (_removeServicesToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_removeServicesToolbar, NativeBridge.SW_SHOW);
            if (_startStandaloneToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_startStandaloneToolbar, NativeBridge.SW_SHOW);
        }

        internal void Hide()
        {
            if (_disposed) return;
            if (_actionToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_actionToolbar, NativeBridge.SW_HIDE);
            if (_updateToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_updateToolbar, NativeBridge.SW_HIDE);
            if (_installServiceToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_installServiceToolbar, NativeBridge.SW_HIDE);
            if (_removeServicesToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_removeServicesToolbar, NativeBridge.SW_HIDE);
            if (_startStandaloneToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_startStandaloneToolbar, NativeBridge.SW_HIDE);
        }

        private void CreateToolbar()
        {
            var fontAnchor = NativeBridge.FindChildById(_parent, NativeBridge.ZapretCheckVersionButtonId);
            var font = fontAnchor != IntPtr.Zero
                ? NativeBridge.SendMessage(fontAnchor, NativeBridge.WM_GETFONT, IntPtr.Zero, IntPtr.Zero)
                : IntPtr.Zero;

            _actionToolbar = CreateHost(1, ToolbarHeight);
            _repairBroadcastButton = CreateButton(_actionToolbar, "Починка трансляции", RepairBroadcastButtonId, 0, 0, 1, ToolbarHeight, font);
            _repairConnectionButton = CreateButton(_actionToolbar, "Починка подключения", RepairConnectionButtonId, 0, 0, 1, ToolbarHeight, font);
            _gameFilterButton = CreateButton(_actionToolbar, "Игровой фильтр 1.10.2", GameFilterButtonId, 0, 0, 1, ToolbarHeight, font);
            _managerButton = CreateButton(_actionToolbar, "Менеджер 1.10.2", ManagerButtonId, 0, 0, 1, ToolbarHeight, font);
            PositionActionToolbar();
        }

        private void CreateLegacyUpdateProxy()
        {
            _legacyCheckVersionButton = FindVisibleButtonByCaption("Проверить версию", "Check version");
            _legacyDownloadButton = FindVisibleButtonByCaption("Скачать и установить", "Download and install");
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
            CreateButton(_updateToolbar, NativeBridge.ReadWindowText(_legacyCheckVersionButton), LegacyCheckVersionButtonId,
                check.Left - left, check.Top - top, check.Width, check.Height, font);
            CreateButton(_updateToolbar, NativeBridge.ReadWindowText(_legacyDownloadButton), LegacyDownloadButtonId,
                download.Left - left, download.Top - top, download.Width, download.Height, font);

            NativeBridge.ShowWindow(_legacyCheckVersionButton, NativeBridge.SW_HIDE);
            NativeBridge.ShowWindow(_legacyDownloadButton, NativeBridge.SW_HIDE);
            PositionUpdateToolbar();
        }

        private void CreateInstallServiceProxy()
        {
            _legacyInstallServiceButton = NativeBridge.FindChildById(_parent, InstallServiceProxyButtonId);
            if (_legacyInstallServiceButton == IntPtr.Zero) return;
            var bounds = NativeBridge.GetChildClientBounds(_parent, _legacyInstallServiceButton);
            if (bounds == null) return;

            var font = NativeBridge.SendMessage(_legacyInstallServiceButton, NativeBridge.WM_GETFONT, IntPtr.Zero, IntPtr.Zero);
            var caption = NativeBridge.ReadWindowText(_legacyInstallServiceButton);
            _installServiceToolbar = CreateHost(bounds.Width, bounds.Height);
            CreateButton(_installServiceToolbar, caption, InstallServiceProxyButtonId, 0, 0, bounds.Width, bounds.Height, font);
            NativeBridge.ShowWindow(_legacyInstallServiceButton, NativeBridge.SW_HIDE);
            PositionInstallServiceProxy();
        }

        private void PositionInstallServiceProxy()
        {
            if (_installServiceToolbar == IntPtr.Zero || _legacyInstallServiceButton == IntPtr.Zero) return;
            var bounds = NativeBridge.GetChildClientBounds(_parent, _legacyInstallServiceButton);
            if (bounds != null) NativeBridge.PositionChildWindow(_installServiceToolbar, bounds);
        }

        private void CreateRemoveServicesProxy()
        {
            _legacyRemoveServicesButton = NativeBridge.FindChildById(_parent, RemoveServicesProxyButtonId);
            if (_legacyRemoveServicesButton == IntPtr.Zero) return;
            var bounds = NativeBridge.GetChildClientBounds(_parent, _legacyRemoveServicesButton);
            if (bounds == null) return;

            var font = NativeBridge.SendMessage(_legacyRemoveServicesButton, NativeBridge.WM_GETFONT, IntPtr.Zero, IntPtr.Zero);
            var caption = NativeBridge.ReadWindowText(_legacyRemoveServicesButton);
            _removeServicesToolbar = CreateHost(bounds.Width, bounds.Height);
            CreateButton(_removeServicesToolbar, caption, RemoveServicesProxyButtonId, 0, 0, bounds.Width, bounds.Height, font);
            NativeBridge.ShowWindow(_legacyRemoveServicesButton, NativeBridge.SW_HIDE);
            PositionRemoveServicesProxy();
        }

        private void PositionRemoveServicesProxy()
        {
            if (_removeServicesToolbar == IntPtr.Zero || _legacyRemoveServicesButton == IntPtr.Zero) return;
            var bounds = NativeBridge.GetChildClientBounds(_parent, _legacyRemoveServicesButton);
            if (bounds != null) NativeBridge.PositionChildWindow(_removeServicesToolbar, bounds);
        }

        private void CreateStartStandaloneProxy()
        {
            _legacyStartStandaloneButton = NativeBridge.FindChildById(_parent, StartStandaloneProxyButtonId);
            if (_legacyStartStandaloneButton == IntPtr.Zero) return;
            var bounds = NativeBridge.GetChildClientBounds(_parent, _legacyStartStandaloneButton);
            if (bounds == null) return;

            var font = NativeBridge.SendMessage(_legacyStartStandaloneButton, NativeBridge.WM_GETFONT, IntPtr.Zero, IntPtr.Zero);
            var caption = NativeBridge.ReadWindowText(_legacyStartStandaloneButton);
            _startStandaloneToolbar = CreateHost(bounds.Width, bounds.Height);
            CreateButton(_startStandaloneToolbar, caption, StartStandaloneProxyButtonId, 0, 0, bounds.Width, bounds.Height, font);
            NativeBridge.ShowWindow(_legacyStartStandaloneButton, NativeBridge.SW_HIDE);
            PositionStartStandaloneProxy();
        }

        private void PositionStartStandaloneProxy()
        {
            if (_startStandaloneToolbar == IntPtr.Zero || _legacyStartStandaloneButton == IntPtr.Zero) return;
            var bounds = NativeBridge.GetChildClientBounds(_parent, _legacyStartStandaloneButton);
            if (bounds != null) NativeBridge.PositionChildWindow(_startStandaloneToolbar, bounds);
        }

        private string ReadSelectedStrategy()
        {
            NativeBridge.ChildInfo strategyCombo = null;
            var bestStrategyCount = 0;
            foreach (var child in NativeBridge.GetChildren(_parent))
            {
                if (!child.Visible || !string.Equals(child.ClassName, "ComboBox", StringComparison.OrdinalIgnoreCase)) continue;
                var count = NativeBridge.SendMessage(child.Handle, CB_GETCOUNT, IntPtr.Zero, IntPtr.Zero).ToInt32();
                if (count <= 0) continue;

                var strategyCount = 0;
                for (var i = 0; i < count; i++)
                {
                    if (IsStrategyFileName(ReadComboItem(child.Handle, i))) strategyCount++;
                }
                if (strategyCount <= bestStrategyCount) continue;
                strategyCombo = child;
                bestStrategyCount = strategyCount;
            }
            if (strategyCombo == null || bestStrategyCount < 2) return string.Empty;

            var index = NativeBridge.SendMessage(strategyCombo.Handle, CB_GETCURSEL, IntPtr.Zero, IntPtr.Zero).ToInt32();
            if (index < 0) return string.Empty;
            var selected = ReadComboItem(strategyCombo.Handle, index);
            return IsStrategyFileName(selected) ? selected : string.Empty;
        }

        private static string ReadComboItem(IntPtr combo, int index)
        {
            var length = NativeBridge.SendMessage(combo, CB_GETLBTEXTLEN, new IntPtr(index), IntPtr.Zero).ToInt32();
            if (length < 0) return string.Empty;
            var value = new StringBuilder(length + 1);
            SendMessageBuffer(combo, CB_GETLBTEXT, new IntPtr(index), value);
            return value.ToString().Trim();
        }

        private static bool IsStrategyFileName(string value)
        {
            return !string.IsNullOrWhiteSpace(value) &&
                   string.Equals(Path.GetFileName(value), value, StringComparison.Ordinal) &&
                   value.StartsWith("general", StringComparison.OrdinalIgnoreCase) &&
                   value.EndsWith(".bat", StringComparison.OrdinalIgnoreCase);
        }

        private void StartSelectedStrategyUsingUpstreamBatch()
        {
            var selected = ReadSelectedStrategy();
            if (string.IsNullOrWhiteSpace(selected)) throw new InvalidOperationException("Сначала выберите стратегию Zapret.");
            if (!string.Equals(Path.GetFileName(selected), selected, StringComparison.Ordinal) ||
                !selected.StartsWith("general", StringComparison.OrdinalIgnoreCase) ||
                !selected.EndsWith(".bat", StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("Некорректное имя стратегии Zapret: " + selected);

            var zapretRoot = Path.GetFullPath(GetZapretRoot());
            var strategy = Path.GetFullPath(Path.Combine(zapretRoot, selected));
            if (!string.Equals(Path.GetDirectoryName(strategy), zapretRoot, StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("Стратегия находится вне bundled Zapret: " + selected);
            if (!File.Exists(strategy)) throw new FileNotFoundException("Выбранная стратегия Zapret не найдена.", strategy);

            var state = ZapretRuntimeState.Read(_applicationRoot);
            if (state.ZapretServiceRunning)
                throw new InvalidOperationException("Служба zapret уже запущена. Сначала остановите или удалите службу перед standalone-запуском.");
            if (state.BundledWinwsRunning)
            {
                RefreshRuntimeStatus();
                return;
            }

            var info = new ProcessStartInfo("cmd.exe")
            {
                UseShellExecute = false,
                CreateNoWindow = true,
                Arguments = "/d /c \"\"" + strategy + "\"\"",
                WorkingDirectory = zapretRoot
            };

            using (var process = Process.Start(info))
            {
                if (process == null) throw new InvalidOperationException("Не удалось запустить выбранную стратегию Zapret: " + selected);
                try
                {
                    var deadline = DateTime.UtcNow.AddSeconds(10);
                    while (DateTime.UtcNow < deadline)
                    {
                        state = ZapretRuntimeState.Read(_applicationRoot);
                        if (state.BundledWinwsRunning)
                        {
                            RefreshRuntimeStatus();
                            return;
                        }
                        if (process.HasExited)
                            throw new InvalidOperationException("Стратегия Zapret завершилась до запуска bundled winws.exe. Стратегия: " + selected + ". Код выхода: " + process.ExitCode);
                        Thread.Sleep(200);
                    }
                    throw new TimeoutException("Стратегия Zapret не запустила bundled winws.exe вовремя: " + selected);
                }
                finally
                {
                    try { if (!process.HasExited) process.Kill(); } catch { }
                }
            }
        }

        private void InstallSelectedStrategyUsingUpstreamManager()
        {
            var selected = ReadSelectedStrategy();
            if (string.IsNullOrWhiteSpace(selected)) throw new InvalidOperationException("Сначала выберите стратегию Zapret.");

            var zapretRoot = GetZapretRoot();
            var service = Path.Combine(zapretRoot, "service.bat");
            if (!File.Exists(service)) throw new FileNotFoundException("service.bat Zapret 1.10.2 не найден.", service);
            var menuIndex = FindStrategyMenuIndex(zapretRoot, selected);
            if (menuIndex <= 0) throw new InvalidOperationException("Выбранная стратегия отсутствует в upstream manager: " + selected);

            var directManager = BuildDirectUpstreamInstallManager(service, menuIndex);
            try
            {
                var info = new ProcessStartInfo("cmd.exe")
                {
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    Arguments = "/d /c \"\"" + directManager + "\" admin\"",
                    WorkingDirectory = zapretRoot
                };

                using (var process = Process.Start(info))
                {
                    if (process == null) throw new InvalidOperationException("Не удалось запустить upstream service.bat.");
                    try
                    {
                        var deadline = DateTime.UtcNow.AddSeconds(30);
                        while (DateTime.UtcNow < deadline)
                        {
                            var state = ZapretRuntimeState.Read(_applicationRoot);
                            if (state.ZapretServiceRunning)
                            {
                                RefreshRuntimeStatus();
                                return;
                            }
                            if (process.HasExited)
                                throw new InvalidOperationException("Upstream service.bat завершился до запуска службы. Стратегия: " + selected + ". Код выхода: " + process.ExitCode);
                            Thread.Sleep(200);
                        }
                        throw new TimeoutException("Upstream service.bat не запустил службу zapret. Стратегия: " + selected + ".");
                    }
                    finally
                    {
                        try { if (!process.HasExited) process.Kill(); } catch { }
                    }
                }
            }
            finally
            {
                try { if (File.Exists(directManager)) File.Delete(directManager); } catch { }
            }
        }

        private static string BuildDirectUpstreamInstallManager(string service, int menuIndex)
        {
            var source = File.ReadAllText(service);
            const string rootPrompt = "set /p menu_choice=   Select option (0-12): ";
            const string strategyPrompt = "set /p \"choice=Input option (0-!count!, default: 0): \"";
            var rootFirst = source.IndexOf(rootPrompt, StringComparison.Ordinal);
            var strategyFirst = source.IndexOf(strategyPrompt, StringComparison.Ordinal);
            if (rootFirst < 0 || source.IndexOf(rootPrompt, rootFirst + rootPrompt.Length, StringComparison.Ordinal) >= 0)
                throw new InvalidDataException("Flowseal root menu prompt is missing or ambiguous.");
            if (strategyFirst < 0 || source.IndexOf(strategyPrompt, strategyFirst + strategyPrompt.Length, StringComparison.Ordinal) >= 0)
                throw new InvalidDataException("Flowseal strategy prompt is missing or ambiguous.");

            var newline = source.IndexOf("\r\n", StringComparison.Ordinal) >= 0 ? "\r\n" : "\n";
            var rootReplacement = "if defined DPOP_INSTALL_ONCE exit /b" + newline
                + "set \"DPOP_INSTALL_ONCE=1\"" + newline
                + "set \"menu_choice=1\"";
            var strategyReplacement = "set \"choice=" + menuIndex.ToString() + "\"";
            var modified = source.Replace(rootPrompt, rootReplacement).Replace(strategyPrompt, strategyReplacement);
            var directory = Path.GetDirectoryName(service);
            if (string.IsNullOrWhiteSpace(directory)) throw new InvalidDataException("Flowseal service manager directory is invalid.");

            var directManager = Path.Combine(directory, "service-dpop-install-" + Guid.NewGuid().ToString("N") + ".bat");
            File.WriteAllText(directManager, modified, new UTF8Encoding(false));
            return directManager;
        }

        private void RemoveUsingUpstreamManager()
        {
            var zapretRoot = GetZapretRoot();
            var service = Path.Combine(zapretRoot, "service.bat");
            if (!File.Exists(service)) throw new FileNotFoundException("service.bat Zapret 1.10.2 не найден.", service);

            var removeManager = BuildDirectUpstreamRemoveManager(service);
            try
            {
                var info = new ProcessStartInfo("cmd.exe")
                {
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    Arguments = "/d /c \"\"" + removeManager + "\" admin\"",
                    WorkingDirectory = zapretRoot
                };

                using (var process = Process.Start(info))
                {
                    if (process == null) throw new InvalidOperationException("Не удалось запустить upstream удаление service.bat.");
                    try
                    {
                        var deadline = DateTime.UtcNow.AddSeconds(15);
                        while (DateTime.UtcNow < deadline)
                        {
                            var state = ZapretRuntimeState.Read(_applicationRoot);
                            if (!state.ZapretServiceExists && !state.BundledWinwsRunning &&
                                !state.WinDivertServiceExists && !state.WinDivert14ServiceExists)
                            {
                                RefreshRuntimeStatus();
                                return;
                            }
                            if (process.HasExited)
                                throw new InvalidOperationException("Upstream service.bat завершился до полного удаления Zapret/WinDivert. Код выхода: " + process.ExitCode);
                            Thread.Sleep(200);
                        }
                        throw new TimeoutException("Upstream service.bat не завершил удаление zapret/WinDivert вовремя.");
                    }
                    finally
                    {
                        try { if (!process.HasExited) process.Kill(); } catch { }
                    }
                }
            }
            finally
            {
                try { if (File.Exists(removeManager)) File.Delete(removeManager); } catch { }
            }
        }

        private static string BuildDirectUpstreamRemoveManager(string service)
        {
            var source = File.ReadAllText(service);
            const string rootPrompt = "set /p menu_choice=   Select option (0-12): ";
            var rootFirst = source.IndexOf(rootPrompt, StringComparison.Ordinal);
            if (rootFirst < 0 || source.IndexOf(rootPrompt, rootFirst + rootPrompt.Length, StringComparison.Ordinal) >= 0)
                throw new InvalidDataException("Flowseal root menu prompt is missing or ambiguous.");

            var newline = source.IndexOf("\r\n", StringComparison.Ordinal) >= 0 ? "\r\n" : "\n";
            var rootReplacement = "if defined DPOP_REMOVE_ONCE exit /b" + newline
                + "set \"DPOP_REMOVE_ONCE=1\"" + newline
                + "set \"menu_choice=2\"";
            var modified = source.Replace(rootPrompt, rootReplacement);
            var directory = Path.GetDirectoryName(service);
            if (string.IsNullOrWhiteSpace(directory)) throw new InvalidDataException("Flowseal service manager directory is invalid.");

            var removeManager = Path.Combine(directory, "service-dpop-remove-" + Guid.NewGuid().ToString("N") + ".bat");
            File.WriteAllText(removeManager, modified, new UTF8Encoding(false));
            return removeManager;
        }

        private static int FindStrategyMenuIndex(string zapretRoot, string selected)
        {
            var desired = selected.EndsWith(".bat", StringComparison.OrdinalIgnoreCase) ? selected : selected + ".bat";
            var strategies = new List<string>();
            foreach (var file in Directory.GetFiles(zapretRoot, "*.bat", SearchOption.TopDirectoryOnly))
            {
                var name = Path.GetFileName(file);
                if (name.StartsWith("service", StringComparison.OrdinalIgnoreCase)) continue;
                strategies.Add(name);
            }
            strategies.Sort(delegate(string left, string right)
            {
                return StringComparer.OrdinalIgnoreCase.Compare(StrategySortKey(left), StrategySortKey(right));
            });
            for (var i = 0; i < strategies.Count; i++)
                if (string.Equals(strategies[i], desired, StringComparison.OrdinalIgnoreCase)) return i + 1;
            return -1;
        }

        private static string StrategySortKey(string value)
        {
            return Regex.Replace(value ?? string.Empty, "(\\d+)", delegate(Match match)
            {
                return match.Value.PadLeft(8, '0');
            });
        }

        private IntPtr CreateHost(int width, int height)
        {
            var handle = CreateWindowEx(0, HostClassName, string.Empty, WS_CHILD | WS_VISIBLE,
                0, 0, Math.Max(1, width), Math.Max(1, height), _parent, IntPtr.Zero, GetModuleHandle(null), IntPtr.Zero);
            if (handle == IntPtr.Zero) throw new InvalidOperationException("Could not create Zapret bridge toolbar.");
            lock (Sync) Hosts[handle] = this;
            return handle;
        }

        private static IntPtr CreateButton(IntPtr host, string text, int id, int x, int y, int width, int height, IntPtr font)
        {
            var button = CreateWindowEx(0, "Button", text,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                x, y, Math.Max(1, width), Math.Max(1, height), host, new IntPtr(id), GetModuleHandle(null), IntPtr.Zero);
            if (button == IntPtr.Zero) throw new InvalidOperationException("Could not create Zapret button id=" + id + ".");
            if (font != IntPtr.Zero) NativeBridge.SendMessage(button, WM_SETFONT, font, new IntPtr(1));
            return button;
        }

        private void PositionActionToolbar()
        {
            var additionalHandle = FindVisibleControlByCaption("Дополнительно", "Additional", "Static");
            var testsHandle = FindVisibleControlByCaption("Тесты", "Tests", "Button");
            var additional = NativeBridge.GetChildClientBounds(_parent, additionalHandle);
            var tests = NativeBridge.GetChildClientBounds(_parent, testsHandle);
            if (additional == null || tests == null || _actionToolbar == IntPtr.Zero) return;

            var right = tests.Right;
            var minimumLeft = additional.Right + 12;
            var availableWidth = Math.Max(1, right - minimumLeft);
            var usedWidth = LayoutActionButtons(availableWidth);
            if (usedWidth <= 0) return;
            var left = Math.Max(minimumLeft, right - usedWidth);
            var top = Math.Max(0, additional.Top - 4);

            NativeBridge.PositionChildWindow(_actionToolbar, new NativeBridge.ClientBounds
            {
                Left = left,
                Top = top,
                Right = Math.Min(right, left + usedWidth),
                Bottom = top + ToolbarHeight
            });
        }

        private int LayoutActionButtons(int availableWidth)
        {
            var buttons = new[] { _repairBroadcastButton, _repairConnectionButton, _gameFilterButton, _managerButton };
            if (availableWidth <= ButtonGap * (buttons.Length - 1)) return 0;

            var desired = new int[buttons.Length];
            var desiredTotal = 0;
            for (var i = 0; i < buttons.Length; i++)
            {
                var caption = NativeBridge.ReadWindowText(buttons[i]);
                var measured = TextRenderer.MeasureText(caption ?? string.Empty, SystemFonts.MessageBoxFont).Width;
                desired[i] = Math.Max(72, measured + 24);
                desiredTotal += desired[i];
            }

            var gaps = ButtonGap * (buttons.Length - 1);
            var contentWidth = Math.Max(buttons.Length, availableWidth - gaps);
            var widths = new int[buttons.Length];
            if (desiredTotal <= contentWidth)
            {
                var extra = contentWidth - desiredTotal;
                for (var i = 0; i < buttons.Length; i++)
                {
                    var share = extra / (buttons.Length - i);
                    widths[i] = desired[i] + share;
                    extra -= share;
                }
            }
            else
            {
                var remaining = contentWidth;
                var remainingDesired = desiredTotal;
                for (var i = 0; i < buttons.Length; i++)
                {
                    var slotsLeft = buttons.Length - i;
                    var width = i == buttons.Length - 1
                        ? remaining
                        : Math.Max(1, (int)Math.Floor((double)remaining * desired[i] / Math.Max(1, remainingDesired)));
                    width = Math.Min(width, remaining - Math.Max(0, slotsLeft - 1));
                    widths[i] = Math.Max(1, width);
                    remaining -= widths[i];
                    remainingDesired -= desired[i];
                }
            }

            var x = 0;
            for (var i = 0; i < buttons.Length; i++)
            {
                NativeBridge.PositionChildWindow(buttons[i], new NativeBridge.ClientBounds
                {
                    Left = x,
                    Top = 0,
                    Right = x + widths[i],
                    Bottom = ToolbarHeight
                });
                x += widths[i];
                if (i + 1 < buttons.Length) x += ButtonGap;
            }
            return Math.Min(availableWidth, x);
        }

        private IntPtr FindVisibleControlByCaption(string russian, string english, string className)
        {
            var handle = NativeBridge.FindChildByText(_parent, russian, className, true);
            return handle != IntPtr.Zero ? handle : NativeBridge.FindChildByText(_parent, english, className, true);
        }

        private IntPtr FindVisibleButtonByCaption(string russian, string english)
        {
            return FindVisibleControlByCaption(russian, english, "Button");
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
                    if (id == StartStandaloneProxyButtonId) StartSelectedStrategyUsingUpstreamBatch();
                    else if (id == InstallServiceProxyButtonId) InstallSelectedStrategyUsingUpstreamManager();
                    else if (id == RemoveServicesProxyButtonId) RemoveUsingUpstreamManager();
                    else if (id == RepairBroadcastButtonId) RepairBroadcast();
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
            if (msg == WM_ERASEBKGND)
            {
                RECT rect;
                if (GetClientRect(hwnd, out rect))
                {
                    var brush = NativeBridge.IsDarkThemeSelected(_parent) ? DarkBackgroundBrush : LightBackgroundBrush;
                    FillRect(wParam, ref rect, brush);
                }
                return new IntPtr(1);
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        internal void RefreshRuntimeStatus()
        {
            if (_disposed) return;
            _nextRuntimeRefreshUtc = DateTime.UtcNow.AddSeconds(1);
            var state = ZapretRuntimeState.Read(_applicationRoot);
            NativeBridge.ChildInfo upper = null;
            foreach (var child in NativeBridge.GetChildren(_parent))
            {
                if (!child.Visible || !string.Equals(child.ClassName, "Edit", StringComparison.OrdinalIgnoreCase)) continue;
                if (upper == null || child.Top < upper.Top) upper = child;
            }
            if (upper == null) return;

            var on = state.ZapretServiceRunning || state.BundledWinwsRunning;
            var service = state.ZapretServiceRunning ? "RUNNING" : (state.ZapretServiceExists ? "STOPPED" : "OFF");
            var winws = state.BundledWinwsRunning ? "ON" : "OFF";
            NativeBridge.WriteWindowText(upper.Handle,
                "Zapret: " + (on ? "ON" : "OFF") + "  •  service: " + service + "  •  winws.exe: " + winws);
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
                    hbrBackground = IntPtr.Zero,
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
            if (_legacyInstallServiceButton != IntPtr.Zero)
            {
                try { NativeBridge.ShowWindow(_legacyInstallServiceButton, NativeBridge.SW_SHOW); } catch { }
            }
            if (_legacyRemoveServicesButton != IntPtr.Zero)
            {
                try { NativeBridge.ShowWindow(_legacyRemoveServicesButton, NativeBridge.SW_SHOW); } catch { }
            }
            if (_legacyStartStandaloneButton != IntPtr.Zero)
            {
                try { NativeBridge.ShowWindow(_legacyStartStandaloneButton, NativeBridge.SW_SHOW); } catch { }
            }
            foreach (var handle in new[] { _actionToolbar, _updateToolbar, _installServiceToolbar, _removeServicesToolbar, _startStandaloneToolbar })
            {
                if (handle == IntPtr.Zero) continue;
                lock (Sync) Hosts.Remove(handle);
                try { DestroyWindow(handle); } catch { }
            }
            _actionToolbar = IntPtr.Zero;
            _updateToolbar = IntPtr.Zero;
            _installServiceToolbar = IntPtr.Zero;
            _removeServicesToolbar = IntPtr.Zero;
            _startStandaloneToolbar = IntPtr.Zero;
        }
    }
}