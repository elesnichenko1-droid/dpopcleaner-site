using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

namespace DPopCleaner.SimpleUpdate
{
    internal sealed class TrayRamBadgeHost : IDisposable
    {
        private const uint NIM_ADD = 0x00000000;
        private const uint NIM_MODIFY = 0x00000001;
        private const uint NIM_DELETE = 0x00000002;
        private const uint NIF_MESSAGE = 0x00000001;
        private const uint NIF_ICON = 0x00000002;
        private const uint NIF_TIP = 0x00000004;
        private const uint TrayIconId = 1;
        private const int TrayCallbackMessage = 0x8051;
        private const int WM_LBUTTONDBLCLK = 0x0203;
        private const int WM_RBUTTONUP = 0x0205;
        private const int WM_CONTEXTMENU = 0x007B;

        private readonly TrayMessageWindow _messageWindow;
        private readonly ContextMenuStrip _menu;
        private Icon _currentIcon;
        private IntPtr _mainWindow;
        private int _lastPercent = -1;
        private DateTime _nextBadgeUtc = DateTime.MinValue;
        private DateTime _nextSuppressUtc = DateTime.MinValue;
        private bool _added;
        private bool _enabled;
        private bool _disposed;

        internal TrayRamBadgeHost(IntPtr mainWindow)
        {
            _mainWindow = mainWindow;
            _menu = new ContextMenuStrip();
            var openItem = new ToolStripMenuItem("Открыть DPopCleaner");
            openItem.Click += delegate { RestoreMainWindow(); };
            _menu.Items.Add(openItem);

            // Keep one native HWND for the entire launcher lifetime. System.Windows.Forms.NotifyIcon
            // owns an internal window that can be recreated; Explorer can then retain the old
            // (HWND,uID) identity and show a duplicate icon. This window never changes its handle.
            _messageWindow = new TrayMessageWindow(OnTrayMessage, OnTaskbarCreated);
        }

        internal IntPtr MessageWindowHandle { get { return _messageWindow.Handle; } }
        internal uint IconId { get { return TrayIconId; } }

        internal void ReattachMainWindow(IntPtr mainWindow)
        {
            if (_disposed) return;
            _mainWindow = mainWindow;
        }

        internal void Update(int coreProcessId, IntPtr mainWindow, bool enabled)
        {
            if (_disposed) return;
            if (mainWindow != IntPtr.Zero) _mainWindow = mainWindow;
            _enabled = enabled;

            if (!enabled)
            {
                RemoveTrayIcon();
                return;
            }

            var now = DateTime.UtcNow;
            if (now >= _nextSuppressUtc)
            {
                LegacyTrayIconSuppressor.RemoveIconsForProcess(coreProcessId);
                BridgeTrayGhostSuppressor.CleanupCurrentProcess(_messageWindow.Handle, TrayIconId);
                _nextSuppressUtc = now.AddMilliseconds(250);
            }

            if (now >= _nextBadgeUtc)
            {
                var percent = ReadMemoryLoad();
                if (percent != _lastPercent || _currentIcon == null)
                {
                    ReplaceIcon(RenderRamBadge(percent));
                    _lastPercent = percent;
                }
                PublishTrayIcon();
                _nextBadgeUtc = now.AddMilliseconds(1000);
            }
            else if (!_added)
            {
                PublishTrayIcon();
            }
        }

        private void OnTrayMessage(int message)
        {
            if (_disposed) return;
            if (message == WM_LBUTTONDBLCLK)
            {
                RestoreMainWindow();
                return;
            }
            if (message == WM_RBUTTONUP || message == WM_CONTEXTMENU)
            {
                try { _menu.Show(Cursor.Position); } catch { }
            }
        }

        private void OnTaskbarCreated()
        {
            if (_disposed || !_enabled) return;
            // Explorer restart discards all notification icons. The stable HWND/uID is reused.
            _added = false;
            PublishTrayIcon();
        }

        private void PublishTrayIcon()
        {
            if (_disposed || !_enabled || _currentIcon == null || _messageWindow.Handle == IntPtr.Zero) return;
            var data = CreateNotifyIconData(NIF_MESSAGE | NIF_ICON | NIF_TIP);
            var operation = _added ? NIM_MODIFY : NIM_ADD;
            if (Shell_NotifyIcon(operation, ref data)) _added = true;
        }

