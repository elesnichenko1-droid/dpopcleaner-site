[CmdletBinding()]
param(
    [string]$Stage = "_release/0.4.17/stage",
    [switch]$RequireCompanions
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$contractPath = Join-Path $root 'v0417/contracts/core.json'
$allowlistPath = Join-Path $root 'v0417/stage-allowlist.txt'
$payloadRoot = Join-Path $root 'v0417/payload'
$zapretRoot = Join-Path $root '_release/0.4.17/third-party/Zapret'
$stageRoot = if ([IO.Path]::IsPathRooted($Stage)) { $Stage } else { Join-Path $root $Stage }

if (-not (Test-Path -LiteralPath $contractPath -PathType Leaf)) { throw 'Missing v0417/contracts/core.json.' }
if (-not (Test-Path -LiteralPath $allowlistPath -PathType Leaf)) { throw 'Missing v0417/stage-allowlist.txt.' }

$expectedAllowlist = @(
    'DPopCleaner.exe',
    'SimpleUpdate.exe',
    'Zapret/',
    'Languages/',
    'Shell/',
    'Documentation/',
    'Modules/DPop.Common.dll',
    'Modules/DiskAnalyzer.exe',
    'Modules/RestoreCenter.exe',
    'Modules/ZapretScreenFix.exe',
    'Resources/'
)
$actualAllowlist = @(Get-Content -LiteralPath $allowlistPath | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if (($actualAllowlist.Count -ne $expectedAllowlist.Count) -or (Compare-Object $expectedAllowlist $actualAllowlist -SyncWindow 0)) {
    throw '0.4.17 stage allowlist differs from the approved exact payload.'
}

$core = Get-Content -Raw -LiteralPath $contractPath | ConvertFrom-Json
$corePath = Join-Path $root ([string]$core.path)
if (-not (Test-Path -LiteralPath $corePath -PathType Leaf)) { throw "Original 0.2.14 core is missing: $corePath" }
if ((Get-Item -LiteralPath $corePath).Length -ne [int64]$core.size) { throw 'Original 0.2.14 core size does not match the immutable contract.' }
$sourceBlob = (& git -C $root hash-object -- $corePath).Trim()
if ($LASTEXITCODE -ne 0 -or $sourceBlob -ne [string]$core.git_blob_sha1) { throw "Original 0.2.14 core Git blob mismatch: $sourceBlob" }

$requiredZapretFiles = @(
    'LICENSE.txt',
    '.service/version.txt',
    'utils/dpop_version.txt',
    'service.bat',
    'general.bat',
    'bin/winws.exe',
    'bin/WinDivert.dll',
    'bin/WinDivert64.sys'
)
if (-not (Test-Path -LiteralPath $zapretRoot -PathType Container)) {
    throw "Prepared Flowseal Zapret payload is missing. Run tools/dpop0417_prepare_zapret.ps1 first: $zapretRoot"
}
foreach ($relative in $requiredZapretFiles) {
    $candidate = Join-Path $zapretRoot ($relative -replace '/', [IO.Path]::DirectorySeparatorChar)
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) { throw "Prepared Zapret payload missing required file: $relative" }
}
foreach ($directory in @('lists','utils')) {
    if (-not (Test-Path -LiteralPath (Join-Path $zapretRoot $directory) -PathType Container)) { throw "Prepared Zapret payload missing required directory: $directory" }
}
$zapretVersion = (Get-Content -Raw -LiteralPath (Join-Path $zapretRoot '.service/version.txt')).Trim()
if ($zapretVersion -ne '1.10.2') { throw "Prepared Zapret version mismatch: $zapretVersion" }
$nativeZapretVersion = (Get-Content -Raw -LiteralPath (Join-Path $zapretRoot 'utils/dpop_version.txt')).Trim()
if ($nativeZapretVersion -ne $zapretVersion) { throw "Frozen-core Zapret version source mismatch: $nativeZapretVersion vs $zapretVersion" }
$strategies = @(Get-ChildItem -LiteralPath $zapretRoot -Filter 'general*.bat' -File | Sort-Object Name)
if ($strategies.Count -eq 0) { throw 'Prepared Zapret payload has no general*.bat strategies.' }

if (Test-Path -LiteralPath $stageRoot) { Remove-Item -LiteralPath $stageRoot -Recurse -Force }
New-Item -ItemType Directory -Path $stageRoot -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stageRoot 'Modules') -Force | Out-Null
$stageZapretRoot = Join-Path $stageRoot 'Zapret'
New-Item -ItemType Directory -Path $stageZapretRoot -Force | Out-Null

Copy-Item -LiteralPath $corePath -Destination (Join-Path $stageRoot ([string]$core.staged_name)) -Force

$launcher = Join-Path $root 'v0417/src/SimpleUpdate/bin/Release/net48/SimpleUpdate.exe'
if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) { throw "SimpleUpdate.exe is missing. Build v0417/src/SimpleUpdate first: $launcher" }
Copy-Item -LiteralPath $launcher -Destination (Join-Path $stageRoot 'SimpleUpdate.exe') -Force

