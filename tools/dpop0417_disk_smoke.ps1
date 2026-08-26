[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ExePath,
    [Parameter(Mandatory)][string]$OutputDir
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ExePath = [IO.Path]::GetFullPath($ExePath)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$fixtureBase = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [IO.Path]::GetTempPath() }
$Fixture = Join-Path $fixtureBase 'dpop0417-disk-fixture'
$report = Join-Path $OutputDir 'disk-smoke-report.json'
$screenshot = Join-Path $OutputDir 'disk-analyzer-1200x850.png'

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class DPop0417DiskSmokeWin32 {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll", SetLastError=true)] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll", SetLastError=true)] public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll", SetLastError=true)] public static extern bool MoveWindow(IntPtr hWnd, int x, int y, int width, int height, bool repaint);
    [DllImport("user32.dll", SetLastError=true)] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);
}
'@

function Write-FixedFile([string]$Path, [int]$Length, [byte]$Value) {
    $bytes = New-Object byte[] $Length
    for ($i = 0; $i -lt $bytes.Length; $i++) { $bytes[$i] = $Value }
    [IO.File]::WriteAllBytes($Path, $bytes)
}

function Resize-Client([IntPtr]$Handle, [int]$Width, [int]$Height) {
    $windowRect = New-Object DPop0417DiskSmokeWin32+RECT
    $clientRect = New-Object DPop0417DiskSmokeWin32+RECT
    if (-not [DPop0417DiskSmokeWin32]::GetWindowRect($Handle, [ref]$windowRect)) {
        throw 'GetWindowRect failed while resizing Disk Analyzer.'
    }
    if (-not [DPop0417DiskSmokeWin32]::GetClientRect($Handle, [ref]$clientRect)) {
        throw 'GetClientRect failed while resizing Disk Analyzer.'
    }

    $nonClientWidth = ($windowRect.Right - $windowRect.Left) - ($clientRect.Right - $clientRect.Left)
    $nonClientHeight = ($windowRect.Bottom - $windowRect.Top) - ($clientRect.Bottom - $clientRect.Top)
    $outerWidth = $Width + $nonClientWidth
    $outerHeight = $Height + $nonClientHeight
    if (-not [DPop0417DiskSmokeWin32]::MoveWindow($Handle, 10, 10, $outerWidth, $outerHeight, $true)) {
        throw 'MoveWindow failed while setting Disk Analyzer smoke viewport.'
    }
    Start-Sleep -Milliseconds 500

    $verify = New-Object DPop0417DiskSmokeWin32+RECT
    if (-not [DPop0417DiskSmokeWin32]::GetClientRect($Handle, [ref]$verify)) {
        throw 'GetClientRect failed while verifying Disk Analyzer viewport.'
    }
    $actualWidth = $verify.Right - $verify.Left
    $actualHeight = $verify.Bottom - $verify.Top
    if ($actualWidth -lt 1000 -or $actualHeight -lt 700) {
        throw "Disk Analyzer constrained client is too small: $actualWidth x $actualHeight"
    }
    return [pscustomobject]@{ Width = $actualWidth; Height = $actualHeight }
}

function Capture-Window([IntPtr]$Handle, [string]$Path) {
    $rect = New-Object DPop0417DiskSmokeWin32+RECT
    if (-not [DPop0417DiskSmokeWin32]::GetWindowRect($Handle, [ref]$rect)) {
        throw 'GetWindowRect failed for Disk Analyzer.'
    }
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -lt 1000 -or $height -lt 700) {
        throw "Disk Analyzer window is unexpectedly small: $width x $height"
    }

    $bitmap = New-Object Drawing.Bitmap($width, $height)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $hdc = $graphics.GetHdc()
    try {
        if (-not [DPop0417DiskSmokeWin32]::PrintWindow($Handle, $hdc, 2)) {
            if (-not [DPop0417DiskSmokeWin32]::PrintWindow($Handle, $hdc, 0)) {
                throw 'PrintWindow failed for Disk Analyzer.'
            }
        }
    }
    finally {
        $graphics.ReleaseHdc($hdc)
        $graphics.Dispose()
    }

    try {
        $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }
}

