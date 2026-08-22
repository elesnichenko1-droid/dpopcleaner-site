[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RepositoryRoot,
    [Parameter(Mandatory)][string]$Destination
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$modulePath = Join-Path $PSScriptRoot 'R3ReleasePolicy.psm1'
Import-Module $modulePath -Force

$resolvedRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$resolvedDestination = [IO.Path]::GetFullPath($Destination)
if ($resolvedDestination.TrimEnd([IO.Path]::DirectorySeparatorChar) -eq $resolvedRoot.TrimEnd([IO.Path]::DirectorySeparatorChar)) {
    throw 'The prepared source destination must not be the repository root.'
}

if (Test-Path -LiteralPath $resolvedDestination) {
    $existing = @(Get-ChildItem -LiteralPath $resolvedDestination -Force)
    if ($existing.Count -gt 0) {
        throw "Prepared source destination is not empty: $resolvedDestination"
    }
} else {
    New-Item -ItemType Directory -Path $resolvedDestination | Out-Null
}

$sourceMap = @(Get-R3SourceMap)
Assert-R3SourceMap -RepositoryRoot $resolvedRoot -SourceMap $sourceMap
$inventory = [Collections.Generic.List[object]]::new()

foreach ($entry in $sourceMap) {
    $sourcePath = Join-Path $resolvedRoot $entry.Source
    $destinationPath = Join-Path $resolvedDestination $entry.Destination
    $destinationDirectory = Split-Path -Parent $destinationPath
    if (-not (Test-Path -LiteralPath $destinationDirectory -PathType Container)) {
        New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
    }
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath
    $inventory.Add([pscustomobject][ordered]@{
        source = $entry.Source.Replace('\', '/')
        destination = $entry.Destination.Replace('\', '/')
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $destinationPath).Hash.ToLowerInvariant()
    })
}

$inventoryPath = Join-Path $resolvedDestination 'source-inventory.json'
$inventory | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $inventoryPath -Encoding utf8
Write-Host "Prepared R3 source tree at $resolvedDestination"
