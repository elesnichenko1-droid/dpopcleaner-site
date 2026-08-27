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
}

public static class DPopUiProbeNative {
    private delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);

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
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    public static DPopUiProbeItem[] GetChildren(IntPtr parent) {
        var items = new List<DPopUiProbeItem>();
        EnumWindowsProc callback = delegate(IntPtr hwnd, IntPtr _) {
            var text = new StringBuilder(512);
            var cls = new StringBuilder(256);
            GetWindowText(hwnd, text, text.Capacity);
            GetClassName(hwnd, cls, cls.Capacity);
            items.Add(new DPopUiProbeItem {
                Handle = hwnd,
                Id = GetDlgCtrlID(hwnd),
                ClassName = cls.ToString(),
                Text = text.ToString(),
                Visible = IsWindowVisible(hwnd)
            });
            return true;
        };
        EnumChildWindows(parent, callback, IntPtr.Zero);
        GC.KeepAlive(callback);
        return items.ToArray();
    }
}
'@
Add-Type -TypeDefinition $source -Language CSharp

function Show-Children([IntPtr]$Parent, [string]$Title) {
    Write-Host $Title
    $items = [DPopUiProbeNative]::GetChildren($Parent)
    $rows = foreach ($item in $items) {
        [pscustomobject]@{
            hwnd = ('0x{0:X}' -f $item.Handle.ToInt64())
            id = $item.Id
            class = $item.ClassName
            text = $item.Text
            visible = $item.Visible
        }
    }
    $rows | Format-Table -AutoSize | Out-String -Width 260 | Write-Host
    return @($items)
}

$exe = (Resolve-Path -LiteralPath $ExePath).Path
$p = Start-Process -FilePath $exe -PassThru
try {
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    do {
        Start-Sleep -Milliseconds 250
        $p.Refresh()
    } while ($p.MainWindowHandle -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)

    if ($p.MainWindowHandle -eq [IntPtr]::Zero) {
        throw 'Original DPopCleaner did not create a top-level window.'
    }

    Write-Host ('MAIN HWND: 0x{0:X}' -f $p.MainWindowHandle.ToInt64())
    $before = Show-Children $p.MainWindowHandle '--- CHILD CONTROLS BEFORE SETTINGS ---'

    $settings = $before | Where-Object {
        $_.ClassName -eq 'Button' -and ($_.Text -match '⚙|Настрой|Settings')
    } | Select-Object -First 1

    if ($settings) {
        Write-Host ("SETTINGS BUTTON: id={0}, text='{1}'" -f $settings.Id, $settings.Text)
        [void][DPopUiProbeNative]::SendMessage($settings.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 1000
        [void](Show-Children $p.MainWindowHandle '--- CHILD CONTROLS AFTER SETTINGS CLICK ---')
    } else {
        Write-Host 'SETTINGS_BUTTON_NOT_ENUMERATED'
    }
} finally {
    if (-not $p.HasExited) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    }
}
