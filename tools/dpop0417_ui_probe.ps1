[CmdletBinding()]
param(
    [string]$ExePath = 'downloads/DPopCleaner_0.2.14_BETA.exe'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$source = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class DPopUiProbeItem {
    public IntPtr Handle;
    public int Id;
    public string ClassName = "";
    public string Text = "";
    public bool Visible;
    public int Left;
    public int Top;
    public int Right;
    public int Bottom;
}

public static class DPopUiProbeNative {
    private delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    private struct RECT { public int Left, Top, Right, Bottom; }

    [StructLayout(LayoutKind.Sequential)]
    private struct POINT { public int X, Y; }

    [DllImport("user32.dll")]
    private static extern bool EnumChildWindows(IntPtr hWndParent, EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

    [DllImport("user32.dll")]
    private static extern int GetDlgCtrlID(IntPtr hWnd);

    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    private static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    private static extern bool ScreenToClient(IntPtr hWnd, ref POINT point);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr CreateWindowEx(
        uint exStyle, string className, string windowName, uint style,
        int x, int y, int width, int height, IntPtr parent, IntPtr menu,
        IntPtr instance, IntPtr param);

    [DllImport("kernel32.dll")]
    private static extern IntPtr GetModuleHandle(string moduleName);

    [DllImport("user32.dll")]
    private static extern bool DestroyWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    public static DPopUiProbeItem[] GetChildren(IntPtr parent) {
        var items = new List<DPopUiProbeItem>();
        EnumWindowsProc callback = delegate(IntPtr hwnd, IntPtr _) {
            var text = new StringBuilder(512);
            var cls = new StringBuilder(256);
            RECT r;
            GetWindowText(hwnd, text, text.Capacity);
            GetClassName(hwnd, cls, cls.Capacity);
            GetWindowRect(hwnd, out r);
            items.Add(new DPopUiProbeItem {
                Handle = hwnd,
                Id = GetDlgCtrlID(hwnd),
                ClassName = cls.ToString(),
                Text = text.ToString(),
                Visible = IsWindowVisible(hwnd),
                Left = r.Left, Top = r.Top, Right = r.Right, Bottom = r.Bottom
            });
            return true;
        };
        EnumChildWindows(parent, callback, IntPtr.Zero);
        GC.KeepAlive(callback);
        return items.ToArray();
    }

    public static IntPtr CreateAutoUpdateCheckbox(IntPtr parent, IntPtr anchor) {
        RECT r;
        if (!GetWindowRect(anchor, out r)) return IntPtr.Zero;
        var pt = new POINT { X = r.Left, Y = r.Bottom + 8 };
        if (!ScreenToClient(parent, ref pt)) return IntPtr.Zero;
        const uint WS_CHILD = 0x40000000;
        const uint WS_VISIBLE = 0x10000000;
        const uint WS_TABSTOP = 0x00010000;
        const uint BS_AUTOCHECKBOX = 0x00000003;
        return CreateWindowEx(0, "Button", "Включить автообновление",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            pt.X, pt.Y, 250, 24, parent, new IntPtr(1490), GetModuleHandle(null), IntPtr.Zero);
    }

    public static void Destroy(IntPtr hWnd) {
        if (hWnd != IntPtr.Zero) DestroyWindow(hWnd);
    }
}
'@
Add-Type -TypeDefinition $source -Language CSharp

function Get-Children([IntPtr]$Parent) {
    return @([DPopUiProbeNative]::GetChildren($Parent))
}

function Show-VisibleSettings([IntPtr]$Parent) {
    $items = Get-Children $Parent
    $rows = foreach ($item in $items | Where-Object { $_.Visible -and ($_.Id -ge 1400 -or $_.Text -eq 'Настройки' -or $_.Text -eq 'Лицензия') }) {
        [pscustomobject]@{
            id = $item.Id
            class = $item.ClassName
            text = $item.Text
            left = $item.Left
            top = $item.Top
            right = $item.Right
            bottom = $item.Bottom
        }
    }
    $rows | Format-Table -AutoSize | Out-String -Width 260 | Write-Host
    return $items
}

$exe = (Resolve-Path -LiteralPath $ExePath).Path
$p = Start-Process -FilePath $exe -PassThru
$added = [IntPtr]::Zero
try {
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    do {
        Start-Sleep -Milliseconds 250
        $p.Refresh()
    } while ($p.MainWindowHandle -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)

    if ($p.MainWindowHandle -eq [IntPtr]::Zero) {
        throw 'Original DPopCleaner did not create a top-level window.'
    }

    $before = Get-Children $p.MainWindowHandle
    Write-Host '--- INITIAL VISIBLE BUTTONS ---'
    $before | Where-Object { $_.Visible -and $_.ClassName -eq 'Button' } | Select-Object Id,Text,Left,Top,Right,Bottom | Format-Table -AutoSize | Out-String -Width 260 | Write-Host

    $settings = $before | Where-Object { $_.Id -eq 906 -and $_.ClassName -eq 'Button' } | Select-Object -First 1
    if (-not $settings) { throw 'Settings button id=906 was not found.' }

    [void][DPopUiProbeNative]::SendMessage($settings.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 900
    Write-Host '--- VISIBLE SETTINGS BEFORE BRIDGE CONTROL ---'
    $settingsChildren = Show-VisibleSettings $p.MainWindowHandle

    $anchor = $settingsChildren | Where-Object { $_.Id -eq 1410 -and $_.ClassName -eq 'Button' } | Select-Object -First 1
    if (-not $anchor) { throw 'Settings anchor id=1410 was not found.' }

    $added = [DPopUiProbeNative]::CreateAutoUpdateCheckbox($p.MainWindowHandle, $anchor.Handle)
    if ($added -eq [IntPtr]::Zero) {
        $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "Cross-process CreateWindowEx failed: $err"
    }
    Start-Sleep -Milliseconds 300

    $after = Get-Children $p.MainWindowHandle
    $checkbox = $after | Where-Object { $_.Id -eq 1490 -and $_.Text -eq 'Включить автообновление' -and $_.Visible } | Select-Object -First 1
    if (-not $checkbox) { throw 'Auto-update checkbox was not visible inside original DPopCleaner settings.' }

    Write-Host ("CROSS_PROCESS_CHECKBOX_OK hwnd=0x{0:X} rect={1},{2}-{3},{4}" -f $checkbox.Handle.ToInt64(), $checkbox.Left, $checkbox.Top, $checkbox.Right, $checkbox.Bottom)
} finally {
    if ($added -ne [IntPtr]::Zero) { [DPopUiProbeNative]::Destroy($added) }
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
}
