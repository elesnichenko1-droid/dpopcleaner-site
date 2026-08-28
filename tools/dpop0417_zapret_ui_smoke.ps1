[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RootPath,
    [string]$OutputDir = '_release/0.4.17/evidence/zapret-ui'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RootPath = if ([IO.Path]::IsPathRooted($RootPath)) { $RootPath } else { Join-Path $repoRoot $RootPath }
$OutputDir = if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repoRoot $OutputDir }
$RootPath = [IO.Path]::GetFullPath($RootPath)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$launcher = Join-Path $RootPath 'SimpleUpdate.exe'
$core = Join-Path $RootPath 'DPopCleaner.exe'
$zapretRoot = Join-Path $RootPath 'Zapret'
$service = Join-Path $zapretRoot 'service.bat'
$winws = Join-Path $zapretRoot 'bin\winws.exe'
$windivert = Join-Path $zapretRoot 'bin\WinDivert64.sys'
foreach ($required in @($launcher, $core, $service, $winws, $windivert)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Zapret UI smoke prerequisite missing: $required" }
}
$strategyFiles = @(Get-ChildItem -LiteralPath $zapretRoot -Filter 'general*.bat' -File | Sort-Object Name)
if ($strategyFiles.Count -eq 0) { throw 'Zapret UI smoke found no general*.bat strategies under the legacy Zapret directory.' }

$native = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class ZapretSmokeChild {
    public IntPtr Handle;
    public int Id;
    public string Text;
    public string ClassName;
    public bool Visible;
}
public static class ZapretSmokeNative {
    private delegate bool EnumProc(IntPtr hwnd, IntPtr p);
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc proc, IntPtr p);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, StringBuilder lp);
    [DllImport("kernel32.dll", SetLastError=true)] private static extern IntPtr OpenProcess(uint access, bool inheritHandle, int processId);
    [DllImport("kernel32.dll", SetLastError=true)] private static extern bool ReadProcessMemory(IntPtr process, IntPtr address, byte[] buffer, IntPtr size, out IntPtr read);
    [DllImport("kernel32.dll")] private static extern bool CloseHandle(IntPtr handle);
    public static ZapretSmokeChild[] Children(IntPtr parent) {
        var list = new List<ZapretSmokeChild>();
        EnumProc cb = delegate(IntPtr h, IntPtr _) {
            var t = new StringBuilder(512); var c = new StringBuilder(128);
            GetWindowText(h,t,t.Capacity); GetClassName(h,c,c.Capacity);
            list.Add(new ZapretSmokeChild { Handle=h, Id=GetDlgCtrlID(h), Text=t.ToString(), ClassName=c.ToString(), Visible=IsWindowVisible(h) });
            return true;
        };
        EnumChildWindows(parent,cb,IntPtr.Zero); GC.KeepAlive(cb); return list.ToArray();
    }
    public static string[] ComboBoxItems(IntPtr hwnd) {
        const uint CB_GETCOUNT = 0x0146;
        const uint CB_GETLBTEXT = 0x0148;
        const uint CB_GETLBTEXTLEN = 0x0149;
        int count = SendMessage(hwnd, CB_GETCOUNT, IntPtr.Zero, IntPtr.Zero).ToInt32();
        var items = new List<string>();
        for (int index = 0; index < count; index++) {
            int length = SendMessage(hwnd, CB_GETLBTEXTLEN, (IntPtr)index, IntPtr.Zero).ToInt32();
            if (length < 0) continue;
            var text = new StringBuilder(length + 1);
            SendMessage(hwnd, CB_GETLBTEXT, (IntPtr)index, text);
            items.Add(text.ToString());
        }
        return items.ToArray();
    }
    public static string ReadUtf16(int processId, IntPtr address, int maxChars) {
        const uint PROCESS_VM_READ = 0x0010;
        const uint PROCESS_QUERY_INFORMATION = 0x0400;
        IntPtr process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, false, processId);
        if (process == IntPtr.Zero) throw new InvalidOperationException("OpenProcess failed: " + Marshal.GetLastWin32Error());
        try {
            var bytes = new byte[maxChars * 2];
            IntPtr read;
            if (!ReadProcessMemory(process, address, bytes, (IntPtr)bytes.Length, out read))
                throw new InvalidOperationException("ReadProcessMemory failed: " + Marshal.GetLastWin32Error());
            int usable = Math.Min(bytes.Length, read.ToInt32());
            int end = 0;
            while (end + 1 < usable && (bytes[end] != 0 || bytes[end + 1] != 0)) end += 2;
            return Encoding.Unicode.GetString(bytes, 0, end);
        } finally {
            CloseHandle(process);
        }
    }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

