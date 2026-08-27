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

public static class DPopUiProbeNative {
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumChildWindows(IntPtr hWndParent, EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern int GetDlgCtrlID(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);
}
'@
Add-Type -TypeDefinition $source -Language CSharp

function Get-ChildWindows([IntPtr]$Parent) {
    $items = New-Object System.Collections.Generic.List[object]
    $callback = [DPopUiProbeNative+EnumWindowsProc]{
        param([IntPtr]$hwnd, [IntPtr]$lParam)
        $text = New-Object System.Text.StringBuilder 512
        $class = New-Object System.Text.StringBuilder 256
        [void][DPopUiProbeNative]::GetWindowText($hwnd, $text, $text.Capacity)
        [void][DPopUiProbeNative]::GetClassName($hwnd, $class, $class.Capacity)
        $items.Add([pscustomobject]@{
            hwnd = ('0x{0:X}' -f $hwnd.ToInt64())
            id = [DPopUiProbeNative]::GetDlgCtrlID($hwnd)
            class = $class.ToString()
            text = $text.ToString()
            visible = [DPopUiProbeNative]::IsWindowVisible($hwnd)
        })
        return $true
    }
    [void][DPopUiProbeNative]::EnumChildWindows($Parent, $callback, [IntPtr]::Zero)
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
    $before = Get-ChildWindows $p.MainWindowHandle
    Write-Host '--- CHILD CONTROLS BEFORE SETTINGS ---'
    $before | Format-Table -AutoSize | Out-String -Width 240 | Write-Host

    $settings = $before | Where-Object {
        $_.class -eq 'Button' -and ($_.text -match '⚙|Настрой|Settings')
    } | Select-Object -First 1

    if ($settings) {
        $settingsHandle = [IntPtr]([Convert]::ToInt64(($settings.hwnd -replace '^0x',''), 16))
        [void][DPopUiProbeNative]::SendMessage($settingsHandle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) # BM_CLICK
        Start-Sleep -Milliseconds 800
        $after = Get-ChildWindows $p.MainWindowHandle
        Write-Host '--- CHILD CONTROLS AFTER SETTINGS CLICK ---'
        $after | Format-Table -AutoSize | Out-String -Width 240 | Write-Host
    } else {
        Write-Host 'SETTINGS_BUTTON_NOT_ENUMERATED'
    }
} finally {
    if (-not $p.HasExited) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    }
}
