param(
    [Parameter(Mandatory = $true)]
    [string]$Exe
)

$ErrorActionPreference = 'Stop'
$resolvedExe = (Resolve-Path -LiteralPath $Exe).Path
$tempRoot = Join-Path $env:RUNNER_TEMP 'dpop0418-close-smoke'
if (-not $env:RUNNER_TEMP) { $tempRoot = Join-Path $env:TEMP 'dpop0418-close-smoke' }
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
$settings = Join-Path $tempRoot 'settings.ini'
Set-Content -LiteralPath $settings -Encoding Ascii -Value "[updates]`r`nauto_check=1`r`n"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class DPopWin32 {
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
}
"@

$oldSlow = $env:DPOP0418_TEST_SLOW_UPDATE_MS
$oldSettings = $env:DPOP0418_SETTINGS_PATH
$env:DPOP0418_TEST_SLOW_UPDATE_MS = '10000'
$env:DPOP0418_SETTINGS_PATH = $settings

try {
    $process = Start-Process -FilePath $resolvedExe -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    $handle = [IntPtr]::Zero
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 50
        $process.Refresh()
        if ($process.HasExited) { throw "DPopCleaner exited before close smoke could run (code $($process.ExitCode))." }
        if ($process.MainWindowHandle -ne 0) {
            $handle = [IntPtr]$process.MainWindowHandle
            break
        }
    }
    if ($handle -eq [IntPtr]::Zero) { throw 'DPopCleaner main window was not created.' }

    # Let the startup timer launch the intentionally slow update worker.
    Start-Sleep -Milliseconds 900
    $process.Refresh()
    if ($process.HasExited) { throw 'DPopCleaner exited before WM_CLOSE.' }

    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    if (-not [DPopWin32]::PostMessage($handle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)) {
        throw 'PostMessage(WM_CLOSE) failed.'
    }

    while (-not $process.HasExited -and $watch.ElapsedMilliseconds -lt 500) {
        Start-Sleep -Milliseconds 10
        $process.Refresh()
    }
    $watch.Stop()

    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw "DPopCleaner did not terminate within 500 ms; elapsed=$($watch.ElapsedMilliseconds) ms."
    }

    Write-Host "DPopCleaner close smoke passed in $($watch.ElapsedMilliseconds) ms while update worker was intentionally slow."
}
finally {
    $env:DPOP0418_TEST_SLOW_UPDATE_MS = $oldSlow
    $env:DPOP0418_SETTINGS_PATH = $oldSettings
}
