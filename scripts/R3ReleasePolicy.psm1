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
        $versionResource -notmatch 'VALUE\s+"ProductVersion",\s*"0\.3\.1 BETA R3\\0"' -or
        $versionResource -notmatch 'VALUE\s+"InternalName",\s*"@DPOP_INTERNAL_NAME@\\0"' -or
        $versionResource -notmatch 'VALUE\s+"OriginalFilename",\s*"@DPOP_ORIGINAL_FILENAME@\\0"') {
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

function Assert-R3FileHash {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedSha256,
        [Parameter(Mandatory)][string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Description does not exist: $Path"
    }
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
    if ($actual -ne $ExpectedSha256.ToLowerInvariant()) {
        throw "Unexpected $Description SHA-256: $actual"
    }
}

function Assert-R3BundleTree {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "Zapret bundle directory does not exist: $Path"
    }
    $requiredFiles = @(
        'general.bat',
        'service.bat',
        'bin/winws.exe',
        'bin/WinDivert.dll',
        'bin/WinDivert64.sys',
        'bin/cygwin1.dll',
        'utils/check_updates.enabled',
        'LICENSE.txt'
    )
    foreach ($relative in $requiredFiles) {
        if (-not (Test-Path -LiteralPath (Join-Path $Path $relative) -PathType Leaf)) {
            throw "Missing required Zapret file: $relative"
        }
    }
    $listFiles = @(Get-ChildItem -LiteralPath (Join-Path $Path 'lists') -File -ErrorAction SilentlyContinue)
    if ($listFiles.Count -eq 0) {
        throw 'Missing required Zapret lists payload.'
    }
}

function Assert-R3StagedPayload {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "R3 staged payload does not exist: $Path"
    }
    $actual = @(Get-ChildItem -LiteralPath $Path -Force | ForEach-Object Name | Sort-Object)
    $expected = @('DPopCleaner.exe', 'DPopUpdater.exe', 'zapret') | Sort-Object
    if (Compare-Object -ReferenceObject $expected -DifferenceObject $actual) {
        throw "Unexpected R3 staged payload: $($actual -join ', ')"
    }
    foreach ($binary in @('DPopCleaner.exe', 'DPopUpdater.exe')) {
        $item = Get-Item -LiteralPath (Join-Path $Path $binary)
        if ($item.PSIsContainer -or $item.Length -le 0) {
            throw "Invalid staged application binary: $binary"
        }
    }
    Assert-R3BundleTree -Path (Join-Path $Path 'zapret')
}

function Assert-R3InstallerDefinition {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "R3 installer definition does not exist: $Path"
    }
    $content = Get-Content -Raw -LiteralPath $Path
    $lines = Get-Content -LiteralPath $Path
    $inFiles = $false
    $payloads = @()
    foreach ($line in $lines) {
        if ($line -match '^\s*\[Files\]\s*$') { $inFiles = $true; continue }
        if ($inFiles -and $line -match '^\s*\[[^]]+\]\s*$') { break }
        if ($inFiles -and $line -match '^\s*Source\s*:') { $payloads += $line }
    }
    if ($payloads.Count -ne 3) {
        throw "R3 installer must contain exactly three payload declarations; found $($payloads.Count)."
    }
    $validPayloads =
        @($payloads | Where-Object { $_ -match 'Source:\s*"\{#SourceDir\}\\DPopCleaner\.exe";\s*DestDir:\s*"\{app\}"' }).Count -eq 1 -and
        @($payloads | Where-Object { $_ -match 'Source:\s*"\{#SourceDir\}\\DPopUpdater\.exe";\s*DestDir:\s*"\{app\}"' }).Count -eq 1 -and
        @($payloads | Where-Object { $_ -match 'Source:\s*"\{#ZapretDir\}\\\*";\s*DestDir:\s*"\{app\}\\zapret"' -and $_ -match 'recursesubdirs' -and $_ -match 'createallsubdirs' }).Count -eq 1
    if (-not $validPayloads) {
        throw 'R3 installer payload declarations do not match the approved staging roots.'
    }
    if ($content -notmatch '#define MyAppVersion "0\.3\.1 BETA R3"' -or
        $content -notmatch 'OutputBaseFilename=DPopCleaner_Setup_0\.3\.1_BETA_R3' -or
        $content -notmatch 'PrivilegesRequired=admin' -or
        $content -notmatch 'SetupIconFile=\{#IconFile\}') {
        throw 'R3 installer identity, privileges, or icon is invalid.'
    }
    if ($content -match '(?im)^\s*Filename\s*:.*(?:service\.bat|general[^;]*\.bat|winws\.exe)') {
        throw 'R3 installer must not start Zapret or WinDivert automatically.'
    }
}

