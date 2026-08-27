[CmdletBinding()]
param(
    [string]$InstallerPath = '_release/0.4.18/installer/DPopCleaner_Setup_0.4.18.exe',
    [string]$OutputDir = '_release/0.4.18/evidence/install'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$InstallerPath = if ([IO.Path]::IsPathRooted($InstallerPath)) { $InstallerPath } else { Join-Path $repoRoot $InstallerPath }
$OutputDir = if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repoRoot $OutputDir }
$InstallerPath = [IO.Path]::GetFullPath($InstallerPath)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)

if (-not (Test-Path -LiteralPath $InstallerPath -PathType Leaf)) { throw "DPopCleaner_Setup_0.4.18.exe not found: $InstallerPath" }
if ([IO.Path]::GetFileName($InstallerPath) -ne 'DPopCleaner_Setup_0.4.18.exe') { throw "Unexpected installer filename: $InstallerPath" }

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$installRoot = Join-Path ([IO.Path]::GetTempPath()) 'dpop0418-installed-smoke'
$diskEvidence = Join-Path $OutputDir 'disk-installed'
$restoreEvidence = Join-Path $OutputDir 'restore-installed'
$reportPath = Join-Path $OutputDir 'install-smoke-report.json'
$localAppData = [Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)
$zapretBackupRoot = Join-Path $localAppData 'DPopCleaner\ZapretBackup'
$installed = $false
$uninstalled = $false
$documentationAclModify = $false
$upgradeSentinelPreserved = $false
$zapretUserListPreserved = $false
$zapretBackupVerified = $false
$zapretVersion = ''
$zapretBackupPath = ''
$iconSmoke = $false

function Assert-File([string]$RelativePath) {
    $path = Join-Path $installRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Installed file missing: $RelativePath" }
    return $path
}

function Assert-Directory([string]$RelativePath) {
    $path = Join-Path $installRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Container)) { throw "Installed directory missing: $RelativePath" }
    return $path
}

function Run-Installer {
    $process = Start-Process -FilePath $InstallerPath -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',"/DIR=$installRoot") -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Silent install failed with exit code $($process.ExitCode)." }
}

