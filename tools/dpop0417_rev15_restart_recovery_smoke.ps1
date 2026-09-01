[CmdletBinding()]
param(
    [string]$RootPath = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$ownedRoot = $null
if ([string]::IsNullOrWhiteSpace($RootPath)) {
    $helper = Join-Path $repoRoot 'v0417/src/SimpleUpdate/bin/Release/net48/SimpleUpdate.exe'
    $coreSource = Join-Path $repoRoot 'downloads/DPopCleaner_0.2.14_BETA.exe'
    if (-not (Test-Path -LiteralPath $helper -PathType Leaf)) { throw "SimpleUpdate not built: $helper" }
    $ownedRoot = Join-Path ([IO.Path]::GetTempPath()) ('DPopRev15RestartSmoke-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $ownedRoot -Force | Out-Null
    Copy-Item -LiteralPath $helper -Destination (Join-Path $ownedRoot 'DPopCleaner.exe') -Force
    Copy-Item -LiteralPath $helper -Destination (Join-Path $ownedRoot 'SimpleUpdate.exe') -Force
    Copy-Item -LiteralPath $coreSource -Destination (Join-Path $ownedRoot 'DPopCleaner.Core.exe') -Force
    $RootPath = $ownedRoot
}
$RootPath = [IO.Path]::GetFullPath($RootPath)
$launcherPath = Join-Path $RootPath 'DPopCleaner.exe'
$corePath = Join-Path $RootPath 'DPopCleaner.Core.exe'
foreach ($required in @($launcherPath, $corePath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "rev.15 restart smoke prerequisite missing: $required" }
}

$settingsPath = Join-Path $RootPath 'SimpleUpdate-rev15-restart-smoke.ini'
$diagnosticMarker = Join-Path $RootPath 'DPopCleaner-bridge-diagnostics.enabled'
$diagnosticLog = Join-Path $RootPath 'DPopCleaner-bridge-diagnostics.log'
Set-Content -LiteralPath $diagnosticMarker -Value 'enabled' -Encoding ascii
Remove-Item -LiteralPath $diagnosticLog -Force -ErrorAction SilentlyContinue

$native = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class Rev15Child {
    public IntPtr Handle;
    public int Id;
    public string Text;
    public string ClassName;
    public bool Visible;
}
public static class Rev15Native {
    private delegate bool EnumProc(IntPtr hwnd, IntPtr p);
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc proc, IntPtr p);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumProc proc, IntPtr p);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);

    public static Rev15Child[] Children(IntPtr parent) {
        var list = new List<Rev15Child>();
        EnumProc cb = delegate(IntPtr h, IntPtr _) {
            var t = new StringBuilder(512); var c = new StringBuilder(128);
            GetWindowText(h,t,t.Capacity); GetClassName(h,c,c.Capacity);
            list.Add(new Rev15Child { Handle=h, Id=GetDlgCtrlID(h), Text=t.ToString(), ClassName=c.ToString(), Visible=IsWindowVisible(h) });
            return true;
        };
        EnumChildWindows(parent,cb,IntPtr.Zero); GC.KeepAlive(cb); return list.ToArray();
    }

    public static IntPtr FindTopWindowForProcess(int processId, string title) {
        IntPtr found = IntPtr.Zero;
        EnumProc cb = delegate(IntPtr h, IntPtr _) {
            uint pid; GetWindowThreadProcessId(h, out pid); if (pid != (uint)processId) return true;
            var t = new StringBuilder(512); GetWindowText(h,t,t.Capacity);
            if (String.Equals(t.ToString(), title, StringComparison.Ordinal)) { found = h; return false; }
            return true;
        };
        EnumWindows(cb,IntPtr.Zero); GC.KeepAlive(cb); return found;
    }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Get-Core([int]$ExcludeId = -1) {
    foreach ($candidate in @(Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue)) {
        try {
            if ($candidate.Id -eq $ExcludeId) { continue }
            if ([IO.Path]::GetFullPath($candidate.Path) -eq [IO.Path]::GetFullPath($corePath)) { return $candidate }
        } catch { }
    }
    return $null
}

function Wait-Core([int]$ExcludeId = -1, [int]$Seconds = 12) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    do {
        Start-Sleep -Milliseconds 100
        $candidate = Get-Core -ExcludeId $ExcludeId
        if ($candidate) {
            $candidate.Refresh()
            if (-not $candidate.HasExited -and $candidate.MainWindowHandle -ne [IntPtr]::Zero) { return $candidate }
        }
    } while ([DateTime]::UtcNow -lt $deadline)
    return $null
}