# Runtime evidence from the frozen 0.2.14 process shows that its Zapret root is
# <DPopCleaner.exe directory>\Zapret. service.bat, general*.bat and bin\winws.exe
# are resolved relative to that legacy subdirectory, so keep the verified
# Flowseal payload there without modifying the immutable core. The same core
# also reads utils\dpop_version.txt for the native Zapret Center version row.
Copy-Item -LiteralPath (Join-Path $zapretRoot 'LICENSE.txt') -Destination (Join-Path $stageZapretRoot 'LICENSE.txt') -Force
foreach ($batch in @(Get-ChildItem -LiteralPath $zapretRoot -Filter '*.bat' -File | Sort-Object Name)) {
    Copy-Item -LiteralPath $batch.FullName -Destination (Join-Path $stageZapretRoot $batch.Name) -Force
}
foreach ($directory in @('.service','bin','lists','utils')) {
    $source = Join-Path $zapretRoot $directory
    $destination = Join-Path $stageZapretRoot $directory
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Get-ChildItem -LiteralPath $source -Force | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $destination -Recurse -Force }
}

function Copy-ApprovedDirectory([string]$Name) {
    $source = Join-Path $payloadRoot $Name
    $destination = Join-Path $stageRoot $Name
    if (-not (Test-Path -LiteralPath $source -PathType Container)) {
        New-Item -ItemType Directory -Path $destination -Force | Out-Null
        return
    }
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Get-ChildItem -LiteralPath $source -Force | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $destination -Recurse -Force }
}
Copy-ApprovedDirectory 'Languages'
Copy-ApprovedDirectory 'Shell'
Copy-ApprovedDirectory 'Documentation'
Copy-ApprovedDirectory 'Resources'

$commonDll = Join-Path $root 'v0417/src/DPop.Common/bin/Release/net48/DPop.Common.dll'
if (-not (Test-Path -LiteralPath $commonDll -PathType Leaf)) { throw "DPop.Common.dll is missing. Build v0417/src/DPop.Common first: $commonDll" }
Copy-Item -LiteralPath $commonDll -Destination (Join-Path $stageRoot 'Modules/DPop.Common.dll') -Force

$companions = @(
    @{ Name = 'DiskAnalyzer.exe'; Path = 'v0417/src/DiskAnalyzer/bin/Release/net48/DiskAnalyzer.exe' },
    @{ Name = 'RestoreCenter.exe'; Path = 'v0417/src/RestoreCenter/bin/Release/net48/RestoreCenter.exe' },
    @{ Name = 'ZapretScreenFix.exe'; Path = 'v0417/src/ZapretScreenFix/bin/Release/net48/ZapretScreenFix.exe' }
)
foreach ($companion in $companions) {
    $source = Join-Path $root $companion.Path
    if (Test-Path -LiteralPath $source -PathType Leaf) {
        Copy-Item -LiteralPath $source -Destination (Join-Path $stageRoot ('Modules/' + $companion.Name)) -Force
    } elseif ($RequireCompanions) {
        throw "Required 0.4.17 companion is missing: $($companion.Name)"
    }
}

foreach ($relative in $requiredZapretFiles) {
    $candidate = Join-Path $stageZapretRoot ($relative -replace '/', [IO.Path]::DirectorySeparatorChar)
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) { throw "Staged Zapret payload missing required file: Zapret/$relative" }
}
$stagedNativeVersion = (Get-Content -Raw -LiteralPath (Join-Path $stageZapretRoot 'utils/dpop_version.txt')).Trim()
if ($stagedNativeVersion -ne $zapretVersion) { throw "Staged frozen-core Zapret version source mismatch: $stagedNativeVersion" }
$stagedStrategies = @(Get-ChildItem -LiteralPath $stageZapretRoot -Filter 'general*.bat' -File)
if ($stagedStrategies.Count -eq 0) { throw 'Staged Zapret payload contains no general*.bat strategies under Zapret/.' }

$stagedCore = Join-Path $stageRoot ([string]$core.staged_name)
$stagedBlob = (& git -C $root hash-object -- $stagedCore).Trim()
if ($LASTEXITCODE -ne 0 -or $stagedBlob -ne [string]$core.git_blob_sha1) { throw "Staged DPopCleaner.exe changed from the preserved original: $stagedBlob" }

Write-Host "Staged immutable core: $stagedBlob"
Write-Host "Staged Flowseal Zapret: $zapretVersion; native_version_source=$stagedNativeVersion; strategies=$($stagedStrategies.Count); root=$stageZapretRoot; winws.exe=$(Join-Path $stageZapretRoot 'bin/winws.exe')"
Write-Host "Staged launcher: $(Join-Path $stageRoot 'SimpleUpdate.exe')"
Write-Host "0.4.17 stage ready: $stageRoot"
