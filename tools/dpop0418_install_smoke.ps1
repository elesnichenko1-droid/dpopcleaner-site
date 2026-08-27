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

if (-not (Test-Path -LiteralPath $InstallerPath -PathType Leaf)) {
    throw "DPopCleaner_Setup_0.4.18.exe not found: $InstallerPath"
}
if ([IO.Path]::GetFileName($InstallerPath) -ne 'DPopCleaner_Setup_0.4.18.exe') {
    throw "Unexpected installer filename: $InstallerPath"
}

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$installRoot = Join-Path ([IO.Path]::GetTempPath()) 'dpop0418-installed-smoke'
$diskEvidence = Join-Path $OutputDir 'disk-installed'
$restoreEvidence = Join-Path $OutputDir 'restore-installed'
$reportPath = Join-Path $OutputDir 'install-smoke-report.json'
$installed = $false
$uninstalled = $false
$documentationAclModify = $false
$upgradeSentinelPreserved = $false

function Assert-File([string]$RelativePath) {
    $path = Join-Path $installRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Installed file missing: $RelativePath"
    }
    return $path
}

function Run-Installer {
    $process = Start-Process -FilePath $InstallerPath -ArgumentList @(
        '/VERYSILENT',
        '/SUPPRESSMSGBOXES',
        '/NORESTART',
        '/SP-',
        "/DIR=$installRoot"
    ) -Wait -PassThru
    if ($process.ExitCode -ne 0) {
        throw "Silent install failed with exit code $($process.ExitCode)."
    }
}

try {
    if (Test-Path -LiteralPath $installRoot) {
        Remove-Item -LiteralPath $installRoot -Recurse -Force
    }

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

    $coreVersion = (Get-Item -LiteralPath $core).VersionInfo.FileVersion
    $updaterVersion = (Get-Item -LiteralPath $updater).VersionInfo.FileVersion
    if ($coreVersion -ne '0.4.18.1') { throw "Installed DPopCleaner.exe FileVersion mismatch: $coreVersion" }
    if ($updaterVersion -ne '0.4.18.1') { throw "Installed DPopUpdater.exe FileVersion mismatch: $updaterVersion" }

    $documentationPath = Join-Path $installRoot 'Documentation'
    $usersSidValue = 'S-1-5-32-545'
    $acl = Get-Acl -LiteralPath $documentationPath
    foreach ($rule in $acl.Access) {
        try { $ruleSid = $rule.IdentityReference.Translate([System.Security.Principal.SecurityIdentifier]).Value }
        catch { $ruleSid = '' }
        $hasModify = (($rule.FileSystemRights -band [System.Security.AccessControl.FileSystemRights]::Modify) -eq [System.Security.AccessControl.FileSystemRights]::Modify)
        if ($ruleSid -eq $usersSidValue -and $rule.AccessControlType -eq [System.Security.AccessControl.AccessControlType]::Allow -and $hasModify) {
            $documentationAclModify = $true
            break
        }
    }
    if (-not $documentationAclModify) {
        throw "Documentation ACL does not grant BUILTIN\Users ($usersSidValue) Modify: $documentationPath"
    }

    # Reinstall in-place with user-owned data present. The installer must not erase it.
    $sentinel = Join-Path $documentationPath 'upgrade-preserve-smoke.txt'
    Set-Content -LiteralPath $sentinel -Encoding utf8 -Value 'preserve-me'
    Run-Installer
    $upgradeSentinelPreserved = (Test-Path -LiteralPath $sentinel -PathType Leaf) -and ((Get-Content -Raw -LiteralPath $sentinel).Trim() -eq 'preserve-me')
    if (-not $upgradeSentinelPreserved) { throw 'In-place reinstall destroyed user Documentation data.' }

    & (Join-Path $PSScriptRoot 'dpop0418_close_smoke.ps1') -Exe $core
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    & (Join-Path $PSScriptRoot 'dpop0417_disk_smoke.ps1') -ExePath $diskExe -OutputDir $diskEvidence
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    & (Join-Path $PSScriptRoot 'dpop0417_restore_smoke.ps1') -ExePath $restoreExe -OutputDir $restoreEvidence
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

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
        documentation_acl_modify = [bool]$documentationAclModify
        upgrade_sentinel_preserved = [bool]$upgradeSentinelPreserved
        close_smoke = $true
        disk_smoke = (Test-Path -LiteralPath (Join-Path $diskEvidence 'disk-smoke-report.json') -PathType Leaf)
        restore_smoke = (Test-Path -LiteralPath (Join-Path $restoreEvidence 'restore-smoke-report.json') -PathType Leaf)
        zapret_screen_fix = $true
        uninstalled = [bool]$uninstalled
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $reportPath -Encoding utf8

    Write-Host 'Installed DPopCleaner.exe: 0.4.18.1 PASS'
    Write-Host 'Installed DPopUpdater.exe: 0.4.18.1 PASS'
    Write-Host 'In-place Documentation preservation: PASS'
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
    if (Test-Path -LiteralPath $installRoot) {
        Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