try {
    if (Test-Path -LiteralPath $installRoot) { Remove-Item -LiteralPath $installRoot -Recurse -Force }

    Run-Installer
    $installed = $true

    $core = Assert-File 'DPopCleaner.exe'
    $updater = Assert-File 'DPopUpdater.exe'
    [void](Assert-File 'Modules\DPop.Common.dll')
    $diskExe = Assert-File 'Modules\DiskAnalyzer.exe'
    $restoreExe = Assert-File 'Modules\RestoreCenter.exe'
    [void](Assert-File 'Modules\ZapretScreenFix.exe')
    [void](Assert-File 'Languages\ru.json')
    [void](Assert-File 'Languages\en.json')
    [void](Assert-File 'Shell\shell.json')
    [void](Assert-File 'Shell\commands\disk-analyzer.json')
    [void](Assert-File 'Shell\commands\restore-center.json')
    [void](Assert-File 'Shell\commands\zapret-screen-fix.json')
    [void](Assert-File 'Documentation\README.txt')
    [void](Assert-File 'Documentation\THIRD_PARTY_NOTICES.txt')

    [void](Assert-Directory 'ThirdParty\Zapret')
    $zapretLists = Assert-Directory 'ThirdParty\Zapret\lists'
    [void](Assert-Directory 'ThirdParty\Zapret\utils')
    [void](Assert-File 'ThirdParty\Zapret\LICENSE.txt')
    [void](Assert-File 'ThirdParty\Zapret\service.bat')
    $generalStrategy = Assert-File 'ThirdParty\Zapret\general.bat'
    [void](Assert-File 'ThirdParty\Zapret\bin\winws.exe')
    [void](Assert-File 'ThirdParty\Zapret\bin\WinDivert.dll')
    [void](Assert-File 'ThirdParty\Zapret\bin\WinDivert64.sys')
    $zapretVersionFile = Assert-File 'ThirdParty\Zapret\.service\version.txt'
    $zapretVersion = (Get-Content -LiteralPath $zapretVersionFile -Raw).Trim()
    if ($zapretVersion -ne '1.10.2') { throw "Installed bundled Zapret version mismatch: $zapretVersion" }
    $strategyText = Get-Content -LiteralPath $generalStrategy -Raw
    if ($strategyText -notmatch 'discord\.media') { throw 'Installed general.bat does not contain the required discord.media strategy section.' }

    $coreVersion = (Get-Item -LiteralPath $core).VersionInfo.FileVersion
    $updaterVersion = (Get-Item -LiteralPath $updater).VersionInfo.FileVersion
    if ($coreVersion -ne '0.4.18.2') { throw "Installed DPopCleaner.exe FileVersion mismatch: $coreVersion" }
    if ($updaterVersion -ne '0.4.18.2') { throw "Installed DPopUpdater.exe FileVersion mismatch: $updaterVersion" }

    & (Join-Path $PSScriptRoot 'dpop0418_icon_smoke.ps1') -MainExe $core -UpdaterExe $updater -Installer $InstallerPath
    $iconSmoke = $true

    $documentationPath = Join-Path $installRoot 'Documentation'
    $usersSidValue = 'S-1-5-32-545'
    $acl = Get-Acl -LiteralPath $documentationPath
    foreach ($rule in $acl.Access) {
        try { $ruleSid = $rule.IdentityReference.Translate([System.Security.Principal.SecurityIdentifier]).Value } catch { $ruleSid = '' }
        $hasModify = (($rule.FileSystemRights -band [System.Security.AccessControl.FileSystemRights]::Modify) -eq [System.Security.AccessControl.FileSystemRights]::Modify)
        if ($ruleSid -eq $usersSidValue -and $rule.AccessControlType -eq [System.Security.AccessControl.AccessControlType]::Allow -and $hasModify) {
            $documentationAclModify = $true
            break
        }
    }
    if (-not $documentationAclModify) { throw "Documentation ACL does not grant BUILTIN\Users ($usersSidValue) Modify: $documentationPath" }

    $sentinel = Join-Path $documentationPath 'upgrade-preserve-smoke.txt'
    Set-Content -LiteralPath $sentinel -Encoding utf8 -Value 'preserve-me'
    $zapretUserList = Join-Path $zapretLists 'list-general-user.txt'
    $zapretSentinelValue = 'dpop0418-preserve-zapret-user-list'
    Set-Content -LiteralPath $zapretUserList -Encoding utf8 -Value $zapretSentinelValue

    $backupsBefore = @()
    if (Test-Path -LiteralPath $zapretBackupRoot -PathType Container) {
        $backupsBefore = @(Get-ChildItem -LiteralPath $zapretBackupRoot -Directory -Force | ForEach-Object { $_.FullName })
    }

    Run-Installer

    $upgradeSentinelPreserved = (Test-Path -LiteralPath $sentinel -PathType Leaf) -and ((Get-Content -Raw -LiteralPath $sentinel).Trim() -eq 'preserve-me')
    if (-not $upgradeSentinelPreserved) { throw 'In-place reinstall destroyed user Documentation data.' }
    $zapretUserListPreserved = (Test-Path -LiteralPath $zapretUserList -PathType Leaf) -and ((Get-Content -Raw -LiteralPath $zapretUserList).Trim() -eq $zapretSentinelValue)
    if (-not $zapretUserListPreserved) { throw 'In-place reinstall destroyed ThirdParty\Zapret\lists\list-general-user.txt.' }

    if (Test-Path -LiteralPath $zapretBackupRoot -PathType Container) {
        $newBackup = Get-ChildItem -LiteralPath $zapretBackupRoot -Directory -Force |
            Where-Object { $backupsBefore -notcontains $_.FullName } |
            Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
        if ($newBackup) {
            $zapretBackupPath = $newBackup.FullName
            $backupSentinel = Join-Path $newBackup.FullName 'list-general-user.txt'
            $zapretBackupVerified = (Test-Path -LiteralPath $backupSentinel -PathType Leaf) -and ((Get-Content -Raw -LiteralPath $backupSentinel).Trim() -eq $zapretSentinelValue)
        }
    }
    if (-not $zapretBackupVerified) { throw 'ZapretBackup did not retain the pre-upgrade list-general-user.txt bytes.' }

    [void](Assert-File 'ThirdParty\Zapret\LICENSE.txt')
    [void](Assert-File 'ThirdParty\Zapret\service.bat')
    [void](Assert-File 'ThirdParty\Zapret\general.bat')
    [void](Assert-File 'ThirdParty\Zapret\bin\winws.exe')
    [void](Assert-File 'ThirdParty\Zapret\bin\WinDivert.dll')
    [void](Assert-File 'ThirdParty\Zapret\bin\WinDivert64.sys')
    $reinstalledZapretVersion = (Get-Content -LiteralPath (Assert-File 'ThirdParty\Zapret\.service\version.txt') -Raw).Trim()
    if ($reinstalledZapretVersion -ne '1.10.2') { throw "Reinstalled bundled Zapret version mismatch: $reinstalledZapretVersion" }

    & (Join-Path $PSScriptRoot 'dpop0418_close_smoke.ps1') -Exe $core
    & (Join-Path $PSScriptRoot 'dpop0417_disk_smoke.ps1') -ExePath $diskExe -OutputDir $diskEvidence
    & (Join-Path $PSScriptRoot 'dpop0417_restore_smoke.ps1') -ExePath $restoreExe -OutputDir $restoreEvidence

    $uninstaller = Assert-File 'unins000.exe'
    $uninstall = Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait -PassThru
    if ($uninstall.ExitCode -ne 0) { throw "Silent uninstall failed with exit code $($uninstall.ExitCode)." }

    Start-Sleep -Milliseconds 500
    $uninstalled = -not (Test-Path -LiteralPath (Join-Path $installRoot 'DPopCleaner.exe') -PathType Leaf)
    if (-not $uninstalled) { throw 'Installed DPopCleaner.exe remained after uninstall.' }

    [pscustomobject]@{
        installer = $InstallerPath
        install_root = $installRoot
        dpopcleaner_file_version = $coreVersion
        updater_file_version = $updaterVersion
        icon_smoke = [bool]$iconSmoke
        documentation_acl_modify = [bool]$documentationAclModify
        upgrade_sentinel_preserved = [bool]$upgradeSentinelPreserved
        bundled_zapret_version = $zapretVersion
        zapret_user_list_preserved = [bool]$zapretUserListPreserved
        zapret_backup_verified = [bool]$zapretBackupVerified
        zapret_backup_path = $zapretBackupPath
        close_smoke = $true
        disk_smoke = (Test-Path -LiteralPath (Join-Path $diskEvidence 'disk-smoke-report.json') -PathType Leaf)
        restore_smoke = (Test-Path -LiteralPath (Join-Path $restoreEvidence 'restore-smoke-report.json') -PathType Leaf)
        zapret_screen_fix = $true
        uninstalled = [bool]$uninstalled
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $reportPath -Encoding utf8

    Write-Host 'Installed DPopCleaner.exe: 0.4.18.2 PASS'
    Write-Host 'Installed DPopUpdater.exe: 0.4.18.2 PASS'
    Write-Host 'Installed application/installer icons: PASS'
    Write-Host 'Installed bundled Flowseal Zapret: 1.10.2 PASS'
    Write-Host 'In-place Documentation preservation: PASS'
    Write-Host 'In-place Zapret *-user.txt preservation + ZapretBackup: PASS'
    Write-Host 'Installed non-blocking close smoke: PASS'
    Write-Host 'Installed companion module smokes: PASS'
    Write-Host 'Silent uninstall: PASS'
}
finally {
    if ($installed -and -not $uninstalled) {
        $uninstaller = Join-Path $installRoot 'unins000.exe'
        if (Test-Path -LiteralPath $uninstaller -PathType Leaf) {
            try { Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait | Out-Null } catch { }
        }
    }
    if (Test-Path -LiteralPath $installRoot) { Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue }
}
