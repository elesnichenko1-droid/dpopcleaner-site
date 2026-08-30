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
        private readonly NotifyIcon _notifyIcon;
        private readonly ToolStripMenuItem _openItem;
        private Icon _currentIcon;
        private IntPtr _mainWindow;
        private int _lastPercent = -1;
        private DateTime _nextBadgeUtc = DateTime.MinValue;
        private DateTime _nextSuppressUtc = DateTime.MinValue;
        private bool _disposed;

        internal TrayRamBadgeHost(IntPtr mainWindow)
        {
            _mainWindow = mainWindow;
            _openItem = new ToolStripMenuItem("Открыть DPopCleaner");
            _openItem.Click += delegate { RestoreMainWindow(); };
            var menu = new ContextMenuStrip();
            menu.Items.Add(_openItem);

            _notifyIcon = new NotifyIcon
            {
                Text = "DPopCleaner",
                Visible = false,
                ContextMenuStrip = menu
            };
            _notifyIcon.DoubleClick += delegate { RestoreMainWindow(); };
        }

        internal void Update(int coreProcessId, IntPtr mainWindow, bool enabled)
        {
            if (_disposed) return;
            if (mainWindow != IntPtr.Zero) _mainWindow = mainWindow;

            if (!enabled)
            {
                if (_notifyIcon.Visible) _notifyIcon.Visible = false;
                return;
            }

            var now = DateTime.UtcNow;
            if (now >= _nextSuppressUtc)
            {
                LegacyTrayIconSuppressor.RemoveIconsForProcess(coreProcessId);
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
                _notifyIcon.Text = "DPopCleaner — ОЗУ " + percent + "%";
                _nextBadgeUtc = now.AddMilliseconds(1000);
            }

            if (!_notifyIcon.Visible) _notifyIcon.Visible = true;
        }

        private void RestoreMainWindow()
        {
            if (_mainWindow == IntPtr.Zero) return;
            NativeBridge.ShowWindow(_mainWindow, 9); // SW_RESTORE
            NativeBridge.SetForegroundWindowSafe(_mainWindow);
        }

        private static int ReadMemoryLoad()
        {
            var status = new MEMORYSTATUSEX();
            status.dwLength = (uint)Marshal.SizeOf(typeof(MEMORYSTATUSEX));
            if (!GlobalMemoryStatusEx(ref status)) return 0;
            return Math.Max(0, Math.Min(100, (int)status.dwMemoryLoad));
        }

        private static Icon RenderRamBadge(int percent)
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
                {
                    var format = new StringFormat
                    {
                        Alignment = StringAlignment.Center,
                        LineAlignment = StringAlignment.Center
                    };
                    graphics.DrawString(text, font, textBrush, new RectangleF(1, 16, 30, 15), format);
                }

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
            _notifyIcon.Icon = _currentIcon;
            if (previous != null) previous.Dispose();
        }

        public void Dispose()
        {
            if (_disposed) return;
            _disposed = true;
            _notifyIcon.Visible = false;
            _notifyIcon.Dispose();
            _openItem.Dispose();
            if (_currentIcon != null) _currentIcon.Dispose();
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

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool GlobalMemoryStatusEx(ref MEMORYSTATUSEX lpBuffer);

        [DllImport("user32.dll")]
        private static extern bool DestroyIcon(IntPtr hIcon);
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
            foreach (var toolbar in FindTrayToolbars())
                removed += RemoveFromToolbar(toolbar, processId);
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
                if (string.Equals(name.ToString(), "ToolbarWindow32", StringComparison.Ordinal))
                    result.Add(hwnd);
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

            var process = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
                false, explorerPid);
            if (process == IntPtr.Zero) return 0;

            var buttonSize = Marshal.SizeOf(typeof(TBBUTTON64));
            var remote = VirtualAllocEx(process, IntPtr.Zero, new UIntPtr((uint)buttonSize), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (remote == IntPtr.Zero)
            {
                CloseHandle(process);
                return 0;
            }

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
                    if (!ReadRemote(process, new IntPtr(unchecked((long)button.dwData.ToUInt64())), out tray)) continue;
                    if (tray.hwnd == IntPtr.Zero) continue;

                    uint trayOwner;
                    GetWindowThreadProcessId(tray.hwnd, out trayOwner);
                    if (trayOwner != (uint)ownerProcessId) continue;

                    var data = new NOTIFYICONDATA();
                    data.cbSize = (uint)Marshal.SizeOf(typeof(NOTIFYICONDATA));
                    data.hWnd = tray.hwnd;
                    data.uID = tray.uID;
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
            finally
            {
                Marshal.FreeHGlobal(local);
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct TBBUTTON64
        {
            public int iBitmap;
            public int idCommand;
            public byte fsState;
            public byte fsStyle;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 6)] public byte[] bReserved;
            public UIntPtr dwData;
            public IntPtr iString;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct TRAYDATA64
        {
            public IntPtr hwnd;
            public uint uID;
            public uint uCallbackMessage;
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

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern IntPtr FindWindow(string className, string windowName);

        [DllImport("user32.dll")]
        private static extern bool EnumChildWindows(IntPtr parent, EnumChildProc callback, IntPtr lParam);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern int GetClassName(IntPtr hwnd, StringBuilder className, int maxCount);

        [DllImport("user32.dll")]
        private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);

        [DllImport("user32.dll")]
        private static extern IntPtr SendMessage(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr OpenProcess(uint access, bool inheritHandle, uint processId);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr VirtualAllocEx(IntPtr process, IntPtr address, UIntPtr size, uint allocationType, uint protect);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool VirtualFreeEx(IntPtr process, IntPtr address, UIntPtr size, uint freeType);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool ReadProcessMemory(IntPtr process, IntPtr address, IntPtr buffer, UIntPtr size, out UIntPtr bytesRead);

        [DllImport("kernel32.dll")]
        private static extern bool CloseHandle(IntPtr handle);

        [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
        private static extern bool Shell_NotifyIcon(uint message, ref NOTIFYICONDATA data);
    }
}