function New-R3ReleaseManifest {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$InstallerPath,
        [Parameter(Mandatory)][bool]$Signed
    )

    if (-not (Test-Path -LiteralPath $InstallerPath -PathType Leaf)) {
        throw "R3 installer does not exist: $InstallerPath"
    }
    if ([IO.Path]::GetFileName($InstallerPath) -ne $script:R3Contract.AssetName) {
        throw "Unexpected R3 installer filename: $([IO.Path]::GetFileName($InstallerPath))"
    }
    $item = Get-Item -LiteralPath $InstallerPath
    if ($item.Length -le 0) { throw 'R3 installer must not be empty.' }

    [pscustomobject][ordered]@{
        product = 'DPopCleaner'
        channel = 'beta'
        version = '0.3.1'
        version_code = 3013
        revision = 3
        mandatory = $false
        download_url = $script:R3Contract.ReleaseUrl
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $InstallerPath).Hash.ToLowerInvariant()
        size = [int64]$item.Length
        signed = $Signed
        available = $true
        notes_url = 'https://elesnichenko1-droid.github.io/dpopcleaner-site/'
        install_args = '/SILENT /NORESTART'
    }
}

function Assert-R3ReleaseManifest {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][object]$Manifest,
        [switch]$Published
    )

    function Read-Field([string]$Name) {
        $property = $Manifest.PSObject.Properties[$Name]
        if ($null -eq $property) { return $null }
        return $property.Value
    }

    $valid =
        (Read-Field 'product') -eq 'DPopCleaner' -and
        (Read-Field 'channel') -eq 'beta' -and
        (Read-Field 'version') -eq '0.3.1' -and
        [int](Read-Field 'version_code') -eq 3013 -and
        [int](Read-Field 'revision') -eq 3 -and
        (Read-Field 'install_args') -eq '/SILENT /NORESTART'

    if ($Published) {
        $valid = $valid -and
            (Read-Field 'available') -eq $true -and
            (Read-Field 'download_url') -eq $script:R3Contract.ReleaseUrl -and
            (Read-Field 'sha256') -is [string] -and
            (Read-Field 'sha256') -match '^[a-f0-9]{64}$' -and
            [int64](Read-Field 'size') -gt 0 -and
            (Read-Field 'signed') -is [bool]
    } else {
        $valid = $valid -and (Read-Field 'available') -eq $false
    }
    if (-not $valid) { throw 'Invalid R3 release manifest.' }
}

