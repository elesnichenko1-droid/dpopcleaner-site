[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InstallerPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev16-zapret-functional'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$InstallerPath = if ([IO.Path]::IsPathRooted($InstallerPath)) { $InstallerPath } else { Join-Path $repoRoot $InstallerPath }
$InstallerPath = [IO.Path]::GetFullPath($InstallerPath)
$OutputDir = if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repoRoot $OutputDir }
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
if (-not (Test-Path -LiteralPath $InstallerPath -PathType Leaf)) { throw "rev.16 Zapret installer missing: $InstallerPath" }

$installRoot = Join-Path ([IO.Path]::GetTempPath()) ('dpop0417-rev16-zapret-' + [Guid]::NewGuid().ToString('N'))
$launcherPath = Join-Path $installRoot 'DPopCleaner.exe'
$corePath = Join-Path $installRoot 'DPopCleaner.Core.exe'
$winwsPath = Join-Path $installRoot 'Zapret\bin\winws.exe'
$serviceBat = Join-Path $installRoot 'Zapret\service.bat'
$settingsPath = Join-Path $installRoot 'SimpleUpdate-rev16-zapret-smoke.ini'

$native = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class Rev16ZapretChild {
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

public static class Rev16ZapretNative {
    private delegate bool EnumProc(IntPtr hwnd, IntPtr p);
    [StructLayout(LayoutKind.Sequential)] private struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc proc, IntPtr p);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int max);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder text, int max);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] private static extern IntPtr SendMessageText(IntPtr hwnd, uint msg, IntPtr wp, StringBuilder text);

    public static Rev16ZapretChild[] Children(IntPtr parent) {
        var result = new List<Rev16ZapretChild>();
        EnumProc cb = delegate(IntPtr h, IntPtr _) {
            var text = new StringBuilder(1024); var cls = new StringBuilder(128); RECT rect;
            GetWindowText(h,text,text.Capacity); GetClassName(h,cls,cls.Capacity); GetWindowRect(h,out rect);
            result.Add(new Rev16ZapretChild { Handle=h, Id=GetDlgCtrlID(h), Text=text.ToString(), ClassName=cls.ToString(), Visible=IsWindowVisible(h), Left=rect.Left, Top=rect.Top, Right=rect.Right, Bottom=rect.Bottom });
            return true;
        };
        EnumChildWindows(parent,cb,IntPtr.Zero); GC.KeepAlive(cb); return result.ToArray();
    }

    public static string[] ComboItems(IntPtr combo) {
        const uint CB_GETCOUNT=0x0146, CB_GETLBTEXT=0x0148, CB_GETLBTEXTLEN=0x0149;
        int count=SendMessage(combo,CB_GETCOUNT,IntPtr.Zero,IntPtr.Zero).ToInt32();
        var values=new List<string>();
        for(int i=0;i<count;i++) {
            int len=SendMessage(combo,CB_GETLBTEXTLEN,(IntPtr)i,IntPtr.Zero).ToInt32();
            if(len<0) continue;
            var text=new StringBuilder(len+1); SendMessageText(combo,CB_GETLBTEXT,(IntPtr)i,text); values.Add(text.ToString());
        }
        return values.ToArray();
    }

    public static void SelectCombo(IntPtr parent, IntPtr combo, int index) {
        const uint CB_SETCURSEL=0x014E, WM_COMMAND=0x0111; const int CBN_SELCHANGE=1;
        if(SendMessage(combo,CB_SETCURSEL,(IntPtr)index,IntPtr.Zero).ToInt32()<0) throw new InvalidOperationException("CB_SETCURSEL failed");
        int id=GetDlgCtrlID(combo); int wp=(CBN_SELCHANGE<<16)|(id&0xffff);
        SendMessage(parent,WM_COMMAND,(IntPtr)wp,combo);
    }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Get-ExactProcess([string]$Name, [string]$ExactPath) {
    foreach ($candidate in @(Get-Process -Name $Name -ErrorAction SilentlyContinue)) {
        try {
            if ($candidate.Path -and [IO.Path]::GetFullPath($candidate.Path) -eq [IO.Path]::GetFullPath($ExactPath)) { return $candidate }
        } catch { }
    }
    return $null
}

