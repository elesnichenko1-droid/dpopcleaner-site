Set-StrictMode -Version Latest

$script:R3Contract = [pscustomobject][ordered]@{
    Product = 'DPopCleaner'
    Channel = 'beta'
    DisplayVersion = '0.3.1 BETA R3'
    Version = '0.3.1'
    VersionCode = 3013
    Revision = 3
    Tag = 'v0.3.1-beta-r3'
    AssetName = 'DPopCleaner_Setup_0.3.1_BETA_R3.exe'
    ReleaseUrl = 'https://github.com/elesnichenko1-droid/dpopcleaner-site/releases/download/v0.3.1-beta-r3/DPopCleaner_Setup_0.3.1_BETA_R3.exe'
}

$script:ZapretContract = [pscustomobject][ordered]@{
    Version = '1.10.1'
    ArchiveUrl = 'https://github.com/Flowseal/zapret-discord-youtube/releases/download/1.10.1/zapret-discord-youtube-1.10.1.zip'
    ArchiveSha256 = 'f748d61fec75e4edc992cb5b09d554e914197c68c690384aceb61f143d8f76c9'
    LicenseUrl = 'https://raw.githubusercontent.com/Flowseal/zapret-discord-youtube/1.10.1/LICENSE.txt'
    LicenseSha256 = 'fe3983a1e91206ad1a530bcfae01fad207020cb61882edd62c1e3cb5f8d5d430'
}

$script:SourceMap = @(
    @{ Source = 'CMakeLists.txt'; Destination = 'CMakeLists.txt' },
    @{ Source = 'CMakePresets.json'; Destination = 'CMakePresets.json' },
    @{ Source = 'main.cpp'; Destination = 'src/main.cpp' },
    @{ Source = 'MainWindow.cpp'; Destination = 'src/app/MainWindow.cpp' },
    @{ Source = 'MainWindow.h'; Destination = 'src/app/MainWindow.h' },
    @{ Source = 'Logger.cpp'; Destination = 'src/core/Logger.cpp' },
    @{ Source = 'Logger.h'; Destination = 'src/core/Logger.h' },
    @{ Source = 'Paths.cpp'; Destination = 'src/core/Paths.cpp' },
    @{ Source = 'Paths.h'; Destination = 'src/core/Paths.h' },
    @{ Source = 'SingleInstance.cpp'; Destination = 'src/core/SingleInstance.cpp' },
    @{ Source = 'SingleInstance.h'; Destination = 'src/core/SingleInstance.h' },
    @{ Source = 'Version.h'; Destination = 'src/core/Version.h' },
    @{ Source = 'Applications.cpp'; Destination = 'src/modules/Applications.cpp' },
    @{ Source = 'Applications.h'; Destination = 'src/modules/Applications.h' },
    @{ Source = 'Cleaner.cpp'; Destination = 'src/modules/Cleaner.cpp' },
    @{ Source = 'Cleaner.h'; Destination = 'src/modules/Cleaner.h' },
    @{ Source = 'DPopGuard.cpp'; Destination = 'src/modules/DPopGuard.cpp' },
    @{ Source = 'DPopGuard.h'; Destination = 'src/modules/DPopGuard.h' },
    @{ Source = 'StartupManager.cpp'; Destination = 'src/modules/StartupManager.cpp' },
    @{ Source = 'StartupManager.h'; Destination = 'src/modules/StartupManager.h' },
    @{ Source = 'SystemInfo.cpp'; Destination = 'src/modules/SystemInfo.cpp' },
    @{ Source = 'SystemInfo.h'; Destination = 'src/modules/SystemInfo.h' },
    @{ Source = 'ZapretManager.cpp'; Destination = 'src/modules/ZapretManager.cpp' },
    @{ Source = 'ZapretManager.h'; Destination = 'src/modules/ZapretManager.h' },
    @{ Source = 'ZapretPolicy.cpp'; Destination = 'src/modules/ZapretPolicy.cpp' },
    @{ Source = 'ZapretPolicy.h'; Destination = 'src/modules/ZapretPolicy.h' },
    @{ Source = 'Hash.cpp'; Destination = 'src/update/Hash.cpp' },
    @{ Source = 'Hash.h'; Destination = 'src/update/Hash.h' },
    @{ Source = 'Signature.cpp'; Destination = 'src/update/Signature.cpp' },
    @{ Source = 'Signature.h'; Destination = 'src/update/Signature.h' },
    @{ Source = 'UpdateClient.cpp'; Destination = 'src/update/UpdateClient.cpp' },
    @{ Source = 'UpdateClient.h'; Destination = 'src/update/UpdateClient.h' },
    @{ Source = 'UpdateConfig.h'; Destination = 'src/update/UpdateConfig.h' },
    @{ Source = 'UpdateManifest.cpp'; Destination = 'src/update/UpdateManifest.cpp' },
    @{ Source = 'UpdateManifest.h'; Destination = 'src/update/UpdateManifest.h' },
    @{ Source = 'UpdatePolicy.cpp'; Destination = 'src/update/UpdatePolicy.cpp' },
    @{ Source = 'UpdatePolicy.h'; Destination = 'src/update/UpdatePolicy.h' },
    @{ Source = 'UpdaterMain.cpp'; Destination = 'src/updater/main.cpp' },
    @{ Source = 'tests/UpdatePolicyTests.cpp'; Destination = 'tests/UpdatePolicyTests.cpp' },
    @{ Source = 'tests/SingleInstanceTests.cpp'; Destination = 'tests/SingleInstanceTests.cpp' },
    @{ Source = 'tests/ZapretPolicyTests.cpp'; Destination = 'tests/ZapretPolicyTests.cpp' },
    @{ Source = 'app.manifest'; Destination = 'resources/app.manifest' },
    @{ Source = 'updater.manifest'; Destination = 'resources/updater.manifest' },
    @{ Source = 'app.rc'; Destination = 'resources/app.rc' },
    @{ Source = 'updater.rc'; Destination = 'resources/updater.rc' },
    @{ Source = 'dpopcleaner.ico'; Destination = 'resources/dpopcleaner.ico' },
    @{ Source = 'version.rc.in'; Destination = 'resources/version.rc.in' }
)