function Open-SettingsAndWaitBridge([Diagnostics.Process]$Core, [int]$Seconds = 8) {
    $children = @([Rev15Native]::Children($Core.MainWindowHandle))
    $gear = $children | Where-Object { $_.Id -eq 906 -and $_.ClassName -eq 'Button' } | Select-Object -First 1
    if (-not $gear) { throw "Settings gear id=906 missing for core pid=$($Core.Id)." }
    [void][Rev15Native]::SendMessage($gear.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)

    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    do {
        Start-Sleep -Milliseconds 120
        $children = @([Rev15Native]::Children($Core.MainWindowHandle))
        $host = $children | Where-Object { $_.Id -eq 1492 -and $_.Visible } | Select-Object -First 1
        if ($host) { return $children }
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Enhanced Settings host id=1492 did not appear for core pid=$($Core.Id)."
}

function Ensure-TraySetting([Diagnostics.Process]$Core, [object[]]$Children) {
    $BM_GETCHECK = 0x00F0
    $BM_CLICK = 0x00F5
    $trayProxy = $Children | Where-Object { $_.Id -eq 1503 } | Select-Object -First 1
    if (-not $trayProxy) { throw "Tray Settings proxy id=1503 missing for core pid=$($Core.Id)." }
    if ([Rev15Native]::SendMessage($trayProxy.Handle, $BM_GETCHECK, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -ne 1) {
        [void][Rev15Native]::SendMessage($trayProxy.Handle, $BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 250
    }
    $save = $Children | Where-Object { $_.Id -eq 1401 -and $_.Visible } | Select-Object -First 1
    if ($save) { [void][Rev15Native]::SendMessage($save.Handle, $BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero); Start-Sleep -Milliseconds 250 }
}

function Wait-TrayHost([Diagnostics.Process]$Launcher, [int]$Seconds = 6) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    do {
        Start-Sleep -Milliseconds 120
        $Launcher.Refresh()
        if ($Launcher.HasExited) { return [IntPtr]::Zero }
        $host = [Rev15Native]::FindTopWindowForProcess($Launcher.Id, 'DPopCleaner.TrayRamBadgeHost')
        if ($host -ne [IntPtr]::Zero) { return $host }
    } while ([DateTime]::UtcNow -lt $deadline)
    return [IntPtr]::Zero
}

$launcher = $null
$core1 = $null
$core2 = $null
try {
    $launcher = Start-Process -FilePath $launcherPath -ArgumentList @('--no-update-check','--settings-path',('"' + $settingsPath + '"')) -WorkingDirectory $RootPath -PassThru
    $core1 = Wait-Core
    if (-not $core1) { throw 'Initial frozen core did not create a window.' }

    $firstSettings = @(Open-SettingsAndWaitBridge -Core $core1)
    Ensure-TraySetting -Core $core1 -Children $firstSettings
    $firstTrayHost = Wait-TrayHost -Launcher $launcher
    if ($firstTrayHost -eq [IntPtr]::Zero) { throw 'Initial digital RAM tray host is missing.' }

    $oldCoreId = $core1.Id
    Stop-Process -Id $oldCoreId -Force
    $core1.WaitForExit(5000) | Out-Null
    $core1 = $null

    # Simulate the frozen core's Application.Restart sequence used by language changes: same EXE,
    # new PID, same installation path, while the bridge launcher must remain the long-lived owner.
    $core2 = Start-Process -FilePath $corePath -WorkingDirectory $RootPath -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(12)
    do { Start-Sleep -Milliseconds 100; $core2.Refresh(); $launcher.Refresh() } while (($core2.MainWindowHandle -eq [IntPtr]::Zero -or $launcher.HasExited) -and [DateTime]::UtcNow -lt $deadline)
    if ($launcher.HasExited) { throw 'Bridge launcher exited when frozen core was replaced during language restart.' }
    if ($core2.HasExited -or $core2.MainWindowHandle -eq [IntPtr]::Zero) { throw 'Successor frozen core did not create a window.' }
    if ($core2.Id -eq $oldCoreId) { throw 'Restart smoke did not obtain a successor PID.' }

    $secondSettings = @(Open-SettingsAndWaitBridge -Core $core2)
    Ensure-TraySetting -Core $core2 -Children $secondSettings
    $secondTrayHost = Wait-TrayHost -Launcher $launcher
    if ($secondTrayHost -eq [IntPtr]::Zero) { throw 'Digital RAM tray host was not recreated after frozen core restart.' }

    $diagDeadline = [DateTime]::UtcNow.AddSeconds(4)
    $diag = ''
    do {
        Start-Sleep -Milliseconds 100
        if (Test-Path -LiteralPath $diagnosticLog -PathType Leaf) { $diag = Get-Content -Raw -LiteralPath $diagnosticLog }
    } while ($diag -notmatch ('core-restart-attached pid=' + [regex]::Escape($core2.Id.ToString())) -and [DateTime]::UtcNow -lt $diagDeadline)
    if ($diag -notmatch ('core-restart-attached pid=' + [regex]::Escape($core2.Id.ToString()))) {
        throw "Launcher did not record adoption of successor core pid=$($core2.Id)."
    }

    Write-Host "REV15_RESTART_RECOVERY old_core=$oldCoreId new_core=$($core2.Id) launcher=$($launcher.Id) tray_host=0x$($secondTrayHost.ToInt64().ToString('X'))"
    Write-Host 'REV15_LANGUAGE_RESTART_BRIDGE_SMOKE_OK'
    Write-Host 'REV15_LANGUAGE_RESTART_RAM_TRAY_SMOKE_OK'

    Stop-Process -Id $core2.Id -Force
    $core2.WaitForExit(5000) | Out-Null
    $core2 = $null
    if (-not $launcher.WaitForExit(5000)) { throw 'Bridge launcher did not exit after successor core closed normally.' }
    $launcher = $null
}
catch {
    if (Test-Path -LiteralPath $diagnosticLog -PathType Leaf) {
        Write-Host '===== REV15_RESTART_BRIDGE_DIAGNOSTICS ====='
        Get-Content -Raw -LiteralPath $diagnosticLog | Write-Host
    }
    throw
}
finally {
    if ($core1 -and -not $core1.HasExited) { Stop-Process -Id $core1.Id -Force -ErrorAction SilentlyContinue }
    if ($core2 -and -not $core2.HasExited) { Stop-Process -Id $core2.Id -Force -ErrorAction SilentlyContinue }
    if ($launcher -and -not $launcher.HasExited) { Stop-Process -Id $launcher.Id -Force -ErrorAction SilentlyContinue }
    Remove-Item -LiteralPath $diagnosticMarker -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $diagnosticLog -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $settingsPath -Force -ErrorAction SilentlyContinue
    if ($ownedRoot -and (Test-Path -LiteralPath $ownedRoot)) { Remove-Item -LiteralPath $ownedRoot -Recurse -Force -ErrorAction SilentlyContinue }
}
