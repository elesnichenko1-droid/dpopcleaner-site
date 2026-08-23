[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RepositoryRoot,
    [Parameter(Mandatory)][string]$Destination
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$resolvedRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$resolvedDestination = [IO.Path]::GetFullPath($Destination)

& (Join-Path $resolvedRoot 'scripts/Prepare-R3Source.ps1') `
    -RepositoryRoot $resolvedRoot `
    -Destination $resolvedDestination

Copy-Item (Join-Path $resolvedRoot 'v032/CMakeLists.txt') (Join-Path $resolvedDestination 'CMakeLists.txt') -Force
Copy-Item (Join-Path $resolvedRoot 'v032/MainWindow.cpp') (Join-Path $resolvedDestination 'src/app/MainWindow.cpp') -Force
Copy-Item (Join-Path $resolvedRoot 'v032/Version.h') (Join-Path $resolvedDestination 'src/core/Version.h') -Force
Copy-Item (Join-Path $resolvedRoot 'v032/version.rc.in') (Join-Path $resolvedDestination 'resources/version.rc.in') -Force

$uiRoot = Join-Path $resolvedRoot 'v032/ui'
if (Test-Path -LiteralPath $uiRoot -PathType Container) {
    Copy-Item $uiRoot (Join-Path $resolvedDestination 'src/ui') -Recurse -Force
}

$moduleRoot = Join-Path $resolvedRoot 'v032/modules'
if (Test-Path -LiteralPath $moduleRoot -PathType Container) {
    New-Item -ItemType Directory -Path (Join-Path $resolvedDestination 'src/modules') -Force | Out-Null
    Copy-Item (Join-Path $moduleRoot '*') (Join-Path $resolvedDestination 'src/modules') -Recurse -Force
}

$testsRoot = Join-Path $resolvedRoot 'v032/tests'
if (Test-Path -LiteralPath $testsRoot -PathType Container) {
    $preparedTests = Join-Path $resolvedDestination 'tests/v032'
    New-Item -ItemType Directory -Path $preparedTests -Force | Out-Null
    Copy-Item (Join-Path $testsRoot '*') $preparedTests -Recurse -Force
}

$overlayRoot = Join-Path $resolvedRoot 'v032'
$inventory = @(
    Get-ChildItem $overlayRoot -File -Recurse |
        Sort-Object FullName |
        ForEach-Object {
            [pscustomobject][ordered]@{
                path = $_.FullName.Substring($overlayRoot.Length + 1).Replace('\', '/')
                sha256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        }
)
$inventory | ConvertTo-Json -Depth 4 |
    Set-Content (Join-Path $resolvedDestination 'v032-overlay-inventory.json') -Encoding utf8
Write-Host "Prepared DPopCleaner 0.3.2 FULL overlay at $resolvedDestination"