function Get-R3ReleaseContract {
    [CmdletBinding()]
    param()
    return $script:R3Contract.PSObject.Copy()
}

function Get-R3ZapretContract {
    [CmdletBinding()]
    param()
    return $script:ZapretContract.PSObject.Copy()
}

function Get-R3SourceMap {
    [CmdletBinding()]
    param()
    foreach ($entry in $script:SourceMap) {
        [pscustomobject][ordered]@{
            Source = [string]$entry.Source
            Destination = [string]$entry.Destination
        }
    }
}

function Assert-R3SourceMap {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$RepositoryRoot,
        [Parameter(Mandatory)][object[]]$SourceMap
    )

    $resolvedRoot = [IO.Path]::GetFullPath($RepositoryRoot).TrimEnd([IO.Path]::DirectorySeparatorChar)
    $destinations = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)

    foreach ($entry in $SourceMap) {
        $source = [string]$entry.Source
        $destination = ([string]$entry.Destination).Replace('\', '/')
        if ([string]::IsNullOrWhiteSpace($source) -or [string]::IsNullOrWhiteSpace($destination)) {
            throw 'R3 source entries require non-empty source and destination paths.'
        }
        if ([IO.Path]::IsPathRooted($destination) -or $destination -match '(^|/)\.\.(/|$)') {
            throw "Unsafe R3 destination: $destination"
        }
        if (-not $destinations.Add($destination)) {
            throw "Duplicate R3 destination: $destination"
        }

        $sourcePath = [IO.Path]::GetFullPath((Join-Path $resolvedRoot $source))
        $rootPrefix = $resolvedRoot + [IO.Path]::DirectorySeparatorChar
        if (-not $sourcePath.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "R3 source escapes repository: $source"
        }
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
            throw "Missing tracked R3 source: $source"
        }
    }
}

