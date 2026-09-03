[CmdletBinding()]
param(
    [string]$InstallerPath,
    [string]$RootPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev16-zapret-functional'
)

$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($InstallerPath) -eq [string]::IsNullOrWhiteSpace($RootPath)) {
    throw 'Pass exactly one of -InstallerPath or -RootPath.'
}

$native=@'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;

public sealed class Rev16ZapretChild {
    public IntPtr Handle; public int Id; public string Text; public string ClassName; public bool Visible; public int Left; public int Top; public int Right; public int Bottom;
}
public static class Rev16ZapretNative {
    private delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
    [StructLayout(LayoutKind.Sequential)] private struct RECT { public int Left,Top,Right,Bottom; }
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc proc, IntPtr lParam);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd,StringBuilder text,int max);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd,StringBuilder text,int max);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hwnd,out RECT rect);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd,uint msg,IntPtr wp,IntPtr lp);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd,uint msg,IntPtr wp,IntPtr lp);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern IntPtr SendMessage(IntPtr hwnd,uint msg,IntPtr wp,StringBuilder lp);
    [DllImport("user32.dll")] private static extern IntPtr GetParent(IntPtr hwnd);

    public static Rev16ZapretChild[] Children(IntPtr parent) {
        var result=new List<Rev16ZapretChild>();
        EnumProc cb=delegate(IntPtr h,IntPtr _) {
            var t=new StringBuilder(1024); var c=new StringBuilder(128); RECT r;
            GetWindowText(h,t,t.Capacity); GetClassName(h,c,c.Capacity); GetWindowRect(h,out r);
            result.Add(new Rev16ZapretChild{Handle=h,Id=GetDlgCtrlID(h),Text=t.ToString(),ClassName=c.ToString(),Visible=IsWindowVisible(h),Left=r.Left,Top=r.Top,Right=r.Right,Bottom=r.Bottom});
            return true;
        };
        EnumChildWindows(parent,cb,IntPtr.Zero); GC.KeepAlive(cb); return result.ToArray();
    }
    public static string[] ComboItems(IntPtr combo) {
        const uint GETCOUNT=0x0146,GETTEXT=0x0148,GETLEN=0x0149;
        var values=new List<string>(); int count=SendMessage(combo,GETCOUNT,IntPtr.Zero,IntPtr.Zero).ToInt32();
        for(int i=0;i<count;i++){int len=SendMessage(combo,GETLEN,(IntPtr)i,IntPtr.Zero).ToInt32(); var b=new StringBuilder(Math.Max(1,len+1)); SendMessage(combo,GETTEXT,(IntPtr)i,b); values.Add(b.ToString());}
        return values.ToArray();
    }
    public static void SelectCombo(IntPtr main,IntPtr combo,int index) {
        const uint CB_SETCURSEL=0x014E,WM_COMMAND=0x0111; const int CBN_SELCHANGE=1;
        SendMessage(combo,CB_SETCURSEL,(IntPtr)index,IntPtr.Zero);
        var parent=GetParent(combo); if(parent==IntPtr.Zero) parent=main;
        long wp=((long)CBN_SELCHANGE<<16)|(uint)(ushort)GetDlgCtrlID(combo);
        SendMessage(parent,WM_COMMAND,(IntPtr)wp,combo);
    }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Get-Children([IntPtr]$Window){ @([Rev16ZapretNative]::Children($Window)) }
