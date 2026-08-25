[CmdletBinding()]
param(
    [string]$Stage = "_release/0.4.17/stage",
    [switch]$RequireCompanions
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Final installer/package jobs must call this script with -RequireCompanions.
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$contractPath = Join-Path $root 'v0417/contracts/core.json'
$allowlistPath = Join-Path $root 'v0417/stage-allowlist.txt'
$payloadRoot = Join-Path $root 'v0417/payload'
$stageRoot = if ([IO.Path]::IsPathRooted($Stage)) { $Stage } else { Join-Path $root $Stage }

if (-not (Test-Path -LiteralPath $contractPath -PathType Leaf)) {
    throw 'Missing v0417/contracts/core.json.'
}
if (-not (Test-Path -LiteralPath $allowlistPath -PathType Leaf)) {
    throw 'Missing v0417/stage-allowlist.txt.'
}

$expectedAllowlist = @(
    'DPopCleaner.exe',
    'Languages/',
    'Shell/',
    'Documentation/',
    'Modules/DPop.Common.dll',
    'Modules/DiskAnalyzer.exe',
    'Modules/RestoreCenter.exe',
    'Resources/'
)
$actualAllowlist = @(Get-Content -LiteralPath $allowlistPath | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if (($actualAllowlist.Count -ne $expectedAllowlist.Count) -or (Compare-Object $expectedAllowlist $actualAllowlist -SyncWindow 0)) {
    throw '0.4.17 stage allowlist differs from the approved exact payload.'
}

$core = Get-Content -Raw -LiteralPath $contractPath | ConvertFrom-Json
$corePath = Join-Path $root ([string]$core.path)
if (-not (Test-Path -LiteralPath $corePath -PathType Leaf)) {
    throw "Original 0.2.14 core is missing: $corePath"
}
if ((Get-Item -LiteralPath $corePath).Length -ne [int64]$core.size) {
    throw 'Original 0.2.14 core size does not match the immutable contract.'
}

$sourceBlob = (& git -C $root hash-object -- $corePath).Trim()
if ($LASTEXITCODE -ne 0 -or $sourceBlob -ne [string]$core.git_blob_sha1) {
    throw "Original 0.2.14 core Git blob mismatch: $sourceBlob"
}

if (Test-Path -LiteralPath $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $stageRoot -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stageRoot 'Modules') -Force | Out-Null

Copy-Item -LiteralPath $corePath -Destination (Join-Path $stageRoot ([string]$core.staged_name)) -Force

function Copy-ApprovedDirectory([string]$Name) {
    $source = Join-Path $payloadRoot $Name
    $destination = Join-Path $stageRoot $Name
    if (-not (Test-Path -LiteralPath $source -PathType Container)) {
        New-Item -ItemType Directory -Path $destination -Force | Out-Null
        return
    }
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Get-ChildItem -LiteralPath $source -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $destination -Recurse -Force
    }
}

Copy-ApprovedDirectory 'Languages'
Copy-ApprovedDirectory 'Shell'
Copy-ApprovedDirectory 'Documentation'
Copy-ApprovedDirectory 'Resources'

$commonDll = Join-Path $root 'v0417/src/DPop.Common/bin/Release/net48/DPop.Common.dll'
if (-not (Test-Path -LiteralPath $commonDll -PathType Leaf)) {
    throw "DPop.Common.dll is missing. Build v0417/src/DPop.Common first: $commonDll"
}
Copy-Item -LiteralPath $commonDll -Destination (Join-Path $stageRoot 'Modules/DPop.Common.dll') -Force

$companions = @(
    @{ Name = 'DiskAnalyzer.exe'; Path = 'v0417/src/DiskAnalyzer/bin/Release/net48/DiskAnalyzer.exe' },
    @{ Name = 'RestoreCenter.exe'; Path = 'v0417/src/RestoreCenter/bin/Release/net48/RestoreCenter.exe' }
)
foreach ($companion in $companions) {
    $source = Join-Path $root $companion.Path
    if (Test-Path -LiteralPath $source -PathType Leaf) {
        Copy-Item -LiteralPath $source -Destination (Join-Path $stageRoot ('Modules/' + $companion.Name)) -Force
    } elseif ($RequireCompanions) {
        throw "Required 0.4.17 companion is missing: $($companion.Name)"
    }
}

$stagedCore = Join-Path $stageRoot ([string]$core.staged_name)
$stagedBlob = (& git -C $root hash-object -- $stagedCore).Trim()
if ($LASTEXITCODE -ne 0 -or $stagedBlob -ne [string]$core.git_blob_sha1) {
    throw "Staged DPopCleaner.exe changed from the preserved original: $stagedBlob"
}

Write-Host "Staged immutable core: $stagedBlob"
Write-Host "0.4.17 stage ready: $stageRoot"
