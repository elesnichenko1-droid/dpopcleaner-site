$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-Equal {
    param(
        [Parameter(Mandatory)][string]$Name,
        [AllowNull()]$Actual,
        [AllowNull()]$Expected
    )

    if ($Actual -ne $Expected) {
        throw "${Name}: expected '$Expected', got '$Actual'."
    }
    Write-Host "PASS: $Name"
}

function Assert-Throws {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][scriptblock]$Action,
        [Parameter(Mandatory)][string]$MessagePattern
    )

    try {
        & $Action
    } catch {
        if ($_.Exception.Message -notmatch $MessagePattern) {
            throw "${Name}: expected error matching '$MessagePattern', got '$($_.Exception.Message)'."
        }
        Write-Host "PASS: $Name"
        return
    }
    throw "${Name}: expected an exception."
}

$root = Split-Path -Parent $PSScriptRoot
$modulePath = Join-Path $root 'scripts/R3ReleasePolicy.psm1'
Import-Module $modulePath -Force

$contract = Get-R3ReleaseContract
Assert-Equal 'uses exact display version' $contract.DisplayVersion '0.3.1 BETA R3'
Assert-Equal 'uses exact application version' $contract.Version '0.3.1'
Assert-Equal 'uses exact internal version code' $contract.VersionCode 3013
Assert-Equal 'uses exact revision' $contract.Revision 3
Assert-Equal 'uses a new immutable release tag' $contract.Tag 'v0.3.1-beta-r3'
Assert-Equal 'uses the exact installer asset name' $contract.AssetName 'DPopCleaner_Setup_0.3.1_BETA_R3.exe'

$sourceMap = @(Get-R3SourceMap)
Assert-Equal 'maps the main application source' ($sourceMap | Where-Object Destination -eq 'src/main.cpp').Source 'main.cpp'
Assert-Equal 'maps the updater source' ($sourceMap | Where-Object Destination -eq 'src/updater/main.cpp').Source 'UpdaterMain.cpp'
Assert-Equal 'maps the icon resource' ($sourceMap | Where-Object Destination -eq 'resources/dpopcleaner.ico').Source 'dpopcleaner.ico'

Assert-Throws 'rejects a missing tracked source' {
    Assert-R3SourceMap -RepositoryRoot $root -SourceMap @(
        [pscustomobject]@{ Source = 'definitely-missing.cpp'; Destination = 'src/main.cpp' }
    )
} 'Missing tracked R3 source'

Assert-Throws 'rejects two sources with the same destination' {
    Assert-R3SourceMap -RepositoryRoot $root -SourceMap @(
        [pscustomobject]@{ Source = 'main.cpp'; Destination = 'src/main.cpp' },
        [pscustomobject]@{ Source = 'UpdaterMain.cpp'; Destination = 'src/main.cpp' }
    )
} 'Duplicate R3 destination'

Assert-R3VersionHeader -Path (Join-Path $root 'Version.h')
Write-Host 'PASS: accepts a version header aligned with the R3 contract'

Assert-R3ResourceDefinitions `
    -AppManifestPath (Join-Path $root 'app.manifest') `
    -UpdaterManifestPath (Join-Path $root 'updater.manifest') `
    -AppResourcePath (Join-Path $root 'app.rc') `
    -UpdaterResourcePath (Join-Path $root 'updater.rc') `
    -VersionResourcePath (Join-Path $root 'version.rc.in') `
    -InstallerDefinitionPath (Join-Path $root 'DPopCleaner.iss')
Write-Host 'PASS: application updater and installer resources match the R3 contract'

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('dpop-r3-source-' + [guid]::NewGuid().ToString('N'))
$destination = Join-Path $tempRoot 'prepared'
try {
    New-Item -ItemType Directory -Path $tempRoot | Out-Null
    $wrongVersionHeader = Join-Path $tempRoot 'Version.h'
    @'
#pragma once
namespace dpop::version {
inline constexpr wchar_t kProductName[] = L"DPopCleaner";
inline constexpr wchar_t kVersion[] = L"0.3.1";
inline constexpr wchar_t kDisplayVersion[] = L"0.3.1 BETA R3";
inline constexpr wchar_t kChannel[] = L"beta";
inline constexpr int kVersionCode = 301;
inline constexpr int kRevision = 3;
}
'@ | Set-Content -LiteralPath $wrongVersionHeader -Encoding utf8
    Assert-Throws 'rejects a header with the old internal version code' {
        Assert-R3VersionHeader -Path $wrongVersionHeader
    } 'Version header does not match R3 release contract'

    $wrongManifest = Join-Path $tempRoot 'wrong.manifest'
    @'
<?xml version="1.0" encoding="UTF-8"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
    <security><requestedPrivileges><requestedExecutionLevel level="asInvoker" uiAccess="false"/></requestedPrivileges></security>
  </trustInfo>
</assembly>
'@ | Set-Content -LiteralPath $wrongManifest -Encoding utf8
    Assert-Throws 'rejects an application manifest without elevation' {
        Assert-R3ResourceDefinitions `
            -AppManifestPath $wrongManifest `
            -UpdaterManifestPath (Join-Path $root 'updater.manifest') `
            -AppResourcePath (Join-Path $root 'app.rc') `
            -UpdaterResourcePath (Join-Path $root 'updater.rc') `
            -VersionResourcePath (Join-Path $root 'version.rc.in') `
            -InstallerDefinitionPath (Join-Path $root 'DPopCleaner.iss')
    } 'Application manifest must require administrator rights'

    & (Join-Path $root 'scripts/Prepare-R3Source.ps1') -RepositoryRoot $root -Destination $destination

    $inventoryPath = Join-Path $destination 'source-inventory.json'
    if (-not (Test-Path -LiteralPath $inventoryPath -PathType Leaf)) {
        throw 'prepares a machine-readable inventory: inventory is missing.'
    }
    $inventory = @(Get-Content -Raw -LiteralPath $inventoryPath | ConvertFrom-Json)
    Assert-Equal 'prepares exactly the mapped files' $inventory.Count $sourceMap.Count

    foreach ($entry in $inventory) {
        $preparedPath = Join-Path $destination $entry.destination
        if (-not (Test-Path -LiteralPath $preparedPath -PathType Leaf)) {
            throw "prepared file is missing: $($entry.destination)"
        }
        $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $preparedPath).Hash.ToLowerInvariant()
        Assert-Equal "records hash for $($entry.destination)" $entry.sha256 $actualHash
    }
    Write-Host 'PASS: prepares the complete tracked source tree without generated C++'
} finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}

Write-Host 'All R3 release policy tests passed.'
