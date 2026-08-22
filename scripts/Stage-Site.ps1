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

$screenshot = Join-Path $rootPath 'assets/dpopcleaner-0.3.1-r3.png'
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
