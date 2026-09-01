[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$InstallerPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev15-restart'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$InstallerPath = if ([IO.Path]::IsPathRooted($InstallerPath)) { $InstallerPath } else { Join-Path $repoRoot $InstallerPath }
$OutputDir = if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repoRoot $OutputDir }
$InstallerPath = [IO.Path]::GetFullPath($InstallerPath)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
if (-not (Test-Path -LiteralPath $InstallerPath -PathType Leaf)) { throw "rev.15 installer missing: $InstallerPath" }
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$installRoot = Join-Path ([IO.Path]::GetTempPath()) ('dpop0417-rev15-restart-' + [Guid]::NewGuid().ToString('N'))
$installed = $false
try {
    $install = Start-Process -FilePath $InstallerPath -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',"/DIR=$installRoot") -Wait -PassThru
    if ($install.ExitCode -ne 0) { throw "rev.15 silent install failed: $($install.ExitCode)" }
    $installed = $true

    & (Join-Path $PSScriptRoot 'dpop0417_rev15_restart_recovery_smoke.ps1') -RootPath $installRoot
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    [pscustomobject]@{
        installer = $InstallerPath
        install_root = $installRoot
        restart_bridge = $true
        ram_tray_after_restart = $true
    } | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $OutputDir 'rev15-installed-restart-smoke-report.json') -Encoding utf8

    Write-Host 'REV15_INSTALLED_LANGUAGE_RESTART_BRIDGE_SMOKE_OK'
    Write-Host 'REV15_INSTALLED_LANGUAGE_RESTART_RAM_TRAY_SMOKE_OK'
}
finally {
    if ($installed) {
        $uninstaller = Join-Path $installRoot 'unins000.exe'
        if (Test-Path -LiteralPath $uninstaller -PathType Leaf) {
            try { Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait | Out-Null } catch { }
        }
    }
    if (Test-Path -LiteralPath $installRoot) { Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue }
}
