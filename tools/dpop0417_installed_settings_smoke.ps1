[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RootPath,
    [string]$OutputDir = '_release/0.4.17/evidence/installed-settings'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RootPath = if ([IO.Path]::IsPathRooted($RootPath)) { $RootPath } else { Join-Path $repoRoot $RootPath }
$OutputDir = if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repoRoot $OutputDir }
$RootPath = [IO.Path]::GetFullPath($RootPath)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$launcherPath = Join-Path $RootPath 'DPopCleaner.exe'
$corePath = Join-Path $RootPath 'DPopCleaner.Core.exe'
foreach ($required in @($launcherPath, $corePath, (Join-Path $RootPath 'SimpleUpdate.exe'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Installed Settings smoke prerequisite missing: $required" }
}
$settingsPath = Join-Path $OutputDir 'SimpleUpdate-installed-settings.ini'
Remove-Item -LiteralPath $settingsPath -Force -ErrorAction SilentlyContinue

$native = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class InstalledSettingsChild {
    public IntPtr Handle;
    public int Id;
    public string Text;
    public string ClassName;
    public bool Visible;
    public int Top;
}
public static class InstalledSettingsNative {
    private delegate bool EnumProc(IntPtr hwnd, IntPtr p);
    [StructLayout(LayoutKind.Sequential)] private struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc proc, IntPtr p);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    public static InstalledSettingsChild[] Children(IntPtr parent) {
        var list = new List<InstalledSettingsChild>();
        EnumProc cb = delegate(IntPtr h, IntPtr _) {
            var t = new StringBuilder(512); var c = new StringBuilder(128); RECT r;
            GetWindowText(h,t,t.Capacity); GetClassName(h,c,c.Capacity); GetWindowRect(h,out r);
            list.Add(new InstalledSettingsChild { Handle=h, Id=GetDlgCtrlID(h), Text=t.ToString(), ClassName=c.ToString(), Visible=IsWindowVisible(h), Top=r.Top });
            return true;
        };
        EnumChildWindows(parent, cb, IntPtr.Zero); GC.KeepAlive(cb); return list.ToArray();
    }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

$launcherProcess = $null
$coreProcess = $null
try {
    $launcherProcess = Start-Process -FilePath $launcherPath -ArgumentList @('--no-update-check','--settings-path',('"' + $settingsPath + '"')) -WorkingDirectory $RootPath -PassThru

    $deadline = [DateTime]::UtcNow.AddSeconds(18)
    do {
        Start-Sleep -Milliseconds 250
        foreach ($candidate in @(Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue)) {
            try {
                if ([IO.Path]::GetFullPath($candidate.Path) -eq [IO.Path]::GetFullPath($corePath)) {
                    $coreProcess = $candidate
                    $coreProcess.Refresh()
                    break
                }
            } catch { }
        }
    } while (($null -eq $coreProcess -or $coreProcess.MainWindowHandle -eq [IntPtr]::Zero) -and [DateTime]::UtcNow -lt $deadline)
    if ($null -eq $coreProcess -or $coreProcess.MainWindowHandle -eq [IntPtr]::Zero) {
        throw 'Installed DPopCleaner.exe did not launch DPopCleaner.Core.exe through the bridge.'
    }

    $children = [InstalledSettingsNative]::Children($coreProcess.MainWindowHandle)
    $gear = $children | Where-Object { $_.Id -eq 906 -and $_.ClassName -eq 'Button' } | Select-Object -First 1
    if (-not $gear) { throw 'Installed authentic Settings gear id=906 not found.' }
    [void][InstalledSettingsNative]::SendMessage($gear.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)

    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    $scrollHost = $null
    $autoUpdate = $null
    $checkNow = $null
    $licenseHeading = $null
    $settingsChildren = @()
    do {
        Start-Sleep -Milliseconds 150
        $settingsChildren = [InstalledSettingsNative]::Children($coreProcess.MainWindowHandle)
        $scrollHost = $settingsChildren | Where-Object { $_.Id -eq 1492 -and $_.Visible } | Select-Object -First 1
        $autoUpdate = $settingsChildren | Where-Object { $_.Id -eq 1490 -and $_.Text -eq 'Включить автообновление приложения' -and $_.Visible } | Select-Object -First 1
        $checkNow = $settingsChildren | Where-Object { $_.Id -eq 1491 -and $_.Text -eq 'Проверить обновления' -and $_.Visible } | Select-Object -First 1
        $licenseHeading = $settingsChildren | Where-Object { $_.Id -eq 1493 -and $_.Text -eq 'Лицензия' -and $_.Visible } | Select-Object -First 1
    } while ((-not $scrollHost -or -not $autoUpdate -or -not $checkNow -or -not $licenseHeading) -and [DateTime]::UtcNow -lt $deadline)

    if (-not $scrollHost) { throw 'Installed scrollable Settings host id=1492 is missing.' }
    if (-not $autoUpdate) { throw 'Installed application auto-update control id=1490 is missing.' }
    if (-not $checkNow) { throw 'Installed check-update control id=1491 is missing.' }
    if (-not $licenseHeading) { throw 'Installed License heading id=1493 is missing from scroll area.' }

    $legacyVersion = $settingsChildren | Where-Object { $_.Visible -and $_.Text -eq 'v0.2.11 BETA' } | Select-Object -First 1
    if ($legacyVersion) { throw 'Installed legacy v0.2.11 BETA badge is still visible.' }

    $beforeTop = $licenseHeading.Top
    $WM_MOUSEWHEEL = 0x020A
    $wheelDown = [IntPtr]::new([long]0xFF880000)
    [void][InstalledSettingsNative]::SendMessage($scrollHost.Handle, $WM_MOUSEWHEEL, $wheelDown, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 400
    $after = [InstalledSettingsNative]::Children($coreProcess.MainWindowHandle)
    $licenseAfter = $after | Where-Object { $_.Id -eq 1493 -and $_.Text -eq 'Лицензия' } | Select-Object -First 1
    if (-not $licenseAfter) { throw 'Installed License heading disappeared after WM_MOUSEWHEEL.' }
    if ($licenseAfter.Top -ge $beforeTop) { throw "Installed Settings did not scroll: before=$beforeTop after=$($licenseAfter.Top)" }

    [pscustomobject]@{
        launcher = $launcherPath
        core = $corePath
        scroll_host = $true
        auto_update_control = $true
        check_update_control = $true
        license_in_scroll = $true
        legacy_version_hidden = $true
        mouse_wheel_scroll = $true
    } | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $OutputDir 'installed-settings-smoke-report.json') -Encoding utf8

    Stop-Process -Id $coreProcess.Id -Force
    $coreProcess.WaitForExit(5000) | Out-Null
    if (-not $launcherProcess.WaitForExit(6000)) { throw 'Installed DPopCleaner.exe bridge did not exit after core closed.' }
    $coreProcess = $null
    $launcherProcess = $null

    Write-Host 'INSTALLED_SETTINGS_BRIDGE_SMOKE_OK'
}
finally {
    if ($coreProcess -and -not $coreProcess.HasExited) { Stop-Process -Id $coreProcess.Id -Force -ErrorAction SilentlyContinue }
    if ($launcherProcess -and -not $launcherProcess.HasExited) { Stop-Process -Id $launcherProcess.Id -Force -ErrorAction SilentlyContinue }
}
