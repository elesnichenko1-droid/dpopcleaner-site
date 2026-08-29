[CmdletBinding()]
param(
    [string]$OutputRoot = '_release/0.4.17/third-party/Zapret',
    [string]$ArchivePath = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$PinnedVersion = '1.10.2'
$PinnedArchiveName = 'zapret-discord-youtube-1.10.2.zip'
$PinnedUrl = 'https://github.com/Flowseal/zapret-discord-youtube/releases/download/1.10.2/zapret-discord-youtube-1.10.2.zip'
$PinnedSize = [int64]1508077
$PinnedSha256 = '5eaac9fb2e4b1abd693487452a3ff3f4dfe9578a45f9ddddfa4bc1f5a6bb62d5'
$PinnedLicenseSha256 = 'fe3983a1e91206ad1a530bcfae01fad207020cb61882edd62c1e3cb5f8d5d430'
$PinnedVersionSha256 = '34d597db43ca53b2fd72ccbdd1af7a0fe238c2c0b8321dad8f43a1613143fc62'

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$output = if ([IO.Path]::IsPathRooted($OutputRoot)) { $OutputRoot } else { Join-Path $root $OutputRoot }
$work = Join-Path $root '_release/0.4.17/third-party/work'
$downloads = Join-Path $root '_release/0.4.17/third-party/downloads'
$metadataRoot = Join-Path $root 'v0417/third_party/flowseal-1.10.2'
$pinnedLicense = Join-Path $metadataRoot 'LICENSE.txt'
$pinnedVersionFile = Join-Path $metadataRoot 'version.txt'
$archive = if ($ArchivePath) {
    if ([IO.Path]::IsPathRooted($ArchivePath)) { $ArchivePath } else { Join-Path $root $ArchivePath }
} else {
    Join-Path $downloads $PinnedArchiveName
}

function New-Directory([string]$Path) {
    New-Item -ItemType Directory -Path $Path -Force | Out-Null
}

function Assert-Sha256([string]$Path, [string]$Expected, [string]$Description) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Description is missing: $Path"
    }
    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Expected) {
        throw "$Description SHA-256 mismatch. Expected $Expected, got $actual."
    }
}

Assert-Sha256 $pinnedLicense $PinnedLicenseSha256 'Pinned Flowseal LICENSE.txt'
Assert-Sha256 $pinnedVersionFile $PinnedVersionSha256 'Pinned Flowseal version.txt'
if ((Get-Content -Raw -LiteralPath $pinnedVersionFile).Trim() -ne $PinnedVersion) {
    throw "Pinned Flowseal version metadata is not $PinnedVersion."
}

if (-not $ArchivePath) {
    New-Directory $downloads
    if (Test-Path -LiteralPath $archive -PathType Leaf) { Remove-Item -LiteralPath $archive -Force }
    Write-Host "Downloading pinned Flowseal Zapret $PinnedVersion from $PinnedUrl"
    Invoke-WebRequest -Uri $PinnedUrl -OutFile $archive -UseBasicParsing -MaximumRedirection 10
}
if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) {
    throw "Pinned Zapret archive not found: $archive"
}

$actualSize = (Get-Item -LiteralPath $archive).Length
if ($actualSize -ne $PinnedSize) {
    throw "Pinned Zapret ZIP size mismatch. Expected $PinnedSize, got $actualSize."
}
$actualSha = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualSha -ne $PinnedSha256) {
    throw "Pinned Zapret ZIP SHA-256 mismatch. Expected $PinnedSha256, got $actualSha."
}

if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
New-Directory $work
$extractRoot = Join-Path $work 'extract'
New-Directory $extractRoot
Expand-Archive -LiteralPath $archive -DestinationPath $extractRoot -Force

$candidates = @(Get-ChildItem -LiteralPath $extractRoot -Filter 'service.bat' -File -Recurse -Force | Where-Object {
    Test-Path -LiteralPath (Join-Path $_.Directory.FullName 'general.bat') -PathType Leaf
})
if ($candidates.Count -ne 1) {
    throw "Could not resolve one exact Flowseal payload root. Candidates: $($candidates.Count)."
}
$payloadRoot = $candidates[0].Directory.FullName

