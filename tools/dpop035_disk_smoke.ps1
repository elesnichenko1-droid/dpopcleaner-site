[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ExePath,
    [Parameter(Mandatory)][string]$OutputDir
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$ExePath = [IO.Path]::GetFullPath($ExePath)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) { throw "EXE missing: $ExePath" }
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$fixture = Join-Path $env:RUNNER_TEMP 'dpop035-disk-fixture'
Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path (Join-Path $fixture 'Users/Test/Cache') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $fixture 'Windows/Logs') -Force | Out-Null
[IO.File]::WriteAllBytes((Join-Path $fixture 'root.bin'), (New-Object byte[] 4096))
[IO.File]::WriteAllBytes((Join-Path $fixture 'Users/user.bin'), (New-Object byte[] 16384))
[IO.File]::WriteAllBytes((Join-Path $fixture 'Users/Test/Cache/cache.bin'), (New-Object byte[] 32768))
[IO.File]::WriteAllBytes((Join-Path $fixture 'Windows/Logs/log.bin'), (New-Object byte[] 8192))

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class DPop035DiskWin32 {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
  public delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent, EnumProc cb, IntPtr data);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder b, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder b, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern bool SetWindowText(IntPtr h, string text);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,uint msg,IntPtr wp,IntPtr lp);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool GetWindowRect(IntPtr h,out RECT r);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool GetClientRect(IntPtr h,out RECT r);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool MoveWindow(IntPtr h,int x,int y,int w,int ht,bool repaint);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
  public static IntPtr[] Children(IntPtr parent) {
    var result = new List<IntPtr>();
    EnumChildWindows(parent, (h,_) => { result.Add(h); return true; }, IntPtr.Zero);
    return result.ToArray();
  }
}
'@

function Text([IntPtr]$h) { $b=New-Object Text.StringBuilder 512; [void][DPop035DiskWin32]::GetWindowText($h,$b,$b.Capacity); $b.ToString() }
function Class([IntPtr]$h) { $b=New-Object Text.StringBuilder 128; [void][DPop035DiskWin32]::GetClassName($h,$b,$b.Capacity); $b.ToString() }
function Find-Control([IntPtr]$main,[string]$class,[string]$text='') {
  foreach($h in [DPop035DiskWin32]::Children($main)) {
    if(-not [DPop035DiskWin32]::IsWindowVisible($h)) { continue }
    if((Class $h) -ne $class) { continue }
    if($text -and (Text $h) -ne $text) { continue }
    return $h
  }
  return [IntPtr]::Zero
}
function Resize-Client([IntPtr]$h,[int]$w,[int]$ht) {
  $wr=New-Object DPop035DiskWin32+RECT; $cr=New-Object DPop035DiskWin32+RECT
  if(-not [DPop035DiskWin32]::GetWindowRect($h,[ref]$wr)){throw 'GetWindowRect failed'}
  if(-not [DPop035DiskWin32]::GetClientRect($h,[ref]$cr)){throw 'GetClientRect failed'}
  $ow=$w+(($wr.Right-$wr.Left)-($cr.Right-$cr.Left)); $oh=$ht+(($wr.Bottom-$wr.Top)-($cr.Bottom-$cr.Top))
  if(-not [DPop035DiskWin32]::MoveWindow($h,$wr.Left,$wr.Top,$ow,$oh,$true)){throw 'MoveWindow failed'}
  Start-Sleep -Milliseconds 500
}
function Capture([IntPtr]$h,[string]$name) {
  $r=New-Object DPop035DiskWin32+RECT
  if(-not [DPop035DiskWin32]::GetWindowRect($h,[ref]$r)){throw 'capture rect failed'}
  $w=$r.Right-$r.Left; $ht=$r.Bottom-$r.Top
  $bmp=New-Object Drawing.Bitmap($w,$ht); $g=[Drawing.Graphics]::FromImage($bmp); $dc=$g.GetHdc()
  try { if(-not [DPop035DiskWin32]::PrintWindow($h,$dc,2)){ if(-not [DPop035DiskWin32]::PrintWindow($h,$dc,0)){throw 'PrintWindow failed'} } }
  finally { $g.ReleaseHdc($dc); $g.Dispose() }
  try { $path=Join-Path $OutputDir ($name+'.png'); $bmp.Save($path,[Drawing.Imaging.ImageFormat]::Png); return $path }
  finally { $bmp.Dispose() }
}

$p=$null
try {
  $p=Start-Process -FilePath $ExePath -PassThru
  $deadline=(Get-Date).AddSeconds(30)
  do { Start-Sleep -Milliseconds 250; $p.Refresh(); if($p.HasExited){throw 'App exited before window appeared'} } while($p.MainWindowHandle -eq 0 -and (Get-Date)-lt $deadline)
  if($p.MainWindowHandle -eq 0){throw 'Main window timeout'}
  $main=[IntPtr]$p.MainWindowHandle
  Resize-Client $main 1200 850

  $diskTab=Find-Control $main 'Button' 'Диск'
  if($diskTab -eq [IntPtr]::Zero){throw 'Disk navigation button not found'}
  [void][DPop035DiskWin32]::SendMessage($diskTab,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
  Start-Sleep -Milliseconds 700

  $edit=Find-Control $main 'Edit'
  $scan=Find-Control $main 'Button' 'Сканировать'
  $stop=Find-Control $main 'Button' 'Стоп'
  if($edit -eq [IntPtr]::Zero -or $scan -eq [IntPtr]::Zero -or $stop -eq [IntPtr]::Zero){throw 'Disk scan controls not found'}
  if(-not [DPop035DiskWin32]::SetWindowText($edit,$fixture)){throw 'Could not set disk fixture path'}
  [void][DPop035DiskWin32]::SendMessage($scan,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)

  $deadline=(Get-Date).AddSeconds(20)
  do { Start-Sleep -Milliseconds 200; $p.Refresh(); if($p.HasExited){throw 'App exited during disk scan'} } while([DPop035DiskWin32]::IsWindowEnabled($stop) -and (Get-Date)-lt $deadline)
  if([DPop035DiskWin32]::IsWindowEnabled($stop)){throw 'Disk fixture scan did not finish in time'}
  if(-not [DPop035DiskWin32]::IsWindowEnabled($scan)){throw 'Scan button was not restored after completion'}

  $capture1200=Capture $main 'disk-1200x850'
  Resize-Client $main 1100 700
  $capture1100=Capture $main 'disk-1100x700'

  [pscustomobject]@{
    target='DPopCleaner 0.3.5 BETA R1'
    fixture=$fixture
    scan_finished=$true
    capture_1200=$capture1200
    capture_1100=$capture1100
  } | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OutputDir 'disk-smoke-report.json') -Encoding utf8

  [void][DPop035DiskWin32]::SendMessage($main,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)
  if(-not $p.WaitForExit(10000)){throw 'App did not close after disk smoke'}
  if($p.ExitCode -ne 0){throw "App exited with code $($p.ExitCode)"}
}
finally {
  if($null -ne $p -and -not $p.HasExited){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue}
  Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue
}
