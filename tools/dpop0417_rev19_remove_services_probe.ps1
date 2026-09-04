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
public sealed class R19ProbeItem {
 public long Hwnd; public int Id; public string Text; public string ClassName; public bool Visible; public bool Enabled;
 public int Left; public int Top; public int Right; public int Bottom; public long Parent; public int OwnerPid; public long Style; public long Theme;
 public long ParentHwnd; public int ParentId; public string ParentText; public string ParentClass; public bool ParentVisible; public int ParentPid;
 public int ParentLeft; public int ParentTop; public int ParentRight; public int ParentBottom;
}
public sealed class R19ProbeRect { public int Left; public int Top; public int Right; public int Bottom; }
public static class R19ProbeNative {
 private delegate bool EnumProc(IntPtr h,IntPtr p);
 [StructLayout(LayoutKind.Sequential)] private struct RECT{public int L,T,R,B;}
 [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr p,EnumProc cb,IntPtr x);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr h,StringBuilder b,int n);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr h,StringBuilder b,int n);
 [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr h);
 [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] private static extern bool IsWindowEnabled(IntPtr h);
 [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr h,out RECT r);
 [DllImport("user32.dll")] private static extern IntPtr GetParent(IntPtr h);
 [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);
 [DllImport("user32.dll",EntryPoint="GetWindowLongPtrW")] private static extern IntPtr GetWindowLongPtr64(IntPtr h,int i);
 [DllImport("user32.dll",EntryPoint="GetWindowLongW")] private static extern int GetWindowLong32(IntPtr h,int i);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr a,int x,int y,int w,int z,uint f);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
 [DllImport("user32.dll")] private static extern bool InvalidateRect(IntPtr h,IntPtr r,bool erase);
 [DllImport("user32.dll")] private static extern bool RedrawWindow(IntPtr h,IntPtr r,IntPtr region,uint flags);
 [DllImport("user32.dll")] private static extern bool UpdateWindow(IntPtr h);
 [DllImport("uxtheme.dll")] private static extern IntPtr GetWindowTheme(IntPtr h);
 static string Text(IntPtr h){var b=new StringBuilder(1024);GetWindowText(h,b,b.Capacity);return b.ToString();}
 static string Cls(IntPtr h){var b=new StringBuilder(128);GetClassName(h,b,b.Capacity);return b.ToString();}
 static long Style(IntPtr h){return IntPtr.Size==8?GetWindowLongPtr64(h,-16).ToInt64():GetWindowLong32(h,-16);}
 public static R19ProbeRect Bounds(IntPtr h){RECT r;if(!GetWindowRect(h,out r))return null;return new R19ProbeRect{Left=r.L,Top=r.T,Right=r.R,Bottom=r.B};}
 public static void ForceRedraw(IntPtr h){if(h==IntPtr.Zero)return;InvalidateRect(h,IntPtr.Zero,true);RedrawWindow(h,IntPtr.Zero,IntPtr.Zero,0x0001|0x0004|0x0080|0x0100);UpdateWindow(h);}
 public static R19ProbeItem[] Probe(IntPtr root,int id){
  var list=new List<R19ProbeItem>();EnumProc cb=delegate(IntPtr h,IntPtr _){if(GetDlgCtrlID(h)!=id)return true;RECT r;GetWindowRect(h,out r);var p=GetParent(h);RECT pr=new RECT();if(p!=IntPtr.Zero)GetWindowRect(p,out pr);uint pid=0,ppid=0;GetWindowThreadProcessId(h,out pid);if(p!=IntPtr.Zero)GetWindowThreadProcessId(p,out ppid);
   list.Add(new R19ProbeItem{Hwnd=h.ToInt64(),Id=id,Text=Text(h),ClassName=Cls(h),Visible=IsWindowVisible(h),Enabled=IsWindowEnabled(h),Left=r.L,Top=r.T,Right=r.R,Bottom=r.B,Parent=p.ToInt64(),OwnerPid=(int)pid,Style=Style(h),Theme=GetWindowTheme(h).ToInt64(),ParentHwnd=p.ToInt64(),ParentId=p==IntPtr.Zero?0:GetDlgCtrlID(p),ParentText=p==IntPtr.Zero?"":Text(p),ParentClass=p==IntPtr.Zero?"":Cls(p),ParentVisible=p!=IntPtr.Zero&&IsWindowVisible(p),ParentPid=(int)ppid,ParentLeft=pr.L,ParentTop=pr.T,ParentRight=pr.R,ParentBottom=pr.B});return true;};
  EnumChildWindows(root,cb,IntPtr.Zero);GC.KeepAlive(cb);return list.ToArray();
 }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Wait-Until([int]$Seconds,[string]$What,[scriptblock]$Condition){$d=[DateTime]::UtcNow.AddSeconds($Seconds);do{if(& $Condition){return};Start-Sleep -Milliseconds 120}while([DateTime]::UtcNow-lt$d);throw "Timed out waiting for $What"}
