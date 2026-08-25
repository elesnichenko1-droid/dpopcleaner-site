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

$settingsRoot = Join-Path $env:RUNNER_TEMP 'dpop035-settings-smoke'
$settingsPath = Join-Path $settingsRoot 'settings.json'
Remove-Item -LiteralPath $settingsRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $settingsRoot -Force | Out-Null
@'
{
  "schema_version": 2,
  "confirm_destructive": true,
  "large_file_mb": 500,
  "duplicate_min_mb": 10,
  "run_at_startup": false,
  "always_run_as_admin": false,
  "check_updates_at_startup": false,
  "quick_guard_at_startup": false,
  "check_update_cache_at_startup": false,
  "background_junk_monitor": false,
  "tray_enabled": false,
  "close_behavior": 0,
  "memory_auto_trim_enabled": false,
  "memory_auto_trim_percent": 80,
  "memory_auto_trim_interval_minutes": 15,
  "memory_scope": 0,
  "clean_exclusions": []
}
'@ | Set-Content -LiteralPath $settingsPath -Encoding utf8

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class DPop035SettingsWin32 {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
  public delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent, EnumProc cb, IntPtr data);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder b, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern bool SetWindowText(IntPtr h, string text);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr parent, int id);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,uint msg,IntPtr wp,IntPtr lp);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool GetWindowRect(IntPtr h,out RECT r);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
  public static IntPtr[] Children(IntPtr parent) {
    var result = new List<IntPtr>();
    EnumChildWindows(parent, (h,_) => { result.Add(h); return true; }, IntPtr.Zero);
    return result.ToArray();
  }
}
'@

function Text([IntPtr]$h) {
  if($h -eq [IntPtr]::Zero){return ''}
  $b=New-Object Text.StringBuilder 4096
  [void][DPop035SettingsWin32]::GetWindowText($h,$b,$b.Capacity)
  return $b.ToString()
}
function Find-VisibleById([IntPtr]$main,[int]$id) {
  foreach($h in [DPop035SettingsWin32]::Children($main)) {
    if(-not [DPop035SettingsWin32]::IsWindowVisible($h)){continue}
    if([DPop035SettingsWin32]::GetDlgCtrlID($h) -eq $id){return $h}
  }
  return [IntPtr]::Zero
}
function Start-App {
  $p=Start-Process -FilePath $ExePath -PassThru
  $deadline=(Get-Date).AddSeconds(30)
  do {
    Start-Sleep -Milliseconds 200
    $p.Refresh()
    if($p.HasExited){throw "DPopCleaner exited before window appeared. ExitCode=$($p.ExitCode)"}
  } while($p.MainWindowHandle -eq 0 -and (Get-Date)-lt $deadline)
  if($p.MainWindowHandle -eq 0){throw 'Main window timeout'}
  return $p
}
function Open-CleaningSettings([Diagnostics.Process]$p) {
  $main=[IntPtr]$p.MainWindowHandle
  $settingsNav=Find-VisibleById $main 1012
  if($settingsNav -eq [IntPtr]::Zero){throw 'Settings navigation button (1012) not found'}
  [void][DPop035SettingsWin32]::SendMessage($main,0x0111,[IntPtr]1012,$settingsNav)
  Start-Sleep -Milliseconds 300

  $large=Find-VisibleById $main 3343
  if($large -eq [IntPtr]::Zero){
    # Settings opens in General; resolve the page via its visible General control.
    $general=Find-VisibleById $main 3320
    if($general -eq [IntPtr]::Zero){throw 'Visible Settings page control not found'}
    $settingsPage=[DPop035SettingsWin32]::GetParent($general)
  } else {
    $settingsPage=[DPop035SettingsWin32]::GetParent($large)
  }
  if($settingsPage -eq [IntPtr]::Zero){throw 'Settings page parent not found'}

  $cleaningButton=[DPop035SettingsWin32]::GetDlgItem($settingsPage,3301)
  if($cleaningButton -eq [IntPtr]::Zero){throw 'Cleaning settings section (3301) not found'}
  [void][DPop035SettingsWin32]::SendMessage($settingsPage,0x0111,[IntPtr]3301,$cleaningButton)
  Start-Sleep -Milliseconds 250
  $large=[DPop035SettingsWin32]::GetDlgItem($settingsPage,3343)
  if($large -eq [IntPtr]::Zero -or -not [DPop035SettingsWin32]::IsWindowVisible($large)){
    throw 'Large-file setting (3343) is not visible after selecting Cleaning'
  }
  return [pscustomobject]@{ Main=$main; Page=$settingsPage; Large=$large }
}
function Close-App([Diagnostics.Process]$p,[IntPtr]$main) {
  [void][DPop035SettingsWin32]::SendMessage($main,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)
  if(-not $p.WaitForExit(10000)){throw 'DPopCleaner did not exit after WM_CLOSE'}
  if($p.ExitCode -ne 0){throw "DPopCleaner exited with code $($p.ExitCode)"}
}
function Capture([IntPtr]$h,[string]$name) {
  $r=New-Object DPop035SettingsWin32+RECT
  if(-not [DPop035SettingsWin32]::GetWindowRect($h,[ref]$r)){throw 'capture rect failed'}
  $w=$r.Right-$r.Left; $ht=$r.Bottom-$r.Top
  $bmp=New-Object Drawing.Bitmap($w,$ht); $g=[Drawing.Graphics]::FromImage($bmp); $dc=$g.GetHdc()
  try {
    if(-not [DPop035SettingsWin32]::PrintWindow($h,$dc,2)){
      if(-not [DPop035SettingsWin32]::PrintWindow($h,$dc,0)){throw 'PrintWindow failed'}
    }
  } finally { $g.ReleaseHdc($dc); $g.Dispose() }
  try {
    $path=Join-Path $OutputDir ($name+'.png')
    $bmp.Save($path,[Drawing.Imaging.ImageFormat]::Png)
    return $path
  } finally { $bmp.Dispose() }
}

