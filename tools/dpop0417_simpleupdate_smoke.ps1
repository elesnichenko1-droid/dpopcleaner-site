[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$helper = Join-Path $root 'v0417/src/SimpleUpdate/bin/Release/net48/SimpleUpdate.exe'
$core = Join-Path $root 'downloads/DPopCleaner_0.2.14_BETA.exe'
if (-not (Test-Path -LiteralPath $helper -PathType Leaf)) { throw "SimpleUpdate not built: $helper" }

$work = Join-Path ([IO.Path]::GetTempPath()) ('DPopSimpleUpdateSmoke-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work -Force | Out-Null
Copy-Item -LiteralPath $helper -Destination (Join-Path $work 'SimpleUpdate.exe') -Force
Copy-Item -LiteralPath $core -Destination (Join-Path $work 'DPopCleaner.exe') -Force
$settings = Join-Path $work 'SimpleUpdate.ini'

$native = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class SmokeChild {
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
public static class SmokeNative {
    private delegate bool EnumProc(IntPtr hwnd, IntPtr p);
    [StructLayout(LayoutKind.Sequential)] private struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc proc, IntPtr p);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    public static SmokeChild[] Children(IntPtr parent) {
        var list = new List<SmokeChild>();
        EnumProc cb = delegate(IntPtr h, IntPtr _) {
            var t = new StringBuilder(512); var c = new StringBuilder(128); RECT r;
            GetWindowText(h,t,t.Capacity); GetClassName(h,c,c.Capacity); GetWindowRect(h,out r);
            list.Add(new SmokeChild { Handle=h, Id=GetDlgCtrlID(h), Text=t.ToString(), ClassName=c.ToString(), Visible=IsWindowVisible(h), Left=r.Left, Top=r.Top, Right=r.Right, Bottom=r.Bottom });
            return true;
        };
        EnumChildWindows(parent,cb,IntPtr.Zero); GC.KeepAlive(cb); return list.ToArray();
    }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

$launcher = $null
$coreProcess = $null
try {
    $launcher = Start-Process -FilePath (Join-Path $work 'SimpleUpdate.exe') -ArgumentList @('--no-update-check','--settings-path',('"' + $settings + '"')) -WorkingDirectory $work -PassThru

    $deadline = [DateTime]::UtcNow.AddSeconds(18)
    do {
        Start-Sleep -Milliseconds 250
        $candidate = Get-Process -Name 'DPopCleaner' -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq (Join-Path $work 'DPopCleaner.exe') } | Select-Object -First 1
        if ($candidate) { $coreProcess = $candidate; $coreProcess.Refresh() }
    } while (($null -eq $coreProcess -or $coreProcess.MainWindowHandle -eq [IntPtr]::Zero) -and [DateTime]::UtcNow -lt $deadline)

    if ($null -eq $coreProcess -or $coreProcess.MainWindowHandle -eq [IntPtr]::Zero) { throw 'SimpleUpdate did not launch the authentic DPopCleaner core.' }

    $children = [SmokeNative]::Children($coreProcess.MainWindowHandle)
    $gear = $children | Where-Object { $_.Id -eq 906 -and $_.ClassName -eq 'Button' } | Select-Object -First 1
    if (-not $gear) { throw 'Authentic Settings gear id=906 not found.' }
    [void][SmokeNative]::SendMessage($gear.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)

    $deadline = [DateTime]::UtcNow.AddSeconds(6)
    $scrollHost = $null
    $checkbox = $null
    $checkNow = $null
    $licenseHeading = $null
    do {
        Start-Sleep -Milliseconds 150
        $settingsChildren = [SmokeNative]::Children($coreProcess.MainWindowHandle)
        $scrollHost = $settingsChildren | Where-Object { $_.Id -eq 1492 -and $_.Visible } | Select-Object -First 1
        $checkbox = $settingsChildren | Where-Object { $_.Id -eq 1490 -and $_.Text -eq 'Включить автообновление' -and $_.Visible } | Select-Object -First 1
        $checkNow = $settingsChildren | Where-Object { $_.Id -eq 1491 -and $_.Text -eq 'Проверить обновления' -and $_.Visible } | Select-Object -First 1
        $licenseHeading = $settingsChildren | Where-Object { $_.Id -eq 1493 -and $_.Text -eq 'Лицензия' -and $_.Visible } | Select-Object -First 1
    } while ((-not $scrollHost -or -not $checkbox -or -not $checkNow -or -not $licenseHeading) -and [DateTime]::UtcNow -lt $deadline)

    if (-not $scrollHost) { throw 'Scrollable additional-settings host id=1492 was not bridged into authentic Settings.' }
    if (-not $checkbox) { throw 'Auto-update checkbox was not bridged into the scroll host.' }
    if (-not $checkNow) { throw 'Check-update button was not bridged into the scroll host.' }
    if (-not $licenseHeading) { throw 'License section was not placed inside the scroll host.' }

    $legacyVersion = $settingsChildren | Where-Object { $_.Text -eq 'v0.2.11 BETA' -and $_.Visible } | Select-Object -First 1
    if ($legacyVersion) { throw 'Legacy bottom-right v0.2.11 BETA badge must be hidden by SimpleUpdate.' }

    $beforeScrollTop = $licenseHeading.Top
    $WM_MOUSEWHEEL = 0x020A
    $wheelDown = [IntPtr]::new([long]0xFF880000)
    [void][SmokeNative]::SendMessage($scrollHost.Handle, $WM_MOUSEWHEEL, $wheelDown, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 400
    $afterWheel = [SmokeNative]::Children($coreProcess.MainWindowHandle)
    $licenseAfter = $afterWheel | Where-Object { $_.Id -eq 1493 -and $_.Text -eq 'Лицензия' } | Select-Object -First 1
    if (-not $licenseAfter) { throw 'License heading disappeared after WM_MOUSEWHEEL.' }
    if ($licenseAfter.Top -ge $beforeScrollTop) { throw "Scrollable Settings did not move on WM_MOUSEWHEEL: before=$beforeScrollTop after=$($licenseAfter.Top)" }

    $BM_GETCHECK = 0x00F0
    $BM_CLICK = 0x00F5
    $checked = [SmokeNative]::SendMessage($checkbox.Handle, $BM_GETCHECK, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
    if ($checked -ne 1) { throw "Auto-update checkbox should default checked, actual=$checked" }

    [void][SmokeNative]::SendMessage($checkbox.Handle, $BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 800
    if (-not (Test-Path -LiteralPath $settings -PathType Leaf)) { throw 'SimpleUpdate did not persist checkbox state.' }
    $saved = Get-Content -Raw -LiteralPath $settings
    if ($saved -notmatch 'auto_update=0') { throw "Expected auto_update=0, got: $saved" }

    Stop-Process -Id $coreProcess.Id -Force
    $coreProcess.WaitForExit(5000) | Out-Null
    if (-not $launcher.WaitForExit(6000)) { throw 'SimpleUpdate did not exit after DPopCleaner closed.' }

    Write-Host 'SIMPLEUPDATE_SCROLLABLE_SETTINGS_UI_SMOKE_OK'
}
finally {
    if ($coreProcess -and -not $coreProcess.HasExited) { Stop-Process -Id $coreProcess.Id -Force -ErrorAction SilentlyContinue }
    if ($launcher -and -not $launcher.HasExited) { Stop-Process -Id $launcher.Id -Force -ErrorAction SilentlyContinue }
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}