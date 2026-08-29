[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RootPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev9-runtime'
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
$settingsPath = Join-Path $OutputDir 'SimpleUpdate-rev9.ini'
foreach ($required in @($launcherPath, $corePath, (Join-Path $RootPath 'Zapret\.service\version.txt'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "rev.9 runtime smoke prerequisite missing: $required" }
}

$native = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class Rev9Child {
    public IntPtr Handle;
    public int Id;
    public string Text;
    public string ClassName;
    public bool Visible;
    public int Left;
    public int Top;
    public int Right;
    public int Bottom;
}

public static class Rev9Native {
    private delegate bool EnumProc(IntPtr hwnd, IntPtr p);
    [StructLayout(LayoutKind.Sequential)] private struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc proc, IntPtr p);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);

    public static Rev9Child[] Children(IntPtr parent) {
        var list = new List<Rev9Child>();
        EnumProc cb = delegate(IntPtr h, IntPtr _) {
            var t = new StringBuilder(512); var c = new StringBuilder(128); RECT r;
            GetWindowText(h,t,t.Capacity); GetClassName(h,c,c.Capacity); GetWindowRect(h,out r);
            list.Add(new Rev9Child {
                Handle=h, Id=GetDlgCtrlID(h), Text=t.ToString(), ClassName=c.ToString(), Visible=IsWindowVisible(h),
                Left=r.Left, Top=r.Top, Right=r.Right, Bottom=r.Bottom
            });
            return true;
        };
        EnumChildWindows(parent,cb,IntPtr.Zero); GC.KeepAlive(cb); return list.ToArray();
    }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Get-Children([IntPtr]$Window) { @([Rev9Native]::Children($Window)) }
function Click-VisibleButton([IntPtr]$Window, [string]$Text) {
    $button = Get-Children $Window | Where-Object { $_.Visible -and $_.ClassName -eq 'Button' -and $_.Text -eq $Text } | Select-Object -First 1
    if (-not $button) { throw "Button not found: $Text" }
    [void][Rev9Native]::SendMessage($button.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 500
}
function Rect-Key($Item) {
    if (-not $Item) { return '<missing>' }
    return "$($Item.Left),$($Item.Top),$($Item.Right),$($Item.Bottom)"
}

$launcher = $null
$core = $null
try {
    Remove-Item -LiteralPath $settingsPath -Force -ErrorAction SilentlyContinue
    $launcher = Start-Process -FilePath $launcherPath -ArgumentList @('--no-update-check','--settings-path',('"' + $settingsPath + '"')) -WorkingDirectory $RootPath -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(18)
    do {
        Start-Sleep -Milliseconds 250
        foreach ($candidate in @(Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue)) {
            try {
                if ([IO.Path]::GetFullPath($candidate.Path) -eq [IO.Path]::GetFullPath($corePath)) { $core=$candidate; $core.Refresh(); break }
            } catch { }
        }
    } while (($null -eq $core -or $core.MainWindowHandle -eq [IntPtr]::Zero) -and [DateTime]::UtcNow -lt $deadline)
    if ($null -eq $core -or $core.MainWindowHandle -eq [IntPtr]::Zero) { throw 'rev.9 installed core window did not appear.' }
    $window = $core.MainWindowHandle

    # Zapret regression from the user screenshot: the frozen 1.9.9d updater button must not remain clickable.
    Click-VisibleButton $window 'Zapret'
    $deadline = [DateTime]::UtcNow.AddSeconds(7)
    do {
        Start-Sleep -Milliseconds 150
        $zapretChildren = Get-Children $window
        $legacyUpdater = $zapretChildren | Where-Object { $_.Id -eq 1715 -and $_.ClassName -eq 'Button' } | Select-Object -First 1
        $proxyUpdater = $zapretChildren | Where-Object { $_.Id -eq 1724 -and $_.ClassName -eq 'Button' } | Select-Object -First 1
        $versionStatus = $zapretChildren | Where-Object { $_.Visible -and $_.ClassName -eq 'Static' -and $_.Text -like 'Zapret 1.10.2*' } | Select-Object -First 1
    } while ((-not $legacyUpdater -or -not $proxyUpdater -or -not $versionStatus) -and [DateTime]::UtcNow -lt $deadline)

    if (-not $legacyUpdater) { throw 'Frozen Zapret updater button id=1715 was not found for replacement.' }
    if ($legacyUpdater.Visible) { throw 'Broken frozen Zapret updater button is still visible' }
    if (-not $proxyUpdater -or -not $proxyUpdater.Visible -or $proxyUpdater.Text -ne 'Скачать и установить') { throw 'Zapret updater proxy is not visible' }
    if (-not $versionStatus) { throw 'Displayed Zapret version is not 1.10.2' }

    $bundledVersion = (Get-Content -Raw -LiteralPath (Join-Path $RootPath 'Zapret\.service\version.txt')).Trim()
    if ($bundledVersion -ne '1.10.2') { throw "Bundled Zapret version changed unexpectedly: $bundledVersion" }

    # Settings regression from the user screenshot: bridge must not re-anchor the host to its own scrolled proxies.
    $gear = Get-Children $window | Where-Object { $_.Id -eq 906 -and $_.ClassName -eq 'Button' } | Select-Object -First 1
    if (-not $gear) { throw 'Settings gear id=906 not found.' }
    [void][Rev9Native]::SendMessage($gear.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)

    $deadline = [DateTime]::UtcNow.AddSeconds(7)
    do {
        Start-Sleep -Milliseconds 150
        $settingsChildren = Get-Children $window
        $scrollHost = $settingsChildren | Where-Object { $_.Id -eq 1492 -and $_.Visible } | Select-Object -First 1
    } while (-not $scrollHost -and [DateTime]::UtcNow -lt $deadline)
    if (-not $scrollHost) { throw 'Settings scroll host id=1492 missing.' }
    $initialRect = Rect-Key $scrollHost

    $WM_MOUSEWHEEL = 0x020A
    $wheelDown = [IntPtr]::new([long]0xFF880000)
    [void][Rev9Native]::SendMessage($scrollHost.Handle, $WM_MOUSEWHEEL, $wheelDown, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 1400
    $afterWheel = Get-Children $window | Where-Object { $_.Id -eq 1492 -and $_.Visible } | Select-Object -First 1
    if ((Rect-Key $afterWheel) -ne $initialRect) {
        throw "Settings scroll host drifted after wheel/timer refresh: initial=$initialRect current=$(Rect-Key $afterWheel)"
    }

    Click-VisibleButton $window 'Обзор'
    [void][Rev9Native]::SendMessage($gear.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 1200
    $afterRoundtrip = Get-Children $window | Where-Object { $_.Id -eq 1492 -and $_.Visible } | Select-Object -First 1
    if ((Rect-Key $afterRoundtrip) -ne $initialRect) {
        throw "Settings scroll host drifted after tab roundtrip: initial=$initialRect current=$(Rect-Key $afterRoundtrip)"
    }

    [pscustomobject]@{
        zapret_legacy_updater_hidden = (-not $legacyUpdater.Visible)
        zapret_proxy_visible = [bool]$proxyUpdater.Visible
        displayed_zapret_version = $versionStatus.Text
        bundled_zapret_version = $bundledVersion
        settings_initial_rect = $initialRect
        settings_after_wheel_rect = (Rect-Key $afterWheel)
        settings_after_roundtrip_rect = (Rect-Key $afterRoundtrip)
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputDir 'rev9-runtime-smoke-report.json') -Encoding utf8

    Write-Host 'REV9_ZAPRET_RUNTIME_FIX_OK legacy_updater_hidden=true proxy_visible=true version=1.10.2'
    Write-Host "REV9_SETTINGS_POSITION_STABLE_OK rect=$initialRect"
}
finally {
    if ($core) { try { $core.Refresh(); if (-not $core.HasExited) { Stop-Process -Id $core.Id -Force -ErrorAction SilentlyContinue } } catch { } }
    if ($launcher) { try { $launcher.Refresh(); if (-not $launcher.HasExited) { Stop-Process -Id $launcher.Id -Force -ErrorAction SilentlyContinue } } catch { } }
}
