[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RootPath,
    [string]$OutputDir='_release/0.4.17/evidence/rev19-zapret-cleanup'
)

$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.Drawing
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RootPath=if([IO.Path]::IsPathRooted($RootPath)){$RootPath}else{Join-Path $repoRoot $RootPath}
$OutputDir=if([IO.Path]::IsPathRooted($OutputDir)){$OutputDir}else{Join-Path $repoRoot $OutputDir}
$RootPath=[IO.Path]::GetFullPath($RootPath);$OutputDir=[IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Path $OutputDir -Force|Out-Null
$launcherPath=Join-Path $RootPath 'DPopCleaner.exe';$corePath=Join-Path $RootPath 'DPopCleaner.Core.exe'

$native=@'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public sealed class R19CaptureItem { public long Hwnd; public int Id; public string ClassName; public bool Visible; public int Left,Top,Right,Bottom; public int OwnerPid; }
public sealed class R19CaptureRect { public int Left,Top,Right,Bottom; }
public static class R19CaptureNative {
 private delegate bool EnumProc(IntPtr h,IntPtr p);
 [StructLayout(LayoutKind.Sequential)] private struct RECT{public int L,T,R,B;}
 [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr p,EnumProc cb,IntPtr x);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr h,StringBuilder b,int n);
 [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr h);
 [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr h,out RECT r);
 [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr a,int x,int y,int w,int z,uint f);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
 static string Cls(IntPtr h){var b=new StringBuilder(128);GetClassName(h,b,b.Capacity);return b.ToString();}
 public static R19CaptureRect Bounds(IntPtr h){RECT r;if(!GetWindowRect(h,out r))return null;return new R19CaptureRect{Left=r.L,Top=r.T,Right=r.R,Bottom=r.B};}
 public static R19CaptureItem[] Probe(IntPtr root,int id){var list=new List<R19CaptureItem>();EnumProc cb=delegate(IntPtr h,IntPtr _){if(GetDlgCtrlID(h)!=id)return true;RECT r;if(!GetWindowRect(h,out r))return true;uint pid=0;GetWindowThreadProcessId(h,out pid);list.Add(new R19CaptureItem{Hwnd=h.ToInt64(),Id=id,ClassName=Cls(h),Visible=IsWindowVisible(h),Left=r.L,Top=r.T,Right=r.R,Bottom=r.B,OwnerPid=(int)pid});return true;};EnumChildWindows(root,cb,IntPtr.Zero);GC.KeepAlive(cb);return list.ToArray();}
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Wait-Until([int]$Seconds,[string]$What,[scriptblock]$Condition){$d=[DateTime]::UtcNow.AddSeconds($Seconds);do{if(& $Condition){return};Start-Sleep -Milliseconds 120}while([DateTime]::UtcNow-lt$d);throw "Timed out waiting for $What"}
function Get-Core { foreach($p in @(Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue)){try{if($p.Path -and [IO.Path]::GetFullPath($p.Path).Equals([IO.Path]::GetFullPath($corePath),[StringComparison]::OrdinalIgnoreCase)-and$p.MainWindowHandle-ne[IntPtr]::Zero){return $p}}catch{}};$null }
function Capture-Print([IntPtr]$Window,[string]$Path){$r=[R19CaptureNative]::Bounds($Window);if(-not$r){throw 'window bounds missing'};$w=[Math]::Max(1,$r.Right-$r.Left);$h=[Math]::Max(1,$r.Bottom-$r.Top);$bmp=[Drawing.Bitmap]::new($w,$h);$g=[Drawing.Graphics]::FromImage($bmp);$dc=$g.GetHdc();try{[void][R19CaptureNative]::PrintWindow($Window,$dc,2)}finally{$g.ReleaseHdc($dc);$g.Dispose()};try{$bmp.Save($Path,[Drawing.Imaging.ImageFormat]::Png)}finally{$bmp.Dispose()}}
function Capture-Screen([IntPtr]$Window,[string]$Path){$r=[R19CaptureNative]::Bounds($Window);if(-not$r){throw 'window bounds missing'};$w=[Math]::Max(1,$r.Right-$r.Left);$h=[Math]::Max(1,$r.Bottom-$r.Top);$bmp=[Drawing.Bitmap]::new($w,$h);$g=[Drawing.Graphics]::FromImage($bmp);try{$g.CopyFromScreen($r.Left,$r.Top,0,0,[Drawing.Size]::new($w,$h))}finally{$g.Dispose()};try{$bmp.Save($Path,[Drawing.Imaging.ImageFormat]::Png)}finally{$bmp.Dispose()}}
function Get-CropColorCount([string]$Path,$Item,$RootBounds){$bmp=[Drawing.Bitmap]::new($Path);$colors=[Collections.Generic.HashSet[int]]::new();try{$l=[Math]::Max(0,$Item.Left-$RootBounds.Left);$t=[Math]::Max(0,$Item.Top-$RootBounds.Top);$r=[Math]::Min($bmp.Width,$Item.Right-$RootBounds.Left);$b=[Math]::Min($bmp.Height,$Item.Bottom-$RootBounds.Top);for($y=$t;$y-lt$b;$y++){for($x=$l;$x-lt$r;$x++){[void]$colors.Add($bmp.GetPixel($x,$y).ToArgb())}}}finally{$bmp.Dispose()};$colors.Count}
function Get-ColorCount([string]$Path){$bmp=[Drawing.Bitmap]::new($Path);$colors=[Collections.Generic.HashSet[int]]::new();try{for($y=0;$y-lt$bmp.Height;$y++){for($x=0;$x-lt$bmp.Width;$x++){[void]$colors.Add($bmp.GetPixel($x,$y).ToArgb())}}}finally{$bmp.Dispose()};$colors.Count}

$launcher=$null;$core=$null
try{
 $settings=Join-Path $OutputDir 'rev19-1702-1024-settings.ini';@('auto_update=0','tray_icon=1')|Set-Content -LiteralPath $settings -Encoding ascii
 $launcher=Start-Process -FilePath $launcherPath -ArgumentList @('--no-update-check','--settings-path',('"'+$settings+'"')) -WorkingDirectory $RootPath -PassThru
 Wait-Until 18 'core' {$script:core=Get-Core;$null-ne$script:core};$core=$script:core;$window=$core.MainWindowHandle
 $tab=@([R19CaptureNative]::Probe($window,905))|Where-Object{$_.Visible}|Select-Object -First 1;if(-not$tab){throw 'Zapret tab 905 missing'}
 [void][R19CaptureNative]::SendMessage([IntPtr]$tab.Hwnd,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
 Wait-Until 8 'launcher remove-services 1702' {@([R19CaptureNative]::Probe($window,1702))|Where-Object{$_.Visible -and $_.OwnerPid-eq$launcher.Id}|Measure-Object|Select-Object -ExpandProperty Count}
 [void][R19CaptureNative]::SetWindowPos($window,[IntPtr]::Zero,0,0,1024,768,0x0002-bor0x0004-bor0x0010-bor0x0400)
 Start-Sleep -Milliseconds 1100
 $rootBounds=[R19CaptureNative]::Bounds($window)
 $button=@([R19CaptureNative]::Probe($window,1702))|Where-Object{$_.Visible -and $_.OwnerPid-eq$launcher.Id}|Select-Object -First 1
 if(-not$button){throw 'visible launcher 1702 missing at 1024 capture time'}
 $screen=Join-Path $OutputDir 'rev19-1702-1024-screen.png';$mainPrint=Join-Path $OutputDir 'rev19-1702-1024-main-print.png';$buttonPrint=Join-Path $OutputDir 'rev19-1702-1024-button-print.png'
 Capture-Screen $window $screen;Capture-Print $window $mainPrint;Capture-Print ([IntPtr]$button.Hwnd) $buttonPrint
 $screenColors=Get-CropColorCount $screen $button $rootBounds;$mainColors=Get-CropColorCount $mainPrint $button $rootBounds;$buttonColors=Get-ColorCount $buttonPrint
 $result=[pscustomobject]@{width=1024;height=768;hwnd=$button.Hwnd;owner_pid=$button.OwnerPid;rect=@($button.Left,$button.Top,$button.Right,$button.Bottom);screen_colors=$screenColors;main_print_colors=$mainColors;button_print_colors=$buttonColors;screen=$screen;main_print=$mainPrint;button_print=$buttonPrint}
 $result|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $OutputDir 'rev19-1702-1024-capture-probe.json') -Encoding utf8
 Write-Host "REV19_1702_1024_CAPTURE screen=$screenColors mainPrint=$mainColors buttonPrint=$buttonColors rect=$($button.Left),$($button.Top)-$($button.Right),$($button.Bottom)"
}
finally{
 foreach($name in @('DPopCleaner','DPopCleaner.Core','SimpleUpdate')){foreach($p in @(Get-Process -Name $name -ErrorAction SilentlyContinue)){try{if($p.Path -and [IO.Path]::GetFullPath($p.Path).StartsWith($RootPath,[StringComparison]::OrdinalIgnoreCase)){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue}}catch{}}}
}
