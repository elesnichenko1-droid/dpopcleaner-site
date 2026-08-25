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

$settingsRoot = Join-Path $env:RUNNER_TEMP ('dpop035-settings-visual-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $settingsRoot -Force | Out-Null
@'
{
  "schema_version": 2,
  "confirm_destructive": true,
  "large_file_mb": 500,
  "duplicate_min_mb": 10,
  "run_at_startup": false,
  "always_run_as_admin": false,
  "check_updates_at_startup": true,
  "quick_guard_at_startup": false,
  "check_update_cache_at_startup": false,
  "background_junk_monitor": false,
  "tray_enabled": false,
  "close_behavior": 0,
  "memory_auto_trim_enabled": false,
  "memory_auto_trim_percent": 80,
  "memory_auto_trim_interval_minutes": 15,
  "memory_scope": 0,
  "clean_exclusions": ["C:\\Keep\\Example"]
}
'@ | Set-Content -LiteralPath (Join-Path $settingsRoot 'settings.json') -Encoding utf8

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class DPop035SettingsVisualWin32 {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
  public delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent, EnumProc cb, IntPtr data);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr parent, int id);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,uint msg,IntPtr wp,IntPtr lp);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool GetWindowRect(IntPtr h,out RECT r);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool GetClientRect(IntPtr h,out RECT r);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool MoveWindow(IntPtr h,int x,int y,int w,int ht,bool repaint);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder b, int n);
  public static IntPtr[] Children(IntPtr parent) {
    var result = new List<IntPtr>();
    EnumChildWindows(parent, (h,_) => { result.Add(h); return true; }, IntPtr.Zero);
    return result.ToArray();
  }
}
'@

function Text([IntPtr]$h) {
  if($h -eq [IntPtr]::Zero){ return '' }
  $b=New-Object Text.StringBuilder 4096
  [void][DPop035SettingsVisualWin32]::GetWindowText($h,$b,$b.Capacity)
  return $b.ToString()
}
function Find-VisibleById([IntPtr]$main,[int]$id) {
  foreach($h in [DPop035SettingsVisualWin32]::Children($main)) {
    if(-not [DPop035SettingsVisualWin32]::IsWindowVisible($h)){ continue }
    if([DPop035SettingsVisualWin32]::GetDlgCtrlID($h) -eq $id){ return $h }
  }
  return [IntPtr]::Zero
}
function Resize-Client([IntPtr]$h,[int]$w,[int]$ht) {
  $wr=New-Object DPop035SettingsVisualWin32+RECT
  $cr=New-Object DPop035SettingsVisualWin32+RECT
  if(-not [DPop035SettingsVisualWin32]::GetWindowRect($h,[ref]$wr)){ throw 'GetWindowRect failed' }
  if(-not [DPop035SettingsVisualWin32]::GetClientRect($h,[ref]$cr)){ throw 'GetClientRect failed' }
  $outerW=$w+(($wr.Right-$wr.Left)-($cr.Right-$cr.Left))
  $outerH=$ht+(($wr.Bottom-$wr.Top)-($cr.Bottom-$cr.Top))
  if(-not [DPop035SettingsVisualWin32]::MoveWindow($h,$wr.Left,$wr.Top,$outerW,$outerH,$true)){ throw 'MoveWindow failed' }
  Start-Sleep -Milliseconds 450
}
function Capture([IntPtr]$h,[string]$name) {
  $r=New-Object DPop035SettingsVisualWin32+RECT
  if(-not [DPop035SettingsVisualWin32]::GetWindowRect($h,[ref]$r)){ throw 'capture rect failed' }
  $w=$r.Right-$r.Left; $ht=$r.Bottom-$r.Top
  $bmp=New-Object Drawing.Bitmap($w,$ht)
  $g=[Drawing.Graphics]::FromImage($bmp)
  $dc=$g.GetHdc()
  try {
    if(-not [DPop035SettingsVisualWin32]::PrintWindow($h,$dc,2)){
      if(-not [DPop035SettingsVisualWin32]::PrintWindow($h,$dc,0)){ throw 'PrintWindow failed' }
    }
  } finally { $g.ReleaseHdc($dc); $g.Dispose() }
  try {
    $path=Join-Path $OutputDir ($name+'.png')
    $bmp.Save($path,[Drawing.Imaging.ImageFormat]::Png)
    return $path
  } finally { $bmp.Dispose() }
}

