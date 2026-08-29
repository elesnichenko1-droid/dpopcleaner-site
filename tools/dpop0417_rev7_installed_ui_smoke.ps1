[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RootPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev7-installed-ui'
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
$dpopUpdatePath = Join-Path $RootPath 'DPopUpdate.exe'
$simpleUpdatePath = Join-Path $RootPath 'SimpleUpdate.exe'
$settingsPath = Join-Path $OutputDir 'SimpleUpdate-rev7.ini'
foreach ($required in @($launcherPath, $corePath, $simpleUpdatePath, $dpopUpdatePath, (Join-Path $RootPath 'Zapret\service.bat'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "rev.7 installed UI smoke prerequisite missing: $required" }
}
if ((Get-FileHash -LiteralPath $dpopUpdatePath -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $simpleUpdatePath -Algorithm SHA256).Hash) {
    throw 'DPopUpdate.exe must be the verified SimpleUpdate compatibility binary.'
}
Write-Host 'Legacy Zapret updater compatibility module: PASS'
Remove-Item -LiteralPath $settingsPath -Force -ErrorAction SilentlyContinue

$native = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class Rev7InstalledChild {
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
public static class Rev7InstalledNative {
    private delegate bool EnumProc(IntPtr hwnd, IntPtr p);
    [StructLayout(LayoutKind.Sequential)] private struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc proc, IntPtr p);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hwnd, int command);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] private static extern IntPtr SendMessageText(IntPtr hwnd, uint msg, IntPtr wp, StringBuilder lp);
    public static Rev7InstalledChild[] Children(IntPtr parent) {
        var list = new List<Rev7InstalledChild>();
        EnumProc cb = delegate(IntPtr h, IntPtr _) {
            var t = new StringBuilder(512); var c = new StringBuilder(128); RECT r;
            GetWindowText(h,t,t.Capacity); GetClassName(h,c,c.Capacity); GetWindowRect(h,out r);
            list.Add(new Rev7InstalledChild { Handle=h, Id=GetDlgCtrlID(h), Text=t.ToString(), ClassName=c.ToString(), Visible=IsWindowVisible(h), Left=r.Left, Top=r.Top, Right=r.Right, Bottom=r.Bottom });
            return true;
        };
        EnumChildWindows(parent,cb,IntPtr.Zero); GC.KeepAlive(cb); return list.ToArray();
    }
    public static string[] ComboItems(IntPtr combo) {
        const uint CB_GETCOUNT=0x0146, CB_GETLBTEXT=0x0148, CB_GETLBTEXTLEN=0x0149;
        int count=SendMessage(combo,CB_GETCOUNT,IntPtr.Zero,IntPtr.Zero).ToInt32();
        var items=new List<string>();
        for(int i=0;i<count;i++) {
            int len=SendMessage(combo,CB_GETLBTEXTLEN,(IntPtr)i,IntPtr.Zero).ToInt32();
            var text=new StringBuilder(Math.Max(1,len+1));
            SendMessageText(combo,CB_GETLBTEXT,(IntPtr)i,text); items.Add(text.ToString());
        }
        return items.ToArray();
    }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Children([IntPtr]$Window) { @([Rev7InstalledNative]::Children($Window)) }
function Click-Button([IntPtr]$Window, [string]$Text) {
    $button = Children $Window | Where-Object { $_.Visible -and $_.ClassName -eq 'Button' -and $_.Text -eq $Text } | Select-Object -First 1
    if (-not $button) { throw "Button not found: $Text" }
    [void][Rev7InstalledNative]::SendMessage($button.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 450
}
function Test-RectOverlap($A, $B) {
    return ($A.Left -lt $B.Right -and $A.Right -gt $B.Left -and $A.Top -lt $B.Bottom -and $A.Bottom -gt $B.Top)
}

$launcher = $null
$core = $null
try {
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
    if ($null -eq $core -or $core.MainWindowHandle -eq [IntPtr]::Zero) { throw 'rev.7 installed core window did not appear.' }
    $window = $core.MainWindowHandle

    # Lifecycle regression: temporary hide/restore must never kill the frozen application.
    $SW_HIDE = 0
    $SW_SHOW = 5
    [void][Rev7InstalledNative]::ShowWindow($window, $SW_HIDE)
    Start-Sleep -Milliseconds 900
    $core.Refresh(); $launcher.Refresh()
    if ($core.HasExited) { throw 'Core exited after SW_HIDE.' }
    if ($launcher.HasExited) { throw 'Bridge exited after SW_HIDE.' }
    [void][Rev7InstalledNative]::ShowWindow($window, $SW_SHOW)
    Start-Sleep -Milliseconds 900
    $core.Refresh()
    if ($core.HasExited) { throw 'Core exited while restoring after SW_SHOW.' }

    # Existing RAM page and existing threshold ComboBox id=1956 must now expose 5..95.
    Click-Button $window 'ОЗУ'
    $deadline = [DateTime]::UtcNow.AddSeconds(6)
    $ramCombo = $null
    do {
        $ramCombo = Children $window | Where-Object { $_.Visible -and $_.Id -eq 1956 -and $_.ClassName -eq 'ComboBox' } | Select-Object -First 1
        if (-not $ramCombo) { Start-Sleep -Milliseconds 150 }
    } while (-not $ramCombo -and [DateTime]::UtcNow -lt $deadline)
    if (-not $ramCombo) { throw 'Existing RAM threshold ComboBox id=1956 missing.' }
    $ramItems = @([Rev7InstalledNative]::ComboItems($ramCombo.Handle))
    if ($ramItems.Count -ne 19) { throw "RAM threshold count must be 19, got $($ramItems.Count): $($ramItems -join ', ')" }
    if ($ramItems[0] -ne '5%' -or $ramItems[-1] -ne '95%') { throw "RAM threshold must span 5%..95%: $($ramItems -join ', ')" }
    [void][Rev7InstalledNative]::SendMessage($ramCombo.Handle, 0x014E, [IntPtr]::new(0), [IntPtr]::Zero)
    [void][Rev7InstalledNative]::SendMessage($ramCombo.Handle, 0x014E, [IntPtr]::new(18), [IntPtr]::Zero)

    # Existing Zapret page gets only additional actions; the old buttons remain present.
    Click-Button $window 'Zapret'
    $deadline = [DateTime]::UtcNow.AddSeconds(6)
    $zapretChildren = @()
    do {
        $zapretChildren = Children $window
        $newCount = @($zapretChildren | Where-Object { $_.Visible -and $_.Text -in @('Починка трансляции','Починка подключения','Игровой фильтр 1.10.2','Менеджер 1.10.2') }).Count
        if ($newCount -lt 4) { Start-Sleep -Milliseconds 150 }
    } while ($newCount -lt 4 -and [DateTime]::UtcNow -lt $deadline)
    foreach ($label in @('Починка трансляции','Починка подключения','Игровой фильтр 1.10.2','Менеджер 1.10.2')) {
        if (-not ($zapretChildren | Where-Object { $_.Visible -and $_.Text -eq $label } | Select-Object -First 1)) { throw "Zapret rev.7 action missing: $label" }
    }
    foreach ($legacyLabel in @('Проверить версию','Скачать и установить','Диагностика','Тесты')) {
        if (-not ($zapretChildren | Where-Object { $_.Visible -and $_.Text -eq $legacyLabel } | Select-Object -First 1)) { throw "Existing Zapret control disappeared: $legacyLabel" }
    }

    $actions = @($zapretChildren | Where-Object { $_.Visible -and $_.Text -in @('Починка трансляции','Починка подключения','Игровой фильтр 1.10.2','Менеджер 1.10.2') })
    for ($i = 0; $i -lt $actions.Count; $i++) {
        for ($j = $i + 1; $j -lt $actions.Count; $j++) {
            if (Test-RectOverlap $actions[$i] $actions[$j]) {
                throw "Zapret actions overlap: '$($actions[$i].Text)' and '$($actions[$j].Text)'"
            }
        }
    }
    $additional = $zapretChildren | Where-Object { $_.Visible -and $_.ClassName -eq 'Static' -and $_.Text -eq 'Дополнительно' } | Select-Object -First 1
    $apply = $zapretChildren | Where-Object { $_.Visible -and $_.Id -eq 1704 -and $_.Text -eq 'Применить' } | Select-Object -First 1
    if (-not $additional -or -not $apply) { throw 'Zapret safe-row anchors are missing.' }
    foreach ($action in $actions) {
        if ($action.Top -lt ($additional.Top - 8) -or $action.Bottom -gt ($apply.Top - 4)) {
            throw "Compact Zapret toolbar escaped the safe row: $($action.Text) [$($action.Top),$($action.Bottom)]"
        }
    }
    $legacyZapretIds = @(1701,1713,1714,1703,1709,1715,1702,1716,1717,1704,1705,1707,1708,1710,1711)
    $legacyZapretControls = @($zapretChildren | Where-Object { $_.Visible -and $_.Id -in $legacyZapretIds })
    foreach ($action in $actions) {
        foreach ($legacy in $legacyZapretControls) {
            if (Test-RectOverlap $action $legacy) {
                throw "Existing Zapret control overlaps compact action toolbar: '$($action.Text)' vs '$($legacy.Text)'"
            }
        }
    }

    $zapretVersion = (Get-Content -Raw -LiteralPath (Join-Path $RootPath 'Zapret\.service\version.txt')).Trim()
    if ($zapretVersion -ne '1.10.2') { throw "Bundled Zapret must remain 1.10.2, got $zapretVersion" }

    # Existing Settings left area becomes the scroll host; right exclusions stay frozen UI.
    $gear = Children $window | Where-Object { $_.Id -eq 906 -and $_.ClassName -eq 'Button' } | Select-Object -First 1
    if (-not $gear) { throw 'Settings gear id=906 not found.' }
    [void][Rev7InstalledNative]::SendMessage($gear.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    $deadline = [DateTime]::UtcNow.AddSeconds(7)
    $settingsChildren = @()
    $scrollHost = $null
    do {
        Start-Sleep -Milliseconds 150
        $settingsChildren = Children $window
        $scrollHost = $settingsChildren | Where-Object { $_.Visible -and $_.Id -eq 1492 } | Select-Object -First 1
        $auto = $settingsChildren | Where-Object { $_.Visible -and $_.Id -eq 1490 -and $_.Text -eq 'Включить автообновление приложения' } | Select-Object -First 1
        $license = $settingsChildren | Where-Object { $_.Id -eq 1493 -and $_.Text -eq 'Лицензия' } | Select-Object -First 1
    } while ((-not $scrollHost -or -not $auto -or -not $license) -and [DateTime]::UtcNow -lt $deadline)
    if (-not $scrollHost) { throw 'Settings scroll host id=1492 missing.' }
    if (-not $auto) { throw 'Автообновление приложения missing from Settings scroll list.' }
    foreach ($label in @('Фоновый контроль мусора каждые 30 минут','Быстрый DPopGuard-скан при запуске','Проверять кэш Windows Update при запуске','Работать в трее и отслеживать новые установки','Автозапуск DPopCleaner вместе с Windows','Запускать приложение от имени администратора')) {
        if (-not ($settingsChildren | Where-Object { $_.Text -eq $label -and $_.Visible } | Select-Object -First 1)) { throw "Settings scroll proxy missing: $label" }
    }
    if (-not ($settingsChildren | Where-Object { $_.Text -eq 'Исключения очистки' -and $_.Visible } | Select-Object -First 1)) { throw 'Right-side cleanup exclusions layout disappeared.' }
    if ($settingsChildren | Where-Object { $_.Text -eq 'v0.2.11 BETA' -and $_.Visible } | Select-Object -First 1) { throw 'Legacy v0.2.11 BETA is visible.' }

    $beforeTop = $license.Top
    $WM_MOUSEWHEEL = 0x020A
    $wheelDown = [IntPtr]::new([long]0xFF880000)
    [void][Rev7InstalledNative]::SendMessage($scrollHost.Handle, $WM_MOUSEWHEEL, $wheelDown, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 350
    $licenseAfter = Children $window | Where-Object { $_.Id -eq 1493 -and $_.Text -eq 'Лицензия' } | Select-Object -First 1
    if (-not $licenseAfter -or $licenseAfter.Top -ge $beforeTop) { throw "Settings WM_MOUSEWHEEL did not move content: before=$beforeTop after=$($licenseAfter.Top)" }

    [pscustomobject]@{
        lifecycle_hide_restore = $true
        ram_range = '5%-95%'
        ram_threshold_items = $ramItems.Count
        zapret_version = $zapretVersion
        zapret_repairs = $true
        zapret_1102_actions = $true
        zapret_compact_toolbar = $true
        legacy_zapret_updater_module = $true
        settings_existing_controls_scroll = $true
        application_auto_update_in_scroll = $true
        legacy_version_hidden = $true
    } | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $OutputDir 'rev7-installed-ui-smoke-report.json') -Encoding utf8

    Write-Host 'REV7_INSTALLED_FUNCTIONAL_UI_SMOKE_OK'
}
finally {
    if ($core -and -not $core.HasExited) { Stop-Process -Id $core.Id -Force -ErrorAction SilentlyContinue }
    if ($launcher -and -not $launcher.HasExited) { Stop-Process -Id $launcher.Id -Force -ErrorAction SilentlyContinue }
}