$p = $null
try {
    if (Test-Path -LiteralPath $Fixture) {
        Remove-Item $Fixture -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Fixture -Force | Out-Null
    $sub = Join-Path $Fixture 'sub'
    New-Item -ItemType Directory -Path $sub -Force | Out-Null
    Write-FixedFile (Join-Path $Fixture 'a.bin') 100 0x41
    Write-FixedFile (Join-Path $sub 'b.bin') 300 0x42

    if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report -Force }
    if (Test-Path -LiteralPath $screenshot) { Remove-Item -LiteralPath $screenshot -Force }

    $arguments = @(
        '--root', ('"' + $Fixture + '"'),
        '--smoke-report', ('"' + $report + '"'),
        '--lang', 'ru'
    )
    $p = Start-Process -FilePath $ExePath -ArgumentList $arguments -PassThru

    $windowDeadline = (Get-Date).AddSeconds(30)
    do {
        Start-Sleep -Milliseconds 200
        $p.Refresh()
        if ($p.HasExited) { throw "Disk Analyzer exited early with code $($p.ExitCode)." }
    } while ($p.MainWindowHandle -eq 0 -and (Get-Date) -lt $windowDeadline)
    if ($p.MainWindowHandle -eq 0) { throw 'Disk Analyzer main window timeout.' }

    $actualClient = Resize-Client ([IntPtr]$p.MainWindowHandle) 1200 850

    $reportDeadline = (Get-Date).AddSeconds(30)
    while (-not (Test-Path -LiteralPath $report) -and (Get-Date) -lt $reportDeadline) {
        Start-Sleep -Milliseconds 200
        $p.Refresh()
        if ($p.HasExited) { throw "Disk Analyzer exited before smoke report with code $($p.ExitCode)." }
    }
    if (-not (Test-Path -LiteralPath $report)) { throw 'disk-smoke-report.json timeout.' }

    $data = Get-Content -Raw -LiteralPath $report | ConvertFrom-Json
    if ([int64]$data.logical_bytes -ne 400) { throw "Unexpected logical bytes: $($data.logical_bytes)" }
    if ([int64]$data.file_count -ne 2) { throw "Unexpected file count: $($data.file_count)" }
    if ([int64]$data.folder_count -ne 1) { throw "Unexpected folder count: $($data.folder_count)" }
    if ([int]$data.row_count -ne 4) { throw "Unexpected grid row count: $($data.row_count)" }
    if (@($data.columns).Count -ne 7) { throw "Unexpected column count: $(@($data.columns).Count)" }
    if ([string]$data.target -ne 'DPopCleaner 0.4.17 Disk Analyzer') { throw "Unexpected target: $($data.target)" }
    if (-not ($data.PSObject.Properties.Name -contains 'toolbar_overflow')) { throw 'Disk smoke report lacks toolbar_overflow.' }
    if ([bool]$data.toolbar_overflow) { throw 'Disk Analyzer toolbar overflows the constrained viewport.' }

    Capture-Window ([IntPtr]$p.MainWindowHandle) $screenshot
    if (-not (Test-Path -LiteralPath $screenshot -PathType Leaf)) { throw 'Disk Analyzer screenshot missing.' }
    if ((Get-Item -LiteralPath $screenshot).Length -lt 10000) { throw 'Disk Analyzer screenshot is unexpectedly small.' }

    [pscustomobject]@{
        target = [string]$data.target
        fixture = $Fixture
        logical_bytes = [int64]$data.logical_bytes
        file_count = [int64]$data.file_count
        folder_count = [int64]$data.folder_count
        row_count = [int]$data.row_count
        columns = @($data.columns)
        requested_client_size = '1200x850'
        actual_client_size = ("{0}x{1}" -f $actualClient.Width, $actualClient.Height)
        toolbar_overflow = [bool]$data.toolbar_overflow
        screenshot = [IO.Path]::GetFileName($screenshot)
        app_report = [IO.Path]::GetFileName($report)
    } | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OutputDir 'disk-smoke-summary.json') -Encoding utf8

    [void]$p.CloseMainWindow()
    if (-not $p.WaitForExit(10000)) {
        Stop-Process -Id $p.Id -Force
        throw 'Disk Analyzer did not close after smoke capture.'
    }
    if ($p.ExitCode -ne 0) { throw "Disk Analyzer exited with code $($p.ExitCode)." }
}
finally {
    if ($null -ne $p -and -not $p.HasExited) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    }
    if (Test-Path -LiteralPath $Fixture) {
        Remove-Item $Fixture -Recurse -Force
    }
}
