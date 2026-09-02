[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$InstallerPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev16-zapret-presentation'
)

$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

$native=@'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class Rev16PresentationChild {
    public IntPtr Handle; public int Id; public string Text; public string ClassName; public bool Visible;
    public int Left; public int Top; public int Right; public int Bottom;
}
public static class Rev16PresentationNative {
    private delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
    [StructLayout(LayoutKind.Sequential)] private struct RECT { public int Left,Top,Right,Bottom; }
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc proc, IntPtr lParam);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd,StringBuilder text,int max);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd,StringBuilder text,int max);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hwnd,out RECT rect);
    [DllImport("user32.dll")] private static extern IntPtr GetParent(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd,uint msg,IntPtr wp,IntPtr lp);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd,uint msg,IntPtr wp,IntPtr lp);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern IntPtr SendMessage(IntPtr hwnd,uint msg,IntPtr wp,StringBuilder lp);
    [DllImport("user32.dll",EntryPoint="GetWindowLongPtrW")] private static extern IntPtr GetWindowLongPtr64(IntPtr hwnd,int index);
    [DllImport("user32.dll",EntryPoint="GetWindowLongW")] private static extern int GetWindowLong32(IntPtr hwnd,int index);

    public static Rev16PresentationChild[] Children(IntPtr parent) {
        var result=new List<Rev16PresentationChild>();
        EnumProc cb=delegate(IntPtr h,IntPtr _) {
            var t=new StringBuilder(1024); var c=new StringBuilder(128); RECT r;
            GetWindowText(h,t,t.Capacity); GetClassName(h,c,c.Capacity); GetWindowRect(h,out r);
            result.Add(new Rev16PresentationChild{Handle=h,Id=GetDlgCtrlID(h),Text=t.ToString(),ClassName=c.ToString(),Visible=IsWindowVisible(h),Left=r.Left,Top=r.Top,Right=r.Right,Bottom=r.Bottom});
            return true;
        };
        EnumChildWindows(parent,cb,IntPtr.Zero); GC.KeepAlive(cb); return result.ToArray();
    }
    public static Rev16PresentationChild Bounds(IntPtr hwnd) {
        RECT r; if(!GetWindowRect(hwnd,out r)) return null;
        var t=new StringBuilder(1024); var c=new StringBuilder(128); GetWindowText(hwnd,t,t.Capacity); GetClassName(hwnd,c,c.Capacity);
        return new Rev16PresentationChild{Handle=hwnd,Id=GetDlgCtrlID(hwnd),Text=t.ToString(),ClassName=c.ToString(),Visible=IsWindowVisible(hwnd),Left=r.Left,Top=r.Top,Right=r.Right,Bottom=r.Bottom};
    }
    public static string[] ComboItems(IntPtr combo) {
        const uint GETCOUNT=0x0146,GETTEXT=0x0148,GETLEN=0x0149;
        var values=new List<string>(); int count=SendMessage(combo,GETCOUNT,IntPtr.Zero,IntPtr.Zero).ToInt32();
        for(int i=0;i<count;i++){int len=SendMessage(combo,GETLEN,(IntPtr)i,IntPtr.Zero).ToInt32(); var b=new StringBuilder(Math.Max(1,len+1)); SendMessage(combo,GETTEXT,(IntPtr)i,b); values.Add(b.ToString());}
        return values.ToArray();
    }
    public static void SelectCombo(IntPtr main,IntPtr combo,int index) {
        const uint CB_SETCURSEL=0x014E,WM_COMMAND=0x0111; const int CBN_SELCHANGE=1;
        SendMessage(combo,CB_SETCURSEL,(IntPtr)index,IntPtr.Zero);
        var parent=GetParent(combo); if(parent==IntPtr.Zero) parent=main;
        long wp=((long)CBN_SELCHANGE<<16)|(uint)(ushort)GetDlgCtrlID(combo);
        SendMessage(parent,WM_COMMAND,(IntPtr)wp,combo);
    }
    public static long Style(IntPtr hwnd) {
        const int GWL_STYLE=-16;
        return IntPtr.Size==8 ? GetWindowLongPtr64(hwnd,GWL_STYLE).ToInt64() : GetWindowLong32(hwnd,GWL_STYLE);
    }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Get-Children([IntPtr]$Window) { @([Rev16PresentationNative]::Children($Window)) }