$p=$null
try {
  $env:DPOP_SETTINGS_ROOT=$settingsRoot
  $p=Start-Process -FilePath $ExePath -PassThru
  $deadline=(Get-Date).AddSeconds(30)
  do {
    Start-Sleep -Milliseconds 200
    $p.Refresh()
    if($p.HasExited){ throw "App exited before Settings visual smoke. ExitCode=$($p.ExitCode)" }
  } while($p.MainWindowHandle -eq 0 -and (Get-Date)-lt $deadline)
  if($p.MainWindowHandle -eq 0){ throw 'Main window timeout' }
  $main=[IntPtr]$p.MainWindowHandle

  $gear=Find-VisibleById $main 1100
  if($gear -eq [IntPtr]::Zero){ throw 'Settings gear 1100 not found' }
  [void][DPop035SettingsVisualWin32]::SendMessage($main,0x0111,[IntPtr]1100,$gear)
  Start-Sleep -Milliseconds 350

  $general=Find-VisibleById $main 3320
  if($general -eq [IntPtr]::Zero){ throw 'Settings General control 3320 not visible' }
  $page=[DPop035SettingsVisualWin32]::GetParent($general)
  if($page -eq [IntPtr]::Zero){ throw 'Settings page parent not found' }

  $sections=@(
    @{ Id=3300; Name='general' },
    @{ Id=3301; Name='cleaning' },
    @{ Id=3302; Name='memory' },
    @{ Id=3303; Name='protection' },
    @{ Id=3304; Name='exclusions' }
  )
  $captures=@()
  foreach($size in @(@{W=1200;H=850;Tag='1200x850'}, @{W=1100;H=700;Tag='1100x700'})) {
    Resize-Client $main $size.W $size.H
    foreach($section in $sections) {
      $button=[DPop035SettingsVisualWin32]::GetDlgItem($page,$section.Id)
      if($button -eq [IntPtr]::Zero){ throw "Settings section button $($section.Id) missing" }
      [void][DPop035SettingsVisualWin32]::SendMessage($page,0x0111,[IntPtr]$section.Id,$button)
      Start-Sleep -Milliseconds 180
      foreach($actionId in 3430..3433) {
        $action=[DPop035SettingsVisualWin32]::GetDlgItem($page,$actionId)
        if($action -eq [IntPtr]::Zero -or -not [DPop035SettingsVisualWin32]::IsWindowVisible($action)) {
          throw "Settings bottom action $actionId not visible in $($section.Name) at $($size.Tag)"
        }
      }
      $capture=Capture $main ("settings-{0}-{1}" -f $section.Name,$size.Tag)
      $captures += [pscustomobject]@{ section=$section.Name; size=$size.Tag; path=$capture }
    }
  }

  [pscustomobject]@{
    target='DPopCleaner 0.3.5 BETA R1'
    section_count=5
    sizes=@('1200x850','1100x700')
    captures=$captures
  } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDir 'settings-visual-report.json') -Encoding utf8

  [void][DPop035SettingsVisualWin32]::SendMessage($main,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)
  if(-not $p.WaitForExit(10000)){ throw 'App did not close after Settings visual smoke' }
  if($p.ExitCode -ne 0){ throw "App exited with code $($p.ExitCode)" }
  $p=$null
}
finally {
  Remove-Item Env:DPOP_SETTINGS_ROOT -ErrorAction SilentlyContinue
  if($null -ne $p -and -not $p.HasExited){ Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
  Remove-Item -LiteralPath $settingsRoot -Recurse -Force -ErrorAction SilentlyContinue
}
