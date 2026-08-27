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
$diskEvidence = Join-Path $OutputDir 'disk-installed'
$restoreEvidence = Join-Path $OutputDir 'restore-installed'
$zapretEvidence = Join-Path $OutputDir 'zapret-installed'
$reportPath = Join-Path $OutputDir 'install-smoke-report.json'
$expectedCoreBlob = 'efd0eff1f4962319282363fa85595c25e0cebe11'
$installed = $false
$uninstalled = $false
$documentationAclModify = $false
$launcherSmoke = $false
$zapretScreenFixPresent = $false
$zapretRuntimePresent = $false
$zapretUiSmoke = $false
$launcherProcess = $null
$launcherCoreProcess = $null

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

    $core = Assert-File 'DPopCleaner.exe'
    $launcher = Assert-File 'SimpleUpdate.exe'
    [void](Assert-File 'LICENSE.txt')
    [void](Assert-File 'service.bat')
    [void](Assert-File 'general.bat')
    [void](Assert-File '.service\version.txt')
    [void](Assert-File 'bin\winws.exe')
    [void](Assert-File 'bin\WinDivert.dll')
    [void](Assert-File 'bin\WinDivert64.sys')
    if (-not (Test-Path -LiteralPath (Join-Path $installRoot 'lists') -PathType Container)) { throw 'Installed Zapret lists directory missing.' }
    if (-not (Test-Path -LiteralPath (Join-Path $installRoot 'utils') -PathType Container)) { throw 'Installed Zapret utils directory missing.' }
    $zapretVersion = (Get-Content -Raw -LiteralPath (Join-Path $installRoot '.service\version.txt')).Trim()
    if ($zapretVersion -ne '1.10.2') { throw "Installed Zapret version mismatch: $zapretVersion" }
    $installedStrategies = @(Get-ChildItem -LiteralPath $installRoot -Filter 'general*.bat' -File)
    if ($installedStrategies.Count -eq 0) { throw 'Installed Zapret has no general*.bat strategies.' }
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

    $launcherSettings = Join-Path $OutputDir 'SimpleUpdate-installed-smoke.ini'
    Remove-Item -LiteralPath $launcherSettings -Force -ErrorAction SilentlyContinue
    $launcherProcess = Start-Process -FilePath $launcher -ArgumentList @('--no-update-check','--settings-path',('"' + $launcherSettings + '"')) -WorkingDirectory $installRoot -PassThru
    $launcherDeadline = [DateTime]::UtcNow.AddSeconds(18)
    do {
        Start-Sleep -Milliseconds 250
        foreach ($candidate in @(Get-Process -Name 'DPopCleaner' -ErrorAction SilentlyContinue)) {
            try { if ([IO.Path]::GetFullPath($candidate.Path) -eq [IO.Path]::GetFullPath($core)) { $launcherCoreProcess = $candidate; break } } catch { }
        }
    } while ($null -eq $launcherCoreProcess -and [DateTime]::UtcNow -lt $launcherDeadline)
    if ($null -eq $launcherCoreProcess) { throw 'Installed SimpleUpdate.exe did not launch the preserved DPopCleaner.exe core.' }
    Stop-Process -Id $launcherCoreProcess.Id -Force
    $launcherCoreProcess.WaitForExit(5000) | Out-Null
    if (-not $launcherProcess.WaitForExit(6000)) { throw 'Installed SimpleUpdate.exe did not exit after its DPopCleaner core closed.' }
    $launcherSmoke = $true
    $launcherCoreProcess = $null
    $launcherProcess = $null

    & (Join-Path $PSScriptRoot 'dpop0417_zapret_ui_smoke.ps1') -RootPath $installRoot -OutputDir $zapretEvidence
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $zapretUiSmoke = Test-Path -LiteralPath (Join-Path $zapretEvidence 'zapret-ui-smoke-report.json') -PathType Leaf
    if (-not $zapretUiSmoke) { throw 'Installed authentic Zapret UI smoke report was not produced.' }

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
        installed_core_blob = $installedCoreBlob
        expected_core_blob = $expectedCoreBlob
        simpleupdate_launcher_smoke = [bool]$launcherSmoke
        zapret_runtime_present = [bool]$zapretRuntimePresent
        zapret_version = $zapretVersion
        zapret_strategy_files = $installedStrategies.Count
        zapret_authentic_ui_smoke = [bool]$zapretUiSmoke
        zapret_screen_fix_present = [bool]$zapretScreenFixPresent
        documentation_acl_modify = [bool]$documentationAclModify
        disk_smoke = (Test-Path -LiteralPath (Join-Path $diskEvidence 'disk-smoke-report.json') -PathType Leaf)
        restore_smoke = (Test-Path -LiteralPath (Join-Path $restoreEvidence 'restore-smoke-report.json') -PathType Leaf)
        uninstalled = [bool]$uninstalled
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $reportPath -Encoding utf8

    Write-Host "Installed immutable core: $installedCoreBlob"
    Write-Host "Installed Flowseal Zapret $zapretVersion: PASS; strategies=$($installedStrategies.Count)"
    Write-Host 'Authentic installed Zapret Center strategy discovery: PASS'
    Write-Host 'Installed SimpleUpdate launcher smoke: PASS'
    Write-Host 'Installed ZapretScreenFix companion: PASS'
    Write-Host 'Installed Documentation ACL: BUILTIN\Users Modify PASS'
    Write-Host 'Installed Disk Analyzer smoke: PASS'
    Write-Host 'Installed Restore Center smoke: PASS'
    Write-Host 'Silent uninstall: PASS'
}
finally {
    if ($launcherCoreProcess -and -not $launcherCoreProcess.HasExited) { Stop-Process -Id $launcherCoreProcess.Id -Force -ErrorAction SilentlyContinue }
    if ($launcherProcess -and -not $launcherProcess.HasExited) { Stop-Process -Id $launcherProcess.Id -Force -ErrorAction SilentlyContinue }
    if ($installed -and -not $uninstalled) {
        $uninstaller = Join-Path $installRoot 'unins000.exe'
        if (Test-Path -LiteralPath $uninstaller -PathType Leaf) { try { Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait | Out-Null } catch { } }
    }
    if (Test-Path -LiteralPath $installRoot) { Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue }
}
