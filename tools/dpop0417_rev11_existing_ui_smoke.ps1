[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InstallerPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev11-existing-ui'
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

$installRoot = Join-Path $env:TEMP 'dpop0417-rev11-existing-ui'
$settingsPath = Join-Path $OutputDir 'SimpleUpdate-rev11.ini'
$reportPath = Join-Path $OutputDir 'rev11-existing-ui-smoke-report.json'
Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $settingsPath,$reportPath -Force -ErrorAction SilentlyContinue

$native = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class Rev11Child {
    public IntPtr Handle;
    public int Id;
    public string Text;
    public string ClassName;
    public bool Visible;
    public int Left;
    public int Top;
    public int Right;
    public int Bottom;
    public long Style;
}
public static class Rev11Native {
    private delegate bool EnumProc(IntPtr hwnd, IntPtr p);
    [StructLayout(LayoutKind.Sequential)] private struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc proc, IntPtr p);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll", EntryPoint="GetWindowLongPtrW")] private static extern IntPtr GetWindowLongPtr64(IntPtr hwnd, int index);
    [DllImport("user32.dll", EntryPoint="GetWindowLongW")] private static extern int GetWindowLong32(IntPtr hwnd, int index);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    private static long StyleOf(IntPtr hwnd) { return IntPtr.Size == 8 ? GetWindowLongPtr64(hwnd, -16).ToInt64() : GetWindowLong32(hwnd, -16); }
    public static Rev11Child[] Children(IntPtr parent) {
        var list = new List<Rev11Child>();
        EnumProc cb = delegate(IntPtr h, IntPtr _) {
            var t = new StringBuilder(512); var c = new StringBuilder(128); RECT r;
            GetWindowText(h,t,t.Capacity); GetClassName(h,c,c.Capacity); GetWindowRect(h,out r);
            list.Add(new Rev11Child { Handle=h, Id=GetDlgCtrlID(h), Text=t.ToString(), ClassName=c.ToString(), Visible=IsWindowVisible(h), Left=r.Left, Top=r.Top, Right=r.Right, Bottom=r.Bottom, Style=StyleOf(h) });
            return true;
        };
        EnumChildWindows(parent, cb, IntPtr.Zero); GC.KeepAlive(cb); return list.ToArray();
    }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Children([IntPtr]$Window) { @([Rev11Native]::Children($Window)) }
function Find-Child([IntPtr]$Window, [int]$Id, [switch]$Visible) {
    Children $Window | Where-Object { $_.Id -eq $Id -and ((-not $Visible) -or $_.Visible) } | Select-Object -First 1
}
function Click-Id([IntPtr]$Window, [int]$Id) {
    $child = Find-Child $Window $Id
    if (-not $child) { throw "Control id=$Id not found." }
    [void][Rev11Native]::SendMessage($child.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
}
function Same-Rect($A, $B) {
    return $A.Left -eq $B.Left -and $A.Top -eq $B.Top -and $A.Right -eq $B.Right -and $A.Bottom -eq $B.Bottom
}
function Wait-Visible([IntPtr]$Window, [int]$Id, [int]$TimeoutMs = 6000) {
    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMs)
    do {
        Start-Sleep -Milliseconds 100
        $child = Find-Child $Window $Id -Visible
        if ($child) { return $child }
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Visible control id=$Id did not appear."
}
function Wait-NativeZapretStatus([IntPtr]$Window, [string]$Version, [int]$TimeoutMs = 6000) {
    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMs)
    do {
        Start-Sleep -Milliseconds 100
        $status = Children $Window | Where-Object {
            $_.Visible -and $_.ClassName -eq 'Edit' -and $_.Text -like ("Zapret " + $Version + "*") -and $_.Text -like '*•*'
        } | Select-Object -First 1
        if ($status) { return $status }
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Native Zapret status Edit did not show installed version $Version."
}

$launcher = $null
$core = $null
$installed = $false
$uninstalled = $false
try {
    $setup = Start-Process -FilePath $InstallerPath -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',('/DIR=' + $installRoot)) -Wait -PassThru
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

    $launcher = Start-Process -FilePath $launcherPath -ArgumentList @('--no-update-check','--settings-path',('"' + $settingsPath + '"')) -WorkingDirectory $installRoot -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    do {
        Start-Sleep -Milliseconds 200
        foreach ($candidate in @(Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue)) {
            try {
                if ([IO.Path]::GetFullPath($candidate.Path) -eq [IO.Path]::GetFullPath($corePath)) { $core = $candidate; $core.Refresh(); break }
            } catch {}
        }
    } while (($null -eq $core -or $core.MainWindowHandle -eq [IntPtr]::Zero) -and [DateTime]::UtcNow -lt $deadline)
    if ($null -eq $core -or $core.MainWindowHandle -eq [IntPtr]::Zero) { throw 'Installed DPopCleaner core window did not appear.' }
    $window = $core.MainWindowHandle

    # Aggressive Settings wheel sequence. The host itself must never move while its children scroll.
    Click-Id $window 906
    $settingsPane0 = Wait-Visible $window 1492
    $initialRect = $settingsPane0
    $wheelDown = [IntPtr]::new([long]0xFF880000)
    $wheelUp = [IntPtr]::new([long]0x00780000)
    for ($round = 0; $round -lt 4; $round++) {
        for ($i = 0; $i -lt 4; $i++) { [void][Rev11Native]::SendMessage($settingsPane0.Handle, 0x020A, $wheelDown, [IntPtr]::Zero) }
        for ($i = 0; $i -lt 3; $i++) { [void][Rev11Native]::SendMessage($settingsPane0.Handle, 0x020A, $wheelUp, [IntPtr]::Zero) }
    }
    Start-Sleep -Milliseconds 700
    $settingsPaneAfterWheel = Wait-Visible $window 1492
    if (-not (Same-Rect $initialRect $settingsPaneAfterWheel)) { throw 'Settings host drifted during aggressive wheel sequence.' }

    for ($cycle = 1; $cycle -le 3; $cycle++) {
        Click-Id $window 905
        Start-Sleep -Milliseconds 250
        Click-Id $window 906
        $settingsPane = Wait-Visible $window 1492
        Start-Sleep -Milliseconds 300
        if (-not (Same-Rect $initialRect $settingsPane)) { throw "Settings host moved after reopen cycle ${cycle}." }
    }

    # Zapret rev.11 must edit the original frozen-core status Edit in place. No ID 1726 proxy may exist.
    Click-Id $window 905
    foreach ($id in @(1720,1721,1722,1723,1724,1725)) { [void](Wait-Visible $window $id) }
    $proxy = Find-Child $window 1726
    if ($proxy) { throw 'Rev.11 must not create version proxy id=1726.' }

    foreach ($id in @(1720,1721,1722,1723,1724,1725)) {
        $button = Wait-Visible $window $id
        $buttonType = ($button.Style -band 0x0000000F)
        if ($buttonType -ne 0x0000000B) { throw "Zapret bridge button id=$id is not BS_OWNERDRAW; style=0x$('{0:X}' -f $button.Style)" }
    }

    $nativeStatus = Wait-NativeZapretStatus $window $bundleVersion
    $nativeStatusHandle = $nativeStatus.Handle
    $deadline = [DateTime]::UtcNow.AddSeconds(4)
    do {
        $children = Children $window
        if ($children | Where-Object { $_.Id -eq 1726 } | Select-Object -First 1) { throw 'Version proxy id=1726 appeared while Zapret was open.' }
        $fresh = $children | Where-Object {
            $_.Visible -and $_.Handle -eq $nativeStatusHandle -and $_.ClassName -eq 'Edit' -and $_.Text -like ("Zapret " + $bundleVersion + "*") -and $_.Text -like '*•*'
        } | Select-Object -First 1
        if (-not $fresh) { throw 'Existing native Zapret status Edit handle changed, disappeared, or lost the installed version.' }
        $stale = $children | Where-Object { $_.Visible -and $_.Text -like 'Zapret 1.9.9d*' } | Select-Object -First 1
        if ($stale) { throw 'Stale Zapret 1.9.9d became visible again.' }
        Start-Sleep -Milliseconds 150
    } while ([DateTime]::UtcNow -lt $deadline)

    $game = Find-Child $window 1722 -Visible
    $manager = Find-Child $window 1723 -Visible
    if ($game.Text -ne ("Игровой фильтр " + $bundleVersion)) { throw "Game filter button version mismatch: $($game.Text)" }
    if ($manager.Text -ne ("Менеджер " + $bundleVersion)) { throw "Manager button version mismatch: $($manager.Text)" }

    [ordered]@{
        settings_fixed_rect = $true
        settings_aggressive_wheel_rounds = 4
        settings_reopen_cycles = 3
        zapret_installed_version = $bundleVersion
        zapret_native_status_class = 'Edit'
        zapret_native_status_handle = $nativeStatusHandle.ToInt64()
        zapret_native_status_handle_stable = $true
        zapret_version_proxy_1726_absent = $true
        stale_199d_absent = $true
        zapret_owner_draw_buttons = 6
    } | ConvertTo-Json | Set-Content -LiteralPath $reportPath -Encoding utf8

    Write-Host 'REV11_EXISTING_UI_SMOKE_OK'
    Write-Host 'Settings aggressive wheel + repaint stability: PASS'
    Write-Host "Existing native Zapret status Edit rewritten in place to ${bundleVersion}: PASS"
    Write-Host 'Zapret version proxy id=1726 absent: PASS'
    Write-Host 'Zapret native status Edit handle stable for 4 seconds: PASS'
}
finally {
    if ($core) { try { if (-not $core.HasExited) { Stop-Process -Id $core.Id -Force -ErrorAction SilentlyContinue } } catch {} }
    if ($launcher) { try { if (-not $launcher.HasExited) { Stop-Process -Id $launcher.Id -Force -ErrorAction SilentlyContinue } } catch {} }
    Start-Sleep -Milliseconds 400
    $uninstaller = Join-Path $installRoot 'unins000.exe'
    if (Test-Path -LiteralPath $uninstaller -PathType Leaf) {
        try {
            $u = Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait -PassThru
            $uninstalled = ($u.ExitCode -eq 0)
        } catch {}
    }
    if (-not $uninstalled) { Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue }
}
