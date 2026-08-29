[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InstallerPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev9-zapret-update'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$InstallerPath = if ([IO.Path]::IsPathRooted($InstallerPath)) { $InstallerPath } else { Join-Path $repoRoot $InstallerPath }
$OutputDir = if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repoRoot $OutputDir }
$InstallerPath = [IO.Path]::GetFullPath($InstallerPath)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

if (-not (Test-Path -LiteralPath $InstallerPath -PathType Leaf)) { throw "Installer missing: $InstallerPath" }

$installRoot = Join-Path $env:TEMP 'dpop0417-rev9-zapret-click-smoke'
$settingsPath = Join-Path $OutputDir 'SimpleUpdate-rev9.ini'
$markerPath = Join-Path $OutputDir 'zapret-updater-click.marker'
$reportPath = Join-Path $OutputDir 'rev9-zapret-update-smoke-report.json'
Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $settingsPath,$markerPath,$reportPath -Force -ErrorAction SilentlyContinue

$native = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class Rev9ZapretChild {
    public IntPtr Handle;
    public int Id;
    public string Text;
    public string ClassName;
    public bool Visible;
}
public static class Rev9ZapretNative {
    private delegate bool EnumProc(IntPtr hwnd, IntPtr p);
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc proc, IntPtr p);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    public static Rev9ZapretChild[] Children(IntPtr parent) {
        var list = new List<Rev9ZapretChild>();
        EnumProc cb = delegate(IntPtr h, IntPtr _) {
            var t = new StringBuilder(512); var c = new StringBuilder(128);
            GetWindowText(h,t,t.Capacity); GetClassName(h,c,c.Capacity);
            list.Add(new Rev9ZapretChild { Handle=h, Id=GetDlgCtrlID(h), Text=t.ToString(), ClassName=c.ToString(), Visible=IsWindowVisible(h) });
            return true;
        };
        EnumChildWindows(parent,cb,IntPtr.Zero); GC.KeepAlive(cb); return list.ToArray();
    }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Children([IntPtr]$Window) { @([Rev9ZapretNative]::Children($Window)) }
