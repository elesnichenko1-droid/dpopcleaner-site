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

function Invoke-RestoreCli([string[]]$Arguments, [int[]]$AllowedExitCodes = @(0)) {
    $process = Start-Process -FilePath $ExePath -ArgumentList $Arguments -Wait -PassThru
    if ($AllowedExitCodes -notcontains $process.ExitCode) {
        throw "RestoreCenter CLI failed with exit code $($process.ExitCode): $($Arguments -join ' ')"
    }
    return $process.ExitCode
}

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

    [pscustomobject]@{
        target = [IO.Path]::GetFullPath($target)
        reversible_roundtrip = [bool]$reversibleRoundtrip
        nonreversible_restore_exposed = [bool]$nonreversibleRestoreExposed
        backup_preserved = [bool]$backupPreserved
        history_records = @(Get-ChildItem -LiteralPath (Join-Path $documentationRoot 'History') -Filter '*.json' -File).Count
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $reportPath -Encoding utf8

    Write-Host "reversible-roundtrip: PASS"
    Write-Host "nonreversible: PASS"
    Write-Host "backup_preserved: PASS"
    Write-Host "restore-smoke-report.json: $reportPath"
}
finally {
    Remove-Item Env:DPOP0417_SMOKE -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $root) {
        Remove-Item -LiteralPath $root -Recurse -Force
    }
}
