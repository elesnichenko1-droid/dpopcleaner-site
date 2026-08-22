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

$zapretContract = Get-R3ZapretContract
Assert-Equal 'pins official Zapret version' $zapretContract.Version '1.10.1'
Assert-Equal 'pins official Zapret archive SHA-256' $zapretContract.ArchiveSha256 'f748d61fec75e4edc992cb5b09d554e914197c68c690384aceb61f143d8f76c9'
Assert-Equal 'pins official Zapret license SHA-256' $zapretContract.LicenseSha256 'fe3983a1e91206ad1a530bcfae01fad207020cb61882edd62c1e3cb5f8d5d430'

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

    $hashFixture = Join-Path $tempRoot 'hash-fixture.bin'
    [IO.File]::WriteAllBytes($hashFixture, [Text.Encoding]::UTF8.GetBytes('known-r3-fixture'))
    $fixtureHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $hashFixture).Hash.ToLowerInvariant()
    Assert-R3FileHash -Path $hashFixture -ExpectedSha256 $fixtureHash -Description 'test fixture'
    Write-Host 'PASS: accepts an exact expected payload hash'
    Assert-Throws 'rejects a modified payload hash' {
        Assert-R3FileHash -Path $hashFixture -ExpectedSha256 ('0' * 64) -Description 'test fixture'
    } 'Unexpected test fixture SHA-256'

    $bundleFixture = Join-Path $tempRoot 'zapret'
    foreach ($relative in @(
        'general.bat', 'service.bat', 'bin/winws.exe', 'bin/WinDivert.dll',
        'bin/WinDivert64.sys', 'bin/cygwin1.dll', 'utils/check_updates.enabled',
        'LICENSE.txt', 'lists/list-general.txt'
    )) {
        $fixturePath = Join-Path $bundleFixture $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $fixturePath) -Force | Out-Null
        Set-Content -LiteralPath $fixturePath -Value 'fixture' -Encoding utf8
    }
    Assert-R3BundleTree -Path $bundleFixture
    Write-Host 'PASS: accepts a complete Zapret bundle tree'
    Remove-Item -LiteralPath (Join-Path $bundleFixture 'service.bat')
    Assert-Throws 'rejects a bundle without service.bat' {
        Assert-R3BundleTree -Path $bundleFixture
    } 'Missing required Zapret file: service.bat'
    Set-Content -LiteralPath (Join-Path $bundleFixture 'service.bat') -Value 'fixture' -Encoding utf8

    $stageFixture = Join-Path $tempRoot 'stage'
    New-Item -ItemType Directory -Path $stageFixture | Out-Null
    Set-Content -LiteralPath (Join-Path $stageFixture 'DPopCleaner.exe') -Value 'app' -Encoding utf8
    Set-Content -LiteralPath (Join-Path $stageFixture 'DPopUpdater.exe') -Value 'updater' -Encoding utf8
    Copy-Item -LiteralPath $bundleFixture -Destination (Join-Path $stageFixture 'zapret') -Recurse
    Assert-R3StagedPayload -Path $stageFixture
    Write-Host 'PASS: accepts exactly two binaries and the verified Zapret tree'
    Set-Content -LiteralPath (Join-Path $stageFixture 'unexpected.ps1') -Value 'forbidden' -Encoding utf8
    Assert-Throws 'rejects an unexpected staged top-level payload' {
        Assert-R3StagedPayload -Path $stageFixture
    } 'Unexpected R3 staged payload'

    Assert-R3InstallerDefinition -Path (Join-Path $root 'release/DPopCleaner_0.3.1_R3.iss')
    Write-Host 'PASS: installer definition contains only the approved R3 payload'
    $extraPayloadInstaller = Join-Path $tempRoot 'extra-payload.iss'
    $installerText = Get-Content -Raw -LiteralPath (Join-Path $root 'release/DPopCleaner_0.3.1_R3.iss')
    $installerText.Replace('[Icons]', "Source: `"outside.ps1`"; DestDir: `"{app}`"`r`n`r`n[Icons]") |
        Set-Content -LiteralPath $extraPayloadInstaller -Encoding utf8
    Assert-Throws 'rejects an installer payload outside the approved roots' {
        Assert-R3InstallerDefinition -Path $extraPayloadInstaller
    } 'exactly three payload declarations'

    $autoStartInstaller = Join-Path $tempRoot 'autostart-zapret.iss'
    $installerText.Replace('[UninstallDelete]', "[Run]`r`nFilename: `"{app}\\zapret\\service.bat`"`r`n`r`n[UninstallDelete]") |
        Set-Content -LiteralPath $autoStartInstaller -Encoding utf8
    Assert-Throws 'rejects automatic Zapret service startup' {
        Assert-R3InstallerDefinition -Path $autoStartInstaller
    } 'must not start Zapret'

    $installerFixture = Join-Path $tempRoot 'DPopCleaner_Setup_0.3.1_BETA_R3.exe'
    [IO.File]::WriteAllBytes($installerFixture, [Text.Encoding]::UTF8.GetBytes('r3-installer-fixture'))
    $releaseManifest = New-R3ReleaseManifest -InstallerPath $installerFixture -Signed:$false
    if ($releaseManifest.version -ne '0.3.1' -or $releaseManifest.version_code -ne 3013 -or $releaseManifest.revision -ne 3) {
        throw 'creates exact R3 release identity: wrong version fields.'
    }
    if ($releaseManifest.download_url -ne 'https://github.com/elesnichenko1-droid/dpopcleaner-site/releases/download/v0.3.1-beta-r3/DPopCleaner_Setup_0.3.1_BETA_R3.exe') {
        throw 'creates exact R3 release identity: wrong URL.'
    }
    if ($releaseManifest.sha256 -ne 'd586b65e3788f8d9a1267e4f3955c6ebe29d25e542703069048ff299de677f8f' -or $releaseManifest.size -ne 20) {
        throw 'creates exact R3 artifact metadata: wrong hash or size.'
    }
    Assert-R3ReleaseManifest -Manifest $releaseManifest -Published
    Write-Host 'PASS: creates and accepts exact published R3 manifest'

    $badRelease = [pscustomobject][ordered]@{}
    foreach ($property in $releaseManifest.PSObject.Properties) {
        $badRelease | Add-Member -NotePropertyName $property.Name -NotePropertyValue $property.Value
    }
    $badRelease.revision = 2
    Assert-Throws 'rejects wrong R3 release revision' {
        Assert-R3ReleaseManifest -Manifest $badRelease -Published
    } 'Invalid R3 release manifest'

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