function Click-Id([IntPtr]$Window,[int]$Id) {
    $control=Get-Children $Window | Where-Object { $_.Visible -and $_.Id -eq $Id -and $_.ClassName -eq 'Button' } | Select-Object -First 1
    if(-not $control){ throw "Visible button id=$Id not found." }
    if (-not [Rev16ZapretNative]::PostMessage($control.Handle,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)) { throw "Could not queue Zapret click id=$Id." }
    Start-Sleep -Milliseconds 250
}
function Wait-Until([int]$Seconds,[string]$Description,[scriptblock]$Condition) {
    $deadline=[DateTime]::UtcNow.AddSeconds($Seconds)
    do { if(& $Condition){ return }; Start-Sleep -Milliseconds 200 } while([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for $Description."
}
function Wait-CoreWindow {
    $deadline=[DateTime]::UtcNow.AddSeconds(15)
    do {
        $candidate=Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne [IntPtr]::Zero } | Select-Object -First 1
        if($candidate){ return $candidate }
        Start-Sleep -Milliseconds 150
    } while([DateTime]::UtcNow -lt $deadline)
    return $null
}
function Wait-ZapretPage([IntPtr]$Window) {
    $deadline=[DateTime]::UtcNow.AddSeconds(8)
    do {
        $children=Get-Children $Window
        if($children | Where-Object { $_.Visible -and $_.Id -eq 1703 }){ return $children }
        Start-Sleep -Milliseconds 120
    } while([DateTime]::UtcNow -lt $deadline)
    throw 'Zapret page did not become visible.'
}
function Get-StrategyCombo([IntPtr]$Window) {
    $strategyCombos=@()
    foreach($combo in @(Get-Children $Window | Where-Object { $_.Visible -and $_.ClassName -eq 'ComboBox' })) {
        $items=@([Rev16ZapretNative]::ComboItems($combo.Handle))
        $strategyCount=@($items | Where-Object { $_ -match '(?i)^general.*\.bat$' }).Count
        if($strategyCount -ge 2) {
            $strategyCombos += [pscustomobject]@{ Combo=$combo; StrategyCount=$strategyCount }
        }
    }
    if($strategyCombos.Count -eq 0){ return $null }
    ($strategyCombos | Sort-Object StrategyCount -Descending | Select-Object -First 1).Combo
}
function Get-StatusSnapshot([IntPtr]$Window) {
    $edits=@(Get-Children $Window | Where-Object { $_.Visible -and $_.ClassName -eq 'Edit' } | Sort-Object Top | Select-Object -First 2)
    [pscustomobject]@{ Upper=if($edits.Count -gt 0){$edits[0].Text}else{''}; Runtime=if($edits.Count -gt 1){$edits[1].Text}else{''} }
}
function Get-LauncherDialogText($Launcher) {
    try {
        if (-not $Launcher) { return $null }
        $Launcher.Refresh()
        if ($Launcher.HasExited -or $Launcher.MainWindowHandle -eq [IntPtr]::Zero) { return $null }
        $title=[string]$Launcher.MainWindowTitle
        if ($title -notmatch '(?i)Zapret') { return $null }
        $parts=@(Get-Children $Launcher.MainWindowHandle | Where-Object { $_.Visible -and $_.ClassName -eq 'Static' -and -not [string]::IsNullOrWhiteSpace($_.Text) } | ForEach-Object { $_.Text.Trim() })
        if ($parts.Count -gt 0) { return ($title + ' :: ' + ($parts -join ' | ')) }
        return $title
    } catch { return $null }
}

$script:WinwsPath=''
function Get-BundledWinws {
    if([string]::IsNullOrWhiteSpace($script:WinwsPath)){ return @() }
    $expected=[IO.Path]::GetFullPath($script:WinwsPath)
    @(Get-CimInstance Win32_Process -Filter "Name='winws.exe'" -ErrorAction SilentlyContinue | Where-Object {
        $_.ExecutablePath -and [IO.Path]::GetFullPath([string]$_.ExecutablePath).Equals($expected,[StringComparison]::OrdinalIgnoreCase)
    })
}
function Get-ZapretService { Get-CimInstance Win32_Service -Filter "Name='zapret'" -ErrorAction SilentlyContinue }
function Get-WinDivertServices { @(Get-CimInstance Win32_SystemDriver -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^WinDivert' }) }
function Stop-ZapretResidue {
    Get-BundledWinws | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    foreach($name in @('zapret','WinDivert','WinDivert14')) {
        & sc.exe stop $name *> $null
        & sc.exe delete $name *> $null
    }
    Start-Sleep -Milliseconds 500
}

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$installRoot=$null; $installed=$false; $launcherStub=$null; $core=$null
$phase='setup'; $failure=$null; $firstStrategy=$null; $secondStrategy=$null; $firstCommand=$null; $secondCommand=$null; $lastStatus=$null
try {
    if($InstallerPath) {
        $installer=(Resolve-Path -LiteralPath $InstallerPath).Path
        $installRoot=Join-Path ([IO.Path]::GetTempPath()) 'dpop0417-rev16-zapret-functional'
        Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue
        $args=@('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/DIR="'+$installRoot+'"'))
        $setup=Start-Process -FilePath $installer -ArgumentList $args -PassThru -Wait
        if($setup.ExitCode -ne 0){ throw "Installer exit code $($setup.ExitCode)." }
        $installed=$true
    } else {
        $installRoot=(Resolve-Path -LiteralPath $RootPath).Path
    }

    $launcherPath=Join-Path $installRoot 'SimpleUpdate.exe'
    $corePath=Join-Path $installRoot 'DPopCleaner.Core.exe'
    if(-not (Test-Path -LiteralPath $corePath -PathType Leaf)){ $corePath=Join-Path $installRoot 'DPopCleaner.exe' }
    $script:WinwsPath=Join-Path $installRoot 'Zapret\bin\winws.exe'
    foreach($required in @($launcherPath,$corePath,$script:WinwsPath,(Join-Path $installRoot 'Zapret\service.bat'))){ if(-not(Test-Path -LiteralPath $required -PathType Leaf)){ throw "Required file missing: $required" } }

    $settingsPath=Join-Path $installRoot 'rev16-zapret-settings.json'
    $version=(Get-Content -Raw -LiteralPath (Join-Path $installRoot 'Zapret\.service\version.txt')).Trim()
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
    $installDeadline=[DateTime]::UtcNow.AddSeconds(20)
    do {
        $service=Get-ZapretService
        if ($service -and $service.State -eq 'Running') { break }
        $dialog=Get-LauncherDialogText $launcherStub
        if ($dialog) { throw "Install Service UI error: $dialog" }
        Start-Sleep -Milliseconds 200
    } while([DateTime]::UtcNow -lt $installDeadline)
    $service=Get-ZapretService
    if (-not $service -or $service.State -ne 'Running') {
        $managerFiles=@(Get-ChildItem -LiteralPath (Join-Path $installRoot 'Zapret') -Filter 'service-dpop-install-*.bat' -File -ErrorAction SilentlyContinue | ForEach-Object { $_.Name })
        $managerCmds=@(Get-CimInstance Win32_Process -Filter "Name='cmd.exe'" -ErrorAction SilentlyContinue | Where-Object { $_.CommandLine -and $_.CommandLine -like ('*' + $installRoot + '*') } | ForEach-Object { $_.CommandLine })
        throw "Timed out waiting for zapret service after production timeout; managers=$($managerFiles -join ','); cmd=$($managerCmds -join ' || ')"
    }
    Write-Host "REV16_ZAPRET_INSTALL_OK strategy=$firstStrategy state=$($service.State)"

    $phase='service-status'
    Click-Id -Window $window -Id 1703
    Start-Sleep -Milliseconds 800
    $lastStatus=Get-StatusSnapshot -Window $window
    if ($lastStatus.Upper -notmatch '(?i)ON|RUNNING') { throw "Status UI does not reflect running service: upper='$($lastStatus.Upper)' runtime='$($lastStatus.Runtime)'" }

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
    if ($lastStatus.Upper -notmatch '(?i)ON|RUNNING') { throw "Runtime status did not become ON after start: '$($lastStatus.Upper)'" }
    Write-Host "REV16_ZAPRET_START_OK strategy=$firstStrategy command=$firstCommand"

    $phase='standalone-stop'
    Click-Id -Window $window -Id 1714
    Wait-Until -Seconds 10 -Description 'bundled winws.exe stop' -Condition { @(Get-BundledWinws).Count -eq 0 }
    Click-Id -Window $window -Id 1703
    Start-Sleep -Milliseconds 500
    $lastStatus=Get-StatusSnapshot -Window $window
    if ($lastStatus.Upper -notmatch '(?i)OFF|NOT running|STOP') { throw "Runtime status did not become OFF after stop: '$($lastStatus.Upper)'" }
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

Write-Host 'REV16_ZAPRET_FUNCTIONAL_SMOKE_OK'
exit 0
