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
        private readonly string _applicationRoot;
        private readonly SettingsStore _settings;
        private readonly LauncherOptions _options;
        private readonly System.Windows.Forms.Timer _timer;
        private readonly CancellationTokenSource _updateCancellation;
        private readonly HttpClient _http;
        private readonly UpdateClient _updateClient;
        private IntPtr _mainWindow;
        private AdditionalSettingsHost _settingsHost;
        private NativeBridge.ClientBounds _settingsHostBounds;
        private ZapretEnhancementHost _zapretHost;
        private ZapretVisualPolishHost _zapretVisualHost;
        private TrayRamBadgeHost _trayRamHost;
        private bool _traySettingKnown;
        private bool _trayEnabled;
        private bool _lastSetting;
        private bool _iconApplied;
        private bool _automaticCheckStarted;
        private bool _updateCheckRunning;
        private bool _updateInstallInProgress;

        internal LauncherContext(string corePath, string settingsPath, LauncherOptions options)
        {
            if (!File.Exists(corePath)) throw new FileNotFoundException("DPopCleaner core not found.", corePath);
            _applicationRoot = Path.GetDirectoryName(Path.GetFullPath(corePath));
            _options = options ?? LauncherOptions.Parse(new string[0]);
            _settings = new SettingsStore(settingsPath);
            _lastSetting = _settings.LoadAutoUpdateEnabled();
            _updateCancellation = new CancellationTokenSource();
            _http = new HttpClient { Timeout = TimeSpan.FromSeconds(15) };
            _http.DefaultRequestHeaders.TryAddWithoutValidation("User-Agent", "DPopCleaner-SimpleUpdate/0.4.17-rev13");
            _updateClient = new UpdateClient(_http);

            _core = Process.Start(new ProcessStartInfo(corePath)
            {
                WorkingDirectory = _applicationRoot,
                UseShellExecute = true
            });
            if (_core == null) throw new InvalidOperationException("Failed to start DPopCleaner core.");

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

                UpdateTrayRamBadge();

                if (!NativeBridge.IsWindowVisible(_mainWindow))
                {
                    // Temporary visibility changes are not exit signals. The frozen application can
                    // hide itself while minimizing/restoring or working in the tray; only HasExited
                    // above is allowed to terminate the launcher. The RAM tray host intentionally
                    // continues updating before this branch so the one visible tray icon remains live.
                    if (_settingsHost != null) _settingsHost.Hide();
                    if (_zapretHost != null) _zapretHost.Hide();
                    if (_zapretVisualHost != null) _zapretVisualHost.Hide();
                    return;
                }

                if (!_automaticCheckStarted && _options.UpdateCheckEnabled && _lastSetting)
                {
                    _automaticCheckStarted = true;
                    BeginUpdateCheck(false);
                }

                if (!_iconApplied)
                {
                    NativeBridge.ApplyExecutableIcon(_mainWindow, Application.ExecutablePath);
                    _iconApplied = true;
                }

                NativeBridge.HideLegacyVersionBadge(_mainWindow);
                NativeBridge.EnsureRamThresholdRange(_mainWindow);
                UpdateZapretEnhancements();
                UpdateSettingsEnhancements();
            }
            catch (Exception ex)
            {
                // UI bridge failures must never terminate the immutable authentic core.
                BridgeDiagnostics.Record(ex);
            }
        }

        private void UpdateTrayRamBadge()
        {
            var admin = NativeBridge.FindChildById(_mainWindow, NativeBridge.AdminCheckboxId);
            var settings = NativeBridge.FindSettingsCheckboxes(_mainWindow, admin);
            var traySetting = settings.Length == 6 ? settings[3] : IntPtr.Zero;
            if (traySetting != IntPtr.Zero)
            {
                _trayEnabled = NativeBridge.IsChecked(traySetting);
                _traySettingKnown = true;
            }
            if (!_traySettingKnown) return;

            if (_trayRamHost == null)
                _trayRamHost = new TrayRamBadgeHost(_mainWindow);
            _trayRamHost.Update(_core.Id, _mainWindow, _trayEnabled);
            if (_trayEnabled) BridgeTrayGhostSuppressor.CleanupCurrentProcess();
        }

        private void UpdateZapretEnhancements()
        {
            // Use the stable frozen control id instead of a localized caption. Language changes must
            // never make the bridge conclude that the user left the Zapret page.
            var marker = NativeBridge.FindChildById(_mainWindow, NativeBridge.ZapretApplyButtonId);
            var zapretVisible = marker != IntPtr.Zero && NativeBridge.IsWindowVisible(marker);
            if (!zapretVisible)
            {
                if (_zapretHost != null) _zapretHost.Hide();
                if (_zapretVisualHost != null) _zapretVisualHost.Hide();
                return;
            }

            if (_zapretHost == null)
                _zapretHost = new ZapretEnhancementHost(_mainWindow, _applicationRoot);
            else
                _zapretHost.Show();

            if (_zapretVisualHost == null)
                _zapretVisualHost = new ZapretVisualPolishHost(_mainWindow, _applicationRoot);
            else
                _zapretVisualHost.Show();
        }

        private void UpdateSettingsEnhancements()
        {
            var admin = NativeBridge.FindChildById(_mainWindow, NativeBridge.AdminCheckboxId);
            var save = NativeBridge.FindChildById(_mainWindow, NativeBridge.SaveSettingsButtonId);
            var adminVisible = admin != IntPtr.Zero && NativeBridge.IsWindowVisible(admin);
            var saveVisible = save != IntPtr.Zero && NativeBridge.IsWindowVisible(save);
            var settingsVisible = SettingsPageLocator.IsVisible(_mainWindow);

            BridgeDiagnostics.RecordState(
                "settings-probe visible=" + settingsVisible +
                " admin=0x" + admin.ToInt64().ToString("X") + "/" + adminVisible +
                " save=0x" + save.ToInt64().ToString("X") + "/" + saveVisible +
                " host=" + (_settingsHost != null));

            if (!settingsVisible)
            {
                if (_settingsHost != null) _settingsHost.Hide();
                return;
            }

            if (_settingsHost == null)
            {
                if (admin == IntPtr.Zero)
                {
                    BridgeDiagnostics.RecordState("settings-return admin-zero");
                    return;
                }

                var checkboxes = NativeBridge.FindSettingsCheckboxes(_mainWindow, admin);
                BridgeDiagnostics.RecordState(
                    "settings-checkboxes count=" + checkboxes.Length +
                    " startup=0x" + NativeBridge.FindChildById(_mainWindow, NativeBridge.StartupCheckboxId).ToInt64().ToString("X"));

                _settingsHostBounds = NativeBridge.GetSettingsScrollBounds(_mainWindow);
                if (_settingsHostBounds == null)
                {
                    BridgeDiagnostics.RecordState("settings-return bounds-null checkboxes=" + checkboxes.Length);
                    return;
                }

                BridgeDiagnostics.RecordState(
                    "settings-bounds " + _settingsHostBounds.Left + "," + _settingsHostBounds.Top +
                    "-" + _settingsHostBounds.Right + "," + _settingsHostBounds.Bottom);

                var legacyKey = NativeBridge.FindLegacyLicenseEdit(_mainWindow, _settingsHostBounds);
                var legacySave = NativeBridge.FindChildById(_mainWindow, NativeBridge.LicenseSaveButtonId);
                var legacyBuy = NativeBridge.FindChildById(_mainWindow, NativeBridge.LicenseBuyButtonId);
                BridgeDiagnostics.RecordState(
                    "settings-construct key=0x" + legacyKey.ToInt64().ToString("X") +
                    " save=0x" + legacySave.ToInt64().ToString("X") +
                    " buy=0x" + legacyBuy.ToInt64().ToString("X"));

                _settingsHost = new AdditionalSettingsHost(
                    _mainWindow,
                    _settingsHostBounds,
                    admin,
                    _lastSetting,
                    OnAutoUpdateSettingChanged,
                    delegate { BeginUpdateCheck(true); },
                    legacyKey,
                    legacySave,
                    legacyBuy);

                BridgeDiagnostics.RecordState(
                    "settings-created host=0x" + _settingsHost.Handle.ToInt64().ToString("X"));
            }
            else
            {
                // Keep the bounds captured from the authentic page before proxy controls were created.
                // Re-querying by label would recursively find our own proxy checkbox and feed its
                // scrolled coordinates back into the host position, causing the pane to "fly away".
                _settingsHost.Show(_settingsHostBounds);
            }

            SettingsProxyLocalization.Apply(_mainWindow);
            NativeBridge.HideLegacyOverflowControls(_mainWindow, _settingsHost.Handle, _settingsHostBounds);
        }

        private void OnAutoUpdateSettingChanged(bool enabled)
        {
            if (enabled == _lastSetting) return;
            _settings.SaveAutoUpdateEnabled(enabled);
            _lastSetting = enabled;
            if (enabled && !_automaticCheckStarted && _options.UpdateCheckEnabled)
            {
                _automaticCheckStarted = true;
                BeginUpdateCheck(false);
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
            if (_trayRamHost != null) _trayRamHost.Dispose();
            if (_settingsHost != null) _settingsHost.Dispose();
            if (_zapretVisualHost != null) _zapretVisualHost.Dispose();
            if (_zapretHost != null) _zapretHost.Dispose();
            if (_timer != null) _timer.Dispose();
            if (_http != null) _http.Dispose();
            if (_updateCancellation != null) _updateCancellation.Dispose();
            if (_core != null) _core.Dispose();
            base.ExitThreadCore();
        }
    }
}