function Wait-Until([int]$Seconds,[string]$Description,[scriptblock]$Condition) {
    $deadline=[DateTime]::UtcNow.AddSeconds($Seconds)
    do { if(& $Condition){ return }; Start-Sleep -Milliseconds 150 } while([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for $Description."
}
function Get-CoreWindow([string]$Root) {
    $expected=[IO.Path]::GetFullPath((Join-Path $Root 'DPopCleaner.Core.exe'))
    $deadline=[DateTime]::UtcNow.AddSeconds(15)
    do {
        foreach($p in @(Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue)) {
            try {
                $p.Refresh()
                if($p.HasExited -or $p.MainWindowHandle -eq [IntPtr]::Zero){ continue }
                if($p.Path -and [IO.Path]::GetFullPath($p.Path).Equals($expected,[StringComparison]::OrdinalIgnoreCase)){ return $p }
            } catch { }
        }
        Start-Sleep -Milliseconds 150
    } while([DateTime]::UtcNow -lt $deadline)
    throw 'Installed frozen core window did not appear.'
}
function Click-Id([IntPtr]$Window,[int]$Id) {
    $button=Get-Children $Window | Where-Object { $_.Visible -and $_.Id -eq $Id -and $_.ClassName -eq 'Button' } | Select-Object -First 1
    if(-not $button){ throw "Visible button id=$Id not found." }
    if(-not [Rev16PresentationNative]::PostMessage($button.Handle,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)){ throw "Could not click id=$Id." }
    Start-Sleep -Milliseconds 250
}
function Wait-PageButton([IntPtr]$Window,[int]$Id) {
    Wait-Until 8 "page button id=$Id" {
        @(Get-Children $Window | Where-Object { $_.Visible -and $_.Id -eq $Id -and $_.ClassName -eq 'Button' }).Count -ge 1
    }
}
function Find-Journal([IntPtr]$Window) {
    $children=@(Get-Children $Window)
    $lists=@($children | Where-Object { $_.Visible -and $_.ClassName -eq 'ListBox' } | Sort-Object @{Expression={($_.Right-$_.Left)*($_.Bottom-$_.Top)}} -Descending)
    if($lists.Count -eq 0){ throw 'Visible native Journal ListBox was not found outside Zapret.' }
    $list=$lists[0]
    $heading=$children | Where-Object {
        $_.Visible -and $_.ClassName -eq 'Static' -and $_.Bottom -le ($list.Top+8) -and $_.Left -lt $list.Right -and $_.Right -gt $list.Left
    } | Sort-Object @{Expression={[Math]::Max(0,$list.Top-$_.Bottom)}} | Select-Object -First 1
    if(-not $heading){ throw 'Native Journal heading was not found structurally above ListBox.' }
    [pscustomobject]@{Heading=$heading;List=$list}
}
function Open-Ram-And-CaptureJournal([IntPtr]$Window) {
    Click-Id $Window 910
    Start-Sleep -Milliseconds 350
    Find-Journal $Window
}
function Set-NativeTheme([IntPtr]$Window,[ValidateSet('light','dark')][string]$Theme,[string]$Root) {
    Click-Id $Window 906
    Wait-PageButton $Window 1401
    $combos=@(Get-Children $Window | Where-Object { $_.Visible -and $_.ClassName -eq 'ComboBox' } | Sort-Object Top)
    if($combos.Count -lt 2){ throw "Expected Language + Theme ComboBox, found $($combos.Count)." }
    $combo=$combos[1]
    $items=@([Rev16PresentationNative]::ComboItems($combo.Handle))
    $pattern=if($Theme -eq 'light'){'(?i)light|свет'}else{'(?i)dark|т[её]мн|темн'}
    $index=-1
    for($i=0;$i -lt $items.Count;$i++){ if($items[$i] -match $pattern){ $index=$i; break } }
    if($index -lt 0){ throw "Could not resolve $Theme theme in native ComboBox: $($items -join ', ')" }
    [Rev16PresentationNative]::SelectCombo($Window,$combo.Handle,$index)
    Start-Sleep -Milliseconds 250
    Click-Id $Window 1401
    Start-Sleep -Milliseconds 700
    $core=Get-CoreWindow $Root
    [pscustomobject]@{Window=$core.MainWindowHandle;Value=$items[$index]}
}
function Get-VisibleTargetButtons([IntPtr]$Window) {
    $ids=@(1701,1702,1703,1704,1705,1707,1708,1710,1711,1713,1714,1716,1717,1720,1721,1722,1723,1724,1725)
    @(Get-Children $Window | Where-Object { $_.Visible -and $_.ClassName -eq 'Button' -and $ids -contains $_.Id })
}
function Assert-OwnerDrawAndLayout([IntPtr]$Window) {
    $buttons=@(Get-VisibleTargetButtons $Window)
    if($buttons.Count -lt 16){ throw "Too few visible Zapret action buttons for unified presentation: $($buttons.Count)." }
    foreach($b in $buttons) {
        $type=[Rev16PresentationNative]::Style($b.Handle) -band 0xF
        if($type -ne 0xB){ throw "Zapret button id=$($b.Id) is not BS_OWNERDRAW; style=0x$(([Rev16PresentationNative]::Style($b.Handle)).ToString('X'))." }
    }

    $actions=@($buttons | Where-Object { $_.Id -ge 1720 -and $_.Id -le 1723 } | Sort-Object Left)
    if($actions.Count -ne 4){ throw "Expected four rev.16 action buttons, found $($actions.Count)." }
    $tests=Get-Children $Window | Where-Object { $_.Visible -and $_.ClassName -eq 'Button' -and ($_.Text -eq 'Тесты' -or $_.Text -eq 'Tests') } | Select-Object -First 1
    if(-not $tests){ throw 'Native Tests button was not found for action-toolbar right boundary.' }
    for($i=0;$i -lt $actions.Count;$i++) {
        $width=$actions[$i].Right-$actions[$i].Left
        $needed=[Windows.Forms.TextRenderer]::MeasureText([string]$actions[$i].Text,[Drawing.SystemFonts]::MessageBoxFont).Width+8
        if($width -lt $needed){ throw "Zapret action text is clipped id=$($actions[$i].Id) width=$width needed=$needed text='$($actions[$i].Text)'." }
        if($actions[$i].Right -gt $tests.Right){ throw "Zapret action protrudes past panel boundary id=$($actions[$i].Id) right=$($actions[$i].Right) testsRight=$($tests.Right)." }
        if($i -gt 0 -and $actions[$i].Left -lt $actions[$i-1].Right){ throw "Zapret actions overlap ids=$($actions[$i-1].Id),$($actions[$i].Id)." }
    }
    Write-Host 'REV16_ZAPRET_BUTTON_LAYOUT_OK'
    $buttons
}
function Capture-Window([IntPtr]$Window,[object[]]$Buttons,[string]$Path) {
    $bounds=[Rev16PresentationNative]::Bounds($Window)
    if(-not $bounds){ throw 'Could not read main window bounds.' }
    $width=[Math]::Max(1,$bounds.Right-$bounds.Left); $height=[Math]::Max(1,$bounds.Bottom-$bounds.Top)
    $bmp=[Drawing.Bitmap]::new($width,$height)
    $graphics=[Drawing.Graphics]::FromImage($bmp)
    try { $graphics.CopyFromScreen($bounds.Left,$bounds.Top,0,0,[Drawing.Size]::new($width,$height)) }
    finally { $graphics.Dispose() }
    $bmp.Save($Path,[Drawing.Imaging.ImageFormat]::Png)

    $samples=@{}
    foreach($id in @(1704,1720)) {
        $button=$Buttons | Where-Object { $_.Id -eq $id } | Select-Object -First 1
        if(-not $button){ $bmp.Dispose(); throw "Sample button id=$id not found." }
        $x=[Math]::Max(1,[Math]::Min($width-2,$button.Left-$bounds.Left+5))
        $y=[Math]::Max(1,[Math]::Min($height-2,$button.Top-$bounds.Top+5))
        $samples[$id]=$bmp.GetPixel($x,$y)
    }
    $bmp.Dispose()
    $samples
}
function Color-Distance([Drawing.Color]$A,[Drawing.Color]$B) {
    [Math]::Abs([int]$A.R-[int]$B.R)+[Math]::Abs([int]$A.G-[int]$B.G)+[Math]::Abs([int]$A.B-[int]$B.B)
}
function Brightness([Drawing.Color]$Color) { [int]$Color.R+[int]$Color.G+[int]$Color.B }

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$installer=(Resolve-Path -LiteralPath $InstallerPath).Path
$installRoot=Join-Path ([IO.Path]::GetTempPath()) 'dpop0417-rev16-zapret-presentation'
$installed=$false; $launcher=$null; $core=$null; $failure=$null
$lightSamples=$null; $darkSamples=$null
try {
    Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue
    $setup=Start-Process -FilePath $installer -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/DIR="'+$installRoot+'"')) -PassThru -Wait
    if($setup.ExitCode -ne 0){ throw "Installer exit code $($setup.ExitCode)." }
    $installed=$true

    $settingsPath=Join-Path $installRoot 'rev16-presentation-settings.json'
    $launcherPath=Join-Path $installRoot 'SimpleUpdate.exe'
    $launcher=Start-Process -FilePath $launcherPath -ArgumentList @('--no-update-check','--settings-path',('"'+$settingsPath+'"')) -WorkingDirectory $installRoot -PassThru
    $core=Get-CoreWindow $installRoot
    $window=$core.MainWindowHandle

    $light=Set-NativeTheme $window light $installRoot
    $window=$light.Window
    $journal=Open-Ram-And-CaptureJournal $window
    Click-Id $window 905
    Wait-PageButton $window 1703
    Start-Sleep -Milliseconds 500
    if([Rev16PresentationNative]::IsWindowVisible($journal.List.Handle) -or [Rev16PresentationNative]::IsWindowVisible($journal.Heading.Handle)) {
        throw 'Native Journal remained visible on Zapret.'
    }
    $statusEdits=@(Get-Children $window | Where-Object { $_.Visible -and $_.ClassName -eq 'Edit' })
    if($statusEdits.Count -lt 1){ throw 'Zapret upper status block disappeared together with Journal.' }
    Write-Host 'REV16_ZAPRET_JOURNAL_HIDDEN_OK'

    $lightButtons=@(Assert-OwnerDrawAndLayout $window)
    $lightPath=Join-Path $OutputDir 'rev16-zapret-light.png'
    $lightSamples=Capture-Window $window $lightButtons $lightPath
    $lightBridge=[Drawing.Color]$lightSamples[1720]; $lightNative=[Drawing.Color]$lightSamples[1704]
    if((Brightness $lightBridge) -lt 500){ throw "Light bridge button pixel is too dark: $lightBridge" }
    if((Color-Distance $lightBridge $lightNative) -gt 90){ throw "Light native/bridge Zapret buttons are visually inconsistent: bridge=$lightBridge native=$lightNative" }
    Write-Host "REV16_ZAPRET_LIGHT_THEME_OK selected='$($light.Value)' bridge=$lightBridge native=$lightNative"

    Click-Id $window 910
    Start-Sleep -Milliseconds 400
    if(-not [Rev16PresentationNative]::IsWindowVisible($journal.List.Handle) -or -not [Rev16PresentationNative]::IsWindowVisible($journal.Heading.Handle)) {
        throw 'Native Journal was not restored after leaving Zapret.'
    }
    Write-Host 'REV16_ZAPRET_JOURNAL_RESTORED_OK'

    $dark=Set-NativeTheme $window dark $installRoot
    $window=$dark.Window
    $journalDark=Open-Ram-And-CaptureJournal $window
    Click-Id $window 905
    Wait-PageButton $window 1703
    Start-Sleep -Milliseconds 500
    if([Rev16PresentationNative]::IsWindowVisible($journalDark.List.Handle) -or [Rev16PresentationNative]::IsWindowVisible($journalDark.Heading.Handle)) {
        throw 'Native Journal remained visible on Zapret after dark-theme switch.'
    }
    $darkButtons=@(Assert-OwnerDrawAndLayout $window)
    $darkPath=Join-Path $OutputDir 'rev16-zapret-dark.png'
    $darkSamples=Capture-Window $window $darkButtons $darkPath
    $darkBridge=[Drawing.Color]$darkSamples[1720]; $darkNative=[Drawing.Color]$darkSamples[1704]
    if((Brightness $darkBridge) -gt 300){ throw "Dark bridge button pixel is too light: $darkBridge" }
    if((Color-Distance $darkBridge $darkNative) -gt 90){ throw "Dark native/bridge Zapret buttons are visually inconsistent: bridge=$darkBridge native=$darkNative" }
    if((Color-Distance $lightBridge $darkBridge) -lt 250){ throw "Theme switch did not materially change rendered Zapret button pixels: light=$lightBridge dark=$darkBridge" }
    Write-Host "REV16_ZAPRET_DARK_THEME_OK selected='$($dark.Value)' bridge=$darkBridge native=$darkNative"

    [pscustomobject]@{
        light_theme=$light.Value; dark_theme=$dark.Value
        light_bridge_pixel=$lightBridge.ToString(); light_native_pixel=$lightNative.ToString()
        dark_bridge_pixel=$darkBridge.ToString(); dark_native_pixel=$darkNative.ToString()
        light_screenshot=$lightPath; dark_screenshot=$darkPath
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputDir 'rev16-zapret-presentation-report.json') -Encoding utf8
}
catch {
    $failure=$_.Exception.Message
    Write-Host "REV16_ZAPRET_PRESENTATION_RED_DETAIL=$failure"
    throw
}
finally {
    foreach($name in @('DPopCleaner','DPopCleaner.Core','SimpleUpdate')) {
        foreach($p in @(Get-Process -Name $name -ErrorAction SilentlyContinue)) {
            try {
                if($p.Path -and [IO.Path]::GetFullPath($p.Path).StartsWith([IO.Path]::GetFullPath($installRoot),[StringComparison]::OrdinalIgnoreCase)){
                    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
                }
            } catch { }
        }
    }
    if($installed) {
        $uninstaller=Join-Path $installRoot 'unins000.exe'
        if(Test-Path -LiteralPath $uninstaller -PathType Leaf){ try { Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait | Out-Null } catch { } }
        Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Write-Host 'REV16_ZAPRET_PRESENTATION_SMOKE_OK'
exit 0
