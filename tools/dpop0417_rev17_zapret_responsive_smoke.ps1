[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$RootPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev17-zapret-responsive'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# rev.19 keeps the rev.17 gate name for release-contract compatibility, but the
# actual verifier is stronger: it preserves 1024/1366/1680 coverage, adds
# 1908x950, checks the semantic rows, compact service actions and ghost theme.
& (Join-Path $PSScriptRoot 'dpop0417_rev19_zapret_cleanup_smoke.ps1') `
    -RootPath $RootPath `
    -OutputDir $OutputDir `
    -SkipTray
if (-not $?) { throw 'rev.17 compatibility responsive smoke failed through rev.19 verifier.' }

$legacyReport = Join-Path $OutputDir 'rev17-zapret-responsive-report.json'
if (-not (Test-Path -LiteralPath $legacyReport -PathType Leaf)) {
    throw 'rev.17 compatibility responsive report was not produced.'
}
Write-Host 'REV17_ZAPRET_RESPONSIVE_SMOKE_OK'
