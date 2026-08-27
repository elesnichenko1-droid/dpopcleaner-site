using System;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace DPopCleaner.SimpleUpdate
{
    internal sealed class LauncherContext : ApplicationContext
    {
        private readonly Process _core;
        private readonly SettingsStore _settings;
        private readonly LauncherOptions _options;
        private readonly System.Windows.Forms.Timer _timer;
        private readonly CancellationTokenSource _updateCancellation;
        private readonly HttpClient _http;
        private readonly UpdateClient _updateClient;
        private IntPtr _mainWindow;
        private IntPtr _checkbox;
        private IntPtr _checkNowButton;
        private bool _roomMade;
        private bool _lastSetting;
        private bool _iconApplied;
        private bool _mainWindowWasVisible;
        private bool _automaticCheckStarted;
        private bool _updateCheckRunning;
        private bool _updateInstallInProgress;

        internal LauncherContext(string corePath, string settingsPath, LauncherOptions options)
        {
            if (!File.Exists(corePath)) throw new FileNotFoundException("DPopCleaner.exe not found.", corePath);
            _options = options ?? LauncherOptions.Parse(new string[0]);
            _settings = new SettingsStore(settingsPath);
            _lastSetting = _settings.LoadAutoUpdateEnabled();
            _updateCancellation = new CancellationTokenSource();
            _http = new HttpClient { Timeout = TimeSpan.FromSeconds(15) };
            _http.DefaultRequestHeaders.TryAddWithoutValidation("User-Agent", "DPopCleaner-SimpleUpdate/0.4.17-rev3");
            _updateClient = new UpdateClient(_http);

            _core = Process.Start(new ProcessStartInfo(corePath)
            {
                WorkingDirectory = Path.GetDirectoryName(corePath),
                UseShellExecute = true
            });
            if (_core == null) throw new InvalidOperationException("Failed to start DPopCleaner.exe.");

            _timer = new System.Windows.Forms.Timer { Interval = 100 };
            _timer.Tick += OnTick;
            _timer.Start();
        }

        internal Process CoreProcess { get { return _core; } }

        private void OnTick(object sender, EventArgs e)
        {
            try
            {
                _core.Refresh();
                if (_core.HasExited)
                {
                    _timer.Stop();
                    if (!_updateInstallInProgress) ExitThread();
                    return;
                }

                if (_mainWindow == IntPtr.Zero)
                    _mainWindow = _core.MainWindowHandle;
                if (_mainWindow == IntPtr.Zero) return;

                if (NativeBridge.IsWindowVisible(_mainWindow))
                {
                    _mainWindowWasVisible = true;
                    if (!_automaticCheckStarted && _options.UpdateCheckEnabled && _lastSetting)
                    {
                        _automaticCheckStarted = true;
                        BeginUpdateCheck(false);
                    }
                }
                else if (_mainWindowWasVisible)
                {
                    // The authentic 0.2.14 window may hide first and leave its process alive
                    // while shutdown work stalls. Once the visible main window is gone, the
                    // user's close action is final: terminate the lingering core immediately.
                    try { _core.Kill(); } catch { }
                    _timer.Stop();
                    if (!_updateInstallInProgress) ExitThread();
                    return;
                }

                if (!_iconApplied)
                {
                    NativeBridge.ApplyExecutableIcon(_mainWindow, Application.ExecutablePath);
                    _iconApplied = true;
                }

                var admin = NativeBridge.FindChildById(_mainWindow, NativeBridge.AdminCheckboxId);
                var settingsVisible = admin != IntPtr.Zero && NativeBridge.IsWindowVisible(admin);
                if (!settingsVisible)
                {
                    if (_checkbox != IntPtr.Zero) NativeBridge.ShowWindow(_checkbox, NativeBridge.SW_HIDE);
                    if (_checkNowButton != IntPtr.Zero) NativeBridge.ShowWindow(_checkNowButton, NativeBridge.SW_HIDE);
                    return;
                }

                if (!_roomMade)
                {
                    NativeBridge.MakeRoomForAutoUpdate(_mainWindow);
                    _roomMade = true;
                }

                if (_checkbox == IntPtr.Zero)
                {
                    _checkbox = NativeBridge.CreateAutoUpdateCheckbox(_mainWindow, admin, _lastSetting);
                    if (_checkbox == IntPtr.Zero) return;
                }
                else
                {
                    NativeBridge.ShowWindow(_checkbox, NativeBridge.SW_SHOW);
                }

                if (_checkNowButton == IntPtr.Zero)
                {
                    _checkNowButton = NativeBridge.CreateCheckNowButton(_mainWindow, admin);
                }
                else
                {
                    NativeBridge.ShowWindow(_checkNowButton, NativeBridge.SW_SHOW);
                }

                var nowChecked = NativeBridge.SendMessage(_checkbox, NativeBridge.BM_GETCHECK, IntPtr.Zero, IntPtr.Zero).ToInt32() == NativeBridge.BST_CHECKED;
                if (nowChecked != _lastSetting)
                {
                    _settings.SaveAutoUpdateEnabled(nowChecked);
                    _lastSetting = nowChecked;
                    if (nowChecked && !_automaticCheckStarted && _options.UpdateCheckEnabled)
                    {
                        _automaticCheckStarted = true;
                        BeginUpdateCheck(false);
                    }
                }

                if (_checkNowButton != IntPtr.Zero)
                {
                    var manualRequested = NativeBridge.SendMessage(_checkNowButton, NativeBridge.BM_GETCHECK, IntPtr.Zero, IntPtr.Zero).ToInt32() == NativeBridge.BST_CHECKED;
                    if (manualRequested)
                    {
                        NativeBridge.SendMessage(_checkNowButton, NativeBridge.BM_SETCHECK, new IntPtr(NativeBridge.BST_UNCHECKED), IntPtr.Zero);
                        BeginUpdateCheck(true);
                    }
                }
            }
            catch
            {
                // The bridge must never take down the authentic DPopCleaner process because of
                // a helper failure. Only the explicit hidden-main-window shutdown path above may
                // terminate it, because that represents the user's completed close action.
            }
        }

        private void BeginUpdateCheck(bool manual)
        {
            if (!_options.UpdateCheckEnabled)
            {
                if (manual)
                    MessageBox.Show("Проверка обновлений отключена для этого запуска.", "DPopCleaner",
                        MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            if (!manual && !_lastSetting) return;
            if (_updateCheckRunning)
            {
                if (manual)
                    MessageBox.Show("Проверка обновлений уже выполняется.", "DPopCleaner",
                        MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            _updateCheckRunning = true;
            RunUpdateCheckAsync(manual);
        }

        private async void RunUpdateCheckAsync(bool manual)
        {
            try
            {
                var manifest = await _updateClient.CheckForNewerAsync(
                    _options.ManifestUrl,
                    Program.CurrentVersionCode,
                    Program.CurrentRevision,
                    _updateCancellation.Token);

                if (manifest == null)
                {
                    if (manual)
                        MessageBox.Show("Установлена актуальная версия DPopCleaner.", "DPopCleaner",
                            MessageBoxButtons.OK, MessageBoxIcon.Information);
                    return;
                }

                if (!manual && !_lastSetting) return;

                var answer = MessageBox.Show(
                    "Доступна новая версия DPopCleaner " + manifest.Version + ".\r\n\r\nСкачать и установить сейчас?",
                    "Обновление DPopCleaner",
                    MessageBoxButtons.YesNo,
                    MessageBoxIcon.Information,
                    MessageBoxDefaultButton.Button1);
                if (answer != DialogResult.Yes) return;

                var updateRoot = Path.Combine(Path.GetTempPath(), "DPopCleaner", "Updates");
                var packageName = "DPopCleaner_Setup_" + SafeFilePart(manifest.Version) + ".exe";
                var destination = Path.Combine(updateRoot, packageName);
                var verifiedPackage = await _updateClient.DownloadVerifiedPackageAsync(
                    manifest, destination, _updateCancellation.Token);

                _updateInstallInProgress = true;
                await CloseCoreForUpdateAsync();
                StartInstaller(verifiedPackage, manifest.InstallArgs);
                ExitThread();
            }
            catch (OperationCanceledException)
            {
                // Closing DPopCleaner cancels background update I/O immediately.
            }
            catch (Exception ex)
            {
                if (manual)
                {
                    MessageBox.Show(
                        "Не удалось проверить или установить обновление.\r\n\r\n" + ex.Message,
                        "Обновление DPopCleaner",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Error);
                }
            }
            finally
            {
                _updateCheckRunning = false;
            }
        }

        private async Task CloseCoreForUpdateAsync()
        {
            try
            {
                _core.Refresh();
                if (_core.HasExited) return;
                if (_mainWindow != IntPtr.Zero)
                    NativeBridge.PostMessage(_mainWindow, NativeBridge.WM_CLOSE, IntPtr.Zero, IntPtr.Zero);

                for (var i = 0; i < 12; i++)
                {
                    await Task.Delay(50, _updateCancellation.Token);
                    _core.Refresh();
                    if (_core.HasExited) return;
                }

                try { _core.Kill(); } catch { }
                for (var i = 0; i < 10; i++)
                {
                    await Task.Delay(25, _updateCancellation.Token);
                    _core.Refresh();
                    if (_core.HasExited) return;
                }
            }
            catch (InvalidOperationException)
            {
                // Process already exited between refreshes.
            }
        }

        private static void StartInstaller(string path, string arguments)
        {
            var info = new ProcessStartInfo(path)
            {
                UseShellExecute = true,
                WorkingDirectory = Path.GetDirectoryName(path),
                Arguments = arguments ?? string.Empty
            };
            if (Process.Start(info) == null)
                throw new InvalidOperationException("Не удалось запустить проверенный установщик обновления.");
        }

        private static string SafeFilePart(string value)
        {
            if (string.IsNullOrWhiteSpace(value)) return "update";
            var chars = value.ToCharArray();
            var invalid = Path.GetInvalidFileNameChars();
            for (var i = 0; i < chars.Length; i++)
            {
                if (Array.IndexOf(invalid, chars[i]) >= 0) chars[i] = '_';
            }
            return new string(chars);
        }

        protected override void ExitThreadCore()
        {
            try { _updateCancellation.Cancel(); } catch { }
            if (_timer != null) _timer.Dispose();
            if (_http != null) _http.Dispose();
            if (_updateCancellation != null) _updateCancellation.Dispose();
            if (_core != null) _core.Dispose();
            base.ExitThreadCore();
        }
    }
}