$requiredArchiveFiles = @(
    'service.bat',
    'general.bat',
    'bin/winws.exe',
    'bin/WinDivert.dll',
    'bin/WinDivert64.sys'
)
foreach ($relative in $requiredArchiveFiles) {
    $candidate = Join-Path $payloadRoot ($relative -replace '/', [IO.Path]::DirectorySeparatorChar)
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "Verified Flowseal archive missing required file: $relative"
    }
}
foreach ($directory in @('lists', 'utils')) {
    if (-not (Test-Path -LiteralPath (Join-Path $payloadRoot $directory) -PathType Container)) {
        throw "Verified Flowseal archive missing required directory: $directory"
    }
}

# The official release ZIP omits these metadata files. Restore only byte-pinned
# copies from the same immutable 1.10.2 upstream tag.
Copy-Item -LiteralPath $pinnedLicense -Destination (Join-Path $payloadRoot 'LICENSE.txt') -Force
New-Directory (Join-Path $payloadRoot '.service')
Copy-Item -LiteralPath $pinnedVersionFile -Destination (Join-Path $payloadRoot '.service/version.txt') -Force

# Frozen DPopCleaner 0.2.14 does NOT read .service/version.txt for the version
# rendered in Zapret Center. Reverse/runtime diagnostics prove it opens the
# existing legacy compatibility path Zapret\utils\dpop_version.txt as UTF-8;
# when the file is absent it constructs the fallback "1.9.9d" in machine code.
# Feed the immutable core the same byte-pinned 1.10.2 metadata through the path
# it already expects. This changes no core bytes and creates no replacement UI.
Copy-Item -LiteralPath $pinnedVersionFile -Destination (Join-Path $payloadRoot 'utils/dpop_version.txt') -Force

Assert-Sha256 (Join-Path $payloadRoot 'LICENSE.txt') $PinnedLicenseSha256 'Prepared Flowseal LICENSE.txt'
Assert-Sha256 (Join-Path $payloadRoot '.service/version.txt') $PinnedVersionSha256 'Prepared Flowseal version.txt'
Assert-Sha256 (Join-Path $payloadRoot 'utils/dpop_version.txt') $PinnedVersionSha256 'Prepared frozen-core dpop_version.txt'
if ((Get-Content -Raw -LiteralPath (Join-Path $payloadRoot 'utils/dpop_version.txt')).Trim() -ne $PinnedVersion) {
    throw "Prepared frozen-core dpop_version.txt is not $PinnedVersion."
}

$strategies = @(Get-ChildItem -LiteralPath $payloadRoot -Filter 'general*.bat' -File | Sort-Object Name)
if ($strategies.Count -eq 0) {
    throw 'Verified Flowseal payload contains no general*.bat strategies.'
}

if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output -Recurse -Force }
New-Directory $output
Get-ChildItem -LiteralPath $payloadRoot -Force | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $output -Recurse -Force
}

$requiredPreparedFiles = @(
    'LICENSE.txt',
    'service.bat',
    'general.bat',
    '.service/version.txt',
    'utils/dpop_version.txt',
    'bin/winws.exe',
    'bin/WinDivert.dll',
    'bin/WinDivert64.sys'
)
foreach ($relative in $requiredPreparedFiles) {
    $candidate = Join-Path $output ($relative -replace '/', [IO.Path]::DirectorySeparatorChar)
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "Prepared Zapret tree missing required file: $relative"
    }
}
foreach ($directory in @('lists', 'utils')) {
    if (-not (Test-Path -LiteralPath (Join-Path $output $directory) -PathType Container)) {
        throw "Prepared Zapret tree missing required directory: $directory"
    }
}
if ((Get-Content -Raw -LiteralPath (Join-Path $output 'utils/dpop_version.txt')).Trim() -ne $PinnedVersion) {
    throw "Prepared Zapret tree exposes wrong frozen-core version metadata."
}

Write-Host "Pinned Zapret archive verified: $actualSize bytes, SHA-256 $actualSha"
Write-Host "Flowseal Zapret version verified: $PinnedVersion"
Write-Host "Frozen-core Zapret version source verified: utils/dpop_version.txt=$PinnedVersion"
Write-Host "Prepared old-core-compatible Zapret payload: $output"
