[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ExePath,
    [Parameter(Mandatory)][string]$OutputDir
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ExePath = [IO.Path]::GetFullPath($ExePath)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
    throw "DPopCleaner executable does not exist: $ExePath"
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

# 0.3.5 defaults WM_CLOSE to minimize-to-tray. The shared visual smoke needs
# deterministic process shutdown, so isolate its settings and explicitly use
# Exit semantics. Older recovery builds ignore DPOP_SETTINGS_ROOT.
$smokeSettingsRoot = Join-Path $env:RUNNER_TEMP ('dpop-ui-smoke-settings-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $smokeSettingsRoot -Force | Out-Null
$env:DPOP_SETTINGS_ROOT = $smokeSettingsRoot
@'
{
  "schema_version": 2,
  "tray_enabled": false,
  "close_behavior": 0
}
'@ | Set-Content -LiteralPath (Join-Path $smokeSettingsRoot 'settings.json') -Encoding utf8

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class DPop033Win32
{
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool MoveWindow(
        IntPtr hWnd,
        int x,
        int y,
        int width,
        int height,
        bool repaint
    );

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int command);

    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint flags);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(
        IntPtr hWnd,
        uint message,
        IntPtr wParam,
        IntPtr lParam
    );
}
'@

function Get-RectSize {
    param([Parameter(Mandatory)]$Rect)
    return [pscustomobject]@{
        Width = [int]($Rect.Right - $Rect.Left)
        Height = [int]($Rect.Bottom - $Rect.Top)
    }
}

function Resize-Client {
    param(
        [Parameter(Mandatory)][IntPtr]$Handle,
        [Parameter(Mandatory)][int]$Width,
        [Parameter(Mandatory)][int]$Height
    )

    $windowRect = New-Object DPop033Win32+RECT
    $clientRect = New-Object DPop033Win32+RECT
    if (-not [DPop033Win32]::GetWindowRect($Handle, [ref]$windowRect)) {
        throw "GetWindowRect failed. Win32=$([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    if (-not [DPop033Win32]::GetClientRect($Handle, [ref]$clientRect)) {
        throw "GetClientRect failed. Win32=$([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }

    $window = Get-RectSize $windowRect
    $client = Get-RectSize $clientRect
    $outerWidth = $Width + ($window.Width - $client.Width)
    $outerHeight = $Height + ($window.Height - $client.Height)

    if (-not [DPop033Win32]::MoveWindow(
        $Handle,
        $windowRect.Left,
        $windowRect.Top,
        $outerWidth,
        $outerHeight,
        $true
    )) {
        throw "MoveWindow failed. Win32=$([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    Start-Sleep -Milliseconds 600
}

function Get-NearWhiteRatio {
    param([Parameter(Mandatory)][Drawing.Bitmap]$Bitmap)
    $nearWhite = 0
    $sampled = 0
    for ($y = 0; $y -lt $Bitmap.Height; $y += 4) {
        for ($x = 0; $x -lt $Bitmap.Width; $x += 4) {
            $pixel = $Bitmap.GetPixel($x, $y)
            $sampled++
            if ($pixel.R -ge 240 -and $pixel.G -ge 240 -and $pixel.B -ge 240) {
                $nearWhite++
            }
        }
    }
    if ($sampled -eq 0) {
        return 1.0
    }
    return [double]$nearWhite / [double]$sampled
}

function Capture-Window {
    param(
        [Parameter(Mandatory)][IntPtr]$Handle,
        [Parameter(Mandatory)][string]$Name
    )

    $rect = New-Object DPop033Win32+RECT
    if (-not [DPop033Win32]::GetWindowRect($Handle, [ref]$rect)) {
        throw "GetWindowRect failed for capture '$Name'."
    }
    $size = Get-RectSize $rect
    if ($size.Width -lt 200 -or $size.Height -lt 200) {
        throw "Invalid capture size for '$Name': $($size.Width)x$($size.Height)"
    }

    $bitmap = New-Object Drawing.Bitmap($size.Width, $size.Height)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $hdc = [IntPtr]::Zero
    try {
        $hdc = $graphics.GetHdc()
        $ok = [DPop033Win32]::PrintWindow($Handle, $hdc, 2)
        if (-not $ok) {
            # Some desktop sessions reject PW_RENDERFULLCONTENT but accept the
            # classic PrintWindow mode.
            $ok = [DPop033Win32]::PrintWindow($Handle, $hdc, 0)
        }
        if (-not $ok) {
            throw "PrintWindow failed for '$Name'. Win32=$([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
        }
    }
    finally {
        if ($hdc -ne [IntPtr]::Zero) {
            $graphics.ReleaseHdc($hdc)
        }
        $graphics.Dispose()
    }

    try {
        $ratio = Get-NearWhiteRatio $bitmap
        $path = Join-Path $OutputDir ($Name + '.png')
        $bitmap.Save($path, [Drawing.Imaging.ImageFormat]::Png)
        if ($ratio -gt 0.05) {
            throw "White-background regression detected in '$Name': ratio=$ratio"
        }
        return [pscustomobject][ordered]@{
            name = $Name
            path = $path
            width = $size.Width
            height = $size.Height
            near_white_ratio = $ratio
            bytes = (Get-Item -LiteralPath $path).Length
            sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

$process = $null
$report = [ordered]@{
    target = 'DPopCleaner 0.3.3 BETA R1'
    default = $null
    minimum = $null
    maximized = $null
    graceful_close = $false
}

try {
    $process = Start-Process -FilePath $ExePath -PassThru
    $deadline = (Get-Date).AddSeconds(30)
    do {
        Start-Sleep -Milliseconds 250
        $process.Refresh()
        if ($process.HasExited) {
            throw "DPopCleaner exited before its main window appeared. ExitCode=$($process.ExitCode)"
        }
    } while ($process.MainWindowHandle -eq 0 -and (Get-Date) -lt $deadline)

    if ($process.MainWindowHandle -eq 0) {
        throw 'Timed out waiting for DPopCleaner main window.'
    }

    $handle = [IntPtr]$process.MainWindowHandle

    Resize-Client -Handle $handle -Width 1200 -Height 850
    $report.default = Capture-Window -Handle $handle -Name 'default-1200x850'

    Resize-Client -Handle $handle -Width 1100 -Height 700
    $report.minimum = Capture-Window -Handle $handle -Name 'minimum-1100x700'

    [DPop033Win32]::ShowWindow($handle, 3) | Out-Null
    Start-Sleep -Milliseconds 900
    $report.maximized = Capture-Window -Handle $handle -Name 'maximized'

    [DPop033Win32]::SendMessage(
        $handle,
        0x0010,
        [IntPtr]::Zero,
        [IntPtr]::Zero
    ) | Out-Null

    if (-not $process.WaitForExit(10000)) {
        throw 'DPopCleaner did not exit within 10 seconds after WM_CLOSE.'
    }
    if ($process.ExitCode -ne 0) {
        throw "DPopCleaner exited with non-zero code $($process.ExitCode)."
    }
    $report.graceful_close = $true
}
finally {
    if ($null -ne $process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }
    Remove-Item Env:DPOP_SETTINGS_ROOT -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $smokeSettingsRoot -Recurse -Force -ErrorAction SilentlyContinue
}

[pscustomobject]$report |
    ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath (Join-Path $OutputDir 'ui-smoke-report.json') -Encoding utf8

if (-not $report.graceful_close) {
    throw 'UI smoke report says graceful_close=false.'
}
