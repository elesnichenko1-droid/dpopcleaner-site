[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$InstallerPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev18-user-report'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$InstallerPath = if ([IO.Path]::IsPathRooted($InstallerPath)) { $InstallerPath } else { Join-Path $repoRoot $InstallerPath }
$InstallerPath = [IO.Path]::GetFullPath($InstallerPath)
$OutputDir = if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repoRoot $OutputDir }
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
if (-not (Test-Path -LiteralPath $InstallerPath -PathType Leaf)) { throw "rev.18 installer missing: $InstallerPath" }
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$installRoot = Join-Path ([IO.Path]::GetTempPath()) ('dpop0417-rev18-user-' + [Guid]::NewGuid().ToString('N'))
$installed = $false
try {
    $install = Start-Process -FilePath $InstallerPath -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',"/DIR=$installRoot") -Wait -PassThru
    if ($install.ExitCode -ne 0) { throw "rev.18 silent install failed: $($install.ExitCode)" }
    $installed = $true
    & (Join-Path $PSScriptRoot 'dpop0417_rev19_zapret_cleanup_smoke.ps1') -RootPath $installRoot -OutputDir $OutputDir
    if (-not $?) { throw 'rev.18 canonical tray/ghost regression failed through rev.19 visual verifier.' }
    if (-not (Test-Path -LiteralPath (Join-Path $OutputDir 'rev18-user-report-smoke.json') -PathType Leaf)) {
        throw 'rev.18 compatibility user-report smoke report was not produced.'
    }
    Write-Host 'REV18_INSTALLED_USER_REPORT_SMOKE_OK'
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
