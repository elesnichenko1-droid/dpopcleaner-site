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
New-Item -ItemType Directory -Path (Join-Path $destinationPath 'assets') -Force | Out-Null

$requiredFiles = @(
    '.nojekyll',
    'dpopcleaner-icon.png',
    'index.html',
    'release-manifest.js',
    'script.js',
    'styles.css',
    'site-shell.css',
    'version.json',
    'update/beta.json',
    'assets/dpopcleaner-current-settings.png',
    'assets/dpopcleaner-0.4.17-disk.png',
    'assets/dpopcleaner-0.4.17-restore.png'
)

foreach ($relativePath in $requiredFiles) {
    $source = Join-Path $rootPath $relativePath
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Required website file is missing: $relativePath"
    }
    $target = Join-Path $destinationPath $relativePath
    Copy-Item -LiteralPath $source -Destination $target -Force
}

$stableManifest = Join-Path $rootPath 'update/stable.json'
if (Test-Path -LiteralPath $stableManifest -PathType Leaf) {
    Copy-Item -LiteralPath $stableManifest -Destination (Join-Path $destinationPath 'update/stable.json') -Force
}

Write-Host "Staged public website at $destinationPath"
