[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Root,
    [Parameter(Mandatory)][string]$Destination
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$rootPath = (Resolve-Path -LiteralPath $Root).Path
$destinationPath = [IO.Path]::GetFullPath($Destination)
$rootPrefix = $rootPath.TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
if (-not $destinationPath.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe website staging destination: $destinationPath"
}

if (Test-Path -LiteralPath $destinationPath) {
    Remove-Item -LiteralPath $destinationPath -Recurse -Force
}
New-Item -ItemType Directory -Path (Join-Path $destinationPath 'update') -Force | Out-Null

$requiredFiles = @(
    '.nojekyll',
    'dpopcleaner-icon.png',
    'index.html',
    'release-manifest.js',
    'script.js',
    'styles.css',
    'version.json',
    'update/beta.json'
)

foreach ($relativePath in $requiredFiles) {
    $source = Join-Path $rootPath $relativePath
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Required website file is missing: $relativePath"
    }
    $target = Join-Path $destinationPath $relativePath
    Copy-Item -LiteralPath $source -Destination $target -Force
}

$betaManifest = Get-Content -Raw -LiteralPath (Join-Path $rootPath 'update/beta.json') | ConvertFrom-Json
function Get-ManifestField {
    param([Parameter(Mandatory)][object]$Manifest, [Parameter(Mandatory)][string]$Name)
    $property = $Manifest.PSObject.Properties[$Name]
    if ($null -eq $property) { return $null }
    return $property.Value
}
$publishedR3 =
    (Get-ManifestField $betaManifest 'product') -eq 'DPopCleaner' -and
    (Get-ManifestField $betaManifest 'channel') -eq 'beta' -and
    (Get-ManifestField $betaManifest 'version') -eq '0.3.1' -and
    (Get-ManifestField $betaManifest 'version_code') -eq 3013 -and
    (Get-ManifestField $betaManifest 'revision') -eq 3 -and
    (Get-ManifestField $betaManifest 'available') -eq $true

$screenshot = Join-Path $rootPath 'assets/dpopcleaner-0.3.1-r3.png'
if ($publishedR3 -and -not (Test-Path -LiteralPath $screenshot -PathType Leaf)) {
    throw 'Published R3 site requires its verified application screenshot.'
}
if (Test-Path -LiteralPath $screenshot -PathType Leaf) {
    $assetDirectory = Join-Path $destinationPath 'assets'
    New-Item -ItemType Directory -Path $assetDirectory -Force | Out-Null
    Copy-Item -LiteralPath $screenshot -Destination (Join-Path $assetDirectory 'dpopcleaner-0.3.1-r3.png') -Force
}

$stableManifest = Join-Path $rootPath 'update/stable.json'
if (Test-Path -LiteralPath $stableManifest -PathType Leaf) {
    Copy-Item -LiteralPath $stableManifest -Destination (Join-Path $destinationPath 'update/stable.json') -Force
}

Write-Host "Staged public website at $destinationPath"
