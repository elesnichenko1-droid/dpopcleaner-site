[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RootPath,
    [string]$OutputDir='_release/0.4.17/evidence/rev19-zapret-cleanup'
)

$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RootPath=if([IO.Path]::IsPathRooted($RootPath)){$RootPath}else{Join-Path $repoRoot $RootPath}
$OutputDir=if([IO.Path]::IsPathRooted($OutputDir)){$OutputDir}else{Join-Path $repoRoot $OutputDir}
$RootPath=[IO.Path]::GetFullPath($RootPath)
$OutputDir=[IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Path $OutputDir -Force|Out-Null

$launcherPath=Join-Path $RootPath 'DPopCleaner.exe'
$corePath=Join-Path $RootPath 'DPopCleaner.Core.exe'

$native=@'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public sealed class R19SubclassItem {
 public long Hwnd; public int Id; public bool Visible; public int OwnerPid;
 public int Left; public int Top; public int Right; public int Bottom;
 public long InstanceProc; public long ClassProc;
}
public static class R19SubclassNative {
 private delegate bool EnumProc(IntPtr h,IntPtr p);
 [StructLayout(LayoutKind.Sequential)] private struct RECT{public int L,T,R,B;}
 [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr p,EnumProc cb,IntPtr x);
 [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr h);
 [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr h,out RECT r);
 [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);
 [DllImport("user32.dll",EntryPoint="GetWindowLongPtrW")] private static extern IntPtr GetWindowLongPtr64(IntPtr h,int i);
 [DllImport("user32.dll",EntryPoint="GetWindowLongW")] private static extern int GetWindowLong32(IntPtr h,int i);
 [DllImport("user32.dll",EntryPoint="GetClassLongPtrW")] private static extern IntPtr GetClassLongPtr64(IntPtr h,int i);
 [DllImport("user32.dll",EntryPoint="GetClassLongW")] private static extern uint GetClassLong32(IntPtr h,int i);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr a,int x,int y,int w,int z,uint f);
 [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 static long WindowProc(IntPtr h){return IntPtr.Size==8?GetWindowLongPtr64(h,-4).ToInt64():GetWindowLong32(h,-4);}
 static long ClassProc(IntPtr h){return IntPtr.Size==8?GetClassLongPtr64(h,-24).ToInt64():unchecked((int)GetClassLong32(h,-24));}
 public static R19SubclassItem[] Probe(IntPtr root,int id){
  var list=new List<R19SubclassItem>();
  EnumProc cb=delegate(IntPtr h,IntPtr _){
   if(GetDlgCtrlID(h)!=id)return true;RECT r;GetWindowRect(h,out r);uint pid=0;GetWindowThreadProcessId(h,out pid);
   list.Add(new R19SubclassItem{Hwnd=h.ToInt64(),Id=id,Visible=IsWindowVisible(h),OwnerPid=(int)pid,Left=r.L,Top=r.T,Right=r.R,Bottom=r.B,InstanceProc=WindowProc(h),ClassProc=ClassProc(h)});return true;
  };
  EnumChildWindows(root,cb,IntPtr.Zero);GC.KeepAlive(cb);return list.ToArray();
 }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Wait-Until([int]$Seconds,[string]$What,[scriptblock]$Condition){
 $deadline=[DateTime]::UtcNow.AddSeconds($Seconds)
 do{if(& $Condition){return};Start-Sleep -Milliseconds 120}while([DateTime]::UtcNow-lt$deadline)
 throw "Timed out waiting for $What"
}
function Get-Core {
 foreach($p in @(Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue)){
  try{if($p.Path -and [IO.Path]::GetFullPath($p.Path).Equals([IO.Path]::GetFullPath($corePath),[StringComparison]::OrdinalIgnoreCase)-and$p.MainWindowHandle-ne[IntPtr]::Zero){return $p}}catch{}
 }
 $null
}
function Capture-Child($Item,[string]$Path){
 $w=[Math]::Max(1,$Item.Right-$Item.Left);$h=[Math]::Max(1,$Item.Bottom-$Item.Top)
 $screen=[Windows.Forms.Screen]::PrimaryScreen.Bounds
 if($Item.Left-lt$screen.Left-or$Item.Top-lt$screen.Top-or$Item.Right-gt$screen.Right-or$Item.Bottom-gt$screen.Bottom){throw "1702 outside physical screen: $($Item.Left),$($Item.Top)-$($Item.Right),$($Item.Bottom)"}
 $bmp=[Drawing.Bitmap]::new($w,$h);$g=[Drawing.Graphics]::FromImage($bmp)
 try{$g.CopyFromScreen($Item.Left,$Item.Top,0,0,[Drawing.Size]::new($w,$h),[Drawing.CopyPixelOperation]::SourceCopy)}finally{$g.Dispose()}
 try{$bmp.Save($Path,[Drawing.Imaging.ImageFormat]::Png)}finally{$bmp.Dispose()}
}
function Get-ColorCount([string]$Path){
 $bmp=[Drawing.Bitmap]::new($Path);$colors=[Collections.Generic.HashSet[int]]::new()
 try{for($y=0;$y-lt$bmp.Height;$y++){for($x=0;$x-lt$bmp.Width;$x++){[void]$colors.Add($bmp.GetPixel($x,$y).ToArgb())}}}finally{$bmp.Dispose()}
 $colors.Count
}

$launcher=$null;$core=$null
try{
 $settings=Join-Path $OutputDir 'rev19-subclass-probe-settings.ini';@('auto_update=0','tray_icon=0')|Set-Content -LiteralPath $settings -Encoding ascii
 $launcher=Start-Process -FilePath $launcherPath -ArgumentList @('--no-update-check','--settings-path',('"'+$settings+'"')) -WorkingDirectory $RootPath -PassThru
 Wait-Until 18 'third-launch core' {$script:core=Get-Core;$null-ne$script:core}
 $core=$script:core;$window=$core.MainWindowHandle
 $tab=@([R19SubclassNative]::Probe($window,905))|Where-Object{$_.Visible}|Select-Object -First 1
 if(-not$tab){throw 'Zapret tab 905 missing in subclass probe'}
 [void][R19SubclassNative]::SendMessage([IntPtr]$tab.Hwnd,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
 Wait-Until 8 'launcher-owned 1702' {$script:item=@([R19SubclassNative]::Probe($window,1702))|Where-Object{$_.Visible-and$_.OwnerPid-eq$launcher.Id}|Select-Object -First 1;$null-ne$script:item}
 [void][R19SubclassNative]::SetWindowPos($window,[IntPtr]::Zero,0,0,1024,768,0x0002-bor0x0004-bor0x0010-bor0x0400)
 Start-Sleep -Milliseconds 1200
 [void][R19SubclassNative]::BringWindowToTop($window);[void][R19SubclassNative]::SetForegroundWindow($window);Start-Sleep -Milliseconds 220
 $item=@([R19SubclassNative]::Probe($window,1702))|Where-Object{$_.Visible-and$_.OwnerPid-eq$launcher.Id}|Select-Object -First 1
 if(-not$item){throw 'launcher-owned 1702 disappeared in subclass probe'}
 $before=Join-Path $OutputDir 'rev19-subclass-1702-before.png';Capture-Child $item $before;$beforeColors=Get-ColorCount $before
 $wmPaintResult=[R19SubclassNative]::SendMessage([IntPtr]$item.Hwnd,0x000F,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64();Start-Sleep -Milliseconds 180
 $itemAfter=@([R19SubclassNative]::Probe($window,1702))|Where-Object{$_.Visible-and$_.OwnerPid-eq$launcher.Id}|Select-Object -First 1
 $after=Join-Path $OutputDir 'rev19-subclass-1702-after-wm-paint.png';Capture-Child $itemAfter $after;$afterColors=Get-ColorCount $after
 $subclassed=($item.InstanceProc-ne$item.ClassProc)
 $report=[pscustomobject]@{launcher_pid=$launcher.Id;core_pid=$core.Id;hwnd=('0x{0:X}'-f$item.Hwnd);instance_proc=('0x{0:X}'-f$item.InstanceProc);class_proc=('0x{0:X}'-f$item.ClassProc);instance_differs_from_class=$subclassed;before_colors=$beforeColors;after_wm_paint_colors=$afterColors;wm_paint_result=$wmPaintResult;rect=@($item.Left,$item.Top,$item.Right,$item.Bottom)}
 $report|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $OutputDir 'rev19-subclass-probe.json') -Encoding utf8
 Write-Host ("REV19_SUBCLASS_PROBE hwnd=0x{0:X} instance=0x{1:X} class=0x{2:X} subclassed={3} before={4} afterWM_PAINT={5} result={6}" -f $item.Hwnd,$item.InstanceProc,$item.ClassProc,$subclassed,$beforeColors,$afterColors,$wmPaintResult)
}
finally{
 foreach($name in @('DPopCleaner','DPopCleaner.Core','SimpleUpdate')){foreach($p in @(Get-Process -Name $name -ErrorAction SilentlyContinue)){try{if($p.Path-and[IO.Path]::GetFullPath($p.Path).StartsWith($RootPath,[StringComparison]::OrdinalIgnoreCase)){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue}}catch{}}}
}
