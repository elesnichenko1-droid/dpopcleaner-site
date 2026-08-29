[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InstallerPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev10-ui-stability'
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

$installRoot = Join-Path $env:TEMP 'dpop0417-rev10-ui-stability'
$settingsPath = Join-Path $OutputDir 'SimpleUpdate-rev10.ini'
$reportPath = Join-Path $OutputDir 'rev10-ui-stability-smoke-report.json'
Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $settingsPath,$reportPath -Force -ErrorAction SilentlyContinue

$native = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class Rev10Child {
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
public static class Rev10Native {
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
    public static Rev10Child[] Children(IntPtr parent) {
        var list = new List<Rev10Child>();
        EnumProc cb = delegate(IntPtr h, IntPtr _) {
            var t = new StringBuilder(512); var c = new StringBuilder(128); RECT r;
            GetWindowText(h,t,t.Capacity); GetClassName(h,c,c.Capacity); GetWindowRect(h,out r);
            list.Add(new Rev10Child { Handle=h, Id=GetDlgCtrlID(h), Text=t.ToString(), ClassName=c.ToString(), Visible=IsWindowVisible(h), Left=r.Left, Top=r.Top, Right=r.Right, Bottom=r.Bottom, Style=StyleOf(h) });
            return true;
        };
        EnumChildWindows(parent, cb, IntPtr.Zero); GC.KeepAlive(cb); return list.ToArray();
    }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Children([IntPtr]$Window) { @([Rev10Native]::Children($Window)) }
function Find-Child([IntPtr]$Window, [int]$Id, [switch]$Visible) {
    Children $Window | Where-Object { $_.Id -eq $Id -and ((-not $Visible) -or $_.Visible) } | Select-Object -First 1
}
function Click-Id([IntPtr]$Window, [int]$Id) {
    $child = Find-Child $Window $Id
    if (-not $child) { throw "Control id=$Id not found." }
    [void][Rev10Native]::SendMessage($child.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
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

    # Reproduce the user video: Settings -> wheel -> wait -> another tab -> Settings, several times.
    Click-Id $window 906
    $host0 = Wait-Visible $window 1492
    $initialRect = $host0
    $wheelDown = [IntPtr]::new([long]0xFF880000)
    for ($i = 0; $i -lt 3; $i++) { [void][Rev10Native]::SendMessage($host0.Handle, 0x020A, $wheelDown, [IntPtr]::Zero) }
    Start-Sleep -Milliseconds 1400
    $hostAfterWheel = Wait-Visible $window 1492
    if (-not (Same-Rect $initialRect $hostAfterWheel)) {
        throw "Settings host drifted after wheel: initial=($($initialRect.Left),$($initialRect.Top),$($initialRect.Right),$($initialRect.Bottom)) after=($($hostAfterWheel.Left),$($hostAfterWheel.Top),$($hostAfterWheel.Right),$($hostAfterWheel.Bottom))"
    }

    for ($cycle = 1; $cycle -le 3; $cycle++) {
        Click-Id $window 905
        Start-Sleep -Milliseconds 350
        Click-Id $window 906
        $host = Wait-Visible $window 1492
        Start-Sleep -Milliseconds 450
        $hostStable = Wait-Visible $window 1492
        if (-not (Same-Rect $initialRect $hostStable)) { throw "Settings host moved after reopen cycle ${cycle}." }
    }

    # Zapret must show the installed version persistently and bridge buttons must use our dark owner-draw format.
    Click-Id $window 905
    [void](Wait-Visible $window 1724)
    [void](Wait-Visible $window 1725)
    [void](Wait-Visible $window 1720)
    [void](Wait-Visible $window 1721)
    [void](Wait-Visible $window 1722)
    [void](Wait-Visible $window 1723)
    $versionProxy = Wait-Visible $window 1726

    $bridgeIds = @(1720,1721,1722,1723,1724,1725)
    foreach ($id in $bridgeIds) {
        $button = Wait-Visible $window $id
        $buttonType = ($button.Style -band 0x0000000F)
        if ($buttonType -ne 0x0000000B) { throw "Zapret bridge button id=$id is not BS_OWNERDRAW; style=0x$('{0:X}' -f $button.Style)" }
    }

    $deadline = [DateTime]::UtcNow.AddSeconds(3)
    do {
        $children = Children $window
        $fresh = $children | Where-Object { $_.Visible -and $_.Id -eq 1726 -and $_.Text -like ("Zapret " + $bundleVersion + "*") } | Select-Object -First 1
        if (-not $fresh) { throw "Visible Zapret version proxy does not show installed version $bundleVersion." }
        $stale = $children | Where-Object { $_.Visible -and $_.Text -like 'Zapret 1.9.9d*' } | Select-Object -First 1
        if ($stale) { throw 'Stale visible Zapret 1.9.9d returned while rev.10 bridge was active.' }
        Start-Sleep -Milliseconds 150
    } while ([DateTime]::UtcNow -lt $deadline)

    $game = Find-Child $window 1722 -Visible
    $manager = Find-Child $window 1723 -Visible
    if ($game.Text -notlike ("Игровой фильтр " + $bundleVersion)) { throw "Game filter button does not use installed Zapret version: $($game.Text)" }
    if ($manager.Text -notlike ("Менеджер " + $bundleVersion)) { throw "Manager button does not use installed Zapret version: $($manager.Text)" }

    [ordered]@{
        settings_fixed_rect = $true
        settings_reopen_cycles = 3
        zapret_installed_version = $bundleVersion
        zapret_version_proxy_visible = $true
        stale_199d_hidden = $true
        zapret_owner_draw_buttons = 6
    } | ConvertTo-Json | Set-Content -LiteralPath $reportPath -Encoding utf8

    Write-Host 'REV10_UI_STABILITY_SMOKE_OK'
    Write-Host 'Settings fixed bounds after wheel + reopen cycles: PASS'
    Write-Host "Zapret displayed installed version ${bundleVersion} persistently: PASS"
    Write-Host 'Zapret six bridge buttons use dark owner-draw format: PASS'
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