function Wait-CoreWindow([int]$Seconds = 15) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    do {
        Start-Sleep -Milliseconds 150
        $candidate = Get-ExactProcess -Name 'DPopCleaner.Core' -ExactPath $corePath
        if ($candidate) {
            $candidate.Refresh()
            if (-not $candidate.HasExited -and $candidate.MainWindowHandle -ne [IntPtr]::Zero) { return $candidate }
        }
    } while ([DateTime]::UtcNow -lt $deadline)
    return $null
}

function Get-Children([IntPtr]$Window) { @([Rev16ZapretNative]::Children($Window)) }

function Click-Id([IntPtr]$Window, [int]$Id) {
    $control = Get-Children $Window | Where-Object { $_.Visible -and $_.Id -eq $Id -and $_.ClassName -eq 'Button' } | Select-Object -First 1
    if (-not $control) { throw "Zapret control id=$Id is missing or hidden." }
    if (-not [Rev16ZapretNative]::PostMessage($control.Handle,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)) { throw "Could not queue Zapret click id=$Id." }
}

function Wait-ZapretPage([IntPtr]$Window) {
    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    do {
        Start-Sleep -Milliseconds 150
        $children = Get-Children $Window
        $marker = $children | Where-Object { $_.Visible -and $_.Id -eq 1701 } | Select-Object -First 1
    } while (-not $marker -and [DateTime]::UtcNow -lt $deadline)
    if (-not $marker) { throw 'Zapret page did not expose lifecycle controls.' }
    return $children
}

function Get-StrategyCombo([IntPtr]$Window) {
    @(Get-Children $Window | Where-Object { $_.Visible -and $_.ClassName -eq 'ComboBox' } | Sort-Object Top | Select-Object -First 1)[0]
}

function Get-StatusSnapshot([IntPtr]$Window) {
    $edits = @(Get-Children $Window | Where-Object { $_.Visible -and $_.ClassName -eq 'Edit' } | Sort-Object Top | Select-Object -First 2)
    [pscustomobject]@{
        Upper = if ($edits.Count -ge 1) { $edits[0].Text } else { '<missing>' }
        Runtime = if ($edits.Count -ge 2) { $edits[1].Text } else { '<missing>' }
    }
}

function Get-ZapretService {
    Get-CimInstance Win32_Service -Filter "Name='zapret'" -ErrorAction SilentlyContinue
}

function Get-WinDivertServices {
    @(Get-CimInstance Win32_SystemDriver -ErrorAction SilentlyContinue | Where-Object { $_.Name -in @('WinDivert','WinDivert14') })
}

function Get-BundledWinws {
    $expected = [IO.Path]::GetFullPath($winwsPath)
    @(Get-CimInstance Win32_Process -Filter "Name='winws.exe'" -ErrorAction SilentlyContinue | Where-Object {
        $_.ExecutablePath -and ([IO.Path]::GetFullPath($_.ExecutablePath) -eq $expected)
    })
}

