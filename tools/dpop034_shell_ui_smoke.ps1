[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ExePath,
    [Parameter(Mandatory)][string]$OutputDir
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$ExePath=[IO.Path]::GetFullPath($ExePath)
$OutputDir=[IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class DPop034ShellWin32 {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
  [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr hWnd,uint cmd);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hWnd,StringBuilder text,int max);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr hWnd,StringBuilder text,int max);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hWnd,uint msg,IntPtr wp,IntPtr lp);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool GetWindowRect(IntPtr hWnd,out RECT rect);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool GetClientRect(IntPtr hWnd,out RECT rect);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool MoveWindow(IntPtr hWnd,int x,int y,int w,int h,bool repaint);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool PrintWindow(IntPtr hWnd,IntPtr hdc,uint flags);
}
'@
function Get-Text([IntPtr]$h){
  $b=New-Object Text.StringBuilder 512
  [void][DPop034ShellWin32]::GetWindowText($h,$b,$b.Capacity)
  $b.ToString()
}
function Get-Class([IntPtr]$h){
  $b=New-Object Text.StringBuilder 128
  [void][DPop034ShellWin32]::GetClassName($h,$b,$b.Capacity)
  $b.ToString()
}
function Get-DirectChildren([IntPtr]$parent){
  $result=@()
  $child=[DPop034ShellWin32]::GetWindow($parent,5)
  while($child -ne [IntPtr]::Zero){
    $result += $child
    $child=[DPop034ShellWin32]::GetWindow($child,2)
  }
  return $result
}
function Find-DirectButton([IntPtr]$parent,[string]$text){
  foreach($h in Get-DirectChildren $parent){
    if([DPop034ShellWin32]::IsWindowVisible($h) -and (Get-Class $h) -eq 'Button' -and (Get-Text $h) -eq $text){ return $h }
  }
  return [IntPtr]::Zero
}
function Resize-Client([IntPtr]$h,[int]$w,[int]$height){
  $wr=New-Object DPop034ShellWin32+RECT
  $cr=New-Object DPop034ShellWin32+RECT
  if(-not [DPop034ShellWin32]::GetWindowRect($h,[ref]$wr)){throw 'GetWindowRect failed'}
  if(-not [DPop034ShellWin32]::GetClientRect($h,[ref]$cr)){throw 'GetClientRect failed'}
  $ow=$w+(($wr.Right-$wr.Left)-($cr.Right-$cr.Left))
  $oh=$height+(($wr.Bottom-$wr.Top)-($cr.Bottom-$cr.Top))
  if(-not [DPop034ShellWin32]::MoveWindow($h,$wr.Left,$wr.Top,$ow,$oh,$true)){throw 'MoveWindow failed'}
  Start-Sleep -Milliseconds 650
}
function Capture([IntPtr]$h,[string]$name){
  $r=New-Object DPop034ShellWin32+RECT
  if(-not [DPop034ShellWin32]::GetWindowRect($h,[ref]$r)){throw 'capture rect failed'}
  $w=$r.Right-$r.Left; $height=$r.Bottom-$r.Top
  if($w -lt 800 -or $height -lt 550){throw "invalid capture size $w x $height"}
  $bmp=New-Object Drawing.Bitmap($w,$height)
  $g=[Drawing.Graphics]::FromImage($bmp); $hdc=$g.GetHdc()
  try {
    if(-not [DPop034ShellWin32]::PrintWindow($h,$hdc,2)){
      if(-not [DPop034ShellWin32]::PrintWindow($h,$hdc,0)){throw "PrintWindow failed: $name"}
    }
  } finally { $g.ReleaseHdc($hdc); $g.Dispose() }
  try {
    $path=Join-Path $OutputDir ($name+'.png')
    $bmp.Save($path,[Drawing.Imaging.ImageFormat]::Png)
    return $path
  } finally { $bmp.Dispose() }
}
function Open-Section([IntPtr]$main,[string]$label){
  $button=Find-DirectButton $main $label
  if($button -eq [IntPtr]::Zero){throw "Sidebar button not found: $label"}
  [void][DPop034ShellWin32]::SendMessage($button,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
  Start-Sleep -Milliseconds 850
}
$p=$null
try {
  $p=Start-Process -FilePath $ExePath -PassThru
  $deadline=(Get-Date).AddSeconds(30)
  do {
    Start-Sleep -Milliseconds 250; $p.Refresh()
    if($p.HasExited){throw 'App exited early'}
  } while($p.MainWindowHandle -eq 0 -and (Get-Date)-lt $deadline)
  if($p.MainWindowHandle -eq 0){throw 'Main window timeout'}
  $main=[IntPtr]$p.MainWindowHandle
  Resize-Client $main 1100 700

  $expected=@('Обзор','Очистка','ОЗУ','DPopGuard','Автозагрузка','Диск','Приложения','Windows','Дубликаты','Инструменты','Zapret Center','Обновления','Настройки')
  foreach($label in $expected){
    if((Find-DirectButton $main $label) -eq [IntPtr]::Zero){throw "Missing visible sidebar section: $label"}
  }

  Open-Section $main 'Обзор'; $overview=Capture $main 'sidebar-overview-1100x700'
  Open-Section $main 'Автозагрузка'; $startup=Capture $main 'sidebar-startup-1100x700'
  Open-Section $main 'Обновления'; $updates=Capture $main 'sidebar-updates-1100x700'
  Open-Section $main 'Настройки'; $settings=Capture $main 'sidebar-settings-1100x700'

  [pscustomobject]@{
    target='DPopCleaner 0.3.4 BETA R1'
    visible_sidebar_sections=$expected.Count
    overview=$overview
    startup=$startup
    updates=$updates
    settings=$settings
  } | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $OutputDir 'shell-ui-smoke-report.json') -Encoding utf8

  [void][DPop034ShellWin32]::SendMessage($main,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)
  if(-not $p.WaitForExit(10000)){throw 'App did not close'}
  if($p.ExitCode -ne 0){throw "App exited with code $($p.ExitCode)"}
} finally {
  if($null -ne $p -and -not $p.HasExited){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue}
}
