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
public sealed class R19StyleItem {
 public long Hwnd,Parent,Style,WndProc; public int Id,Pid,ThreadId,ParentThreadId; public string Text,ClassName;
 public bool Visible; public int Left,Top,Right,Bottom;
}
public sealed class R19StyleRect { public int Left,Top,Right,Bottom; }
public static class R19StyleNative {
 private delegate bool EnumProc(IntPtr h,IntPtr p);
 [StructLayout(LayoutKind.Sequential)] private struct RECT{public int L,T,R,B;}
 [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr p,EnumProc cb,IntPtr x);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr h,StringBuilder b,int n);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr h,StringBuilder b,int n);
 [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr h);
 [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr h,out RECT r);
 [DllImport("user32.dll")] private static extern IntPtr GetParent(IntPtr h);
 [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);
 [DllImport("user32.dll",EntryPoint="GetWindowLongPtrW")] private static extern IntPtr GetWindowLongPtr64(IntPtr h,int i);
 [DllImport("user32.dll",EntryPoint="GetWindowLongW")] private static extern int GetWindowLong32(IntPtr h,int i);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr a,int x,int y,int w,int z,uint f);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
 [DllImport("user32.dll")] private static extern bool InvalidateRect(IntPtr h,IntPtr r,bool erase);
 [DllImport("user32.dll")] private static extern bool RedrawWindow(IntPtr h,IntPtr r,IntPtr region,uint flags);
 [DllImport("user32.dll")] private static extern bool UpdateWindow(IntPtr h);
 [DllImport("uxtheme.dll",CharSet=CharSet.Unicode)] private static extern int SetWindowTheme(IntPtr h,string subApp,string subIdList);
 static string Text(IntPtr h){var b=new StringBuilder(512);GetWindowText(h,b,b.Capacity);return b.ToString();}
 static string Cls(IntPtr h){var b=new StringBuilder(128);GetClassName(h,b,b.Capacity);return b.ToString();}
 static long Long(IntPtr h,int i){return IntPtr.Size==8?GetWindowLongPtr64(h,i).ToInt64():GetWindowLong32(h,i);}
 public static R19StyleRect Bounds(IntPtr h){RECT r;if(!GetWindowRect(h,out r))return null;return new R19StyleRect{Left=r.L,Top=r.T,Right=r.R,Bottom=r.B};}
 public static R19StyleItem[] Probe(IntPtr root,int id){var list=new List<R19StyleItem>();EnumProc cb=delegate(IntPtr h,IntPtr _){if(GetDlgCtrlID(h)!=id)return true;RECT r;if(!GetWindowRect(h,out r))return true;uint pid=0,ppid=0;var tid=GetWindowThreadProcessId(h,out pid);var p=GetParent(h);var ptid=p==IntPtr.Zero?0:GetWindowThreadProcessId(p,out ppid);list.Add(new R19StyleItem{Hwnd=h.ToInt64(),Parent=p.ToInt64(),Id=id,Pid=(int)pid,ThreadId=(int)tid,ParentThreadId=(int)ptid,Text=Text(h),ClassName=Cls(h),Visible=IsWindowVisible(h),Left=r.L,Top=r.T,Right=r.R,Bottom=r.B,Style=Long(h,-16),WndProc=Long(h,-4)});return true;};EnumChildWindows(root,cb,IntPtr.Zero);GC.KeepAlive(cb);return list.ToArray();}
 public static void EnableRedraw(IntPtr h){SendMessage(h,0x000B,new IntPtr(1),IntPtr.Zero);InvalidateRect(h,IntPtr.Zero,true);RedrawWindow(h,IntPtr.Zero,IntPtr.Zero,0x0001|0x0004|0x0080|0x0100);UpdateWindow(h);}
 public static void SetButtonStyle(IntPtr h,uint style){SendMessage(h,0x00F4,new IntPtr((long)style),new IntPtr(1));InvalidateRect(h,IntPtr.Zero,true);RedrawWindow(h,IntPtr.Zero,IntPtr.Zero,0x0001|0x0004|0x0080|0x0100);UpdateWindow(h);}
 public static void RestorePushTheme(IntPtr h){SendMessage(h,0x00F4,IntPtr.Zero,new IntPtr(1));SetWindowTheme(h,null,null);InvalidateRect(h,IntPtr.Zero,true);RedrawWindow(h,IntPtr.Zero,IntPtr.Zero,0x0001|0x0004|0x0080|0x0100);UpdateWindow(h);}
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Wait-Until([int]$Seconds,[string]$What,[scriptblock]$Condition){$d=[DateTime]::UtcNow.AddSeconds($Seconds);do{if(& $Condition){return};Start-Sleep -Milliseconds 120}while([DateTime]::UtcNow-lt$d);throw "Timed out waiting for $What"}
function Get-Core {foreach($p in @(Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue)){try{if($p.Path-and[IO.Path]::GetFullPath($p.Path).Equals([IO.Path]::GetFullPath($corePath),[StringComparison]::OrdinalIgnoreCase)-and$p.MainWindowHandle-ne[IntPtr]::Zero){return $p}}catch{}};$null}
function Capture-Print([IntPtr]$Window,[string]$Path){$r=[R19StyleNative]::Bounds($Window);if(-not$r){return};$w=[Math]::Max(1,$r.Right-$r.Left);$h=[Math]::Max(1,$r.Bottom-$r.Top);$bmp=[Drawing.Bitmap]::new($w,$h);$g=[Drawing.Graphics]::FromImage($bmp);$dc=$g.GetHdc();try{[void][R19StyleNative]::PrintWindow($Window,$dc,2)}finally{$g.ReleaseHdc($dc);$g.Dispose()};try{$bmp.Save($Path,[Drawing.Imaging.ImageFormat]::Png)}finally{$bmp.Dispose()}}