function Assert-R3WorkflowDefinition {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "R3 workflow does not exist: $Path"
    }
    $content = Get-Content -Raw -LiteralPath $Path
    if ($content -notmatch 'F748D61FEC75E4EDC992CB5B09D554E914197C68C690384ACEB61F143D8F76C9') {
        throw 'Workflow must use the pinned Zapret archive hash.'
    }
    if ($content -notmatch 'FE3983A1E91206AD1A530BCFAE01FAD207020CB61882EDD62C1E3CB5F8D5D430') {
        throw 'Workflow must use the pinned Zapret license hash.'
    }
    if ($content -match '(?im)Set-Content[^\r\n]*(?:\.cpp|\.h|\.rc)(?:\s|$)') {
        throw 'Workflow must not generate C++ source or resource files.'
    }
    if ($content -match '\$binary:') {
        throw 'Workflow contains ambiguous PowerShell variable interpolation before a colon.'
    }

    $requiredGates = @(
        'Run release policy tests',
        'Download and verify Zapret 1.10.1',
        'Prepare tracked source tree',
        'Build Release',
        'Run C++ tests',
        'Verify manifests versions and icons',
        'Stage complete R3 payload',
        'Build R3 installer',
        'Run Defender scans',
        'Generate R3 release manifest',
        'Upload branch artifacts',
        'Publish immutable R3 prerelease',
        'Verify fresh release download',
        'Commit verified site metadata'
    )
    $previous = -1
    foreach ($gate in $requiredGates) {
        $index = $content.IndexOf("name: $gate", [StringComparison]::Ordinal)
        if ($index -lt 0) { throw "Workflow is missing required gate: $gate" }
        if ($index -le $previous) { throw "Workflow gates are out of order at: $gate" }
        $previous = $index
    }
    foreach ($required in @(
        "if: github.ref == 'refs/heads/main'",
        'DPopCleaner_Setup_0.3.1_BETA_R3.exe',
        'v0.3.1-beta-r3',
        'WINDOWS_CERT_PFX_BASE64',
        'WINDOWS_CERT_PASSWORD',
        'MpCmdRun.exe',
        'Remove-MpPreference -ExclusionPath',
        "if (`$outputText -match 'was skipped')",
        "if (`$outputText -notmatch 'found no threats')",
        'scripts/Capture-AppScreenshot.ps1',
        'ref: ${{ github.sha }}',
        '--target $env:GITHUB_SHA',
        'git rev-list -n 1 $env:R3_TAG',
        'git fetch origin main',
        'if ($remoteMain -ne $env:GITHUB_SHA)',
        'Upload verified Pages site',
        'dpopcleaner-pages-site-${{ github.run_id }}'
    )) {
        if (-not $content.Contains($required)) {
            throw "Workflow is missing required release policy text: $required"
        }
    }
}

function Assert-R3PagesWorkflowDefinition {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Pages workflow does not exist: $Path"
    }
    $content = Get-Content -Raw -LiteralPath $Path
    if ($content -match '(?m)^\s{2}workflow_dispatch:\s*$') {
        throw 'Pages workflow must not expose workflow_dispatch.'
    }
    foreach ($required in @(
        'workflow_run:',
        'github.event.workflow_run.conclusion == ''success''',
        'github.event.workflow_run.head_branch == ''main''',
        'actions/download-artifact@v5',
        'run-id: ${{ github.event.workflow_run.id }}',
        'dpopcleaner-pages-site-${{ github.event.workflow_run.id }}',
        'actions/deploy-pages@v5'
    )) {
        if (-not $content.Contains($required)) {
            throw "Pages workflow is missing verified-artifact policy text: $required"
        }
    }
    if ($content.Contains('actions/checkout@') -or $content.Contains('Stage-Site.ps1')) {
        throw 'Pages workflow must deploy the verified artifact without checkout.'
    }
}

function Assert-LegacyReleaseWorkflowsManualOnly {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string[]]$Paths)

    foreach ($path in $Paths) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Legacy workflow does not exist: $path"
        }
        $content = Get-Content -Raw -LiteralPath $path
        if ($content -notmatch '(?m)^\s{2}workflow_dispatch:\s*$' -or
            $content -match '(?m)^\s{2}(?:push|pull_request|schedule):\s*$') {
            throw "Legacy release workflow must be manual-only: $path"
        }
    }
}

Export-ModuleMember -Function Get-R3ReleaseContract, Get-R3ZapretContract, Get-R3SourceMap, Assert-R3SourceMap, Assert-R3VersionHeader, Assert-R3ResourceDefinitions, Assert-R3FileHash, Assert-R3BundleTree, Assert-R3StagedPayload, Assert-R3InstallerDefinition, New-R3ReleaseManifest, Assert-R3ReleaseManifest, Assert-R3WorkflowDefinition, Assert-R3PagesWorkflowDefinition, Assert-LegacyReleaseWorkflowsManualOnly
