using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace DPopCleaner.SimpleUpdate
{
    internal static class NativeBridge
    {
        internal const int SettingsGearId = 906;
        internal const int AdminCheckboxId = 1410;
        internal const int LicenseBuyButtonId = 1407;
        internal const int LicenseSaveButtonId = 1408;
        internal const int SupportButtonId = 1406;
        internal const int AutoUpdateCheckboxId = 1490;
        internal const int CheckNowButtonId = 1491;
        internal const int SettingsScrollHostId = 1492;
        internal const int LicenseHeadingProxyId = 1493;
        internal const int LicenseKeyProxyId = 1494;
        internal const int LicenseSaveProxyId = 1495;
        internal const int LicenseBuyProxyId = 1496;
        internal const uint BM_GETCHECK = 0x00F0;
        internal const uint BM_SETCHECK = 0x00F1;
        internal const uint BM_CLICK = 0x00F5;
        internal const int BST_UNCHECKED = 0;
        internal const int BST_CHECKED = 1;
        internal const uint WM_SETICON = 0x0080;
        internal const uint WM_CLOSE = 0x0010;
        internal const uint WM_GETFONT = 0x0031;
        internal const int ICON_SMALL = 0;
        internal const int ICON_BIG = 1;
        internal const int SW_HIDE = 0;
        internal const int SW_SHOW = 5;

        private delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);

        [StructLayout(LayoutKind.Sequential)]
        private struct RECT { public int Left, Top, Right, Bottom; }

        [StructLayout(LayoutKind.Sequential)]
        private struct POINT { public int X, Y; }

        internal sealed class ChildInfo
        {
            public IntPtr Handle;
            public int Id;
            public string Text;
            public string ClassName;
            public bool Visible;
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
        }

        internal sealed class ClientBounds
        {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
            public int Width { get { return Math.Max(0, Right - Left); } }
            public int Height { get { return Math.Max(0, Bottom - Top); } }
        }

        [DllImport("user32.dll")]
        private static extern bool EnumChildWindows(IntPtr hWndParent, EnumWindowsProc lpEnumFunc, IntPtr lParam);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern bool SetWindowText(IntPtr hWnd, string text);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

        [DllImport("user32.dll")]
        private static extern int GetDlgCtrlID(IntPtr hWnd);

        [DllImport("user32.dll")]
        internal static extern bool IsWindowVisible(IntPtr hWnd);

        [DllImport("user32.dll")]
        private static extern bool IsChild(IntPtr parent, IntPtr child);

        [DllImport("user32.dll")]
        private static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

        [DllImport("user32.dll")]
        private static extern bool ScreenToClient(IntPtr hWnd, ref POINT point);

        [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr CreateWindowEx(uint exStyle, string className, string windowName, uint style,
            int x, int y, int width, int height, IntPtr parent, IntPtr menu, IntPtr instance, IntPtr param);

        [DllImport("kernel32.dll")]
        private static extern IntPtr GetModuleHandle(string moduleName);

        [DllImport("user32.dll")]
        internal static extern IntPtr SendMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);

        [DllImport("user32.dll", SetLastError = true)]
        internal static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);

        [DllImport("user32.dll")]
        internal static extern bool ShowWindow(IntPtr hWnd, int command);

        [DllImport("user32.dll")]
        private static extern bool SetWindowPos(IntPtr hWnd, IntPtr after, int x, int y, int cx, int cy, uint flags);

        [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
        private static extern uint ExtractIconEx(string file, int index, IntPtr[] large, IntPtr[] small, uint count);

        internal static ChildInfo[] GetChildren(IntPtr parent)
        {
            var list = new List<ChildInfo>();
            EnumWindowsProc callback = delegate(IntPtr hwnd, IntPtr _)
            {
                var text = new StringBuilder(512);
                var cls = new StringBuilder(128);
                RECT rect;
                GetWindowText(hwnd, text, text.Capacity);
                GetClassName(hwnd, cls, cls.Capacity);
                GetWindowRect(hwnd, out rect);
                list.Add(new ChildInfo
                {
                    Handle = hwnd,
                    Id = GetDlgCtrlID(hwnd),
                    Text = text.ToString(),
                    ClassName = cls.ToString(),
                    Visible = IsWindowVisible(hwnd),
                    Left = rect.Left,
                    Top = rect.Top,
                    Right = rect.Right,
                    Bottom = rect.Bottom
                });
                return true;
            };
            EnumChildWindows(parent, callback, IntPtr.Zero);
            GC.KeepAlive(callback);
            return list.ToArray();
        }

        internal static IntPtr FindChildById(IntPtr parent, int id)
        {
            foreach (var child in GetChildren(parent)) if (child.Id == id) return child.Handle;
            return IntPtr.Zero;
        }

        internal static ClientBounds GetChildClientBounds(IntPtr parent, IntPtr child)
        {
            if (parent == IntPtr.Zero || child == IntPtr.Zero) return null;
            RECT rect;
            if (!GetWindowRect(child, out rect)) return null;
            var topLeft = new POINT { X = rect.Left, Y = rect.Top };
            var bottomRight = new POINT { X = rect.Right, Y = rect.Bottom };
            if (!ScreenToClient(parent, ref topLeft) || !ScreenToClient(parent, ref bottomRight)) return null;
            return new ClientBounds
            {
                Left = topLeft.X,
                Top = topLeft.Y,
                Right = bottomRight.X,
                Bottom = bottomRight.Y
            };
        }

        internal static ClientBounds GetAdditionalSettingsBounds(IntPtr parent, IntPtr adminAnchor)
        {
            var admin = GetChildClientBounds(parent, adminAnchor);
            if (admin == null) return null;
            var supportHandle = FindChildById(parent, SupportButtonId);
            var support = GetChildClientBounds(parent, supportHandle);
            var x = Math.Max(22, admin.Left - 10);
            var y = admin.Bottom + 8;
            var available = support != null ? support.Top - y - 10 : 190;
            var height = Math.Max(130, Math.Min(190, available));
            return new ClientBounds { Left = x, Top = y, Right = x + 500, Bottom = y + height };
        }

        internal static IntPtr FindLegacyLicenseEdit(IntPtr parent, ClientBounds hostBounds)
        {
            if (hostBounds == null) return IntPtr.Zero;
            foreach (var child in GetChildren(parent))
            {
                if (!string.Equals(child.ClassName, "Edit", StringComparison.OrdinalIgnoreCase)) continue;
                var bounds = GetChildClientBounds(parent, child.Handle);
                if (bounds == null) continue;
                if (bounds.Left < hostBounds.Right && bounds.Right > hostBounds.Left &&
                    bounds.Top >= hostBounds.Top && bounds.Top < hostBounds.Bottom)
                    return child.Handle;
            }
            return IntPtr.Zero;
        }

        internal static string ReadWindowText(IntPtr handle)
        {
            if (handle == IntPtr.Zero) return string.Empty;
            var text = new StringBuilder(1024);
            GetWindowText(handle, text, text.Capacity);
            return text.ToString();
        }

        internal static void WriteWindowText(IntPtr handle, string text)
        {
            if (handle != IntPtr.Zero) SetWindowText(handle, text ?? string.Empty);
        }

        internal static void ClickButton(IntPtr handle)
        {
            if (handle != IntPtr.Zero) SendMessage(handle, BM_CLICK, IntPtr.Zero, IntPtr.Zero);
        }

        internal static void PositionChildWindow(IntPtr handle, ClientBounds bounds)
        {
            if (handle == IntPtr.Zero || bounds == null) return;
            SetWindowPos(handle, IntPtr.Zero, bounds.Left, bounds.Top, bounds.Width, bounds.Height, 0x0004 | 0x0010);
        }

        internal static void HideLegacyOverflowControls(IntPtr parent, IntPtr scrollHost, ClientBounds hostBounds)
        {
            if (hostBounds == null) return;
            foreach (var child in GetChildren(parent))
            {
                if (!child.Visible) continue;
                if (child.Handle == scrollHost || (scrollHost != IntPtr.Zero && IsChild(scrollHost, child.Handle))) continue;
                if (child.Id == SupportButtonId || child.Id >= AutoUpdateCheckboxId) continue;
                var bounds = GetChildClientBounds(parent, child.Handle);
                if (bounds == null) continue;
                var intersects = bounds.Left < hostBounds.Right && bounds.Right > hostBounds.Left &&
                                 bounds.Top < hostBounds.Bottom && bounds.Bottom > hostBounds.Top;
                if (intersects) ShowWindow(child.Handle, SW_HIDE);
            }
        }

        internal static void HideLegacyVersionBadge(IntPtr parent)
        {
            foreach (var child in GetChildren(parent))
            {
                if (!child.Visible) continue;
                var text = (child.Text ?? string.Empty).Trim();
                if (string.Equals(text, "v0.2.11 BETA", StringComparison.OrdinalIgnoreCase) ||
                    (text.StartsWith("v0.", StringComparison.OrdinalIgnoreCase) && text.EndsWith("BETA", StringComparison.OrdinalIgnoreCase)))
                    ShowWindow(child.Handle, SW_HIDE);
            }
        }

        internal static IntPtr CreateAutoUpdateCheckbox(IntPtr parent, IntPtr adminAnchor, bool isChecked)
        {
            RECT rect;
            if (!GetWindowRect(adminAnchor, out rect)) return IntPtr.Zero;
            var point = new POINT { X = rect.Left, Y = rect.Bottom + 8 };
            if (!ScreenToClient(parent, ref point)) return IntPtr.Zero;

            const uint WS_CHILD = 0x40000000;
            const uint WS_VISIBLE = 0x10000000;
            const uint WS_TABSTOP = 0x00010000;
            const uint BS_AUTOCHECKBOX = 0x00000003;
            var checkbox = CreateWindowEx(0, "Button", "Включить автообновление",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                point.X, point.Y, 280, 24, parent, new IntPtr(AutoUpdateCheckboxId), GetModuleHandle(null), IntPtr.Zero);
            if (checkbox != IntPtr.Zero)
                SendMessage(checkbox, BM_SETCHECK, new IntPtr(isChecked ? BST_CHECKED : BST_UNCHECKED), IntPtr.Zero);
            return checkbox;
        }

        internal static IntPtr CreateCheckNowButton(IntPtr parent, IntPtr adminAnchor)
        {
            RECT rect;
            if (!GetWindowRect(adminAnchor, out rect)) return IntPtr.Zero;
            var point = new POINT { X = rect.Left, Y = rect.Bottom + 6 };
            if (!ScreenToClient(parent, ref point)) return IntPtr.Zero;

            const uint WS_CHILD = 0x40000000;
            const uint WS_VISIBLE = 0x10000000;
            const uint WS_TABSTOP = 0x00010000;
            const uint BS_AUTOCHECKBOX = 0x00000003;
            const uint BS_PUSHLIKE = 0x00001000;
            return CreateWindowEx(0, "Button", "Проверить сейчас",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX | BS_PUSHLIKE,
                point.X + 300, point.Y, 160, 27, parent, new IntPtr(CheckNowButtonId), GetModuleHandle(null), IntPtr.Zero);
        }

        internal static void MakeRoomForAutoUpdate(IntPtr parent)
        {
            foreach (var child in GetChildren(parent))
            {
                if (!child.Visible) continue;
                var isLicense = child.Id == LicenseBuyButtonId || child.Id == LicenseSaveButtonId ||
                                (child.Id == 0 && (string.Equals(child.Text, "Лицензия", StringComparison.Ordinal) || child.Left < 530 && child.Top >= 476 && child.Top < 690));
                if (!isLicense) continue;

                var topLeft = new POINT { X = child.Left, Y = child.Top };
                var bottomRight = new POINT { X = child.Right, Y = child.Bottom };
                if (!ScreenToClient(parent, ref topLeft) || !ScreenToClient(parent, ref bottomRight)) continue;
                SetWindowPos(child.Handle, IntPtr.Zero, topLeft.X, topLeft.Y + 30,
                    bottomRight.X - topLeft.X, bottomRight.Y - topLeft.Y, 0x0004 | 0x0010);
            }
        }

        internal static void ApplyExecutableIcon(IntPtr window, string executablePath)
        {
            if (window == IntPtr.Zero || string.IsNullOrWhiteSpace(executablePath)) return;
            var large = new IntPtr[1];
            var small = new IntPtr[1];
            if (ExtractIconEx(executablePath, 0, large, small, 1) == 0) return;
            if (large[0] != IntPtr.Zero) SendMessage(window, WM_SETICON, new IntPtr(ICON_BIG), large[0]);
            if (small[0] != IntPtr.Zero) SendMessage(window, WM_SETICON, new IntPtr(ICON_SMALL), small[0]);
        }
    }
}
