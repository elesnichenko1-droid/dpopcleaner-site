[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$helper = Join-Path $root 'v0417/src/SimpleUpdate/bin/Release/net48/SimpleUpdate.exe'
$coreSource = Join-Path $root 'downloads/DPopCleaner_0.2.14_BETA.exe'
if (-not (Test-Path -LiteralPath $helper -PathType Leaf)) { throw "SimpleUpdate not built: $helper" }

$work = Join-Path ([IO.Path]::GetTempPath()) ('DPopSimpleUpdateSmoke-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work -Force | Out-Null
Copy-Item -LiteralPath $helper -Destination (Join-Path $work 'DPopCleaner.exe') -Force
Copy-Item -LiteralPath $helper -Destination (Join-Path $work 'SimpleUpdate.exe') -Force
Copy-Item -LiteralPath $coreSource -Destination (Join-Path $work 'DPopCleaner.Core.exe') -Force
$corePath = Join-Path $work 'DPopCleaner.Core.exe'
$settings = Join-Path $work 'SimpleUpdate.ini'
$diagnosticMarker = Join-Path $work 'DPopCleaner-bridge-diagnostics.enabled'
$diagnosticLog = Join-Path $work 'DPopCleaner-bridge-diagnostics.log'
Set-Content -LiteralPath $diagnosticMarker -Value 'enabled' -Encoding ascii

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
    [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] private static extern IntPtr SendMessageBuffer(IntPtr hwnd, uint msg, IntPtr wp, StringBuilder lp);
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
    public static string[] ComboItems(IntPtr combo) {
        const uint CB_GETCOUNT = 0x0146, CB_GETLBTEXT = 0x0148, CB_GETLBTEXTLEN = 0x0149;
        var count = SendMessage(combo, CB_GETCOUNT, IntPtr.Zero, IntPtr.Zero).ToInt32();
        var items = new List<string>();
        for (var i = 0; i < count; i++) {
            var length = SendMessage(combo, CB_GETLBTEXTLEN, new IntPtr(i), IntPtr.Zero).ToInt32();
            if (length < 0) continue;
            var value = new StringBuilder(length + 1);
            SendMessageBuffer(combo, CB_GETLBTEXT, new IntPtr(i), value);
            items.Add(value.ToString());
        }
        return items.ToArray();
    }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

$launcher = $null
$coreProcess = $null
try {
    $launcher = Start-Process -FilePath (Join-Path $work 'DPopCleaner.exe') -ArgumentList @('--no-update-check','--settings-path',('"' + $settings + '"')) -WorkingDirectory $work -PassThru

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

    if ($null -eq $coreProcess -or $coreProcess.MainWindowHandle -eq [IntPtr]::Zero) { throw 'DPopCleaner.exe bridge did not launch the authentic DPopCleaner.Core.exe.' }

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
        $checkbox = $settingsChildren | Where-Object { $_.Id -eq 1490 -and $_.Text -eq 'Включить автообновление приложения' -and $_.Visible } | Select-Object -First 1
        $checkNow = $settingsChildren | Where-Object { $_.Id -eq 1491 -and $_.Text -eq 'Проверить обновления' -and $_.Visible } | Select-Object -First 1
        $licenseHeading = $settingsChildren | Where-Object { $_.Id -eq 1493 -and $_.Text -eq 'Лицензия' -and $_.Visible } | Select-Object -First 1
    } while ((-not $scrollHost -or -not $checkbox -or -not $checkNow -or -not $licenseHeading) -and [DateTime]::UtcNow -lt $deadline)

    if (-not $scrollHost) { throw 'Scrollable settings host id=1492 was not bridged into authentic Settings.' }
    if (-not $checkbox) { throw 'Application auto-update checkbox was not bridged into the scroll host.' }
    if (-not $checkNow) { throw 'Check-update button was not bridged into the scroll host.' }
    if (-not $licenseHeading) { throw 'License section was not placed inside the scroll host.' }

    foreach ($label in @('Фоновый контроль мусора каждые 30 минут','Быстрый DPopGuard-скан при запуске','Проверять кэш Windows Update при запуске','Работать в трее и отслеживать новые установки','Автозапуск DPopCleaner вместе с Windows','Запускать приложение от имени администратора')) {
        if (-not ($settingsChildren | Where-Object { $_.Text -eq $label -and $_.Visible } | Select-Object -First 1)) { throw "Legacy Settings proxy missing: $label" }
    }

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

    # Regression: changing the authentic frozen Settings language used to hide bridge id=1492
    # because LauncherContext searched for the Russian heading 'Настройки'. Exercise the real
    # Language ComboBox and require the enhanced Settings host to survive the live locale switch.
    $beforeLanguage = [SmokeNative]::Children($coreProcess.MainWindowHandle)
    $languageCombo = $null
    $englishIndex = -1
    foreach ($combo in @($beforeLanguage | Where-Object { $_.Visible -and $_.ClassName -eq 'ComboBox' })) {
        $items = @([SmokeNative]::ComboItems($combo.Handle))
        for ($i = 0; $i -lt $items.Count; $i++) {
            if ($items[$i] -eq 'English') {
                $languageCombo = $combo
                $englishIndex = $i
                break
            }
        }
        if ($languageCombo) { break }
    }
    if (-not $languageCombo -or $englishIndex -lt 0) { throw 'Authentic Settings Language ComboBox with English option was not found.' }

    $CB_SETCURSEL = 0x014E
    $WM_COMMAND = 0x0111
    $CBN_SELCHANGE = 1
    [void][SmokeNative]::SendMessage($languageCombo.Handle, $CB_SETCURSEL, [IntPtr]::new($englishIndex), [IntPtr]::Zero)
    $languageNotification = [IntPtr]::new([long](($CBN_SELCHANGE -shl 16) -bor ($languageCombo.Id -band 0xffff)))
    [void][SmokeNative]::SendMessage($coreProcess.MainWindowHandle, $WM_COMMAND, $languageNotification, $languageCombo.Handle)

    $deadline = [DateTime]::UtcNow.AddSeconds(7)
    $englishChildren = @()
    $englishHost = $null
    $englishAdminProxy = $null
    $englishAutoUpdate = $null
    $englishLicense = $null
    do {
        Start-Sleep -Milliseconds 150
        $englishChildren = @([SmokeNative]::Children($coreProcess.MainWindowHandle))
        $englishHost = $englishChildren | Where-Object { $_.Id -eq 1492 -and $_.Visible } | Select-Object -First 1
        $englishAdminProxy = $englishChildren | Where-Object { $_.Id -eq 1505 -and $_.Visible -and $_.Text -match '(?i)administrator' } | Select-Object -First 1
        $englishAutoUpdate = $englishChildren | Where-Object { $_.Id -eq 1490 -and $_.Visible -and $_.Text -eq 'Enable application auto-updates' } | Select-Object -First 1
        $englishLicense = $englishChildren | Where-Object { $_.Id -eq 1493 -and $_.Visible -and $_.Text -eq 'License' } | Select-Object -First 1
    } while ((-not $englishHost -or -not $englishAdminProxy -or -not $englishAutoUpdate -or -not $englishLicense) -and [DateTime]::UtcNow -lt $deadline)

    if (-not $englishHost) { throw 'Settings bridge disappeared after switching Language to English; old frozen interface resurfaced.' }
    if (-not $englishAdminProxy) { throw 'Settings checkbox proxies did not follow the authentic English locale.' }
    if (-not $englishAutoUpdate) { throw 'Bridge-owned auto-update control did not switch to English.' }
    if (-not $englishLicense) { throw 'Bridge-owned License section did not switch to English.' }
    if ($englishChildren | Where-Object { $_.Text -eq 'v0.2.11 BETA' -and $_.Visible } | Select-Object -First 1) { throw 'Legacy version badge resurfaced after Language change.' }

    Stop-Process -Id $coreProcess.Id -Force
    $coreProcess.WaitForExit(5000) | Out-Null
    if (-not $launcher.WaitForExit(6000)) { throw 'DPopCleaner.exe bridge did not exit after DPopCleaner.Core.exe closed.' }

    Write-Host 'SIMPLEUPDATE_SCROLLABLE_SETTINGS_UI_SMOKE_OK'
    Write-Host 'SIMPLEUPDATE_SETTINGS_LANGUAGE_SWITCH_SMOKE_OK'
}
catch {
    if (Test-Path -LiteralPath $diagnosticLog -PathType Leaf) {
        Write-Host '===== DPOP_BRIDGE_DIAGNOSTICS_FROM_SMOKE ====='
        Get-Content -Raw -LiteralPath $diagnosticLog | Write-Host
    }
    else {
        Write-Host "Bridge diagnostics were not produced in launcher directory: $diagnosticLog"
    }
    throw
}
finally {
    if ($coreProcess -and -not $coreProcess.HasExited) { Stop-Process -Id $coreProcess.Id -Force -ErrorAction SilentlyContinue }
    if ($launcher -and -not $launcher.HasExited) { Stop-Process -Id $launcher.Id -Force -ErrorAction SilentlyContinue }
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
