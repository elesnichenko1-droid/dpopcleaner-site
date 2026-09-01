from pathlib import Path

path = Path('v0417/src/SimpleUpdate/ZapretEnhancementHost.cs')
text = path.read_text(encoding='utf-8').replace('\r\n', '\n')


def rep(old: str, new: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f'expected exactly one patch anchor, found {count}: {old[:120]!r}')
    text = text.replace(old, new, 1)


rep(
    'using System.Runtime.InteropServices;\nusing System.Windows.Forms;',
    'using System.Runtime.InteropServices;\nusing System.Text;\nusing System.Text.RegularExpressions;\nusing System.Threading;\nusing System.Windows.Forms;'
)

rep(
    '        internal const int LegacyDownloadButtonId = 1725;\n',
    '        internal const int LegacyDownloadButtonId = 1725;\n'
    '        internal const int InstallServiceProxyButtonId = 1701;\n'
)

rep(
    '        private const uint WM_SETFONT = 0x0030;\n',
    '        private const uint WM_SETFONT = 0x0030;\n'
    '        private const uint CB_GETCURSEL = 0x0147;\n'
    '        private const uint CB_GETLBTEXT = 0x0148;\n'
    '        private const uint CB_GETLBTEXTLEN = 0x0149;\n'
)

rep(
    '        private IntPtr _actionToolbar;\n'
    '        private IntPtr _updateToolbar;\n'
    '        private IntPtr _legacyCheckVersionButton;\n'
    '        private IntPtr _legacyDownloadButton;\n'
    '        private bool _disposed;\n',
    '        private IntPtr _actionToolbar;\n'
    '        private IntPtr _updateToolbar;\n'
    '        private IntPtr _installServiceToolbar;\n'
    '        private IntPtr _legacyCheckVersionButton;\n'
    '        private IntPtr _legacyDownloadButton;\n'
    '        private IntPtr _legacyInstallServiceButton;\n'
    '        private DateTime _nextRuntimeRefreshUtc = DateTime.MinValue;\n'
    '        private bool _disposed;\n'
)

rep(
    '        [DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]\n'
    '        private static extern int SetWindowTheme(IntPtr hwnd, string subAppName, string subIdList);\n',
    '        [DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]\n'
    '        private static extern int SetWindowTheme(IntPtr hwnd, string subAppName, string subIdList);\n\n'
    '        [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")]\n'
    '        private static extern IntPtr SendMessageBuffer(IntPtr hwnd, uint msg, IntPtr wParam, StringBuilder lParam);\n'
)

rep(
    '            CreateToolbar();\n'
    '            CreateLegacyUpdateProxy();\n'
    '            RefreshDisplayedZapretVersion();\n',
    '            CreateToolbar();\n'
    '            CreateLegacyUpdateProxy();\n'
    '            CreateInstallServiceProxy();\n'
    '            RefreshDisplayedZapretVersion();\n'
    '            RefreshRuntimeStatus();\n'
)

rep(
    '            PositionActionToolbar();\n'
    '            PositionUpdateToolbar();\n'
    '            RefreshDisplayedZapretVersion();\n',
    '            PositionActionToolbar();\n'
    '            PositionUpdateToolbar();\n'
    '            PositionInstallServiceProxy();\n'
    '            RefreshDisplayedZapretVersion();\n'
    '            if (DateTime.UtcNow >= _nextRuntimeRefreshUtc) RefreshRuntimeStatus();\n'
)

rep(
    '            if (_legacyDownloadButton != IntPtr.Zero)\n'
    '                NativeBridge.ShowWindow(_legacyDownloadButton, NativeBridge.SW_HIDE);\n'
    '            if (_actionToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_actionToolbar, NativeBridge.SW_SHOW);\n'
    '            if (_updateToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_updateToolbar, NativeBridge.SW_SHOW);\n',
    '            if (_legacyDownloadButton != IntPtr.Zero)\n'
    '                NativeBridge.ShowWindow(_legacyDownloadButton, NativeBridge.SW_HIDE);\n'
    '            if (_legacyInstallServiceButton != IntPtr.Zero)\n'
    '                NativeBridge.ShowWindow(_legacyInstallServiceButton, NativeBridge.SW_HIDE);\n'
    '            if (_actionToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_actionToolbar, NativeBridge.SW_SHOW);\n'
    '            if (_updateToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_updateToolbar, NativeBridge.SW_SHOW);\n'
    '            if (_installServiceToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_installServiceToolbar, NativeBridge.SW_SHOW);\n'
)

