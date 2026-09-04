[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$InstallerPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev19-zapret-cleanup'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$InstallerPath = if ([IO.Path]::IsPathRooted($InstallerPath)) { $InstallerPath } else { Join-Path $repoRoot $InstallerPath }
$InstallerPath = [IO.Path]::GetFullPath($InstallerPath)
$OutputDir = if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repoRoot $OutputDir }
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
if (-not (Test-Path -LiteralPath $InstallerPath -PathType Leaf)) { throw "rev.19 installer missing: $InstallerPath" }
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$installRoot = Join-Path ([IO.Path]::GetTempPath()) ('dpop0417-rev19-ui-' + [Guid]::NewGuid().ToString('N'))
$installed = $false
try {
    $install = Start-Process -FilePath $InstallerPath -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',"/DIR=$installRoot") -Wait -PassThru
    if ($install.ExitCode -ne 0) { throw "rev.19 silent install failed: $($install.ExitCode)" }
    $installed = $true
    & (Join-Path $PSScriptRoot 'dpop0417_rev19_zapret_cleanup_smoke.ps1') -RootPath $installRoot -OutputDir $OutputDir
    if (-not $?) { throw 'rev.19 installed Zapret cleanup smoke failed.' }
    if (-not (Test-Path -LiteralPath (Join-Path $OutputDir 'rev19-zapret-cleanup-report.json') -PathType Leaf)) {
        throw 'rev.19 installed Zapret cleanup report was not produced.'
    }
    & (Join-Path $PSScriptRoot 'dpop0417_rev19_remove_services_probe.ps1') -RootPath $installRoot -OutputDir $OutputDir
    if (-not $?) { throw 'rev.19 remove-services HWND probe failed.' }
    if (-not (Test-Path -LiteralPath (Join-Path $OutputDir 'rev19-remove-services-probe.json') -PathType Leaf)) {
        throw 'rev.19 remove-services probe report was not produced.'
    }
    Write-Host 'REV19_INSTALLED_ZAPRET_CLEANUP_SMOKE_OK'
}
finally {
    foreach ($name in @('DPopCleaner','DPopCleaner.Core','SimpleUpdate')) {
        foreach ($p in @(Get-Process -Name $name -ErrorAction SilentlyContinue)) {
            try { if ($p.Path -and [IO.Path]::GetFullPath($p.Path).StartsWith($installRoot,[StringComparison]::OrdinalIgnoreCase)) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } } catch { }
        }
    }
    if ($installed) {
        $uninstaller = Join-Path $installRoot 'unins000.exe'
        if (Test-Path -LiteralPath $uninstaller -PathType Leaf) { try { Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait | Out-Null } catch { } }
    }
    if (Test-Path -LiteralPath $installRoot) { Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue }
}
