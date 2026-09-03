[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$RootPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev17-zapret-responsive'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RootPath = if ([IO.Path]::IsPathRooted($RootPath)) { $RootPath } else { Join-Path $repoRoot $RootPath }
$OutputDir = if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repoRoot $OutputDir }
$RootPath = [IO.Path]::GetFullPath($RootPath)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$launcherPath = Join-Path $RootPath 'DPopCleaner.exe'
$corePath = Join-Path $RootPath 'DPopCleaner.Core.exe'
foreach ($required in @($launcherPath,$corePath,(Join-Path $RootPath 'SimpleUpdate.exe'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "rev.17 responsive smoke prerequisite missing: $required" }
}

$native = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class Rev17ResponsiveChild {
    public IntPtr Handle; public int Id; public string Text; public string ClassName; public bool Visible;
    public int Left; public int Top; public int Right; public int Bottom;
}
public static class Rev17ResponsiveNative {
    private delegate bool EnumProc(IntPtr hwnd, IntPtr p);
    [StructLayout(LayoutKind.Sequential)] private struct RECT { public int Left,Top,Right,Bottom; }
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc proc, IntPtr p);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd,StringBuilder text,int max);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd,StringBuilder text,int max);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hwnd,out RECT rect);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hwnd,IntPtr after,int x,int y,int cx,int cy,uint flags);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd,uint msg,IntPtr wp,IntPtr lp);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd,IntPtr hdc,uint flags);
    public static Rev17ResponsiveChild[] Children(IntPtr parent) {
        var list=new List<Rev17ResponsiveChild>();
        EnumProc cb=delegate(IntPtr h,IntPtr _) {
            var t=new StringBuilder(1024); var c=new StringBuilder(128); RECT r;
            GetWindowText(h,t,t.Capacity); GetClassName(h,c,c.Capacity); GetWindowRect(h,out r);
            list.Add(new Rev17ResponsiveChild{Handle=h,Id=GetDlgCtrlID(h),Text=t.ToString(),ClassName=c.ToString(),Visible=IsWindowVisible(h),Left=r.Left,Top=r.Top,Right=r.Right,Bottom=r.Bottom});
            return true;
        };
        EnumChildWindows(parent,cb,IntPtr.Zero); GC.KeepAlive(cb); return list.ToArray();
    }
    public static Rev17ResponsiveChild Bounds(IntPtr hwnd) {
        RECT r; if(!GetWindowRect(hwnd,out r)) return null;
        var t=new StringBuilder(1024); var c=new StringBuilder(128);
        GetWindowText(hwnd,t,t.Capacity); GetClassName(hwnd,c,c.Capacity);
        return new Rev17ResponsiveChild{Handle=hwnd,Id=GetDlgCtrlID(hwnd),Text=t.ToString(),ClassName=c.ToString(),Visible=IsWindowVisible(hwnd),Left=r.Left,Top=r.Top,Right=r.Right,Bottom=r.Bottom};
    }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Get-Children([IntPtr]$Window) { @([Rev17ResponsiveNative]::Children($Window)) }