function Wait-Until([scriptblock]$Condition, [int]$Seconds, [string]$Description) {
    $deadline=[DateTime]::UtcNow.AddSeconds($Seconds)
    do {
        if (& $Condition) { return }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for $Description"
}

function Stop-ZapretResidue {
    foreach ($process in @(Get-BundledWinws)) { Stop-Process -Id $process.ProcessId -Force -ErrorAction SilentlyContinue }
    foreach ($name in @('zapret','WinDivert','WinDivert14')) {
        & sc.exe stop $name *> $null
        & sc.exe delete $name *> $null
    }
    foreach ($cmd in @(Get-CimInstance Win32_Process -Filter "Name='cmd.exe'" -ErrorAction SilentlyContinue)) {
        try {
            if ($cmd.CommandLine -and ($cmd.CommandLine -like "*$installRoot*" -or $cmd.CommandLine -like '*service.bat*')) {
                Stop-Process -Id $cmd.ProcessId -Force -ErrorAction SilentlyContinue
            }
        } catch { }
    }
}

$launcherStub=$null
$core=$null
$phase='install'
$firstStrategy=''
$secondStrategy=''
$firstCommand=''
$secondCommand=''
$lastStatus=$null
$failure=''
$installed=$false
try {
    $install = Start-Process -FilePath $InstallerPath -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',"/DIR=$installRoot") -Wait -PassThru
    if ($install.ExitCode -ne 0) { throw "rev.16 Zapret silent install failed: $($install.ExitCode)" }
    $installed=$true
    foreach ($required in @($launcherPath,$corePath,$winwsPath,$serviceBat)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Installed Zapret prerequisite missing: $required" }
    }
    $version = (Get-Content -Raw -LiteralPath (Join-Path $installRoot 'Zapret\.service\version.txt')).Trim()
    if ($version -ne '1.10.2') { throw "Installed Zapret version mismatch: $version" }
    $strategies = @(Get-ChildItem -LiteralPath (Join-Path $installRoot 'Zapret') -Filter 'general*.bat' -File | Sort-Object Name)
    if ($strategies.Count -ne 22) { throw "Expected 22 bundled strategies, found $($strategies.Count)." }

    Stop-ZapretResidue
    $launcherStub = Start-Process -FilePath $launcherPath -ArgumentList @('--no-update-check','--settings-path',('"'+$settingsPath+'"')) -WorkingDirectory $installRoot -PassThru
    $core = Wait-CoreWindow
    if (-not $core) { throw 'Installed frozen core window did not appear.' }
    $window=$core.MainWindowHandle
    Click-Id -Window $window -Id 905
    $children=Wait-ZapretPage -Window $window
    $combo=Get-StrategyCombo -Window $window
    if (-not $combo) { throw 'Strategy ComboBox not found.' }
    $items=@([Rev16ZapretNative]::ComboItems($combo.Handle))
    $strategyIndexes=@()
    for($i=0;$i -lt $items.Count;$i++){ if($items[$i] -match '(?i)^general.*\.bat$'){ $strategyIndexes += $i } }
    if($strategyIndexes.Count -lt 2){ throw "Need at least two real strategies; combo=$($items -join ', ')" }

    $firstIndex=$strategyIndexes[0]
    $secondIndex=$strategyIndexes | Where-Object { $items[$_] -ne $items[$firstIndex] } | Select-Object -First 1
    $firstStrategy=$items[$firstIndex]
    $secondStrategy=$items[$secondIndex]
    [Rev16ZapretNative]::SelectCombo($window,$combo.Handle,$firstIndex)

    $phase='install-service'
    Click-Id -Window $window -Id 1701
    Wait-Until -Seconds 12 -Description 'zapret service to exist and run after Install Service UI action' -Condition {
        $service=Get-ZapretService
        $service -and $service.State -eq 'Running'
    }
    $service=Get-ZapretService
    if (-not $service -or $service.State -ne 'Running') { throw 'Install Service UI action did not create a running zapret service.' }
    Write-Host "REV16_ZAPRET_INSTALL_OK strategy=$firstStrategy state=$($service.State)"

    $phase='service-status'
    Click-Id -Window $window -Id 1703
    Start-Sleep -Milliseconds 800
    $lastStatus=Get-StatusSnapshot -Window $window
    if ($lastStatus.Runtime -notmatch '(?i)ON|RUNNING') { throw "Status UI does not reflect running service: upper='$($lastStatus.Upper)' runtime='$($lastStatus.Runtime)'" }

    $phase='remove-before-standalone'
    Click-Id -Window $window -Id 1702
    Wait-Until -Seconds 12 -Description 'zapret service removal' -Condition { -not (Get-ZapretService) }

    $phase='standalone-start'
    Click-Id -Window $window -Id 1713
    Wait-Until -Seconds 10 -Description 'bundled winws.exe start' -Condition { @(Get-BundledWinws).Count -ge 1 }
    $firstWinws=@(Get-BundledWinws) | Select-Object -First 1
    $firstCommand=[string]$firstWinws.CommandLine
    if ([string]::IsNullOrWhiteSpace($firstCommand)) { throw 'Bundled winws.exe started but command line is empty.' }
    Click-Id -Window $window -Id 1703
    Start-Sleep -Milliseconds 500
    $lastStatus=Get-StatusSnapshot -Window $window
    if ($lastStatus.Runtime -notmatch '(?i)ON|RUNNING') { throw "Runtime status did not become ON after start: '$($lastStatus.Runtime)'" }
    Write-Host "REV16_ZAPRET_START_OK strategy=$firstStrategy command=$firstCommand"

    $phase='standalone-stop'
    Click-Id -Window $window -Id 1714
    Wait-Until -Seconds 10 -Description 'bundled winws.exe stop' -Condition { @(Get-BundledWinws).Count -eq 0 }
    Click-Id -Window $window -Id 1703
    Start-Sleep -Milliseconds 500
    $lastStatus=Get-StatusSnapshot -Window $window
    if ($lastStatus.Runtime -notmatch '(?i)OFF|NOT running|STOP') { throw "Runtime status did not become OFF after stop: '$($lastStatus.Runtime)'" }
    Write-Host 'REV16_ZAPRET_STOP_OK'

    $phase='strategy-change'
    [Rev16ZapretNative]::SelectCombo($window,$combo.Handle,[int]$secondIndex)
    Click-Id -Window $window -Id 1713
    Wait-Until -Seconds 10 -Description 'bundled winws.exe start with second strategy' -Condition { @(Get-BundledWinws).Count -ge 1 }
    $secondWinws=@(Get-BundledWinws) | Select-Object -First 1
    $secondCommand=[string]$secondWinws.CommandLine
    if ([string]::IsNullOrWhiteSpace($secondCommand)) { throw 'Second strategy winws.exe command line is empty.' }
    if ($secondCommand -eq $firstCommand) { throw "Changing strategy changed only ComboBox selection; winws command line stayed identical: $secondCommand" }
    Write-Host "REV16_ZAPRET_STRATEGY_CHANGE_OK first=$firstStrategy second=$secondStrategy"

    $phase='final-remove'
    Click-Id -Window $window -Id 1714
    Wait-Until -Seconds 10 -Description 'second bundled winws.exe stop' -Condition { @(Get-BundledWinws).Count -eq 0 }
    Click-Id -Window $window -Id 1702
    Wait-Until -Seconds 10 -Description 'final zapret service absence' -Condition { -not (Get-ZapretService) }
    if (@(Get-BundledWinws).Count -ne 0) { throw 'Bundled winws.exe remains after Remove Services.' }
    Write-Host 'REV16_ZAPRET_REMOVE_OK'
}
catch {
    $failure=$_.Exception.Message
    Write-Host "REV16_ZAPRET_RED_PHASE=$phase"
    Write-Host "REV16_ZAPRET_RED_DETAIL=$failure"
    throw
}
finally {
    try {
        if ($core -and -not $core.HasExited) {
            $lastStatus=Get-StatusSnapshot -Window $core.MainWindowHandle
        }
    } catch { }
    $serviceState=$null
    try { $serviceState=Get-ZapretService } catch { }
    $winwsSnapshot=@()
    try { $winwsSnapshot=@(Get-BundledWinws | ForEach-Object { [pscustomobject]@{ ProcessId=$_.ProcessId; ExecutablePath=$_.ExecutablePath; CommandLine=$_.CommandLine } }) } catch { }
    $divertSnapshot=@()
    try { $divertSnapshot=@(Get-WinDivertServices | ForEach-Object { [pscustomobject]@{ Name=$_.Name; State=$_.State } }) } catch { }

    [pscustomobject]@{
        phase=$phase
        failure=$failure
        install_root=$installRoot
        first_strategy=$firstStrategy
        second_strategy=$secondStrategy
        first_command_line=$firstCommand
        second_command_line=$secondCommand
        upper_status=if($lastStatus){$lastStatus.Upper}else{''}
        runtime_status=if($lastStatus){$lastStatus.Runtime}else{''}
        zapret_service=if($serviceState){[pscustomobject]@{Name=$serviceState.Name;State=$serviceState.State;PathName=$serviceState.PathName}}else{$null}
        winws=$winwsSnapshot
        windivert=$divertSnapshot
    } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDir 'rev16-zapret-functional-report.json') -Encoding utf8

    try { Stop-ZapretResidue } catch { }
    foreach ($name in @('DPopCleaner','DPopCleaner.Core','SimpleUpdate')) {
        foreach ($process in @(Get-Process -Name $name -ErrorAction SilentlyContinue)) {
            try {
                if ($process.Path -and [IO.Path]::GetFullPath($process.Path).StartsWith([IO.Path]::GetFullPath($installRoot),[StringComparison]::OrdinalIgnoreCase)) {
                    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
                }
            } catch { }
        }
    }
    if ($installed) {
        $uninstaller=Join-Path $installRoot 'unins000.exe'
        if (Test-Path -LiteralPath $uninstaller -PathType Leaf) { try { Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait | Out-Null } catch { } }
        Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