function Click-VisibleButton([IntPtr]$Window, [string]$Text, [int]$Id = -1) {
    $button = Children $Window | Where-Object {
        $_.Visible -and $_.ClassName -eq 'Button' -and $_.Text -eq $Text -and ($Id -lt 0 -or $_.Id -eq $Id)
    } | Select-Object -First 1
    if (-not $button) { throw "Visible button not found: '$Text' id=$Id" }
    [void][Rev9ZapretNative]::SendMessage($button.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    return $button
}

$launcher = $null
$core = $null
$installed = $false
$uninstalled = $false
try {
    $args = @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',('/DIR=' + $installRoot))
    $setup = Start-Process -FilePath $InstallerPath -ArgumentList $args -Wait -PassThru
    if ($setup.ExitCode -ne 0) { throw "Installer failed with exit code $($setup.ExitCode)." }
    $installed = $true

    $launcherPath = Join-Path $installRoot 'DPopCleaner.exe'
    $corePath = Join-Path $installRoot 'DPopCleaner.Core.exe'
    $versionPath = Join-Path $installRoot 'Zapret\.service\version.txt'
    foreach ($required in @($launcherPath,$corePath,$versionPath)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Installed prerequisite missing: $required" }
    }
    $bundleVersion = (Get-Content -Raw -LiteralPath $versionPath).Trim()
    if ($bundleVersion -ne '1.10.2') { throw "Installed Zapret bundle version mismatch: $bundleVersion" }

    $env:DPOPCLEANER_ZAPRET_UPDATE_SMOKE_MARKER = $markerPath
    $launcher = Start-Process -FilePath $launcherPath -ArgumentList @('--no-update-check','--settings-path',('"' + $settingsPath + '"')) -WorkingDirectory $installRoot -PassThru

    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    do {
        Start-Sleep -Milliseconds 250
        foreach ($candidate in @(Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue)) {
            try {
                if ([IO.Path]::GetFullPath($candidate.Path) -eq [IO.Path]::GetFullPath($corePath)) { $core = $candidate; $core.Refresh(); break }
            } catch { }
        }
    } while (($null -eq $core -or $core.MainWindowHandle -eq [IntPtr]::Zero) -and [DateTime]::UtcNow -lt $deadline)
    if ($null -eq $core -or $core.MainWindowHandle -eq [IntPtr]::Zero) { throw 'Installed DPopCleaner core window did not appear.' }
    $window = $core.MainWindowHandle

    [void](Click-VisibleButton $window 'Zapret')
    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    $children = @()
    $proxyDownload = $null
    $proxyCheck = $null
    do {
        Start-Sleep -Milliseconds 150
        $children = Children $window
        $proxyDownload = $children | Where-Object { $_.Visible -and $_.ClassName -eq 'Button' -and $_.Id -eq 1725 -and $_.Text -eq 'Скачать и установить' } | Select-Object -First 1
        $proxyCheck = $children | Where-Object { $_.Visible -and $_.ClassName -eq 'Button' -and $_.Id -eq 1724 -and $_.Text -eq 'Проверить версию' } | Select-Object -First 1
    } while ((-not $proxyDownload -or -not $proxyCheck) -and [DateTime]::UtcNow -lt $deadline)
    if (-not $proxyDownload) { throw 'Zapret updater proxy is not visible.' }
    if (-not $proxyCheck) { throw 'Zapret version-check proxy is not visible.' }

    $legacyCheckVisible = $children | Where-Object { $_.Visible -and $_.Id -eq 1709 -and $_.ClassName -eq 'Button' } | Select-Object -First 1
    $legacyDownloadVisible = $children | Where-Object { $_.Visible -and $_.Id -eq 1715 -and $_.ClassName -eq 'Button' } | Select-Object -First 1
    if ($legacyCheckVisible) { throw 'Broken frozen Zapret check-version button is still visible.' }
    if ($legacyDownloadVisible) { throw 'Broken frozen Zapret updater button is still visible.' }

    $displayedVersion = $children | Where-Object { $_.Visible -and $_.ClassName -eq 'Static' -and $_.Text -like 'Zapret 1.10.2*' } | Select-Object -First 1
    if (-not $displayedVersion) { throw 'Displayed Zapret version is not 1.10.2.' }
    if ($children | Where-Object { $_.Visible -and $_.Text -like 'Zapret 1.9.9d*' } | Select-Object -First 1) { throw 'Stale Zapret 1.9.9d display is still visible.' }

    [void][Rev9ZapretNative]::SendMessage($proxyDownload.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    while (-not (Test-Path -LiteralPath $markerPath -PathType Leaf) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (-not (Test-Path -LiteralPath $markerPath -PathType Leaf)) { throw 'Visible Zapret update click did not reach bridge updater.' }
    $marker = (Get-Content -Raw -LiteralPath $markerPath).Trim()
    if ($marker -ne 'BRIDGE_ZAPRET_UPDATER_OK') { throw "Unexpected Zapret updater marker: $marker" }

    [ordered]@{
        installed = $installed
        bundled_zapret_version = $bundleVersion
        proxy_check_visible = $true
        proxy_download_visible = $true
        frozen_check_hidden = $true
        frozen_download_hidden = $true
        displayed_version = '1.10.2'
        click_marker = $marker
    } | ConvertTo-Json | Set-Content -LiteralPath $reportPath -Encoding utf8

    Write-Host 'REV9_ZAPRET_UPDATE_CLICK_SMOKE_OK'
    Write-Host 'Visible Zapret update click -> bridge updater: PASS'
    Write-Host 'Displayed Zapret 1.10.2 from installed version.txt: PASS'
}
finally {
    Remove-Item Env:DPOPCLEANER_ZAPRET_UPDATE_SMOKE_MARKER -ErrorAction SilentlyContinue
    if ($core) { try { if (-not $core.HasExited) { Stop-Process -Id $core.Id -Force -ErrorAction SilentlyContinue } } catch {} }
    if ($launcher) { try { if (-not $launcher.HasExited) { Stop-Process -Id $launcher.Id -Force -ErrorAction SilentlyContinue } } catch {} }
    Start-Sleep -Milliseconds 500
    $uninstaller = Join-Path $installRoot 'unins000.exe'
    if (Test-Path -LiteralPath $uninstaller -PathType Leaf) {
        try {
            $u = Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait -PassThru
            $uninstalled = ($u.ExitCode -eq 0)
        } catch {}
    }
    if (-not $uninstalled) { Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue }
}
