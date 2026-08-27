[CmdletBinding()]
param(
    [string]$Stage = '_release/0.4.18/stage',
    [string]$CoreExe = 'build0418/bin/Release/DPopCleaner.exe',
    [string]$UpdaterExe = 'build0418/bin/Release/DPopUpdater.exe',
    [switch]$RequireCompanions
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$allowlistPath = Join-Path $root 'v0418/stage-allowlist.txt'
$payloadRoot = Join-Path $root 'v0417/payload'
$stageRoot = if ([IO.Path]::IsPathRooted($Stage)) { $Stage } else { Join-Path $root $Stage }
$corePath = if ([IO.Path]::IsPathRooted($CoreExe)) { $CoreExe } else { Join-Path $root $CoreExe }
$updaterPath = if ([IO.Path]::IsPathRooted($UpdaterExe)) { $UpdaterExe } else { Join-Path $root $UpdaterExe }

$expectedAllowlist = @(
    'DPopCleaner.exe',
    'DPopUpdater.exe',
    'Languages/',
    'Shell/',
    'Documentation/',
    'Modules/DPop.Common.dll',
    'Modules/DiskAnalyzer.exe',
    'Modules/RestoreCenter.exe',
    'Modules/ZapretScreenFix.exe',
    'Resources/'
)
if (-not (Test-Path -LiteralPath $allowlistPath -PathType Leaf)) {
    throw 'Missing v0418/stage-allowlist.txt.'
}
$actualAllowlist = @(Get-Content -LiteralPath $allowlistPath | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if (($actualAllowlist.Count -ne $expectedAllowlist.Count) -or (Compare-Object $expectedAllowlist $actualAllowlist -SyncWindow 0)) {
    throw '0.4.18 stage allowlist differs from the approved exact payload.'
}

foreach ($native in @($corePath, $updaterPath)) {
    if (-not (Test-Path -LiteralPath $native -PathType Leaf)) {
        throw "Required 0.4.18 native executable is missing: $native"
    }
}

$coreVersion = (Get-Item -LiteralPath $corePath).VersionInfo.FileVersion
$updaterVersion = (Get-Item -LiteralPath $updaterPath).VersionInfo.FileVersion
if ($coreVersion -ne '0.4.18.1') { throw "Unexpected DPopCleaner.exe FileVersion: $coreVersion" }
if ($updaterVersion -ne '0.4.18.1') { throw "Unexpected DPopUpdater.exe FileVersion: $updaterVersion" }

if (Test-Path -LiteralPath $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $stageRoot -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stageRoot 'Modules') -Force | Out-Null

Copy-Item -LiteralPath $corePath -Destination (Join-Path $stageRoot 'DPopCleaner.exe') -Force
Copy-Item -LiteralPath $updaterPath -Destination (Join-Path $stageRoot 'DPopUpdater.exe') -Force

function Copy-ApprovedDirectory([string]$Name) {
    $source = Join-Path $payloadRoot $Name
    $destination = Join-Path $stageRoot $Name
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    if (Test-Path -LiteralPath $source -PathType Container) {
        Get-ChildItem -LiteralPath $source -Force | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $destination -Recurse -Force
        }
    }
}

Copy-ApprovedDirectory 'Languages'
Copy-ApprovedDirectory 'Shell'
Copy-ApprovedDirectory 'Documentation'
Copy-ApprovedDirectory 'Resources'

$moduleFiles = @(
    @{ Source = 'v0417/src/DPop.Common/bin/Release/net48/DPop.Common.dll'; Dest = 'Modules/DPop.Common.dll' },
    @{ Source = 'v0417/src/DiskAnalyzer/bin/Release/net48/DiskAnalyzer.exe'; Dest = 'Modules/DiskAnalyzer.exe' },
    @{ Source = 'v0417/src/RestoreCenter/bin/Release/net48/RestoreCenter.exe'; Dest = 'Modules/RestoreCenter.exe' },
    @{ Source = 'v0417/src/ZapretScreenFix/bin/Release/net48/ZapretScreenFix.exe'; Dest = 'Modules/ZapretScreenFix.exe' }
)
foreach ($module in $moduleFiles) {
    $source = Join-Path $root $module.Source
    $destination = Join-Path $stageRoot $module.Dest
    if (Test-Path -LiteralPath $source -PathType Leaf) {
        Copy-Item -LiteralPath $source -Destination $destination -Force
    } elseif ($RequireCompanions) {
        throw "Required companion is missing: $($module.Dest)"
    }
}

foreach ($required in @('DPopCleaner.exe', 'DPopUpdater.exe')) {
    if (-not (Test-Path -LiteralPath (Join-Path $stageRoot $required) -PathType Leaf)) {
        throw "Staged file missing: $required"
    }
}
if ($RequireCompanions) {
    foreach ($module in $moduleFiles) {
        if (-not (Test-Path -LiteralPath (Join-Path $stageRoot $module.Dest) -PathType Leaf)) {
            throw "Staged companion missing: $($module.Dest)"
        }
    }
}

Write-Host "DPopCleaner 0.4.18 core FileVersion: $coreVersion"
Write-Host "DPopUpdater 0.4.18 FileVersion: $updaterVersion"
Write-Host "0.4.18 stage ready: $stageRoot"