$first=$null
$second=$null
try {
  $env:DPOP_SETTINGS_ROOT=$settingsRoot

  $first=Start-App
  $firstUi=Open-CleaningSettings $first
  if((Text $firstUi.Large) -ne '500'){throw "Initial large_file_mb UI value is not 500: $(Text $firstUi.Large)"}
  if(-not [DPop035SettingsWin32]::SetWindowText($firstUi.Large,'777')){throw 'Could not edit large_file_mb'}
  if((Text $firstUi.Large) -ne '777'){throw 'Edited large_file_mb did not reach control'}
  $save=[DPop035SettingsWin32]::GetDlgItem($firstUi.Page,3431)
  if($save -eq [IntPtr]::Zero){throw 'Settings Save button (3431) not found'}
  [void][DPop035SettingsWin32]::SendMessage($firstUi.Page,0x0111,[IntPtr]3431,$save)
  Start-Sleep -Milliseconds 250

  if(-not (Test-Path -LiteralPath $settingsPath -PathType Leaf)){throw 'settings.json disappeared after Save'}
  $saved=Get-Content -LiteralPath $settingsPath -Raw
  if($saved -notmatch '"large_file_mb"\s*:\s*777'){throw "settings.json did not persist large_file_mb=777: $saved"}
  Copy-Item -LiteralPath $settingsPath -Destination (Join-Path $OutputDir 'settings-after-save.json') -Force
  Close-App $first $firstUi.Main
  $first=$null

  $second=Start-App
  $secondUi=Open-CleaningSettings $second
  $restored=Text $secondUi.Large
  if($restored -ne '777'){throw "Restart did not restore large_file_mb=777 in UI. Actual=$restored"}
  $capture=Capture $secondUi.Main 'settings-persisted-777'
  Close-App $second $secondUi.Main
  $second=$null

  [pscustomobject]@{
    target='DPopCleaner 0.3.5 BETA R1'
    settings_root=$settingsRoot
    persisted_large_file_mb=777
    restart_restored=$true
    settings_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $OutputDir 'settings-after-save.json')).Hash.ToLowerInvariant()
    capture=$capture
  } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDir 'settings-smoke-report.json') -Encoding utf8
}
finally {
  Remove-Item Env:DPOP_SETTINGS_ROOT -ErrorAction SilentlyContinue
  foreach($p in @($first,$second)) {
    if($null -ne $p -and -not $p.HasExited){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue}
  }
  Remove-Item -LiteralPath $settingsRoot -Recurse -Force -ErrorAction SilentlyContinue
}