$launcher=$null;$core=$null
try{
 $settings=Join-Path $OutputDir 'rev19-style-probe-settings.ini';@('auto_update=0','tray_icon=0')|Set-Content -LiteralPath $settings -Encoding ascii
 $launcher=Start-Process -FilePath $launcherPath -ArgumentList @('--no-update-check','--settings-path',('"'+$settings+'"')) -WorkingDirectory $RootPath -PassThru
 Wait-Until 18 'core' {$script:core=Get-Core;$null-ne$script:core};$core=$script:core;$window=$core.MainWindowHandle
 $tab=@([R19StyleNative]::Probe($window,905))|Where-Object{$_.Visible}|Select-Object -First 1
 if(-not$tab){throw 'Zapret tab 905 missing'}
 [void][R19StyleNative]::SendMessage([IntPtr]$tab.Hwnd,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
 Wait-Until 8 'visible 1702 proxy' {@([R19StyleNative]::Probe($window,1702))|Where-Object{$_.Visible -and $_.Pid-eq$launcher.Id}|Measure-Object|Select-Object -ExpandProperty Count}
 [void][R19StyleNative]::SetWindowPos($window,[IntPtr]::Zero,0,0,1908,950,0x0002-bor0x0004-bor0x0010-bor0x0400);Start-Sleep -Milliseconds 1200
 $samples=@()
 foreach($id in @(1701,1702,1713)){
   $s=@([R19StyleNative]::Probe($window,$id))|Where-Object{$_.Visible -and $_.Pid-eq$launcher.Id}|Select-Object -First 1
   if($s){$samples+=$s;Write-Host ("REV19_STYLE_SAMPLE id={0} text='{1}' style=0x{2:X} wndproc=0x{3:X} tid={4} parentTid={5} rect={6},{7}-{8},{9}" -f $s.Id,$s.Text,$s.Style,$s.WndProc,$s.ThreadId,$s.ParentThreadId,$s.Left,$s.Top,$s.Right,$s.Bottom)}
 }
 $target=$samples|Where-Object{$_.Id-eq1702}|Select-Object -First 1
 if(-not$target){throw 'visible launcher 1702 missing'}
 $button=[IntPtr]$target.Hwnd;$parentHost=[IntPtr]$target.Parent
 Capture-Print $button (Join-Path $OutputDir 'rev19-style-1702-baseline.png')
 Capture-Print $parentHost (Join-Path $OutputDir 'rev19-style-1702-host-baseline.png')
 [R19StyleNative]::EnableRedraw($parentHost);[R19StyleNative]::EnableRedraw($button);Start-Sleep -Milliseconds 250
 Capture-Print $button (Join-Path $OutputDir 'rev19-style-1702-after-setredraw.png')
 Capture-Print $parentHost (Join-Path $OutputDir 'rev19-style-1702-host-after-setredraw.png')
 [R19StyleNative]::SetButtonStyle($button,0x0000000B);Start-Sleep -Milliseconds 250
 Capture-Print $button (Join-Path $OutputDir 'rev19-style-1702-after-bm-ownerdraw.png')
 Capture-Print $parentHost (Join-Path $OutputDir 'rev19-style-1702-host-after-bm-ownerdraw.png')
 [R19StyleNative]::SetButtonStyle($button,0x00000000);Start-Sleep -Milliseconds 100
 [R19StyleNative]::SetButtonStyle($button,0x0000000B);Start-Sleep -Milliseconds 250
 Capture-Print $button (Join-Path $OutputDir 'rev19-style-1702-after-bm-toggle.png')
 Capture-Print $parentHost (Join-Path $OutputDir 'rev19-style-1702-host-after-bm-toggle.png')

 Capture-Print $window (Join-Path $OutputDir 'rev19-style-full-before-system-theme.png')
 foreach($id in @(1701,1702,1713,1720,1721,1722,1723,1724,1725)){
   $bridge=@([R19StyleNative]::Probe($window,$id))|Where-Object{$_.Visible -and $_.Pid-eq$launcher.Id}|Select-Object -First 1
   if($bridge){[R19StyleNative]::RestorePushTheme([IntPtr]$bridge.Hwnd);Write-Host ("REV19_SYSTEM_THEME_RESTORE id={0} hwnd=0x{1:X}" -f $id,$bridge.Hwnd)}
 }
 Start-Sleep -Milliseconds 500
 Capture-Print $window (Join-Path $OutputDir 'rev19-style-full-after-system-theme.png')
 $targetAfter=@([R19StyleNative]::Probe($window,1702))|Where-Object{$_.Visible -and $_.Pid-eq$launcher.Id}|Select-Object -First 1
 if($targetAfter){Capture-Print ([IntPtr]$targetAfter.Hwnd) (Join-Path $OutputDir 'rev19-style-1702-after-system-theme.png')}

 $samples|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $OutputDir 'rev19-button-style-probe.json') -Encoding utf8
 Write-Host 'REV19_BUTTON_STYLE_PROBE_OK'
}
finally{foreach($name in @('DPopCleaner','DPopCleaner.Core','SimpleUpdate')){foreach($p in @(Get-Process -Name $name -ErrorAction SilentlyContinue)){try{if($p.Path-and[IO.Path]::GetFullPath($p.Path).StartsWith($RootPath,[StringComparison]::OrdinalIgnoreCase)){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue}}catch{}}}}