rep(
    '            if (_actionToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_actionToolbar, NativeBridge.SW_HIDE);\n'
    '            if (_updateToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_updateToolbar, NativeBridge.SW_HIDE);\n',
    '            if (_actionToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_actionToolbar, NativeBridge.SW_HIDE);\n'
    '            if (_updateToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_updateToolbar, NativeBridge.SW_HIDE);\n'
    '            if (_installServiceToolbar != IntPtr.Zero) NativeBridge.ShowWindow(_installServiceToolbar, NativeBridge.SW_HIDE);\n'
)

install_methods = r'''        private void CreateInstallServiceProxy()
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

        private string ReadSelectedStrategy()
        {
            NativeBridge.ChildInfo best = null;
            foreach (var child in NativeBridge.GetChildren(_parent))
            {
                if (!child.Visible || !string.Equals(child.ClassName, "ComboBox", StringComparison.OrdinalIgnoreCase)) continue;
                if (best == null || child.Top < best.Top) best = child;
            }
            if (best == null) return string.Empty;

            var index = NativeBridge.SendMessage(best.Handle, CB_GETCURSEL, IntPtr.Zero, IntPtr.Zero).ToInt32();
            if (index < 0) return string.Empty;
            var length = NativeBridge.SendMessage(best.Handle, CB_GETLBTEXTLEN, new IntPtr(index), IntPtr.Zero).ToInt32();
            if (length < 0) return string.Empty;
            var value = new StringBuilder(length + 1);
            SendMessageBuffer(best.Handle, CB_GETLBTEXT, new IntPtr(index), value);
            return value.ToString().Trim();
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

            var info = new ProcessStartInfo("cmd.exe")
            {
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardInput = true,
                Arguments = "/d /c \"\"" + service + "\" admin\"",
                WorkingDirectory = zapretRoot
            };

            using (var process = Process.Start(info))
            {
                if (process == null) throw new InvalidOperationException("Не удалось запустить upstream service.bat.");
                try
                {
                    process.StandardInput.WriteLine("1");
                    process.StandardInput.WriteLine(menuIndex.ToString());
                    process.StandardInput.Flush();

                    var deadline = DateTime.UtcNow.AddSeconds(15);
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

'''
rep('        private IntPtr CreateHost(int width, int height)\n', install_methods + '        private IntPtr CreateHost(int width, int height)\n')

rep(
    '                    if (id == RepairBroadcastButtonId) RepairBroadcast();\n',
    '                    if (id == InstallServiceProxyButtonId) InstallSelectedStrategyUsingUpstreamManager();\n'
    '                    else if (id == RepairBroadcastButtonId) RepairBroadcast();\n'
)

runtime_method = r'''        internal void RefreshRuntimeStatus()
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

'''
rep('        private void RepairBroadcast()\n', runtime_method + '        private void RepairBroadcast()\n')

rep(
    '            foreach (var handle in new[] { _actionToolbar, _updateToolbar })\n',
    '            if (_legacyInstallServiceButton != IntPtr.Zero)\n'
    '            {\n'
    '                try { NativeBridge.ShowWindow(_legacyInstallServiceButton, NativeBridge.SW_SHOW); } catch { }\n'
    '            }\n'
    '            foreach (var handle in new[] { _actionToolbar, _updateToolbar, _installServiceToolbar })\n'
)

rep(
    '            _actionToolbar = IntPtr.Zero;\n'
    '            _updateToolbar = IntPtr.Zero;\n',
    '            _actionToolbar = IntPtr.Zero;\n'
    '            _updateToolbar = IntPtr.Zero;\n'
    '            _installServiceToolbar = IntPtr.Zero;\n'
)

path.write_text(text, encoding='utf-8', newline='\n')
print('REV16_ZAPRET_INSTALL_PATCH_OK')