        private void RemoveTrayIcon()
        {
            if (!_added || _messageWindow.Handle == IntPtr.Zero) return;
            var data = CreateNotifyIconData(0);
            Shell_NotifyIcon(NIM_DELETE, ref data);
            _added = false;
        }

        private NOTIFYICONDATA CreateNotifyIconData(uint flags)
        {
            return new NOTIFYICONDATA
            {
                cbSize = (uint)Marshal.SizeOf(typeof(NOTIFYICONDATA)),
                hWnd = _messageWindow.Handle,
                uID = TrayIconId,
                uFlags = flags,
                uCallbackMessage = TrayCallbackMessage,
                hIcon = _currentIcon == null ? IntPtr.Zero : _currentIcon.Handle,
                szTip = _lastPercent < 0 ? "DPopCleaner" : "DPopCleaner — ОЗУ " + _lastPercent + "%",
                szInfo = string.Empty,
                szInfoTitle = string.Empty
            };
        }

        private void RestoreMainWindow()
        {
            if (_mainWindow == IntPtr.Zero) return;
            ShowWindow(_mainWindow, 9); // SW_RESTORE
            SetForegroundWindow(_mainWindow);
        }

        internal static int ReadMemoryLoad()
        {
            var status = new MEMORYSTATUSEX();
            status.dwLength = (uint)Marshal.SizeOf(typeof(MEMORYSTATUSEX));
            if (!GlobalMemoryStatusEx(ref status)) return 0;
            return Math.Max(0, Math.Min(100, (int)status.dwMemoryLoad));
        }

        internal static Icon RenderRamBadge(int percent)
        {
            using (var bitmap = new Bitmap(32, 32, PixelFormat.Format32bppArgb))
            using (var graphics = Graphics.FromImage(bitmap))
            {
                graphics.Clear(Color.Transparent);
                graphics.SmoothingMode = SmoothingMode.AntiAlias;
                graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
                graphics.PixelOffsetMode = PixelOffsetMode.HighQuality;

                Icon baseIcon = null;
                try { baseIcon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }
                if (baseIcon == null) baseIcon = SystemIcons.Application;
                graphics.DrawIcon(baseIcon, new Rectangle(1, 1, 30, 30));

                using (var badge = new SolidBrush(Color.FromArgb(238, 103, 237, 119)))
                    graphics.FillRectangle(badge, 1, 16, 30, 15);

                var text = percent.ToString();
                var fontSize = text.Length >= 3 ? 9.0f : 11.0f;
                using (var font = new Font("Segoe UI", fontSize, FontStyle.Bold, GraphicsUnit.Pixel))
                using (var textBrush = new SolidBrush(Color.FromArgb(8, 24, 38)))
                using (var format = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center })
                    graphics.DrawString(text, font, textBrush, new RectangleF(1, 16, 30, 15), format);

                var hIcon = bitmap.GetHicon();
                try
                {
                    using (var temporary = Icon.FromHandle(hIcon))
                        return (Icon)temporary.Clone();
                }
                finally
                {
                    DestroyIcon(hIcon);
                }
            }
        }

        private void ReplaceIcon(Icon icon)
        {
            var previous = _currentIcon;
            _currentIcon = icon;
            if (previous != null) previous.Dispose();
        }

        public void Dispose()
        {
            if (_disposed) return;
            RemoveTrayIcon();
            _disposed = true;
            _messageWindow.Dispose();
            _menu.Dispose();
            if (_currentIcon != null) _currentIcon.Dispose();
        }

        private sealed class TrayMessageWindow : NativeWindow, IDisposable
        {
            private readonly Action<int> _trayMessage;
            private readonly Action _taskbarCreatedAction;
            private readonly uint _taskbarCreatedMessage;
            private bool _disposed;

            internal TrayMessageWindow(Action<int> trayMessage, Action taskbarCreatedAction)
            {
                _trayMessage = trayMessage;
                _taskbarCreatedAction = taskbarCreatedAction;
                _taskbarCreatedMessage = RegisterWindowMessage("TaskbarCreated");
                CreateHandle(new CreateParams { Caption = "DPopCleaner.TrayRamBadgeHost" });
            }

