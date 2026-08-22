[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ExecutablePath,
    [Parameter(Mandatory)][string]$OutputPath,
    [int]$TimeoutSeconds = 30
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not (Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) {
    throw "Application executable does not exist: $ExecutablePath"
}

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class DPopWindowCapture {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);
}
'@

$startedAt = Get-Date
$process = Start-Process -FilePath $ExecutablePath -PassThru
try {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        Start-Sleep -Milliseconds 250
        $process.Refresh()
        if ($process.HasExited) { throw "DPopCleaner exited before its main window appeared (exit $($process.ExitCode))." }
    } while ($process.MainWindowHandle -eq 0 -and (Get-Date) -lt $deadline)
    if ($process.MainWindowHandle -eq 0) { throw 'Timed out waiting for the DPopCleaner main window.' }

    $rect = New-Object DPopWindowCapture+RECT
    if (-not [DPopWindowCapture]::GetWindowRect($process.MainWindowHandle, [ref]$rect)) {
        throw 'GetWindowRect failed for DPopCleaner.'
    }
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -lt 800 -or $height -lt 500) { throw "Unexpected DPopCleaner window size: ${width}x${height}." }

    $outputDirectory = Split-Path -Parent ([IO.Path]::GetFullPath($OutputPath))
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    $bitmap = [Drawing.Bitmap]::new($width, $height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        try {
            $hdc = $graphics.GetHdc()
            try {
                if (-not [DPopWindowCapture]::PrintWindow($process.MainWindowHandle, $hdc, 2)) {
                    throw 'PrintWindow failed for DPopCleaner.'
                }
            } finally { $graphics.ReleaseHdc($hdc) }
        } finally { $graphics.Dispose() }
        $bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
    } finally { $bitmap.Dispose() }

    if ((Get-Item -LiteralPath $OutputPath).Length -le 10000) {
        throw 'Captured screenshot is unexpectedly small.'
    }
    [pscustomobject]@{
        process_id = $process.Id
        executable_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $ExecutablePath).Hash.ToLowerInvariant()
        screenshot_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $OutputPath).Hash.ToLowerInvariant()
        width = $width
        height = $height
        started_at = $startedAt.ToUniversalTime().ToString('o')
    }
} finally {
    if (-not $process.HasExited) {
        $null = $process.CloseMainWindow()
        if (-not $process.WaitForExit(10000)) {
            Stop-Process -Id $process.Id -Force
        }
    }
}
