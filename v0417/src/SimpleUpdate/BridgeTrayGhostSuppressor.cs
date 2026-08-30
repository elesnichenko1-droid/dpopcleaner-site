using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;

namespace DPopCleaner.SimpleUpdate
{
    internal static class BridgeTrayGhostSuppressor
    {
        private const string KeepWindowTitle = "DPopCleaner.TrayRamBadgeHost";
        private const uint KeepIconId = 1;
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
        private delegate bool EnumWindowProc(IntPtr hwnd, IntPtr lParam);
        private DateTime _nextCleanupUtc = DateTime.MinValue;

        internal static void CleanupCurrentProcess()
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
            EnumWindowProc callback = delegate(IntPtr hwnd, IntPtr _)
            {
                var className = new StringBuilder(128);
                GetClassName(hwnd, className, className.Capacity);
                if (string.Equals(className.ToString(), "ToolbarWindow32", StringComparison.Ordinal)) result.Add(hwnd);
                return true;
            };
            EnumChildWindows(root, callback, IntPtr.Zero);
            GC.KeepAlive(callback);
        }

        private static void RemoveFromToolbar(IntPtr toolbar, int ownerProcessId, IntPtr keepWindow, uint keepIconId)
        {
            uint explorerPid;
            GetWindowThreadProcessId(toolbar, out explorerPid);
            if (explorerPid == 0) return;

            var process = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, false, explorerPid);
            if (process == IntPtr.Zero) return;
            var buttonSize = Marshal.SizeOf(typeof(TBBUTTON64));
            var remote = VirtualAllocEx(process, IntPtr.Zero, new UIntPtr((uint)buttonSize), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (remote == IntPtr.Zero) { CloseHandle(process); return; }

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
                    if (tray.hwnd == keepWindow && tray.uID == keepIconId) continue;

                    var data = new NOTIFYICONDATA
                    {
                        cbSize = (uint)Marshal.SizeOf(typeof(NOTIFYICONDATA)),
                        hWnd = tray.hwnd,
                        uID = tray.uID
                    };
                    Shell_NotifyIcon(NIM_DELETE, ref data);
                }
            }
            finally
            {
                VirtualFreeEx(process, remote, UIntPtr.Zero, MEM_RELEASE);
                CloseHandle(process);
            }
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

        [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowProc callback, IntPtr lParam);
        [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumWindowProc callback, IntPtr lParam);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern IntPtr FindWindow(string className, string windowName);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int maxCount);
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