$launcherProcess = $null
$coreProcess = $null
$selectedStrategy = ''
$verifiedStrategy = ''
$strategyEntries = @()
$strategyCount = 0
$legacyZapretRoot = ''
try {
    $settings = Join-Path $OutputDir 'SimpleUpdate-zapret-ui.ini'
    Remove-Item -LiteralPath $settings -Force -ErrorAction SilentlyContinue
    $launcherProcess = Start-Process -FilePath $launcher -ArgumentList @('--no-update-check','--settings-path',('"' + $settings + '"')) -WorkingDirectory $RootPath -PassThru

    $deadline = [DateTime]::UtcNow.AddSeconds(18)
    do {
        Start-Sleep -Milliseconds 250
        foreach ($candidate in @(Get-Process -Name 'DPopCleaner' -ErrorAction SilentlyContinue)) {
            try {
                if ([IO.Path]::GetFullPath($candidate.Path) -eq [IO.Path]::GetFullPath($core)) { $coreProcess = $candidate; $coreProcess.Refresh(); break }
            } catch { }
        }
    } while (($null -eq $coreProcess -or $coreProcess.MainWindowHandle -eq [IntPtr]::Zero) -and [DateTime]::UtcNow -lt $deadline)
    if ($null -eq $coreProcess -or $coreProcess.MainWindowHandle -eq [IntPtr]::Zero) { throw 'Authentic DPopCleaner window did not appear for Zapret UI smoke.' }

    $coreProcess.Refresh()
    $moduleBase = $coreProcess.MainModule.BaseAddress
    # Frozen 0.2.14 strategy enumerator reads its base path from RVA 0x64fe0,
    # then appends the literal general*.bat before FindFirstFileW.
    $legacyZapretRoot = [ZapretSmokeNative]::ReadUtf16($coreProcess.Id, [IntPtr]::Add($moduleBase, 0x64fe0), 1200)
    Write-Host "FROZEN_ZAPRET_ROOT_BUFFER=$legacyZapretRoot"
    if ([IO.Path]::GetFullPath($legacyZapretRoot).TrimEnd('\') -ne [IO.Path]::GetFullPath($zapretRoot).TrimEnd('\')) {
        throw "Frozen core Zapret root differs from packaged legacy directory. core=$legacyZapretRoot packaged=$zapretRoot"
    }

    $children = [ZapretSmokeNative]::Children($coreProcess.MainWindowHandle)
    $zapretButton = $children | Where-Object { $_.ClassName -eq 'Button' -and $_.Text -eq 'Zapret' } | Select-Object -First 1
    if (-not $zapretButton) { throw 'Authentic Zapret navigation button was not found.' }
    [void][ZapretSmokeNative]::SendMessage($zapretButton.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)

    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    $combo = $null
    do {
        Start-Sleep -Milliseconds 200
        $children = [ZapretSmokeNative]::Children($coreProcess.MainWindowHandle)
        $combos = @($children | Where-Object { $_.Visible -and $_.ClassName -eq 'ComboBox' })
        foreach ($candidate in $combos) {
            $count = [ZapretSmokeNative]::SendMessage($candidate.Handle, 0x0146, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
            if ($count -gt 0) { $combo = $candidate; $strategyCount = $count; break }
        }
    } while (-not $combo -and [DateTime]::UtcNow -lt $deadline)

    if (-not $combo) { throw 'Zapret Center did not expose a populated strategy ComboBox.' }
    $strategyEntries = @([ZapretSmokeNative]::ComboBoxItems($combo.Handle))
    $validStrategies = @($strategyEntries | Where-Object { $_ -match '(?i)^general.*\.bat$' })
    if ($validStrategies.Count -eq 0) {
        throw "Zapret strategy ComboBox is populated but exposes no general*.bat entries: $($strategyEntries -join ', ')"
    }

    $selectedStrategy = $combo.Text
    if (-not [string]::IsNullOrWhiteSpace($selectedStrategy)) {
        if ($selectedStrategy -match 'Стратегии не найдены|No strategies found') { throw "Old-core Zapret Center still reports missing strategies: $selectedStrategy" }
        if ($selectedStrategy -notmatch '(?i)^general.*\.bat$') { throw "Unexpected Zapret strategy selected by authentic UI: $selectedStrategy" }
        $verifiedStrategy = $selectedStrategy
    } else {
        $verifiedStrategy = $validStrategies[0]
    }

    $visibleText = ($children | Where-Object { $_.Visible } | ForEach-Object { $_.Text }) -join "`n"
    if ($visibleText -match 'Zapret components were not found beside DPopCleaner') { throw 'Authentic Zapret Center still reports missing bundled components.' }

    [pscustomobject]@{
        root = $RootPath
        zapret_root = $zapretRoot
        frozen_strategy_root = $legacyZapretRoot
        bundled_version = (Get-Content -Raw -LiteralPath (Join-Path $zapretRoot '.service\version.txt')).Trim()
        strategy_files = $strategyFiles.Count
        strategy_combo_count = $strategyCount
        strategy_entries = $strategyEntries
        selected_strategy = $selectedStrategy
        verified_strategy = $verifiedStrategy
        winws_present = (Test-Path -LiteralPath $winws -PathType Leaf)
        windivert_present = (Test-Path -LiteralPath $windivert -PathType Leaf)
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputDir 'zapret-ui-smoke-report.json') -Encoding utf8

    $selectedLabel = if ([string]::IsNullOrWhiteSpace($selectedStrategy)) { '<none>' } else { $selectedStrategy }
    Write-Host "AUTHENTIC_ZAPRET_UI_SMOKE_OK root=$zapretRoot verified=$verifiedStrategy selected=$selectedLabel combo_count=$strategyCount bundled_files=$($strategyFiles.Count)"
}
finally {
    if ($coreProcess -and -not $coreProcess.HasExited) { Stop-Process -Id $coreProcess.Id -Force -ErrorAction SilentlyContinue }
    if ($launcherProcess -and -not $launcherProcess.HasExited) { Stop-Process -Id $launcherProcess.Id -Force -ErrorAction SilentlyContinue }
}