function Test-Overlap($A,$B) { $A.Left -lt $B.Right -and $A.Right -gt $B.Left -and $A.Top -lt $B.Bottom -and $A.Bottom -gt $B.Top }
function Wait-Until([int]$Seconds,[string]$Description,[scriptblock]$Condition) {
    $deadline=[DateTime]::UtcNow.AddSeconds($Seconds)
    do { if(& $Condition){ return }; Start-Sleep -Milliseconds 120 } while([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for $Description."
}
function Get-CoreProcess([string]$ExpectedPath) {
    $expected=[IO.Path]::GetFullPath($ExpectedPath)
    foreach($candidate in @(Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue)) {
        try {
            $candidate.Refresh()
            if(-not $candidate.HasExited -and $candidate.MainWindowHandle -ne [IntPtr]::Zero -and $candidate.Path -and [IO.Path]::GetFullPath($candidate.Path).Equals($expected,[StringComparison]::OrdinalIgnoreCase)) { return $candidate }
        } catch { }
    }
    $null
}
function Capture-Window([IntPtr]$Window,[string]$Path) {
    $bounds=[Rev17ResponsiveNative]::Bounds($Window)
    if(-not $bounds){ throw 'Could not read main-window bounds for rev.17 screenshot.' }
    $width=[Math]::Max(1,$bounds.Right-$bounds.Left); $height=[Math]::Max(1,$bounds.Bottom-$bounds.Top)
    $bitmap=[Drawing.Bitmap]::new($width,$height)
    $graphics=[Drawing.Graphics]::FromImage($bitmap)
    $hdc=$graphics.GetHdc()
    try {
        if(-not [Rev17ResponsiveNative]::PrintWindow($Window,$hdc,2)) { throw 'PrintWindow failed for rev.17 responsive screenshot.' }
    }
    finally { $graphics.ReleaseHdc($hdc); $graphics.Dispose() }
    try { $bitmap.Save($Path,[Drawing.Imaging.ImageFormat]::Png) } finally { $bitmap.Dispose() }
}
function Assert-SameRow([object[]]$Buttons,[int[]]$Ids,[string]$Name) {
    $row=@($Buttons | Where-Object { $Ids -contains $_.Id })
    if($row.Count -ne $Ids.Count){ throw "$Name row expected $($Ids.Count) buttons, found $($row.Count)." }
    $tops=@($row | ForEach-Object { $_.Top })
    $bottoms=@($row | ForEach-Object { $_.Bottom })
    if((($tops | Measure-Object -Maximum).Maximum - ($tops | Measure-Object -Minimum).Minimum) -gt 2 -or (($bottoms | Measure-Object -Maximum).Maximum - ($bottoms | Measure-Object -Minimum).Minimum) -gt 2) {
        $detail=@($row | ForEach-Object { 'id='+$_.Id+' ['+$_.Top+','+$_.Bottom+']' }) -join '; '
        throw "$Name row is not aligned: $detail"
    }
    $row
}

$targetIds=@(1701,1702,1703,1704,1705,1707,1708,1710,1711,1713,1714,1716,1717,1720,1721,1722,1723,1724,1725)
$strategyIds=@(1701,1713,1714,1703)
$updateIds=@(1724,1725,1716,1717,1702)
$actionIds=@(1720,1721,1722,1723)
$additionalIds=@(1704,1705,1707,1708,1710,1711)
$settingsPath=Join-Path $OutputDir 'rev17-responsive-settings.json'
Remove-Item -LiteralPath $settingsPath -Force -ErrorAction SilentlyContinue
$launcher=$null; $core=$null
$reports=@()
try {
    $launcher=Start-Process -FilePath $launcherPath -ArgumentList @('--no-update-check','--settings-path',('"'+$settingsPath+'"')) -WorkingDirectory $RootPath -PassThru
    Wait-Until 18 'installed frozen core window' { $script:core=Get-CoreProcess $corePath; $null -ne $script:core }
    $core=$script:core
    $window=$core.MainWindowHandle

    $zapretTab=Get-Children $window | Where-Object { $_.Visible -and $_.ClassName -eq 'Button' -and $_.Id -eq 905 } | Select-Object -First 1
    if(-not $zapretTab){ throw 'Zapret tab id=905 was not found.' }
    [void][Rev17ResponsiveNative]::SendMessage($zapretTab.Handle,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
    Wait-Until 8 'rev.17 responsive Zapret controls' {
        $visible=@(Get-Children $window | Where-Object { $_.Visible -and $_.ClassName -eq 'Button' -and $targetIds -contains $_.Id })
        $visible.Count -eq $targetIds.Count
    }

    foreach($size in @(
        [pscustomobject]@{Name='base';Width=1024;Height=768},
        [pscustomobject]@{Name='wide';Width=1366;Height=800},
        [pscustomobject]@{Name='user-like';Width=1680;Height=840}
    )) {
        # SWP_NOSENDCHANGING is intentional here: the frozen 0.2.14 core clamps
        # WM_WINDOWPOSCHANGING to its historical max. The smoke must force the
        # physical wide size so rev.17 layout is exercised at real 1366/1680 widths.
        if(-not [Rev17ResponsiveNative]::SetWindowPos($window,[IntPtr]::Zero,0,0,$size.Width,$size.Height,0x0002 -bor 0x0004 -bor 0x0010 -bor 0x0400)) {
            throw "Could not resize frozen window for $($size.Name)."
        }
        Start-Sleep -Milliseconds 900

        $windowBounds=[Rev17ResponsiveNative]::Bounds($window)
        if(-not $windowBounds){ throw "$($size.Name): could not read resized main-window bounds." }
        $actualWidth=$windowBounds.Right-$windowBounds.Left
        $actualHeight=$windowBounds.Bottom-$windowBounds.Top
        if($actualWidth -lt ($size.Width-16)) {
            throw "$($size.Name): requested width $($size.Width) was not actually reached; actual=${actualWidth}x${actualHeight}. Wide-screen responsive evidence would be invalid."
        }
        $children=@(Get-Children $window)
        $buttons=@($children | Where-Object { $_.Visible -and $_.ClassName -eq 'Button' -and $targetIds -contains $_.Id })
        if($buttons.Count -ne $targetIds.Count){ throw "$($size.Name): expected $($targetIds.Count) target buttons, found $($buttons.Count)." }

        $status=$children | Where-Object { $_.Visible -and $_.ClassName -eq 'Edit' } | Sort-Object Top | Select-Object -First 1
        if(-not $status){ throw "$($size.Name): upper Zapret status block not found." }
        $combos=@($children | Where-Object { $_.Visible -and $_.ClassName -eq 'ComboBox' } | Sort-Object Top)
        if($combos.Count -lt 2){ throw "$($size.Name): expected strategy + filter ComboBoxes, found $($combos.Count)." }

        $safeRight=$windowBounds.Right-8
        for($i=0;$i -lt $buttons.Count;$i++) {
            $b=$buttons[$i]
            if($b.Left -lt ($status.Left-2) -or $b.Right -gt $safeRight) { throw "$($size.Name): button id=$($b.Id) escaped resized client width [$($status.Left),$safeRight] => [$($b.Left),$($b.Right)]." }
            $width=$b.Right-$b.Left
            $needed=[Windows.Forms.TextRenderer]::MeasureText([string]$b.Text,[Drawing.SystemFonts]::MessageBoxFont).Width+8
            if($width -lt $needed){ throw "$($size.Name): clipped button id=$($b.Id) width=$width needed=$needed text='$($b.Text)'." }
            for($j=$i+1;$j -lt $buttons.Count;$j++) {
                if(Test-Overlap $b $buttons[$j]){ throw "$($size.Name): overlapping buttons id=$($b.Id) and id=$($buttons[$j].Id)." }
            }
            foreach($combo in $combos) {
                if(Test-Overlap $b $combo){ throw "$($size.Name): button id=$($b.Id) overlaps ComboBox [$($combo.Left),$($combo.Top),$($combo.Right),$($combo.Bottom)]." }
            }
        }

        foreach($caption in @('Стратегия','Дополнительно')) {
            $label=$children | Where-Object { $_.Visible -and $_.ClassName -eq 'Static' -and $_.Text -eq $caption } | Select-Object -First 1
            if($label) {
                foreach($b in $buttons) { if(Test-Overlap $b $label){ throw "$($size.Name): button id=$($b.Id) overlaps '$caption' heading." } }
            }
        }

        $strategy=@(Assert-SameRow $buttons $strategyIds 'strategy')
        $update=@(Assert-SameRow $buttons $updateIds 'update')
        $action=@(Assert-SameRow $buttons $actionIds 'bridge-action')
        $additional=@(Assert-SameRow $buttons $additionalIds 'additional')
        if((($strategy | Measure-Object Bottom -Maximum).Maximum) -ge (($update | Measure-Object Top -Minimum).Minimum)) { throw "$($size.Name): strategy and update rows overlap vertically." }
        if((($update | Measure-Object Bottom -Maximum).Maximum) -ge (($action | Measure-Object Top -Minimum).Minimum)) { throw "$($size.Name): update and bridge-action rows overlap vertically." }
        if((($action | Measure-Object Bottom -Maximum).Maximum) -ge (($additional | Measure-Object Top -Minimum).Minimum)) { throw "$($size.Name): bridge-action and additional rows overlap vertically." }

        $heights=@($buttons | ForEach-Object { $_.Bottom-$_.Top })
        $minHeight=($heights | Measure-Object -Minimum).Minimum; $maxHeight=($heights | Measure-Object -Maximum).Maximum
        if(($maxHeight-$minHeight) -gt 2){
            $heightDetail=@($buttons | Sort-Object Id | ForEach-Object { 'id='+$_.Id+' h='+($_.Bottom-$_.Top)+' y='+$_.Top+'..'+$_.Bottom }) -join '; '
            throw "$($size.Name): unified Zapret button heights diverged min=$minHeight max=$maxHeight; $heightDetail"
        }
        $rightmost=($buttons | Measure-Object Right -Maximum).Maximum
        $unusedRight=$windowBounds.Right-$rightmost
        if($unusedRight -gt 48){ throw "$($size.Name): responsive rows leave too much unused window width: windowRight=$($windowBounds.Right) rightmost=$rightmost unused=$unusedRight." }

        $shot=Join-Path $OutputDir ("rev17-zapret-{0}-{1}x{2}.png" -f $size.Name,$size.Width,$size.Height)
        Capture-Window $window $shot
        $reports += [pscustomobject]@{
            name=$size.Name; requested_width=$size.Width; requested_height=$size.Height
            actual_width=$actualWidth; actual_height=$actualHeight
            status_left=$status.Left; window_right=$windowBounds.Right; rightmost_button=$rightmost; unused_right=$unusedRight
            buttons=$buttons.Count; min_button_height=$minHeight; max_button_height=$maxHeight; screenshot=$shot
        }
        Write-Host "REV17_ZAPRET_RESPONSIVE_SIZE_OK name=$($size.Name) requested=$($size.Width)x$($size.Height) actual=${actualWidth}x${actualHeight} span=$($status.Left)..$rightmost unusedRight=$unusedRight height=$minHeight"
    }

    $reports | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDir 'rev17-zapret-responsive-report.json') -Encoding utf8
    Write-Host 'REV17_ZAPRET_RESPONSIVE_SMOKE_OK'
}
finally {
    foreach($name in @('DPopCleaner','DPopCleaner.Core','SimpleUpdate')) {
        foreach($p in @(Get-Process -Name $name -ErrorAction SilentlyContinue)) {
            try {
                if($p.Path -and [IO.Path]::GetFullPath($p.Path).StartsWith($RootPath,[StringComparison]::OrdinalIgnoreCase)) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
            } catch { }
        }
    }
}
