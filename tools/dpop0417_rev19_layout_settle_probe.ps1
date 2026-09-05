[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$RootPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev19-zapret-cleanup'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RootPath=if([IO.Path]::IsPathRooted($RootPath)){$RootPath}else{Join-Path $repoRoot $RootPath}
$OutputDir=if([IO.Path]::IsPathRooted($OutputDir)){$OutputDir}else{Join-Path $repoRoot $OutputDir}
$RootPath=[IO.Path]::GetFullPath($RootPath)
$OutputDir=[IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Path $OutputDir -Force|Out-Null

$launcherPath=Join-Path $RootPath 'DPopCleaner.exe'
$corePath=Join-Path $RootPath 'DPopCleaner.Core.exe'
foreach($required in @($launcherPath,$corePath)){if(-not(Test-Path -LiteralPath $required -PathType Leaf)){throw "rev.19 settle prerequisite missing: $required"}}

$native=@'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public sealed class Rev19SettleChild {
 public IntPtr Handle; public int Id; public string ClassName; public bool Visible; public int Left; public int Top; public int Right; public int Bottom;
}
public static class Rev19SettleNative {
 private delegate bool EnumProc(IntPtr hwnd,IntPtr p);
 [StructLayout(LayoutKind.Sequential)] private struct RECT { public int Left,Top,Right,Bottom; }
 [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent,EnumProc cb,IntPtr p);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd,StringBuilder text,int max);
 [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
 [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
 [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hwnd,out RECT r);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hwnd,IntPtr after,int x,int y,int cx,int cy,uint flags);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd,uint msg,IntPtr wp,IntPtr lp);
 public static Rev19SettleChild[] Children(IntPtr parent){
  var list=new List<Rev19SettleChild>(); EnumProc cb=delegate(IntPtr h,IntPtr _){RECT r;var c=new StringBuilder(128);GetClassName(h,c,c.Capacity);GetWindowRect(h,out r);list.Add(new Rev19SettleChild{Handle=h,Id=GetDlgCtrlID(h),ClassName=c.ToString(),Visible=IsWindowVisible(h),Left=r.Left,Top=r.Top,Right=r.Right,Bottom=r.Bottom});return true;};
  EnumChildWindows(parent,cb,IntPtr.Zero);GC.KeepAlive(cb);return list.ToArray();
 }
 public static Rev19SettleChild Bounds(IntPtr h){if(h==IntPtr.Zero)return null;RECT r;if(!GetWindowRect(h,out r))return null;return new Rev19SettleChild{Handle=h,Left=r.Left,Top=r.Top,Right=r.Right,Bottom=r.Bottom};}
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Get-Children([IntPtr]$Window){@([Rev19SettleNative]::Children($Window))}
function Wait-Until([int]$Seconds,[string]$Description,[scriptblock]$Condition){$deadline=[DateTime]::UtcNow.AddSeconds($Seconds);do{if(& $Condition){return};Start-Sleep -Milliseconds 120}while([DateTime]::UtcNow-lt$deadline);throw "Timed out waiting for $Description."}
function Get-CoreProcess([string]$ExpectedPath){$expected=[IO.Path]::GetFullPath($ExpectedPath);foreach($candidate in @(Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue)){try{$candidate.Refresh();if(-not$candidate.HasExited-and$candidate.MainWindowHandle-ne[IntPtr]::Zero-and$candidate.Path-and[IO.Path]::GetFullPath($candidate.Path).Equals($expected,[StringComparison]::OrdinalIgnoreCase)){return $candidate}}catch{}};$null}
function Get-RowTop($Buttons,[int[]]$Ids,[string]$Name){$row=@($Buttons|Where-Object{$Ids-contains$_.Id});if($row.Count-ne$Ids.Count){throw "$Name expected $($Ids.Count), found $($row.Count)."};$tops=@($row|Select-Object -ExpandProperty Top -Unique);if($tops.Count-ne1){throw "$Name is not one row: $($tops-join',')."};[int]$tops[0]}

$targetIds=@(1701,1702,1703,1704,1705,1707,1708,1710,1711,1713,1714,1716,1717,1720,1721,1722,1723,1724,1725)
$strategyIds=@(1701,1713,1714);$updateIds=@(1724,1725,1716,1717);$actionIds=@(1720,1721,1722,1723);$additionalIds=@(1704,1705,1707,1708);$serviceIds=@(1703,1702,1710,1711)
$settingsPath=Join-Path $OutputDir 'rev19-layout-settle-settings.ini';@('auto_update=0','tray_icon=1')|Set-Content -LiteralPath $settingsPath -Encoding ascii
$screenHeight=[Windows.Forms.Screen]::PrimaryScreen.Bounds.Height
$launcher=$null;$core=$null;$samples=@()
try{
 Add-Type -AssemblyName System.Windows.Forms
 $launcher=Start-Process -FilePath $launcherPath -ArgumentList @('--no-update-check','--settings-path',('"'+$settingsPath+'"')) -WorkingDirectory $RootPath -PassThru
 Wait-Until 18 'rev.19 settle core window' {$script:core=Get-CoreProcess $corePath;$null-ne$script:core}
 $core=$script:core;$window=$core.MainWindowHandle
 $tab=Get-Children $window|Where-Object{$_.Visible-and$_.ClassName-eq'Button'-and$_.Id-eq905}|Select-Object -First 1
 if(-not$tab){throw 'rev.19 settle Zapret tab missing.'};[void][Rev19SettleNative]::SendMessage($tab.Handle,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
 Wait-Until 8 'rev.19 settle target buttons' {$c=@(Get-Children $window);@($c|Where-Object{$_.Visible-and$_.ClassName-eq'Button'-and$targetIds-contains$_.Id}).Count-eq19}
 foreach($size in @(@(1024,768),@(1366,800),@(1680,840),@(1908,950))){$w=[int]$size[0];$h=[int]$size[1];$y=if($h-gt$screenHeight){$screenHeight-$h-8}else{0};if(-not[Rev19SettleNative]::SetWindowPos($window,[IntPtr]::Zero,0,$y,$w,$h,0x0002-bor0x0004-bor0x0010-bor0x0400)){throw "settle resize ${w}x${h} failed"};Start-Sleep -Milliseconds 1100}
 for($i=0;$i-lt16;$i++){
  $wb=[Rev19SettleNative]::Bounds($window);$children=@(Get-Children $window);$buttons=@($children|Where-Object{$_.Visible-and$_.ClassName-eq'Button'-and$targetIds-contains$_.Id});if($buttons.Count-ne19){throw "settle sample $i expected 19 buttons, found $($buttons.Count)"}
  $strategyTop=Get-RowTop $buttons $strategyIds 'strategy';$updateTop=Get-RowTop $buttons $updateIds 'update';$actionTop=Get-RowTop $buttons $actionIds 'action';$additionalTop=Get-RowTop $buttons $additionalIds 'additional';$serviceTop=Get-RowTop $buttons $serviceIds 'service';$service=@($buttons|Where-Object{$serviceIds-contains$_.Id});$serviceBottom=($service|Measure-Object Bottom -Maximum).Maximum;$unusedBottom=$wb.Bottom-$serviceBottom
  $sample=[pscustomobject]@{index=$i;strategy_top=$strategyTop;update_top=$updateTop;action_top=$actionTop;additional_top=$additionalTop;service_top=$serviceTop;service_bottom=$serviceBottom;unused_bottom=$unusedBottom};$samples+=$sample
  Write-Host "REV19_LAYOUT_SETTLE_SAMPLE index=$i rows=$strategyTop/$updateTop/$actionTop/$additionalTop/$serviceTop serviceBottom=$serviceBottom unusedBottom=$unusedBottom"
  Start-Sleep -Milliseconds 250
 }
 $samples|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $OutputDir 'rev19-layout-settle-probe.json') -Encoding utf8
 Write-Host 'REV19_LAYOUT_SETTLE_PROBE_OK'
}
finally{
 foreach($p in @($launcher,$core)){if($p){try{if(-not$p.HasExited){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue}}catch{}}}
}
