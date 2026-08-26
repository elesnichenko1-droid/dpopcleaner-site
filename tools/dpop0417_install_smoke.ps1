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

if (-not (Test-Path -LiteralPath $InstallerPath -PathType Leaf)) {
    throw "DPopCleaner_Setup_0.4.17.exe not found: $InstallerPath"
}
if ([IO.Path]::GetFileName($InstallerPath) -ne 'DPopCleaner_Setup_0.4.17.exe') {
    throw "Unexpected installer filename: $InstallerPath"
}

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$installRoot = Join-Path ([IO.Path]::GetTempPath()) 'dpop0417-installed-smoke'
$diskEvidence = Join-Path $OutputDir 'disk-installed'
$restoreEvidence = Join-Path $OutputDir 'restore-installed'
$reportPath = Join-Path $OutputDir 'install-smoke-report.json'
$expectedCoreBlob = 'efd0eff1f4962319282363fa85595c25e0cebe11'
$installed = $false
$uninstalled = $false
$documentationAclModify = $false

function Assert-File([string]$RelativePath) {
    $path = Join-Path $installRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Installed file missing: $RelativePath"
    }
    return $path
}

try {
    if (Test-Path -LiteralPath $installRoot) {
        Remove-Item -LiteralPath $installRoot -Recurse -Force
    }

    $install = Start-Process -FilePath $InstallerPath -ArgumentList @(
        '/VERYSILENT',
        '/SUPPRESSMSGBOXES',
        '/NORESTART',
        '/SP-',
        "/DIR=$installRoot"
    ) -Wait -PassThru
    if ($install.ExitCode -ne 0) {
        throw "Silent install failed with exit code $($install.ExitCode)."
    }
    $installed = $true

    $core = Assert-File 'DPopCleaner.exe'
    [void](Assert-File 'Modules\DPop.Common.dll')
    $diskExe = Assert-File 'Modules\DiskAnalyzer.exe'
    $restoreExe = Assert-File 'Modules\RestoreCenter.exe'
    [void](Assert-File 'Languages\ru.json')
    [void](Assert-File 'Languages\en.json')
    [void](Assert-File 'Shell\shell.json')
    [void](Assert-File 'Shell\commands\disk-analyzer.json')
    [void](Assert-File 'Shell\commands\restore-center.json')
    [void](Assert-File 'Documentation\README.txt')

    $documentationPath = Join-Path $installRoot 'Documentation'
    $usersSidValue = 'S-1-5-32-545'
    $acl = Get-Acl -LiteralPath $documentationPath
    foreach ($rule in $acl.Access) {
        try {
            $ruleSid = $rule.IdentityReference.Translate([System.Security.Principal.SecurityIdentifier]).Value
        }
        catch {
            $ruleSid = ''
        }
        $hasModify = (($rule.FileSystemRights -band [IO.FileSystemRights]::Modify) -eq [IO.FileSystemRights]::Modify)
        if ($ruleSid -eq $usersSidValue -and
            $rule.AccessControlType -eq [Security.AccessControl.AccessControlType]::Allow -and
            $hasModify) {
            $documentationAclModify = $true
            break
        }
    }
    if (-not $documentationAclModify) {
        throw "Documentation ACL does not grant BUILTIN\Users ($usersSidValue) Modify: $documentationPath"
    }

    $installedCoreBlob = (& git -C $repoRoot hash-object -- $core).Trim()
    if ($LASTEXITCODE -ne 0 -or $installedCoreBlob -ne $expectedCoreBlob) {
        throw "Installed immutable core mismatch: $installedCoreBlob"
    }

    & (Join-Path $PSScriptRoot 'dpop0417_disk_smoke.ps1') `
        -ExePath $diskExe `
        -OutputDir $diskEvidence
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    & (Join-Path $PSScriptRoot 'dpop0417_restore_smoke.ps1') `
        -ExePath $restoreExe `
        -OutputDir $restoreEvidence
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $uninstaller = Assert-File 'unins000.exe'
    $uninstall = Start-Process -FilePath $uninstaller -ArgumentList @(
        '/VERYSILENT',
        '/SUPPRESSMSGBOXES',
        '/NORESTART'
    ) -Wait -PassThru
    if ($uninstall.ExitCode -ne 0) {
        throw "Silent uninstall failed with exit code $($uninstall.ExitCode)."
    }

    Start-Sleep -Milliseconds 500
    $uninstalled = -not (Test-Path -LiteralPath (Join-Path $installRoot 'DPopCleaner.exe') -PathType Leaf)
    if (-not $uninstalled) {
        throw 'Installed DPopCleaner.exe remained after uninstall.'
    }

    [pscustomobject]@{
        installer = $InstallerPath
        install_root = $installRoot
        installed_core_blob = $installedCoreBlob
        expected_core_blob = $expectedCoreBlob
        documentation_acl_modify = [bool]$documentationAclModify
        disk_smoke = (Test-Path -LiteralPath (Join-Path $diskEvidence 'disk-smoke-report.json') -PathType Leaf)
        restore_smoke = (Test-Path -LiteralPath (Join-Path $restoreEvidence 'restore-smoke-report.json') -PathType Leaf)
        uninstalled = [bool]$uninstalled
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $reportPath -Encoding utf8

    Write-Host "Installed immutable core: $installedCoreBlob"
    Write-Host 'Installed Documentation ACL: BUILTIN\Users Modify PASS'
    Write-Host 'Installed Disk Analyzer smoke: PASS'
    Write-Host 'Installed Restore Center smoke: PASS'
    Write-Host 'Silent uninstall: PASS'
    Write-Host "install-smoke-report.json: $reportPath"
}
finally {
    if ($installed -and -not $uninstalled) {
        $uninstaller = Join-Path $installRoot 'unins000.exe'
        if (Test-Path -LiteralPath $uninstaller -PathType Leaf) {
            try {
                Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait | Out-Null
            }
            catch { }
        }
    }
    if (Test-Path -LiteralPath $installRoot) {
        Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
