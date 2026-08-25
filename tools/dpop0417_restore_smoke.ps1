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
    throw "RestoreCenter.exe not found: $ExePath"
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$root = Join-Path ([IO.Path]::GetTempPath()) 'dpop0417-restore-smoke'
$documentationRoot = Join-Path $root 'Documentation'
$fixtureRoot = Join-Path $root 'fixture'
$target = Join-Path $fixtureRoot 'settings.json'
$reportPath = Join-Path $OutputDir 'restore-smoke-report.json'
$screenshotPath = Join-Path $OutputDir 'restore-center-1100x700.png'

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class DPop0417RestoreSmokeWin32 {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll", SetLastError=true)] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll", SetLastError=true)] public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll", SetLastError=true)] public static extern bool MoveWindow(IntPtr hWnd, int x, int y, int w, int h, bool repaint);
    [DllImport("user32.dll", SetLastError=true)] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
}
'@

function Invoke-RestoreCli([string[]]$Arguments, [int[]]$AllowedExitCodes = @(0)) {
    $process = Start-Process -FilePath $ExePath -ArgumentList $Arguments -Wait -PassThru
    if ($AllowedExitCodes -notcontains $process.ExitCode) {
        throw "RestoreCenter CLI failed with exit code $($process.ExitCode): $($Arguments -join ' ')"
    }
    return $process.ExitCode
}

function Resize-Client([IntPtr]$Handle, [int]$Width, [int]$Height) {
    $wr = New-Object DPop0417RestoreSmokeWin32+RECT
    $cr = New-Object DPop0417RestoreSmokeWin32+RECT
    if (-not [DPop0417RestoreSmokeWin32]::GetWindowRect($Handle, [ref]$wr)) { throw 'GetWindowRect failed.' }
    if (-not [DPop0417RestoreSmokeWin32]::GetClientRect($Handle, [ref]$cr)) { throw 'GetClientRect failed.' }
    $outerWidth = $Width + (($wr.Right - $wr.Left) - ($cr.Right - $cr.Left))
    $outerHeight = $Height + (($wr.Bottom - $wr.Top) - ($cr.Bottom - $cr.Top))
    if (-not [DPop0417RestoreSmokeWin32]::MoveWindow($Handle, $wr.Left, $wr.Top, $outerWidth, $outerHeight, $true)) {
        throw 'MoveWindow failed.'
    }
    Start-Sleep -Milliseconds 500
}

