using System;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;

namespace DPopCleaner.SimpleUpdate
{
    internal sealed class LauncherContext : ApplicationContext
    {
        private readonly Process _core;
        private readonly SettingsStore _settings;
        private readonly Timer _timer;
        private IntPtr _mainWindow;
        private IntPtr _checkbox;
        private bool _roomMade;
        private bool _lastSetting;
        private bool _iconApplied;
        private bool _mainWindowWasVisible;

        internal LauncherContext(string corePath, string settingsPath)
        {
            if (!File.Exists(corePath)) throw new FileNotFoundException("DPopCleaner.exe not found.", corePath);
            _settings = new SettingsStore(settingsPath);
            _lastSetting = _settings.LoadAutoUpdateEnabled();
            _core = Process.Start(new ProcessStartInfo(corePath) { WorkingDirectory = Path.GetDirectoryName(corePath), UseShellExecute = true });
            if (_core == null) throw new InvalidOperationException("Failed to start DPopCleaner.exe.");

            _timer = new Timer { Interval = 100 };
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
                    ExitThread();
                    return;
                }

                if (_mainWindow == IntPtr.Zero)
                    _mainWindow = _core.MainWindowHandle;
                if (_mainWindow == IntPtr.Zero) return;

                if (NativeBridge.IsWindowVisible(_mainWindow))
                {
                    _mainWindowWasVisible = true;
                }
                else if (_mainWindowWasVisible)
                {
                    // The authentic 0.2.14 window may hide first and leave its process alive
                    // while shutdown work stalls. Once the visible main window is gone, the
                    // user's close action is final: terminate the lingering core immediately.
                    try { _core.Kill(); } catch { }
                    _timer.Stop();
                    ExitThread();
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

                var nowChecked = NativeBridge.SendMessage(_checkbox, NativeBridge.BM_GETCHECK, IntPtr.Zero, IntPtr.Zero).ToInt32() == NativeBridge.BST_CHECKED;
                if (nowChecked != _lastSetting)
                {
                    _settings.SaveAutoUpdateEnabled(nowChecked);
                    _lastSetting = nowChecked;
                }
            }
            catch
            {
                // The helper must never take down the authentic DPopCleaner process because of
                // a bridge failure. Only the explicit hidden-main-window shutdown path above may
                // terminate it, because that represents the user's completed close action.
            }
        }

        protected override void ExitThreadCore()
        {
            if (_timer != null) _timer.Dispose();
            if (_core != null) _core.Dispose();
            base.ExitThreadCore();
        }
    }
}
