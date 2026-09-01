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
    [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] private static extern IntPtr SendMessageBuffer(IntPtr hwnd, uint msg, IntPtr wp, StringBuilder lp);
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

    # Installed regression: switching the authentic native Language ComboBox must keep the bridge
    # host visible and update bridge-owned Settings controls instead of resurfacing the old UI.
    $beforeLanguage = [InstalledSettingsNative]::Children($coreProcess.MainWindowHandle)
    $languageCombo = $null
    $englishIndex = -1
    foreach ($combo in @($beforeLanguage | Where-Object { $_.Visible -and $_.ClassName -eq 'ComboBox' })) {
        $items = @([InstalledSettingsNative]::ComboItems($combo.Handle))
        for ($i = 0; $i -lt $items.Count; $i++) {
            if ($items[$i] -eq 'English') {
                $languageCombo = $combo
                $englishIndex = $i
                break
            }
        }
        if ($languageCombo) { break }
    }
    if (-not $languageCombo -or $englishIndex -lt 0) { throw 'Installed authentic Settings Language ComboBox with English option was not found.' }

    $CB_SETCURSEL = 0x014E
    $WM_COMMAND = 0x0111
    $CBN_SELCHANGE = 1
    [void][InstalledSettingsNative]::SendMessage($languageCombo.Handle, $CB_SETCURSEL, [IntPtr]::new($englishIndex), [IntPtr]::Zero)
    $languageNotification = [IntPtr]::new([long](($CBN_SELCHANGE -shl 16) -bor ($languageCombo.Id -band 0xffff)))
    [void][InstalledSettingsNative]::SendMessage($coreProcess.MainWindowHandle, $WM_COMMAND, $languageNotification, $languageCombo.Handle)

    $deadline = [DateTime]::UtcNow.AddSeconds(7)
    $englishChildren = @()
    $englishHost = $null
    $englishAdminProxy = $null
    $englishAutoUpdate = $null
    $englishLicense = $null
    do {
        Start-Sleep -Milliseconds 150
        $englishChildren = @([InstalledSettingsNative]::Children($coreProcess.MainWindowHandle))
        $englishHost = $englishChildren | Where-Object { $_.Id -eq 1492 -and $_.Visible } | Select-Object -First 1
        $englishAdminProxy = $englishChildren | Where-Object { $_.Id -eq 1505 -and $_.Visible -and $_.Text -match '(?i)administrator' } | Select-Object -First 1
        $englishAutoUpdate = $englishChildren | Where-Object { $_.Id -eq 1490 -and $_.Visible -and $_.Text -eq 'Enable application auto-updates' } | Select-Object -First 1
        $englishLicense = $englishChildren | Where-Object { $_.Id -eq 1493 -and $_.Visible -and $_.Text -eq 'License' } | Select-Object -First 1
    } while ((-not $englishHost -or -not $englishAdminProxy -or -not $englishAutoUpdate -or -not $englishLicense) -and [DateTime]::UtcNow -lt $deadline)

    if (-not $englishHost) { throw 'Installed Settings bridge disappeared after switching Language to English.' }
    if (-not $englishAdminProxy) { throw 'Installed Settings checkbox proxies did not switch to English.' }
    if (-not $englishAutoUpdate) { throw 'Installed bridge-owned auto-update control did not switch to English.' }
    if (-not $englishLicense) { throw 'Installed bridge-owned License section did not switch to English.' }
    if ($englishChildren | Where-Object { $_.Visible -and $_.Text -eq 'v0.2.11 BETA' } | Select-Object -First 1) { throw 'Installed legacy version badge resurfaced after Language change.' }

    [pscustomobject]@{
        launcher = $launcherPath
        core = $corePath
        scroll_host = $true
        auto_update_control = $true
        check_update_control = $true
        license_in_scroll = $true
        legacy_version_hidden = $true
        mouse_wheel_scroll = $true
        language_switch_english = $true
        bridge_survives_language_switch = $true
        english_proxy_controls = $true
    } | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $OutputDir 'installed-settings-smoke-report.json') -Encoding utf8

    Stop-Process -Id $coreProcess.Id -Force
    $coreProcess.WaitForExit(5000) | Out-Null
    if (-not $launcherProcess.WaitForExit(6000)) { throw 'Installed DPopCleaner.exe bridge did not exit after core closed.' }
    $coreProcess = $null
    $launcherProcess = $null

    Write-Host 'INSTALLED_SETTINGS_BRIDGE_SMOKE_OK'
    Write-Host 'INSTALLED_SETTINGS_LANGUAGE_SWITCH_SMOKE_OK'
}
finally {
    if ($coreProcess -and -not $coreProcess.HasExited) { Stop-Process -Id $coreProcess.Id -Force -ErrorAction SilentlyContinue }
    if ($launcherProcess -and -not $launcherProcess.HasExited) { Stop-Process -Id $launcherProcess.Id -Force -ErrorAction SilentlyContinue }
}
