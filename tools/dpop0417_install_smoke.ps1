[CmdletBinding()]
param(
    [string]$InstallerPath = '_release/0.4.17/installer/DPopCleaner_Setup_0.4.17.exe',
    [string]$OutputDir = '_release/0.4.17/evidence/install'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$InstallerPath = if ([IO.Path]::IsPathRooted($InstallerPath)) { $InstallerPath } else { Join-Path $repoRoot $InstallerPath }
$OutputDir = if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repoRoot $OutputDir }
$InstallerPath = [IO.Path]::GetFullPath($InstallerPath)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
if (-not (Test-Path -LiteralPath $InstallerPath -PathType Leaf)) { throw "DPopCleaner_Setup_0.4.17.exe not found: $InstallerPath" }
if ([IO.Path]::GetFileName($InstallerPath) -ne 'DPopCleaner_Setup_0.4.17.exe') { throw "Unexpected installer filename: $InstallerPath" }

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$installRoot = Join-Path ([IO.Path]::GetTempPath()) 'dpop0417-installed-smoke'
$zapretRoot = Join-Path $installRoot 'Zapret'
$diskEvidence = Join-Path $OutputDir 'disk-installed'
$restoreEvidence = Join-Path $OutputDir 'restore-installed'
$zapretEvidence = Join-Path $OutputDir 'zapret-installed'
$settingsEvidence = Join-Path $OutputDir 'settings-installed'
$rev7Evidence = Join-Path $OutputDir 'rev7-ui-installed'
$rev12Evidence = Join-Path $OutputDir 'rev12-native-version'
$rev13Evidence = Join-Path $OutputDir 'rev13-uac-tray'
$reportPath = Join-Path $OutputDir 'install-smoke-report.json'
$expectedCoreBlob = 'efd0eff1f4962319282363fa85595c25e0cebe11'
$installed = $false
$uninstalled = $false
$documentationAclModify = $false
$installedSettingsBridgeSmoke = $false
$rev7FunctionalSmoke = $false
$rev12NativeVersionSmoke = $false
$rev13UacTraySmoke = $false
$zapretScreenFixPresent = $false
$zapretRuntimePresent = $false
$zapretUiSmoke = $false

function Assert-File([string]$RelativePath) {
    $path = Join-Path $installRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Installed file missing: $RelativePath" }
    return $path
}