function Capture-Window([IntPtr]$Handle, [string]$Path) {
    $rect = New-Object DPop0417RestoreSmokeWin32+RECT
    if (-not [DPop0417RestoreSmokeWin32]::GetWindowRect($Handle, [ref]$rect)) { throw 'Capture GetWindowRect failed.' }
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -lt 1000 -or $height -lt 650) { throw "Unexpected Restore Center window size: $width x $height" }

    $bitmap = New-Object Drawing.Bitmap($width, $height)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $hdc = $graphics.GetHdc()
    try {
        if (-not [DPop0417RestoreSmokeWin32]::PrintWindow($Handle, $hdc, 2)) {
            if (-not [DPop0417RestoreSmokeWin32]::PrintWindow($Handle, $hdc, 0)) {
                throw 'PrintWindow failed.'
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

$uiProcess = $null
try {
    if (Test-Path -LiteralPath $root) {
        Remove-Item -LiteralPath $root -Recurse -Force
    }
    New-Item -ItemType Directory -Path $documentationRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null
    [IO.File]::WriteAllText($target, 'before', [Text.UTF8Encoding]::new($false))

    $env:DPOP0417_SMOKE = '1'

    # reversible-roundtrip: create a real reversible file-state history record and backup.
    [void](Invoke-RestoreCli @('--smoke-create-file-record', $documentationRoot, $target))

    $historyFiles = @(Get-ChildItem -LiteralPath (Join-Path $documentationRoot 'History') -Filter '*.json' -File)
    if ($historyFiles.Count -ne 1) { throw "Expected one initial history record, got $($historyFiles.Count)." }
    $initialRecord = Get-Content -Raw -LiteralPath $historyFiles[0].FullName | ConvertFrom-Json
    if ([string]$initialRecord.OperationId -ne 'settings.file') { throw 'Initial record is not settings.file.' }
    if (-not [bool]$initialRecord.RollbackAvailable) { throw 'Initial record is not reversible.' }
    if ([string]::IsNullOrWhiteSpace([string]$initialRecord.BackupReference)) { throw 'Initial record has no backup reference.' }

    $originalBackup = Join-Path (Join-Path $documentationRoot 'Backups') (([string]$initialRecord.BackupReference).Replace('/', [IO.Path]::DirectorySeparatorChar))
    if (-not (Test-Path -LiteralPath $originalBackup -PathType Leaf)) { throw 'Original backup was not created.' }

    [IO.File]::WriteAllText($target, 'after', [Text.UTF8Encoding]::new($false))
    [void](Invoke-RestoreCli @('--smoke-restore-latest', $documentationRoot))

    $restored = [IO.File]::ReadAllText($target)
    $reversibleRoundtrip = $restored -eq 'before'
    if (-not $reversibleRoundtrip) { throw "reversible-roundtrip failed; target contains '$restored'." }

    $backupPreserved = Test-Path -LiteralPath $originalBackup -PathType Leaf
    if (-not $backupPreserved) { throw 'Original backup was consumed or removed by restore.' }

    $historyAfterRestore = @(Get-ChildItem -LiteralPath (Join-Path $documentationRoot 'History') -Filter '*.json' -File | ForEach-Object {
        Get-Content -Raw -LiteralPath $_.FullName | ConvertFrom-Json
    })
    if (-not ($historyAfterRestore | Where-Object { $_.OperationId -eq 'restore.prestate' })) { throw 'restore.prestate record missing.' }
    if (-not ($historyAfterRestore | Where-Object { $_.OperationId -eq 'restore.success' })) { throw 'restore.success record missing.' }

    # nonreversible: append a record that must never expose a successful rollback path.
    [void](Invoke-RestoreCli @('--smoke-create-nonreversible', $documentationRoot, $target))
    $beforeNonReversibleAttempt = [IO.File]::ReadAllText($target)
    $nonReversibleExit = Invoke-RestoreCli @('--smoke-restore-latest', $documentationRoot) @(4)
    $afterNonReversibleAttempt = [IO.File]::ReadAllText($target)
    $nonreversibleRestoreExposed = -not ($nonReversibleExit -eq 4 -and $beforeNonReversibleAttempt -eq $afterNonReversibleAttempt)
    if ($nonreversibleRestoreExposed) { throw 'nonreversible rollback was exposed or modified the target.' }

    # Visual evidence uses the same temporary history through a smoke-only constrained argument.
    if (Test-Path -LiteralPath $screenshotPath) { Remove-Item -LiteralPath $screenshotPath -Force }
    $uiProcess = Start-Process -FilePath $ExePath -ArgumentList @('--smoke-documentation-root', $documentationRoot, '--lang', 'ru') -PassThru
    $deadline = (Get-Date).AddSeconds(30)
    do {
        Start-Sleep -Milliseconds 200
        $uiProcess.Refresh()
        if ($uiProcess.HasExited) { throw "Restore Center UI exited early with code $($uiProcess.ExitCode)." }
    } while ($uiProcess.MainWindowHandle -eq 0 -and (Get-Date) -lt $deadline)
    if ($uiProcess.MainWindowHandle -eq 0) { throw 'Restore Center main window timeout.' }
    if ($uiProcess.MainWindowTitle -notmatch 'Центр восстановления') { throw "Unexpected window title: $($uiProcess.MainWindowTitle)" }

    $main = [IntPtr]$uiProcess.MainWindowHandle
    Resize-Client $main 1100 700
    Start-Sleep -Milliseconds 500
    Capture-Window $main $screenshotPath
    if ((Get-Item -LiteralPath $screenshotPath).Length -lt 10000) { throw 'Restore Center screenshot is unexpectedly small.' }

    [void][DPop0417RestoreSmokeWin32]::SendMessage($main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    if (-not $uiProcess.WaitForExit(10000)) { throw 'Restore Center did not close after WM_CLOSE.' }
    if ($uiProcess.ExitCode -ne 0) { throw "Restore Center UI exited with code $($uiProcess.ExitCode)." }
    $uiProcess = $null

    [pscustomobject]@{
        target = [IO.Path]::GetFullPath($target)
        reversible_roundtrip = [bool]$reversibleRoundtrip
        nonreversible_restore_exposed = [bool]$nonreversibleRestoreExposed
        backup_preserved = [bool]$backupPreserved
        history_records = @(Get-ChildItem -LiteralPath (Join-Path $documentationRoot 'History') -Filter '*.json' -File).Count
        screenshot = $screenshotPath
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $reportPath -Encoding utf8

    Write-Host "reversible-roundtrip: PASS"
    Write-Host "nonreversible: PASS"
    Write-Host "backup_preserved: PASS"
    Write-Host "restore-smoke-report.json: $reportPath"
    Write-Host "restore-center-1100x700.png: $screenshotPath"
}
finally {
    if ($null -ne $uiProcess -and -not $uiProcess.HasExited) {
        Stop-Process -Id $uiProcess.Id -Force -ErrorAction SilentlyContinue
    }
    Remove-Item Env:DPOP0417_SMOKE -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $root) {
        Remove-Item -LiteralPath $root -Recurse -Force
    }
}
