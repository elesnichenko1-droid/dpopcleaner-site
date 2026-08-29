using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace DPopCleaner.SimpleUpdate
{
    internal sealed class ZapretVisualPolishHost : IDisposable
    {
        internal const int VersionStatusProxyId = 1726;

        private const string FallbackZapretVersion = "1.10.2";
        private const string VersionClassName = "DPopCleanerZapretVersionProxy";
        private const int GWL_STYLE = -16;
        private const long BS_TYPEMASK = 0x0000000F;
        private const long BS_OWNERDRAW = 0x0000000B;
        private const uint WS_CHILD = 0x40000000;
        private const uint WS_VISIBLE = 0x10000000;
        private const uint WM_DRAWITEM = 0x002B;
        private const uint WM_PAINT = 0x000F;
        private const uint WM_ERASEBKGND = 0x0014;
        private const uint DT_LEFT = 0x0000;
        private const uint DT_CENTER = 0x0001;
        private const uint DT_VCENTER = 0x0004;
        private const uint DT_SINGLELINE = 0x0020;
        private const int TRANSPARENT = 1;
        private const uint ODS_SELECTED = 0x0001;
        private const uint ODS_DISABLED = 0x0004;
        private const uint ODT_BUTTON = 4;
        private const uint SubclassId = 0xD510;

        private static readonly int[] BridgeButtonIds =
        {
            ZapretEnhancementHost.RepairBroadcastButtonId,
            ZapretEnhancementHost.RepairConnectionButtonId,
            ZapretEnhancementHost.GameFilterButtonId,
            ZapretEnhancementHost.ManagerButtonId,
            ZapretEnhancementHost.LegacyCheckVersionButtonId,
            ZapretEnhancementHost.LegacyDownloadButtonId
        };

        private static readonly object Sync = new object();
        private static readonly Dictionary<IntPtr, ZapretVisualPolishHost> ToolbarOwners = new Dictionary<IntPtr, ZapretVisualPolishHost>();
        private static readonly Dictionary<IntPtr, ZapretVisualPolishHost> VersionOwners = new Dictionary<IntPtr, ZapretVisualPolishHost>();
        private static readonly SubclassProc ToolbarSubclassDelegate = StaticToolbarSubclassProc;
        private static readonly WindowProc VersionWndProcDelegate = StaticVersionWndProc;
        private static readonly IntPtr PageBrush = CreateSolidBrush(Rgb(12, 17, 23));
        private static readonly IntPtr ButtonBrush = CreateSolidBrush(Rgb(18, 27, 38));
        private static readonly IntPtr ButtonPressedBrush = CreateSolidBrush(Rgb(27, 39, 53));
        private static readonly IntPtr ButtonBorderBrush = CreateSolidBrush(Rgb(45, 61, 78));
        private static bool _versionClassRegistered;

        private readonly IntPtr _parent;
        private readonly string _applicationRoot;
        private readonly HashSet<IntPtr> _buttons = new HashSet<IntPtr>();
        private readonly HashSet<IntPtr> _toolbarParents = new HashSet<IntPtr>();
        private IntPtr _legacyVersionStatus;
        private IntPtr _versionProxy;
        private IntPtr _versionFont;
        private NativeBridge.ClientBounds _versionBounds;
        private bool _disposed;

        [StructLayout(LayoutKind.Sequential)]
        private struct RECT { public int Left, Top, Right, Bottom; }

        [StructLayout(LayoutKind.Sequential)]
        private struct DRAWITEMSTRUCT
        {
            public uint CtlType;
            public uint CtlID;
            public uint itemID;
            public uint itemAction;
            public uint itemState;
            public IntPtr hwndItem;
            public IntPtr hDC;
            public RECT rcItem;
            public UIntPtr itemData;
        }

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
        private struct PAINTSTRUCT
        {
            public IntPtr hdc;
            public bool fErase;
            public RECT rcPaint;
            public bool fRestore;
            public bool fIncUpdate;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public byte[] rgbReserved;
        }

        private delegate IntPtr SubclassProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam, UIntPtr subclassId, UIntPtr refData);
        private delegate IntPtr WindowProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);

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
        private static extern IntPtr GetParent(IntPtr hwnd);

        [DllImport("user32.dll")]
        private static extern bool DestroyWindow(IntPtr hwnd);

        [DllImport("user32.dll")]
        private static extern bool InvalidateRect(IntPtr hwnd, IntPtr rect, bool erase);

        [DllImport("user32.dll")]
        private static extern bool GetClientRect(IntPtr hwnd, out RECT rect);

        [DllImport("user32.dll")]
        private static extern IntPtr BeginPaint(IntPtr hwnd, out PAINTSTRUCT paint);

        [DllImport("user32.dll")]
        private static extern bool EndPaint(IntPtr hwnd, ref PAINTSTRUCT paint);

        [DllImport("user32.dll")]
        private static extern int FillRect(IntPtr hdc, ref RECT rect, IntPtr brush);

        [DllImport("user32.dll")]
        private static extern int FrameRect(IntPtr hdc, ref RECT rect, IntPtr brush);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern int DrawText(IntPtr hdc, string text, int count, ref RECT rect, uint format);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int maxCount);

        [DllImport("gdi32.dll")]
        private static extern IntPtr CreateSolidBrush(uint colorRef);

        [DllImport("gdi32.dll")]
        private static extern uint SetTextColor(IntPtr hdc, uint colorRef);

        [DllImport("gdi32.dll")]
        private static extern int SetBkMode(IntPtr hdc, int mode);

        [DllImport("gdi32.dll")]
        private static extern IntPtr SelectObject(IntPtr hdc, IntPtr obj);

        [DllImport("comctl32.dll", SetLastError = true)]
        private static extern bool SetWindowSubclass(IntPtr hwnd, SubclassProc proc, UIntPtr subclassId, UIntPtr refData);

        [DllImport("comctl32.dll")]
        private static extern IntPtr DefSubclassProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);

        [DllImport("comctl32.dll")]
        private static extern bool RemoveWindowSubclass(IntPtr hwnd, SubclassProc proc, UIntPtr subclassId);

        [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW")]
        private static extern IntPtr GetWindowLongPtr64(IntPtr hwnd, int index);

        [DllImport("user32.dll", EntryPoint = "SetWindowLongPtrW")]
        private static extern IntPtr SetWindowLongPtr64(IntPtr hwnd, int index, IntPtr value);

        [DllImport("user32.dll", EntryPoint = "GetWindowLongW")]
        private static extern int GetWindowLong32(IntPtr hwnd, int index);

        [DllImport("user32.dll", EntryPoint = "SetWindowLongW")]
        private static extern int SetWindowLong32(IntPtr hwnd, int index, int value);

        internal ZapretVisualPolishHost(IntPtr parent, string applicationRoot)
        {
            if (parent == IntPtr.Zero) throw new ArgumentException("Parent window is required.", "parent");
            _parent = parent;
            _applicationRoot = Path.GetFullPath(applicationRoot ?? string.Empty);
            EnsureVersionClass();
            CreateVersionStatusProxy();
            Show();
        }

        internal void Show()
        {
            if (_disposed) return;
            EnsureDarkBridgeButtons();
            RefreshVersionStatusProxy();
            if (_versionProxy != IntPtr.Zero) NativeBridge.ShowWindow(_versionProxy, NativeBridge.SW_SHOW);
        }

        internal void Hide()
        {
            if (_disposed) return;
            if (_versionProxy != IntPtr.Zero) NativeBridge.ShowWindow(_versionProxy, NativeBridge.SW_HIDE);
        }

        private void EnsureDarkBridgeButtons()
        {
            foreach (var id in BridgeButtonIds)
            {
                var button = NativeBridge.FindChildById(_parent, id);
                if (button == IntPtr.Zero) continue;
                if (_buttons.Add(button))
                {
                    SetOwnerDrawStyle(button);
                    var toolbar = GetParent(button);
                    if (toolbar != IntPtr.Zero && _toolbarParents.Add(toolbar))
                    {
                        lock (Sync) ToolbarOwners[toolbar] = this;
                        SetWindowSubclass(toolbar, ToolbarSubclassDelegate, new UIntPtr(SubclassId), UIntPtr.Zero);
                    }
                }
                InvalidateRect(button, IntPtr.Zero, true);
            }

            var version = GetInstalledZapretVersion();
            var game = NativeBridge.FindChildById(_parent, ZapretEnhancementHost.GameFilterButtonId);
            var manager = NativeBridge.FindChildById(_parent, ZapretEnhancementHost.ManagerButtonId);
            NativeBridge.WriteWindowText(game, "Игровой фильтр " + version);
            NativeBridge.WriteWindowText(manager, "Менеджер " + version);
        }

        private void CreateVersionStatusProxy()
        {
            foreach (var child in NativeBridge.GetChildren(_parent))
            {
                if (!string.Equals(child.ClassName, "Static", StringComparison.OrdinalIgnoreCase)) continue;
                var text = (child.Text ?? string.Empty).Trim();
                if (!text.StartsWith("Zapret ", StringComparison.OrdinalIgnoreCase)) continue;
                _legacyVersionStatus = child.Handle;
                break;
            }
            if (_legacyVersionStatus == IntPtr.Zero) return;

            _versionBounds = NativeBridge.GetChildClientBounds(_parent, _legacyVersionStatus);
            if (_versionBounds == null) return;
            _versionFont = NativeBridge.SendMessage(_legacyVersionStatus, NativeBridge.WM_GETFONT, IntPtr.Zero, IntPtr.Zero);
            _versionProxy = CreateWindowEx(0, VersionClassName, string.Empty, WS_CHILD | WS_VISIBLE,
                _versionBounds.Left, _versionBounds.Top, _versionBounds.Width, _versionBounds.Height,
                _parent, new IntPtr(VersionStatusProxyId), GetModuleHandle(null), IntPtr.Zero);
            if (_versionProxy == IntPtr.Zero) throw new InvalidOperationException("Could not create Zapret version status proxy.");
            lock (Sync) VersionOwners[_versionProxy] = this;
            NativeBridge.ShowWindow(_legacyVersionStatus, NativeBridge.SW_HIDE);
            RefreshVersionStatusProxy();
        }

        private void RefreshVersionStatusProxy()
        {
            if (_legacyVersionStatus == IntPtr.Zero || _versionProxy == IntPtr.Zero) return;
            var version = GetInstalledZapretVersion();
            var legacyText = NativeBridge.ReadWindowText(_legacyVersionStatus) ?? string.Empty;
            var bullet = legacyText.IndexOf('•');
            var suffix = bullet >= 0 ? "  " + legacyText.Substring(bullet).TrimStart() : string.Empty;
            NativeBridge.WriteWindowText(_versionProxy, "Zapret " + version + suffix);
            NativeBridge.ShowWindow(_legacyVersionStatus, NativeBridge.SW_HIDE);
            NativeBridge.PositionChildWindow(_versionProxy, _versionBounds);
            InvalidateRect(_versionProxy, IntPtr.Zero, true);
        }

        private string GetInstalledZapretVersion()
        {
            try
            {
                var path = Path.Combine(_applicationRoot, "Zapret", ".service", "version.txt");
                if (File.Exists(path))
                {
                    var value = (File.ReadAllText(path) ?? string.Empty).Trim();
                    if (!string.IsNullOrWhiteSpace(value)) return value;
                }
            }
            catch { }
            return FallbackZapretVersion;
        }

        private static void SetOwnerDrawStyle(IntPtr button)
        {
            var style = GetStyle(button);
            style = (style & ~BS_TYPEMASK) | BS_OWNERDRAW;
            SetStyle(button, style);
        }

        private static long GetStyle(IntPtr hwnd)
        {
            return IntPtr.Size == 8 ? GetWindowLongPtr64(hwnd, GWL_STYLE).ToInt64() : GetWindowLong32(hwnd, GWL_STYLE);
        }

        private static void SetStyle(IntPtr hwnd, long style)
        {
            if (IntPtr.Size == 8) SetWindowLongPtr64(hwnd, GWL_STYLE, new IntPtr(style));
            else SetWindowLong32(hwnd, GWL_STYLE, unchecked((int)style));
        }

        private bool OwnsButton(IntPtr hwnd)
        {
            return _buttons.Contains(hwnd);
        }

        private static IntPtr StaticToolbarSubclassProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam, UIntPtr subclassId, UIntPtr refData)
        {
            ZapretVisualPolishHost owner;
            lock (Sync) ToolbarOwners.TryGetValue(hwnd, out owner);
            if (owner != null && msg == WM_DRAWITEM && lParam != IntPtr.Zero)
            {
                var item = (DRAWITEMSTRUCT)Marshal.PtrToStructure(lParam, typeof(DRAWITEMSTRUCT));
                if (item.CtlType == ODT_BUTTON && owner.OwnsButton(item.hwndItem))
                {
                    owner.DrawOwnerButton(ref item);
                    return new IntPtr(1);
                }
            }
            return DefSubclassProc(hwnd, msg, wParam, lParam);
        }

        private void DrawOwnerButton(ref DRAWITEMSTRUCT item)
        {
            var rect = item.rcItem;
            FillRect(item.hDC, ref rect, (item.itemState & ODS_SELECTED) != 0 ? ButtonPressedBrush : ButtonBrush);
            FrameRect(item.hDC, ref rect, ButtonBorderBrush);
            var text = new StringBuilder(256);
            GetWindowText(item.hwndItem, text, text.Capacity);
            SetBkMode(item.hDC, TRANSPARENT);
            SetTextColor(item.hDC, (item.itemState & ODS_DISABLED) != 0 ? Rgb(145, 154, 164) : Rgb(242, 246, 250));
            DrawText(item.hDC, text.ToString(), -1, ref rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        private static void EnsureVersionClass()
        {
            lock (Sync)
            {
                if (_versionClassRegistered) return;
                var wc = new WNDCLASSEX
                {
                    cbSize = (uint)Marshal.SizeOf(typeof(WNDCLASSEX)),
                    lpfnWndProc = Marshal.GetFunctionPointerForDelegate(VersionWndProcDelegate),
                    hInstance = GetModuleHandle(null),
                    hbrBackground = PageBrush,
                    lpszClassName = VersionClassName
                };
                var atom = RegisterClassEx(ref wc);
                if (atom == 0)
                {
                    var error = Marshal.GetLastWin32Error();
                    if (error != 1410) throw new InvalidOperationException("Could not register Zapret version proxy class. Win32=" + error);
                }
                _versionClassRegistered = true;
            }
        }

        private static IntPtr StaticVersionWndProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam)
        {
            ZapretVisualPolishHost owner;
            lock (Sync) VersionOwners.TryGetValue(hwnd, out owner);
            if (owner != null && msg == WM_PAINT)
            {
                owner.PaintVersion(hwnd);
                return IntPtr.Zero;
            }
            if (msg == WM_ERASEBKGND) return new IntPtr(1);
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        private void PaintVersion(IntPtr hwnd)
        {
            PAINTSTRUCT paint;
            var hdc = BeginPaint(hwnd, out paint);
            try
            {
                RECT rect;
                GetClientRect(hwnd, out rect);
                FillRect(hdc, ref rect, PageBrush);
                FrameRect(hdc, ref rect, ButtonBorderBrush);
                var oldFont = IntPtr.Zero;
                if (_versionFont != IntPtr.Zero) oldFont = SelectObject(hdc, _versionFont);
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, Rgb(242, 246, 250));
                var text = new StringBuilder(512);
                GetWindowText(hwnd, text, text.Capacity);
                rect.Left += 7;
                DrawText(hdc, text.ToString(), -1, ref rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                if (oldFont != IntPtr.Zero) SelectObject(hdc, oldFont);
            }
            finally
            {
                EndPaint(hwnd, ref paint);
            }
        }

        private static uint Rgb(byte r, byte g, byte b)
        {
            return (uint)(r | (g << 8) | (b << 16));
        }

        public void Dispose()
        {
            if (_disposed) return;
            _disposed = true;
            foreach (var toolbar in _toolbarParents)
            {
                try { RemoveWindowSubclass(toolbar, ToolbarSubclassDelegate, new UIntPtr(SubclassId)); } catch { }
                lock (Sync) ToolbarOwners.Remove(toolbar);
            }
            _toolbarParents.Clear();
            _buttons.Clear();
            if (_versionProxy != IntPtr.Zero)
            {
                lock (Sync) VersionOwners.Remove(_versionProxy);
                try { DestroyWindow(_versionProxy); } catch { }
                _versionProxy = IntPtr.Zero;
            }
        }
    }
}