try {
    if (Test-Path -LiteralPath $installRoot) { Remove-Item -LiteralPath $installRoot -Recurse -Force }
    $install = Start-Process -FilePath $InstallerPath -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',"/DIR=$installRoot") -Wait -PassThru
    if ($install.ExitCode -ne 0) { throw "Silent install failed with exit code $($install.ExitCode)." }
    $installed = $true

    $appLauncher = Assert-File 'DPopCleaner.exe'
    $core = Assert-File 'DPopCleaner.Core.exe'
    $launcher = Assert-File 'SimpleUpdate.exe'
    $appLauncherHash = (Get-FileHash -LiteralPath $appLauncher -Algorithm SHA256).Hash
    $simpleUpdateHash = (Get-FileHash -LiteralPath $launcher -Algorithm SHA256).Hash
    if ($appLauncherHash -ne $simpleUpdateHash) { throw 'Installed DPopCleaner.exe is not byte-identical to SimpleUpdate.exe bridge.' }

    [void](Assert-File 'Zapret\LICENSE.txt')
    [void](Assert-File 'Zapret\service.bat')
    [void](Assert-File 'Zapret\general.bat')
    $serviceVersionFile = Assert-File 'Zapret\.service\version.txt'
    $nativeVersionFile = Assert-File 'Zapret\utils\dpop_version.txt'
    [void](Assert-File 'Zapret\bin\winws.exe')
    [void](Assert-File 'Zapret\bin\WinDivert.dll')
    [void](Assert-File 'Zapret\bin\WinDivert64.sys')
    if (-not (Test-Path -LiteralPath (Join-Path $zapretRoot 'lists') -PathType Container)) { throw 'Installed Zapret lists directory missing.' }
    if (-not (Test-Path -LiteralPath (Join-Path $zapretRoot 'utils') -PathType Container)) { throw 'Installed Zapret utils directory missing.' }
    $zapretVersion = (Get-Content -Raw -LiteralPath $serviceVersionFile).Trim()
    $nativeZapretVersion = (Get-Content -Raw -LiteralPath $nativeVersionFile).Trim()
    if ($zapretVersion -ne '1.10.2') { throw "Installed Zapret version mismatch: $zapretVersion" }
    if ($nativeZapretVersion -ne $zapretVersion) { throw "Installed frozen-core dpop_version mismatch: $nativeZapretVersion vs $zapretVersion" }
    if ((Get-FileHash -LiteralPath $serviceVersionFile -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $nativeVersionFile -Algorithm SHA256).Hash) {
        throw 'Installed native dpop_version.txt is not byte-identical to pinned .service/version.txt.'
    }
    $installedStrategies = @(Get-ChildItem -LiteralPath $zapretRoot -Filter 'general*.bat' -File)
    if ($installedStrategies.Count -eq 0) { throw 'Installed Zapret has no general*.bat strategies under Zapret\.' }
    $zapretRuntimePresent = $true

    [void](Assert-File 'Modules\DPop.Common.dll')
    $diskExe = Assert-File 'Modules\DiskAnalyzer.exe'
    $restoreExe = Assert-File 'Modules\RestoreCenter.exe'
    [void](Assert-File 'Modules\ZapretScreenFix.exe')
    $zapretScreenFixPresent = $true
    [void](Assert-File 'Languages\ru.json')
    [void](Assert-File 'Languages\en.json')
    [void](Assert-File 'Shell\shell.json')
    [void](Assert-File 'Shell\commands\disk-analyzer.json')
    [void](Assert-File 'Shell\commands\restore-center.json')
    [void](Assert-File 'Shell\commands\zapret-screen-fix.json')
    [void](Assert-File 'Documentation\README.txt')

    $documentationPath = Join-Path $installRoot 'Documentation'
    $usersSidValue = 'S-1-5-32-545'
    $acl = Get-Acl -LiteralPath $documentationPath
    foreach ($rule in $acl.Access) {
        try { $ruleSid = $rule.IdentityReference.Translate([System.Security.Principal.SecurityIdentifier]).Value } catch { $ruleSid = '' }
        $hasModify = (($rule.FileSystemRights -band [System.Security.AccessControl.FileSystemRights]::Modify) -eq [System.Security.AccessControl.FileSystemRights]::Modify)
        if ($ruleSid -eq $usersSidValue -and $rule.AccessControlType -eq [System.Security.AccessControl.AccessControlType]::Allow -and $hasModify) { $documentationAclModify = $true; break }
    }
    if (-not $documentationAclModify) { throw "Documentation ACL does not grant BUILTIN\Users ($usersSidValue) Modify: $documentationPath" }

    $installedCoreBlob = (& git -C $repoRoot hash-object -- $core).Trim()
    if ($LASTEXITCODE -ne 0 -or $installedCoreBlob -ne $expectedCoreBlob) { throw "Installed immutable core mismatch: $installedCoreBlob" }

    & (Join-Path $PSScriptRoot 'dpop0417_installed_settings_smoke.ps1') -RootPath $installRoot -OutputDir $settingsEvidence
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $installedSettingsBridgeSmoke = Test-Path -LiteralPath (Join-Path $settingsEvidence 'installed-settings-smoke-report.json') -PathType Leaf
    if (-not $installedSettingsBridgeSmoke) { throw 'Installed Settings bridge smoke report was not produced.' }

    & (Join-Path $PSScriptRoot 'dpop0417_rev7_installed_ui_smoke.ps1') -RootPath $installRoot -OutputDir $rev7Evidence
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $rev7FunctionalSmoke = Test-Path -LiteralPath (Join-Path $rev7Evidence 'rev7-installed-ui-smoke-report.json') -PathType Leaf
    if (-not $rev7FunctionalSmoke) { throw 'rev.7 installed functional UI smoke report was not produced.' }

    & (Join-Path $PSScriptRoot 'dpop0417_zapret_ui_smoke.ps1') -RootPath $installRoot -OutputDir $zapretEvidence
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $zapretUiSmoke = Test-Path -LiteralPath (Join-Path $zapretEvidence 'zapret-ui-smoke-report.json') -PathType Leaf
    if (-not $zapretUiSmoke) { throw 'Installed authentic Zapret UI smoke report was not produced.' }

    & (Join-Path $PSScriptRoot 'dpop0417_rev12_native_version_smoke.ps1') -InstallerPath $InstallerPath -OutputDir $rev12Evidence
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $rev12NativeVersionSmoke = Test-Path -LiteralPath (Join-Path $rev12Evidence 'rev12-native-version-smoke-report.json') -PathType Leaf
    if (-not $rev12NativeVersionSmoke) { throw 'rev.12 native Zapret version smoke report was not produced.' }
    if (-not (Test-Path -LiteralPath (Join-Path $rev12Evidence 'rev12-zapret-native-version.png') -PathType Leaf)) { throw 'rev.12 native Zapret screenshot evidence was not produced.' }

    & (Join-Path $PSScriptRoot 'dpop0417_rev13_uac_tray_smoke.ps1') -RootPath $installRoot -OutputDir $rev13Evidence
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $rev13UacTraySmoke = Test-Path -LiteralPath (Join-Path $rev13Evidence 'rev13-uac-tray-smoke-report.json') -PathType Leaf
    if (-not $rev13UacTraySmoke) { throw 'rev.13 UAC/tray smoke report was not produced.' }

    & (Join-Path $PSScriptRoot 'dpop0417_disk_smoke.ps1') -ExePath $diskExe -OutputDir $diskEvidence
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $PSScriptRoot 'dpop0417_restore_smoke.ps1') -ExePath $restoreExe -OutputDir $restoreEvidence
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $uninstaller = Assert-File 'unins000.exe'
    $uninstall = Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait -PassThru
    if ($uninstall.ExitCode -ne 0) { throw "Silent uninstall failed with exit code $($uninstall.ExitCode)." }
    Start-Sleep -Milliseconds 500
    $wrapperRemoved = -not (Test-Path -LiteralPath (Join-Path $installRoot 'DPopCleaner.exe') -PathType Leaf)
    $coreRemoved = -not (Test-Path -LiteralPath (Join-Path $installRoot 'DPopCleaner.Core.exe') -PathType Leaf)
    $uninstalled = $wrapperRemoved -and $coreRemoved
    if (-not $uninstalled) { throw 'Installed DPopCleaner launcher/core remained after uninstall.' }

    [pscustomobject]@{
        installer = $InstallerPath
        install_root = $installRoot
        zapret_root = $zapretRoot
        installed_core_path = $core
        installed_core_blob = $installedCoreBlob
        expected_core_blob = $expectedCoreBlob
        legacy_dpopcleaner_path_is_bridge = ($appLauncherHash -eq $simpleUpdateHash)
        installed_settings_bridge_smoke = [bool]$installedSettingsBridgeSmoke
        rev7_functional_ui_smoke = [bool]$rev7FunctionalSmoke
        rev12_native_version_smoke = [bool]$rev12NativeVersionSmoke
        rev13_uac_tray_smoke = [bool]$rev13UacTraySmoke
        zapret_runtime_present = [bool]$zapretRuntimePresent
        zapret_version = $zapretVersion
        zapret_native_version_source = $nativeZapretVersion
        zapret_strategy_files = $installedStrategies.Count
        zapret_authentic_ui_smoke = [bool]$zapretUiSmoke
        zapret_screen_fix_present = [bool]$zapretScreenFixPresent
        documentation_acl_modify = [bool]$documentationAclModify
        disk_smoke = (Test-Path -LiteralPath (Join-Path $diskEvidence 'disk-smoke-report.json') -PathType Leaf)
        restore_smoke = (Test-Path -LiteralPath (Join-Path $restoreEvidence 'restore-smoke-report.json') -PathType Leaf)
        uninstalled = [bool]$uninstalled
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $reportPath -Encoding utf8

    Write-Host "Installed immutable core: $installedCoreBlob at DPopCleaner.Core.exe"
    Write-Host 'Historical DPopCleaner.exe path -> elevated Settings UI bridge: PASS'
    Write-Host 'Installed Settings scroll/auto-update/legacy-version smoke: PASS'
    Write-Host 'Installed rev.7 hide/restore + RAM + Zapret + Settings functional smoke: PASS'
    Write-Host 'Installed rev.12 native Zapret version source + screenshot smoke: PASS'
    Write-Host 'Installed rev.13 UAC + single RAM tray icon smoke: PASS'
    Write-Host "Installed Flowseal Zapret ${zapretVersion}: PASS; native_source=$nativeZapretVersion; root=$zapretRoot; strategies=$($installedStrategies.Count)"
    Write-Host 'Authentic installed Zapret Center strategy discovery: PASS'
    Write-Host 'Installed ZapretScreenFix companion: PASS'
    Write-Host 'Installed Documentation ACL: BUILTIN\Users Modify PASS'
    Write-Host 'Installed Disk Analyzer smoke: PASS'
    Write-Host 'Installed Restore Center smoke: PASS'
    Write-Host 'Silent uninstall: PASS'
}
finally {
    if ($installed -and -not $uninstalled) {
        $uninstaller = Join-Path $installRoot 'unins000.exe'
        if (Test-Path -LiteralPath $uninstaller -PathType Leaf) { try { Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait | Out-Null } catch { } }
    }
    if (Test-Path -LiteralPath $installRoot) { Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue }
}