            protected override void WndProc(ref Message m)
            {
                if ((uint)m.Msg == _taskbarCreatedMessage)
                {
                    if (_taskbarCreatedAction != null) _taskbarCreatedAction();
                }
                else if (m.Msg == TrayCallbackMessage)
                {
                    if (_trayMessage != null) _trayMessage(m.LParam.ToInt32());
                }
                base.WndProc(ref m);
            }

            public void Dispose()
            {
                if (_disposed) return;
                _disposed = true;
                if (Handle != IntPtr.Zero) DestroyHandle();
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct MEMORYSTATUSEX
        {
            public uint dwLength;
            public uint dwMemoryLoad;
            public ulong ullTotalPhys;
            public ulong ullAvailPhys;
            public ulong ullTotalPageFile;
            public ulong ullAvailPageFile;
            public ulong ullTotalVirtual;
            public ulong ullAvailVirtual;
            public ulong ullAvailExtendedVirtual;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct NOTIFYICONDATA
        {
            public uint cbSize;
            public IntPtr hWnd;
            public uint uID;
            public uint uFlags;
            public int uCallbackMessage;
            public IntPtr hIcon;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string szTip;
            public uint dwState;
            public uint dwStateMask;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)] public string szInfo;
            public uint uTimeoutOrVersion;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)] public string szInfoTitle;
            public uint dwInfoFlags;
            public Guid guidItem;
            public IntPtr hBalloonIcon;
        }

        [DllImport("kernel32.dll", SetLastError = true)] private static extern bool GlobalMemoryStatusEx(ref MEMORYSTATUSEX lpBuffer);
        [DllImport("user32.dll")] private static extern bool DestroyIcon(IntPtr hIcon);
        [DllImport("user32.dll")] private static extern bool ShowWindow(IntPtr hWnd, int command);
        [DllImport("user32.dll")] private static extern bool SetForegroundWindow(IntPtr hWnd);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern uint RegisterWindowMessage(string message);
        [DllImport("shell32.dll", CharSet = CharSet.Unicode)] private static extern bool Shell_NotifyIcon(uint message, ref NOTIFYICONDATA data);
    }

    internal static class LegacyTrayIconSuppressor
    {
        private const uint NIM_DELETE = 0x00000002;
        private const int TB_BUTTONCOUNT = 0x0418;
        private const int TB_GETBUTTON = 0x0417;
        private const uint PROCESS_VM_OPERATION = 0x0008;
        private const uint PROCESS_VM_READ = 0x0010;
        private const uint PROCESS_VM_WRITE = 0x0020;
        private const uint PROCESS_QUERY_INFORMATION = 0x0400;
        private const uint MEM_COMMIT = 0x1000;
        private const uint MEM_RESERVE = 0x2000;
        private const uint MEM_RELEASE = 0x8000;
        private const uint PAGE_READWRITE = 0x04;
        private delegate bool EnumChildProc(IntPtr hwnd, IntPtr lParam);

        internal static int RemoveIconsForProcess(int processId)
        {
            if (processId <= 0 || !Environment.Is64BitProcess) return 0;
            var removed = 0;
            foreach (var toolbar in FindTrayToolbars()) removed += RemoveFromToolbar(toolbar, processId);
            return removed;
        }

        private static IEnumerable<IntPtr> FindTrayToolbars()
        {
            var result = new HashSet<IntPtr>();
            AddToolbarChildren(FindWindow("Shell_TrayWnd", null), result);
            AddToolbarChildren(FindWindow("NotifyIconOverflowWindow", null), result);
            return result;
        }

        private static void AddToolbarChildren(IntPtr root, HashSet<IntPtr> result)
        {
            if (root == IntPtr.Zero) return;
            EnumChildProc callback = delegate(IntPtr hwnd, IntPtr _)
            {
                var name = new StringBuilder(128);
                GetClassName(hwnd, name, name.Capacity);
                if (string.Equals(name.ToString(), "ToolbarWindow32", StringComparison.Ordinal)) result.Add(hwnd);
                return true;
            };
            EnumChildWindows(root, callback, IntPtr.Zero);
            GC.KeepAlive(callback);
        }

        private static int RemoveFromToolbar(IntPtr toolbar, int ownerProcessId)
        {
            uint explorerPid;
            GetWindowThreadProcessId(toolbar, out explorerPid);
            if (explorerPid == 0) return 0;
            var process = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, false, explorerPid);
            if (process == IntPtr.Zero) return 0;

            var buttonSize = Marshal.SizeOf(typeof(TBBUTTON64));
            var remote = VirtualAllocEx(process, IntPtr.Zero, new UIntPtr((uint)buttonSize), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (remote == IntPtr.Zero) { CloseHandle(process); return 0; }

            var removed = 0;
            try
            {
                var count = SendMessage(toolbar, TB_BUTTONCOUNT, IntPtr.Zero, IntPtr.Zero).ToInt32();
                for (var i = 0; i < count; i++)
                {
                    if (SendMessage(toolbar, TB_GETBUTTON, new IntPtr(i), remote) == IntPtr.Zero) continue;
                    TBBUTTON64 button;
                    if (!ReadRemote(process, remote, out button) || button.dwData == UIntPtr.Zero) continue;
                    TRAYDATA64 tray;
                    if (!ReadRemote(process, new IntPtr(unchecked((long)button.dwData.ToUInt64())), out tray) || tray.hwnd == IntPtr.Zero) continue;
                    uint trayOwner;
                    GetWindowThreadProcessId(tray.hwnd, out trayOwner);
                    if (trayOwner != (uint)ownerProcessId) continue;
                    var data = new NOTIFYICONDATA { cbSize = (uint)Marshal.SizeOf(typeof(NOTIFYICONDATA)), hWnd = tray.hwnd, uID = tray.uID };
                    if (Shell_NotifyIcon(NIM_DELETE, ref data)) removed++;
                }
            }
            finally
            {
                VirtualFreeEx(process, remote, UIntPtr.Zero, MEM_RELEASE);
                CloseHandle(process);
            }
            return removed;
        }

        private static bool ReadRemote<T>(IntPtr process, IntPtr remote, out T value) where T : struct
        {
            var size = Marshal.SizeOf(typeof(T));
            var local = Marshal.AllocHGlobal(size);
            try
            {
                UIntPtr read;
                if (!ReadProcessMemory(process, remote, local, new UIntPtr((uint)size), out read) || read.ToUInt64() < (ulong)size)
                {
                    value = default(T);
                    return false;
                }
                value = (T)Marshal.PtrToStructure(local, typeof(T));
                return true;
            }
            finally { Marshal.FreeHGlobal(local); }
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct TBBUTTON64
        {
            public int iBitmap;
            public int idCommand;
            public byte fsState;
            public byte fsStyle;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 6)] public byte[] reserved;
            public UIntPtr dwData;
            public IntPtr iString;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct TRAYDATA64
        {
            public IntPtr hwnd;
            public uint uID;
            public uint callback;
            public uint reserved0;
            public uint reserved1;
            public IntPtr hIcon;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct NOTIFYICONDATA
        {
            public uint cbSize;
            public IntPtr hWnd;
            public uint uID;
            public uint uFlags;
            public uint uCallbackMessage;
            public IntPtr hIcon;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string szTip;
            public uint dwState;
            public uint dwStateMask;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)] public string szInfo;
            public uint uTimeoutOrVersion;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)] public string szInfoTitle;
            public uint dwInfoFlags;
            public Guid guidItem;
            public IntPtr hBalloonIcon;
        }

        [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern IntPtr FindWindow(string className, string windowName);
        [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumChildProc callback, IntPtr lParam);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder className, int maxCount);
        [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
        [DllImport("user32.dll")] private static extern IntPtr SendMessage(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern IntPtr OpenProcess(uint access, bool inheritHandle, uint processId);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern IntPtr VirtualAllocEx(IntPtr process, IntPtr address, UIntPtr size, uint allocationType, uint protect);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern bool VirtualFreeEx(IntPtr process, IntPtr address, UIntPtr size, uint freeType);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern bool ReadProcessMemory(IntPtr process, IntPtr address, IntPtr buffer, UIntPtr size, out UIntPtr bytesRead);
        [DllImport("kernel32.dll")] private static extern bool CloseHandle(IntPtr handle);
        [DllImport("shell32.dll", CharSet = CharSet.Unicode)] private static extern bool Shell_NotifyIcon(uint message, ref NOTIFYICONDATA data);
    }
}
