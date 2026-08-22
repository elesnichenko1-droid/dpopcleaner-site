[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ApplicationExe,
    [Parameter(Mandatory)][string]$UpdaterExe,
    [Parameter(Mandatory)][string]$ZapretArchive,
    [Parameter(Mandatory)][string]$ZapretLicense,
    [Parameter(Mandatory)][string]$Destination,
    [string]$InventoryPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

Import-Module (Join-Path $PSScriptRoot 'R3ReleasePolicy.psm1') -Force
$contract = Get-R3ZapretContract
Assert-R3FileHash -Path $ZapretArchive -ExpectedSha256 $contract.ArchiveSha256 -Description 'Zapret archive'
Assert-R3FileHash -Path $ZapretLicense -ExpectedSha256 $contract.LicenseSha256 -Description 'Zapret license'

foreach ($binary in @($ApplicationExe, $UpdaterExe)) {
    if (-not (Test-Path -LiteralPath $binary -PathType Leaf) -or (Get-Item -LiteralPath $binary).Length -le 0) {
        throw "Application binary is missing or empty: $binary"
    }
}

$resolvedDestination = [IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $resolvedDestination) {
    if (@(Get-ChildItem -LiteralPath $resolvedDestination -Force).Count -gt 0) {
        throw "R3 payload destination is not empty: $resolvedDestination"
    }
} else {
    New-Item -ItemType Directory -Path $resolvedDestination | Out-Null
}

$tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar)
$extractRoot = Join-Path $tempBase ('dpop-r3-zapret-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $extractRoot | Out-Null
    Expand-Archive -LiteralPath $ZapretArchive -DestinationPath $extractRoot
    $roots = @(Get-ChildItem -LiteralPath $extractRoot -Directory)
    if ($roots.Count -ne 1 -or $roots[0].Name -ne 'zapret-discord-youtube-1.10.1') {
        throw "Unexpected Zapret archive root: $($roots.Name -join ', ')"
    }

    Copy-Item -LiteralPath $ApplicationExe -Destination (Join-Path $resolvedDestination 'DPopCleaner.exe')
    Copy-Item -LiteralPath $UpdaterExe -Destination (Join-Path $resolvedDestination 'DPopUpdater.exe')
    Copy-Item -LiteralPath $roots[0].FullName -Destination (Join-Path $resolvedDestination 'zapret') -Recurse
    Copy-Item -LiteralPath $ZapretLicense -Destination (Join-Path $resolvedDestination 'zapret/LICENSE.txt') -Force
    Assert-R3StagedPayload -Path $resolvedDestination

    if ([string]::IsNullOrWhiteSpace($InventoryPath)) {
        $InventoryPath = Join-Path (Split-Path -Parent $resolvedDestination) 'r3-payload-inventory.json'
    }
    $resolvedInventory = [IO.Path]::GetFullPath($InventoryPath)
    $destinationPrefix = $resolvedDestination.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if ($resolvedInventory.StartsWith($destinationPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Payload inventory must be stored outside the exact installer staging root.'
    }
    $inventory = @(Get-ChildItem -LiteralPath $resolvedDestination -File -Recurse | ForEach-Object {
        [pscustomobject][ordered]@{
            path = [IO.Path]::GetRelativePath($resolvedDestination, $_.FullName).Replace('\', '/')
            size = [int64]$_.Length
            sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()
        }
    } | Sort-Object path)
    $inventory | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $resolvedInventory -Encoding utf8
    Write-Host "Staged verified R3 payload at $resolvedDestination"
} finally {
    $resolvedExtractRoot = [IO.Path]::GetFullPath($extractRoot)
    $tempPrefix = $tempBase + [IO.Path]::DirectorySeparatorChar
    if ($resolvedExtractRoot.StartsWith($tempPrefix, [StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $resolvedExtractRoot)) {
        Remove-Item -LiteralPath $resolvedExtractRoot -Recurse -Force
    }
}
