using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace DPopCleaner.SimpleUpdate
{
    internal sealed class ZapretVisualPolishHost : IDisposable
    {
        private const string FallbackZapretVersion = "1.10.2";
        private const int GWL_STYLE = -16;
        private const long BS_TYPEMASK = 0x0000000F;
        private const long BS_OWNERDRAW = 0x0000000B;
        private const uint WM_DRAWITEM = 0x002B;
        private const uint DT_CENTER = 0x0001;
        private const uint DT_VCENTER = 0x0004;
        private const uint DT_SINGLELINE = 0x0020;
        private const int TRANSPARENT = 1;
        private const uint ODS_SELECTED = 0x0001;
        private const uint ODS_DISABLED = 0x0004;
        private const uint ODT_BUTTON = 4;
        private const uint ToolbarSubclassId = 0xD510;

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
        private static readonly SubclassProc ToolbarSubclassDelegate = StaticToolbarSubclassProc;
        private static readonly IntPtr ButtonBrush = CreateSolidBrush(Rgb(18, 27, 38));
        private static readonly IntPtr ButtonPressedBrush = CreateSolidBrush(Rgb(27, 39, 53));
        private static readonly IntPtr ButtonBorderBrush = CreateSolidBrush(Rgb(45, 61, 78));

        private readonly IntPtr _parent;
        private readonly string _applicationRoot;
        private readonly HashSet<IntPtr> _buttons = new HashSet<IntPtr>();
        private readonly HashSet<IntPtr> _toolbarParents = new HashSet<IntPtr>();
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

        private delegate IntPtr SubclassProc(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam, UIntPtr subclassId, UIntPtr refData);

        [DllImport("user32.dll")]
        private static extern IntPtr GetParent(IntPtr hwnd);

        [DllImport("user32.dll")]
        private static extern bool InvalidateRect(IntPtr hwnd, IntPtr rect, bool erase);

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
            Show();
        }

        internal void Show()
        {
            if (_disposed) return;
            EnsureDarkBridgeButtons();
        }

        internal void Hide()
        {
            // Rev.12 does not create, replace or rewrite any native Zapret version control.
            // The frozen core renders its own version from Zapret\utils\dpop_version.txt.
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
                        SetWindowSubclass(toolbar, ToolbarSubclassDelegate, new UIntPtr(ToolbarSubclassId), UIntPtr.Zero);
                    }
                    InvalidateRect(button, IntPtr.Zero, true);
                }
            }

            var version = GetInstalledZapretVersion();
            var game = NativeBridge.FindChildById(_parent, ZapretEnhancementHost.GameFilterButtonId);
            var manager = NativeBridge.FindChildById(_parent, ZapretEnhancementHost.ManagerButtonId);
            var gameText = "Игровой фильтр " + version;
            var managerText = "Менеджер " + version;
            if (game != IntPtr.Zero && !string.Equals(NativeBridge.ReadWindowText(game), gameText, StringComparison.Ordinal))
            {
                NativeBridge.WriteWindowText(game, gameText);
                InvalidateRect(game, IntPtr.Zero, true);
            }
            if (manager != IntPtr.Zero && !string.Equals(NativeBridge.ReadWindowText(manager), managerText, StringComparison.Ordinal))
            {
                NativeBridge.WriteWindowText(manager, managerText);
                InvalidateRect(manager, IntPtr.Zero, true);
            }
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
                try { RemoveWindowSubclass(toolbar, ToolbarSubclassDelegate, new UIntPtr(ToolbarSubclassId)); } catch { }
                lock (Sync) ToolbarOwners.Remove(toolbar);
            }
            _toolbarParents.Clear();
            _buttons.Clear();
        }
    }
}
