[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [Parameter(Mandatory = $true)][string]$OutputDir
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exePath = if ([IO.Path]::IsPathRooted($Exe)) { $Exe } else { Join-Path $repoRoot $Exe }
$outputPath = if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repoRoot $OutputDir }
$exePath = [IO.Path]::GetFullPath($exePath)
$outputPath = [IO.Path]::GetFullPath($outputPath)
if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) { throw "DPopCleaner.exe not found: $exePath" }
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null

Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class DPopScreenshotNative {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint flags);
    [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int command);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
}
'@

$settingsPath = Join-Path $outputPath 'capture-settings.ini'
Set-Content -LiteralPath $settingsPath -Encoding utf8 -Value "[updates]`r`nauto_check=0`r`n`r`n[zapret]`r`nstrategy=general.bat`r`n"
$previousSettings = $env:DPOP0418_SETTINGS_PATH
$env:DPOP0418_SETTINGS_PATH = $settingsPath

function Get-MainWindowHandle([Diagnostics.Process]$Process) {
    for ($i = 0; $i -lt 100; $i++) {
        $Process.Refresh()
        if ($Process.HasExited) { throw "DPopCleaner exited before screenshot capture (exit $($Process.ExitCode))." }
        if ($Process.MainWindowHandle -ne [IntPtr]::Zero) { return $Process.MainWindowHandle }
        Start-Sleep -Milliseconds 50
    }
    throw 'DPopCleaner main window did not appear in time.'
}

function Capture-Page([int]$NavCommand, [string]$FileName) {
    $process = Start-Process -FilePath $exePath -PassThru
    try {
        try { [void]$process.WaitForInputIdle(5000) } catch { }
        $hwnd = Get-MainWindowHandle $process
        [void][DPopScreenshotNative]::ShowWindow($hwnd, 9)
        [void][DPopScreenshotNative]::SetForegroundWindow($hwnd)
        [void][DPopScreenshotNative]::SendMessageW($hwnd, 0x0111, [IntPtr]$NavCommand, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 650

        $rect = New-Object DPopScreenshotNative+RECT
        if (-not [DPopScreenshotNative]::GetWindowRect($hwnd, [ref]$rect)) { throw 'GetWindowRect failed.' }
        $width = $rect.Right - $rect.Left
        $height = $rect.Bottom - $rect.Top
        if ($width -lt 1000 -or $height -lt 600) { throw "Unexpected DPopCleaner window size: ${width}x${height}" }

        $bitmap = New-Object System.Drawing.Bitmap($width, $height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $hdc = $graphics.GetHdc()
        try {
            if (-not [DPopScreenshotNative]::PrintWindow($hwnd, $hdc, 2)) { throw 'PrintWindow failed.' }
        }
        finally {
            $graphics.ReleaseHdc($hdc)
            $graphics.Dispose()
        }

        $target = Join-Path $outputPath $FileName
        $bitmap.Save($target, [System.Drawing.Imaging.ImageFormat]::Png)
        $bitmap.Dispose()
        if ((Get-Item -LiteralPath $target).Length -lt 15000) { throw "Captured PNG is unexpectedly small: $target" }
        Write-Host "Captured current UI: $target"
    }
    finally {
        if (-not $process.HasExited) {
            try { [void]$process.CloseMainWindow() } catch { }
            if (-not $process.WaitForExit(2500)) { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue }
        }
    }
}

try {
    Capture-Page 1000 'dpopcleaner-0.4.18-overview.png'
    Capture-Page 1004 'dpopcleaner-0.4.18-zapret.png'
    Capture-Page 1007 'dpopcleaner-0.4.18-settings.png'
}
finally {
    if ($null -eq $previousSettings) { Remove-Item Env:DPOP0418_SETTINGS_PATH -ErrorAction SilentlyContinue }
    else { $env:DPOP0418_SETTINGS_PATH = $previousSettings }
    Remove-Item -LiteralPath $settingsPath -Force -ErrorAction SilentlyContinue
}