function Get-Core { foreach($p in @(Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue)){try{if($p.Path -and [IO.Path]::GetFullPath($p.Path).Equals([IO.Path]::GetFullPath($corePath),[StringComparison]::OrdinalIgnoreCase)-and$p.MainWindowHandle-ne[IntPtr]::Zero){return $p}}catch{}};$null }
function Capture-Print([IntPtr]$Window,[string]$Path){$r=[R19ProbeNative]::Bounds($Window);$w=[Math]::Max(1,$r.Right-$r.Left);$h=[Math]::Max(1,$r.Bottom-$r.Top);$bmp=[Drawing.Bitmap]::new($w,$h);$g=[Drawing.Graphics]::FromImage($bmp);$dc=$g.GetHdc();try{[void][R19ProbeNative]::PrintWindow($Window,$dc,2)}finally{$g.ReleaseHdc($dc);$g.Dispose()};try{$bmp.Save($Path,[Drawing.Imaging.ImageFormat]::Png)}finally{$bmp.Dispose()}}
function Capture-Screen([IntPtr]$Window,[string]$Path){$r=[R19ProbeNative]::Bounds($Window);$w=[Math]::Max(1,$r.Right-$r.Left);$h=[Math]::Max(1,$r.Bottom-$r.Top);$bmp=[Drawing.Bitmap]::new($w,$h);$g=[Drawing.Graphics]::FromImage($bmp);try{$g.CopyFromScreen($r.Left,$r.Top,0,0,[Drawing.Size]::new($w,$h))}finally{$g.Dispose()};try{$bmp.Save($Path,[Drawing.Imaging.ImageFormat]::Png)}finally{$bmp.Dispose()}}

$launcher=$null;$core=$null
try{
 $settings=Join-Path $OutputDir 'rev19-remove-probe-settings.ini';@('auto_update=0','tray_icon=0')|Set-Content -LiteralPath $settings -Encoding ascii
 $launcher=Start-Process -FilePath $launcherPath -ArgumentList @('--no-update-check','--settings-path',('"'+$settings+'"')) -WorkingDirectory $RootPath -PassThru
 Wait-Until 18 'core' {$script:core=Get-Core;$null-ne$script:core};$core=$script:core;$window=$core.MainWindowHandle
 $tab=@([R19ProbeNative]::Probe($window,905))|Where-Object{$_.Visible}|Select-Object -First 1
 if(-not$tab){throw 'Zapret tab 905 missing'};[void][R19ProbeNative]::SendMessage([IntPtr]$tab.Hwnd,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
 Wait-Until 8 'visible remove-services proxy' {@([R19ProbeNative]::Probe($window,1702))|Where-Object{$_.Visible}|Measure-Object|Select-Object -ExpandProperty Count}
 $reports=@()
 foreach($w in @(1024,1366,1680,1908)){
  $h=if($w-eq1024){768}elseif($w-eq1366){800}elseif($w-eq1680){840}else{950}
  [void][R19ProbeNative]::SetWindowPos($window,[IntPtr]::Zero,0,0,$w,$h,0x0002-bor0x0004-bor0x0010-bor0x0400);Start-Sleep -Milliseconds 1200
  $items=@([R19ProbeNative]::Probe($window,1702));$service=@();foreach($id in @(1703,1702,1710,1711)){$service+=@([R19ProbeNative]::Probe($window,$id)|Where-Object{$_.Visible})}
  $reports+=[pscustomobject]@{width=$w;height=$h;items=$items;service=$service}
  foreach($i in $items){Write-Host ("REV19_REMOVE_PROBE size={0} hwnd=0x{1:X} text='{2}' visible={3} enabled={4} rect={5},{6}-{7},{8} pid={9} style=0x{10:X} theme=0x{11:X} parent=0x{12:X} parentClass='{13}' parentId={14} parentVisible={15} parentRect={16},{17}-{18},{19} parentPid={20}" -f $w,$i.Hwnd,$i.Text,$i.Visible,$i.Enabled,$i.Left,$i.Top,$i.Right,$i.Bottom,$i.OwnerPid,$i.Style,$i.Theme,$i.ParentHwnd,$i.ParentClass,$i.ParentId,$i.ParentVisible,$i.ParentLeft,$i.ParentTop,$i.ParentRight,$i.ParentBottom,$i.ParentPid)}
  Write-Host ('REV19_SERVICE_RECTS size='+$w+' '+(@($service|ForEach-Object{'id='+$_.Id+' rect='+$_.Left+','+$_.Top+'-'+$_.Right+','+$_.Bottom+' pid='+$_.OwnerPid})-join '; '))
  if($w-eq1908){
   Capture-Print $window (Join-Path $OutputDir 'rev19-remove-probe-print-before.png');Capture-Screen $window (Join-Path $OutputDir 'rev19-remove-probe-screen-before.png')
   $proxy=$items|Where-Object{$_.Visible -and $_.OwnerPid-eq$launcher.Id}|Select-Object -First 1
   if($proxy){[R19ProbeNative]::ForceRedraw([IntPtr]$proxy.ParentHwnd);[R19ProbeNative]::ForceRedraw([IntPtr]$proxy.Hwnd);Start-Sleep -Milliseconds 300}
   Capture-Print $window (Join-Path $OutputDir 'rev19-remove-probe-print-after.png');Capture-Screen $window (Join-Path $OutputDir 'rev19-remove-probe-screen-after.png')
  }
 }
 $reports|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $OutputDir 'rev19-remove-services-probe.json') -Encoding utf8
 Write-Host 'REV19_REMOVE_SERVICES_PROBE_OK'
}
finally{foreach($name in @('DPopCleaner','DPopCleaner.Core','SimpleUpdate')){foreach($p in @(Get-Process -Name $name -ErrorAction SilentlyContinue)){try{if($p.Path-and[IO.Path]::GetFullPath($p.Path).StartsWith($RootPath,[StringComparison]::OrdinalIgnoreCase)){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue}}catch{}}}}
