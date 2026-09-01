from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(relative_path: str, old: str, new: str) -> None:
    path = ROOT / relative_path
    with path.open('r', encoding='utf-8', newline='') as handle:
        text = handle.read()
    eol = '\r\n' if '\r\n' in text else '\n'
    old_eol = old.replace('\n', eol)
    new_eol = new.replace('\n', eol)
    count = text.count(old_eol)
    if count != 1:
        raise RuntimeError(f'{relative_path}: expected exactly one patch target, found {count}')
    text = text.replace(old_eol, new_eol, 1)
    with path.open('w', encoding='utf-8', newline='') as handle:
        handle.write(text)


replace_once(
    'v0417/src/SimpleUpdate/LauncherContext.cs',
    '''        private void ResetBridgeForRestartedCore()
        {
            if (_trayRamHost != null) _trayRamHost.Dispose();
            if (_settingsHost != null) _settingsHost.Dispose();
            if (_zapretVisualHost != null) _zapretVisualHost.Dispose();
            if (_zapretHost != null) _zapretHost.Dispose();

            _trayRamHost = null;
            _settingsHost = null;
            _zapretVisualHost = null;
            _zapretHost = null;
            _settingsHostBounds = null;
            _mainWindow = IntPtr.Zero;
            _traySettingKnown = false;
            _trayEnabled = false;
            _iconApplied = false;
        }
''',
    '''        private void ResetBridgeForRestartedCore()
        {
            if (_settingsHost != null) _settingsHost.Dispose();
            if (_zapretVisualHost != null) _zapretVisualHost.Dispose();
            if (_zapretHost != null) _zapretHost.Dispose();

            _settingsHost = null;
            _zapretVisualHost = null;
            _zapretHost = null;
            _settingsHostBounds = null;
            _mainWindow = IntPtr.Zero;
            _traySettingKnown = false;
            _trayEnabled = false;
            _iconApplied = false;

            // The notification identity belongs to the launcher, not to a frozen-core PID.
            // Keep its stable HWND/uID alive while the successor core creates a new main HWND.
            if (_trayRamHost != null) _trayRamHost.ReattachMainWindow(IntPtr.Zero);
        }
''')

replace_once(
    'v0417/src/SimpleUpdate/LauncherContext.cs',
    '''            _trayRamHost.Update(_core.Id, _mainWindow, _trayEnabled);
            if (_trayEnabled) BridgeTrayGhostSuppressor.CleanupCurrentProcess();
''',
    '''            _trayRamHost.Update(_core.Id, _mainWindow, _trayEnabled);
''')

replace_once(
    'v0417/src/SimpleUpdate/TrayRamBadgeHost.cs',
    '''            _messageWindow = new TrayMessageWindow(OnTrayMessage, OnTaskbarCreated);
        }

        internal void Update(int coreProcessId, IntPtr mainWindow, bool enabled)
''',
    '''            _messageWindow = new TrayMessageWindow(OnTrayMessage, OnTaskbarCreated);
        }

        internal IntPtr MessageWindowHandle { get { return _messageWindow.Handle; } }
        internal uint IconId { get { return TrayIconId; } }

        internal void ReattachMainWindow(IntPtr mainWindow)
        {
            if (_disposed) return;
            _mainWindow = mainWindow;
        }

        internal void Update(int coreProcessId, IntPtr mainWindow, bool enabled)
''')

replace_once(
    'v0417/src/SimpleUpdate/TrayRamBadgeHost.cs',
    '''            if (now >= _nextSuppressUtc)
            {
                LegacyTrayIconSuppressor.RemoveIconsForProcess(coreProcessId);
                _nextSuppressUtc = now.AddMilliseconds(250);
            }
''',
    '''            if (now >= _nextSuppressUtc)
            {
                LegacyTrayIconSuppressor.RemoveIconsForProcess(coreProcessId);
                BridgeTrayGhostSuppressor.CleanupCurrentProcess(_messageWindow.Handle, TrayIconId);
                _nextSuppressUtc = now.AddMilliseconds(250);
            }
''')

replace_once(
    'v0417/src/SimpleUpdate/BridgeTrayGhostSuppressor.cs',
    '''        private const string KeepWindowTitle = "DPopCleaner.TrayRamBadgeHost";
        private const uint KeepIconId = 1;
''',
    '''''')

replace_once(
    'v0417/src/SimpleUpdate/BridgeTrayGhostSuppressor.cs',
    '''        internal static void CleanupCurrentProcess()
        {
            var now = DateTime.UtcNow;
            if (now < _nextCleanupUtc) return;
            _nextCleanupUtc = now.AddMilliseconds(250);

            var processId = Process.GetCurrentProcess().Id;
            var keepWindow = FindKeepWindow(processId);
            if (keepWindow == IntPtr.Zero) return;

            foreach (var toolbar in FindTrayToolbars())
                RemoveFromToolbar(toolbar, processId, keepWindow, KeepIconId);
        }

        private static IntPtr FindKeepWindow(int processId)
        {
            var found = IntPtr.Zero;
            EnumWindowProc callback = delegate(IntPtr hwnd, IntPtr _)
            {
                uint owner;
                GetWindowThreadProcessId(hwnd, out owner);
                if (owner != (uint)processId) return true;
                var title = new StringBuilder(256);
                GetWindowText(hwnd, title, title.Capacity);
                if (!string.Equals(title.ToString(), KeepWindowTitle, StringComparison.Ordinal)) return true;
                found = hwnd;
                return false;
            };
            EnumWindows(callback, IntPtr.Zero);
            GC.KeepAlive(callback);
            return found;
        }
''',
    '''        internal static void CleanupCurrentProcess(IntPtr keepWindow, uint keepIconId)
        {
            if (keepWindow == IntPtr.Zero) return;
            var now = DateTime.UtcNow;
            if (now < _nextCleanupUtc) return;
            _nextCleanupUtc = now.AddMilliseconds(250);

            var processId = Process.GetCurrentProcess().Id;
            foreach (var toolbar in FindTrayToolbars())
                RemoveFromToolbar(toolbar, processId, keepWindow, keepIconId);
        }
''')

print('REV16_TASK1_PATCH_APPLIED')