function Assert-R3VersionHeader {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Version header does not exist: $Path"
    }

    $content = Get-Content -Raw -LiteralPath $Path
    $expected = @(
        'kProductName\[\]\s*=\s*L"DPopCleaner"',
        'kVersion\[\]\s*=\s*L"0\.3\.1"',
        'kDisplayVersion\[\]\s*=\s*L"0\.3\.1 BETA R3"',
        'kChannel\[\]\s*=\s*L"beta"',
        'kVersionCode\s*=\s*3013\s*;',
        'kRevision\s*=\s*3\s*;'
    )
    if (@($expected | Where-Object { $content -notmatch $_ }).Count -gt 0) {
        throw 'Version header does not match R3 release contract.'
    }
}

function Assert-R3ResourceDefinitions {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$AppManifestPath,
        [Parameter(Mandatory)][string]$UpdaterManifestPath,
        [Parameter(Mandatory)][string]$AppResourcePath,
        [Parameter(Mandatory)][string]$UpdaterResourcePath,
        [Parameter(Mandatory)][string]$VersionResourcePath,
        [Parameter(Mandatory)][string]$InstallerDefinitionPath
    )

    foreach ($path in @($AppManifestPath, $UpdaterManifestPath, $AppResourcePath, $UpdaterResourcePath, $VersionResourcePath, $InstallerDefinitionPath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "R3 resource definition does not exist: $path"
        }
    }

    $appManifest = Get-Content -Raw -LiteralPath $AppManifestPath
    if ($appManifest -notmatch 'requestedExecutionLevel\s+level="requireAdministrator"\s+uiAccess="false"') {
        throw 'Application manifest must require administrator rights.'
    }
    $updaterManifest = Get-Content -Raw -LiteralPath $UpdaterManifestPath
    if ($updaterManifest -notmatch 'requestedExecutionLevel\s+level="asInvoker"\s+uiAccess="false"') {
        throw 'Updater manifest must remain asInvoker.'
    }

    $appResource = Get-Content -Raw -LiteralPath $AppResourcePath
    if ($appResource -notmatch 'IDI_APP_ICON\s+ICON\s+"dpopcleaner\.ico"' -or
        $appResource -notmatch '1\s+RT_MANIFEST\s+"app\.manifest"') {
        throw 'Application resource must embed icon 101 and app.manifest.'
    }
    $updaterResource = Get-Content -Raw -LiteralPath $UpdaterResourcePath
    if ($updaterResource -notmatch 'IDI_APP_ICON\s+ICON\s+"dpopcleaner\.ico"' -or
        $updaterResource -notmatch '1\s+RT_MANIFEST\s+"updater\.manifest"') {
        throw 'Updater resource must embed icon 101 and updater.manifest.'
    }

    $versionResource = Get-Content -Raw -LiteralPath $VersionResourcePath
    if ($versionResource -notmatch 'FILEVERSION\s+0,3,1,3' -or
        $versionResource -notmatch 'PRODUCTVERSION\s+0,3,1,3' -or
        $versionResource -notmatch 'VALUE\s+"ProductVersion",\s*"0\.3\.1 BETA R3\\0"') {
        throw 'Version resource must identify DPopCleaner 0.3.1 BETA R3 revision 3.'
    }

    $installer = Get-Content -Raw -LiteralPath $InstallerDefinitionPath
    $validInstaller =
        $installer -match '#define MyAppVersion "0\.3\.1 BETA R3"' -and
        $installer -match 'PrivilegesRequired=admin' -and
        $installer -match 'SetupIconFile=\{#IconFile\}' -and
        $installer -match 'OutputBaseFilename=DPopCleaner_Setup_0\.3\.1_BETA_R3'
    if (-not $validInstaller) {
        throw 'Installer resources do not match the R3 UAC/icon contract.'
    }
}

Export-ModuleMember -Function Get-R3ReleaseContract, Get-R3ZapretContract, Get-R3SourceMap, Assert-R3SourceMap, Assert-R3VersionHeader, Assert-R3ResourceDefinitions
