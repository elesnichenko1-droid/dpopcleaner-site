[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$RootPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev19-zapret-cleanup',
    [switch]$SkipTray
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
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "rev.19 smoke prerequisite missing: $required" }
}

$native = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class Rev19Child {
    public IntPtr Handle; public int Id; public string Text; public string ClassName; public bool Visible; public int OwnerPid;
    public int Left; public int Top; public int Right; public int Bottom;
}
public sealed class Rev19TrayEntry {
    public IntPtr Toolbar; public IntPtr Hwnd; public uint IconId; public IntPtr HIcon; public int OwnerPid;
    public string Title; public string ClassName; public string ButtonText;
    public string Identity { get { return "0x" + Hwnd.ToInt64().ToString("X") + ":" + IconId.ToString(); } }
    public override string ToString() { return "owner="+OwnerPid+";identity="+Identity+";hicon=0x"+HIcon.ToInt64().ToString("X")+";title="+(Title??"")+";text="+(ButtonText??""); }
}
public static class Rev19Native {
    private const int TB_BUTTONCOUNT=0x0418, TB_GETBUTTON=0x0417, TB_GETBUTTONTEXTW=0x044B;
    private const uint PROCESS_VM_OPERATION=0x0008, PROCESS_VM_READ=0x0010, PROCESS_VM_WRITE=0x0020, PROCESS_QUERY_INFORMATION=0x0400;
    private const uint MEM_COMMIT=0x1000, MEM_RESERVE=0x2000, MEM_RELEASE=0x8000, PAGE_READWRITE=0x04;
    private delegate bool EnumProc(IntPtr hwnd, IntPtr p);
    [StructLayout(LayoutKind.Sequential)] private struct RECT { public int Left,Top,Right,Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X,Y; public POINT(int x,int y){X=x;Y=y;} }
    [StructLayout(LayoutKind.Sequential)] private struct TBBUTTON64 { public int iBitmap; public int idCommand; public byte fsState; public byte fsStyle; [MarshalAs(UnmanagedType.ByValArray,SizeConst=6)] public byte[] reserved; public UIntPtr dwData; public IntPtr iString; }
    [StructLayout(LayoutKind.Sequential)] private struct TRAYDATA64 { public IntPtr hwnd; public uint uID; public uint callback; public uint r0; public uint r1; public IntPtr hIcon; }

    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent,EnumProc cb,IntPtr p);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern IntPtr FindWindow(string cls,string title);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd,StringBuilder text,int max);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd,StringBuilder text,int max);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hwnd,out RECT r);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hwnd,IntPtr after,int x,int y,int cx,int cy,uint flags);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd,uint msg,IntPtr wp,IntPtr lp);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd,IntPtr hdc,uint flags);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT point);
    [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr hwnd,uint flags);
    [DllImport("uxtheme.dll")] public static extern IntPtr GetWindowTheme(IntPtr hwnd);
    [DllImport("kernel32.dll",SetLastError=true)] private static extern IntPtr OpenProcess(uint access,bool inherit,uint pid);
    [DllImport("kernel32.dll",SetLastError=true)] private static extern IntPtr VirtualAllocEx(IntPtr p,IntPtr a,UIntPtr s,uint type,uint protect);
    [DllImport("kernel32.dll",SetLastError=true)] private static extern bool VirtualFreeEx(IntPtr p,IntPtr a,UIntPtr s,uint type);
    [DllImport("kernel32.dll",SetLastError=true)] private static extern bool ReadProcessMemory(IntPtr p,IntPtr a,IntPtr b,UIntPtr s,out UIntPtr read);
    [DllImport("kernel32.dll")] private static extern bool CloseHandle(IntPtr h);

    public static Rev19Child[] Children(IntPtr parent) {
        var list=new List<Rev19Child>(); EnumProc cb=delegate(IntPtr h,IntPtr _) {
            var t=new StringBuilder(1024); var c=new StringBuilder(128); RECT r; uint ownerPid=0;
            GetWindowText(h,t,t.Capacity); GetClassName(h,c,c.Capacity); GetWindowRect(h,out r); GetWindowThreadProcessId(h,out ownerPid);
            list.Add(new Rev19Child{Handle=h,Id=GetDlgCtrlID(h),Text=t.ToString(),ClassName=c.ToString(),Visible=IsWindowVisible(h),OwnerPid=(int)ownerPid,Left=r.Left,Top=r.Top,Right=r.Right,Bottom=r.Bottom}); return true;
        }; EnumChildWindows(parent,cb,IntPtr.Zero); GC.KeepAlive(cb); return list.ToArray();
    }
    public static Rev19Child Bounds(IntPtr hwnd) {
        if(hwnd==IntPtr.Zero)return null; RECT r; if(!GetWindowRect(hwnd,out r))return null; var t=new StringBuilder(1024); var c=new StringBuilder(128); uint ownerPid=0;
        GetWindowText(hwnd,t,t.Capacity); GetClassName(hwnd,c,c.Capacity); GetWindowThreadProcessId(hwnd,out ownerPid);
        return new Rev19Child{Handle=hwnd,Id=GetDlgCtrlID(hwnd),Text=t.ToString(),ClassName=c.ToString(),Visible=IsWindowVisible(hwnd),OwnerPid=(int)ownerPid,Left=r.Left,Top=r.Top,Right=r.Right,Bottom=r.Bottom};
    }
    public static Rev19TrayEntry[] Entries() {
        var result=new List<Rev19TrayEntry>(); foreach(var toolbar in Toolbars())Collect(toolbar,result); return result.ToArray();
    }
    private static List<IntPtr> Toolbars() { var result=new List<IntPtr>(); Add(FindWindow("Shell_TrayWnd",null),result); Add(FindWindow("NotifyIconOverflowWindow",null),result); return result; }
    private static void Add(IntPtr root,List<IntPtr> result) {
        if(root==IntPtr.Zero)return; EnumProc cb=delegate(IntPtr h,IntPtr _) { var s=new StringBuilder(128); GetClassName(h,s,s.Capacity); if(s.ToString()=="ToolbarWindow32"&&!result.Contains(h))result.Add(h); return true; }; EnumChildWindows(root,cb,IntPtr.Zero); GC.KeepAlive(cb);
    }
    private static void Collect(IntPtr toolbar,List<Rev19TrayEntry> result) {
        uint shellPid; GetWindowThreadProcessId(toolbar,out shellPid); if(shellPid==0)return;
        IntPtr p=OpenProcess(PROCESS_VM_OPERATION|PROCESS_VM_READ|PROCESS_VM_WRITE|PROCESS_QUERY_INFORMATION,false,shellPid); if(p==IntPtr.Zero)return;
        int buttonSize=Marshal.SizeOf(typeof(TBBUTTON64)); IntPtr rb=VirtualAllocEx(p,IntPtr.Zero,new UIntPtr((uint)buttonSize),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE); IntPtr rt=VirtualAllocEx(p,IntPtr.Zero,new UIntPtr(2048),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        if(rb==IntPtr.Zero||rt==IntPtr.Zero){if(rb!=IntPtr.Zero)VirtualFreeEx(p,rb,UIntPtr.Zero,MEM_RELEASE);if(rt!=IntPtr.Zero)VirtualFreeEx(p,rt,UIntPtr.Zero,MEM_RELEASE);CloseHandle(p);return;}
        try { int count=SendMessage(toolbar,TB_BUTTONCOUNT,IntPtr.Zero,IntPtr.Zero).ToInt32(); for(int i=0;i<count;i++) {
            if(SendMessage(toolbar,TB_GETBUTTON,new IntPtr(i),rb)==IntPtr.Zero)continue; TBBUTTON64 b; if(!Read(p,rb,out b)||b.dwData==UIntPtr.Zero)continue; TRAYDATA64 d; if(!Read(p,new IntPtr(unchecked((long)b.dwData.ToUInt64())),out d)||d.hwnd==IntPtr.Zero)continue;
            uint owner; GetWindowThreadProcessId(d.hwnd,out owner); var title=new StringBuilder(512); GetWindowText(d.hwnd,title,title.Capacity); var cls=new StringBuilder(128); GetClassName(d.hwnd,cls,cls.Capacity); string text="";
            int n=SendMessage(toolbar,TB_GETBUTTONTEXTW,new IntPtr(b.idCommand),rt).ToInt32(); if(n>0)text=ReadUtf16(p,rt,Math.Min(1023,n+1)); result.Add(new Rev19TrayEntry{Toolbar=toolbar,Hwnd=d.hwnd,IconId=d.uID,HIcon=d.hIcon,OwnerPid=(int)owner,Title=title.ToString(),ClassName=cls.ToString(),ButtonText=text});
        }} finally { VirtualFreeEx(p,rb,UIntPtr.Zero,MEM_RELEASE); VirtualFreeEx(p,rt,UIntPtr.Zero,MEM_RELEASE); CloseHandle(p); }
    }
    private static string ReadUtf16(IntPtr p,IntPtr remote,int chars){int bytes=Math.Max(2,chars*2);IntPtr local=Marshal.AllocHGlobal(bytes);try{UIntPtr read;if(!ReadProcessMemory(p,remote,local,new UIntPtr((uint)bytes),out read))return "";int usable=(int)Math.Min((ulong)bytes,read.ToUInt64());return usable<=0?"":Marshal.PtrToStringUni(local,usable/2).TrimEnd('\0');}finally{Marshal.FreeHGlobal(local);}}
    private static bool Read<T>(IntPtr p,IntPtr remote,out T value) where T:struct {int size=Marshal.SizeOf(typeof(T));IntPtr local=Marshal.AllocHGlobal(size);try{UIntPtr n;if(!ReadProcessMemory(p,remote,local,new UIntPtr((uint)size),out n)||n.ToUInt64()<(ulong)size){value=default(T);return false;}value=(T)Marshal.PtrToStructure(local,typeof(T));return true;}finally{Marshal.FreeHGlobal(local);}}
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Get-Children([IntPtr]$Window) { @([Rev19Native]::Children($Window)) }
function Test-Overlap($A,$B) { $A.Left -lt $B.Right -and $A.Right -gt $B.Left -and $A.Top -lt $B.Bottom -and $A.Bottom -gt $B.Top }
function Wait-Until([int]$Seconds,[string]$Description,[scriptblock]$Condition) {
    $deadline=[DateTime]::UtcNow.AddSeconds($Seconds)
    do { if(& $Condition){ return }; Start-Sleep -Milliseconds 120 } while([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for $Description."
}
function Get-CoreProcess([string]$ExpectedPath) {
    $expected=[IO.Path]::GetFullPath($ExpectedPath)
    foreach($candidate in @(Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue)) {
        try { $candidate.Refresh(); if(-not $candidate.HasExited -and $candidate.MainWindowHandle -ne [IntPtr]::Zero -and $candidate.Path -and [IO.Path]::GetFullPath($candidate.Path).Equals($expected,[StringComparison]::OrdinalIgnoreCase)){return $candidate} } catch { }
    }
    $null
}
function Ensure-CaptureWindowForeground([IntPtr]$Window) {
    if($Window -eq [IntPtr]::Zero){throw 'Cannot foreground an empty rev.19 capture window.'}
    [void][Rev19Native]::SetWindowPos($Window,[IntPtr]::new(-1),0,0,0,0,0x0001 -bor 0x0002 -bor 0x0010 -bor 0x0040)
    $brought=[Rev19Native]::BringWindowToTop($Window)
    $foreground=[Rev19Native]::SetForegroundWindow($Window)
    Start-Sleep -Milliseconds 220
    Write-Host "REV19_CAPTURE_FOREGROUND bring=$brought foreground=$foreground"
}
function Get-BitmapUniqueColorCount([Drawing.Bitmap]$Bitmap) {
    $colors=[Collections.Generic.HashSet[int]]::new()
    for($y=0;$y-lt$Bitmap.Height;$y++){for($x=0;$x-lt$Bitmap.Width;$x++){[void]$colors.Add($Bitmap.GetPixel($x,$y).ToArgb());if($colors.Count-ge4){return $colors.Count}}}
    $colors.Count
}
function Format-WindowEvidence([IntPtr]$Handle) {
    if($Handle-eq[IntPtr]::Zero){return 'hwnd=0x0'}
    $info=[Rev19Native]::Bounds($Handle)
    if(-not$info){return ('hwnd=0x{0:X} unreadable' -f $Handle.ToInt64())}
    "hwnd=0x{0:X} pid={1} id={2} class='{3}' text='{4}' visible={5} rect={6},{7}-{8},{9}" -f $Handle.ToInt64(),$info.OwnerPid,$info.Id,$info.ClassName,$info.Text,$info.Visible,$info.Left,$info.Top,$info.Right,$info.Bottom
}
function Save-PhysicalScreen([string]$Path) {
    $screen=[Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bitmap=[Drawing.Bitmap]::new([Math]::Max(1,$screen.Width),[Math]::Max(1,$screen.Height));$graphics=[Drawing.Graphics]::FromImage($bitmap)
    try{$graphics.CopyFromScreen($screen.Left,$screen.Top,0,0,$screen.Size,[Drawing.CopyPixelOperation]::SourceCopy)}finally{$graphics.Dispose()}
    try{$bitmap.Save($Path,[Drawing.Imaging.ImageFormat]::Png)}finally{$bitmap.Dispose()}
}
function Write-ScreenOccupancyEvidence($Child,[IntPtr]$ExpectedWindow,[string]$Stem) {
    $centerX=[int](($Child.Left+$Child.Right)/2);$centerY=[int](($Child.Top+$Child.Bottom)/2)
    $point=[Rev19Native+POINT]::new($centerX,$centerY)
    $atPoint=[Rev19Native]::WindowFromPoint($point)
    $rootAtPoint=if($atPoint-ne[IntPtr]::Zero){[Rev19Native]::GetAncestor($atPoint,2)}else{[IntPtr]::Zero}
    $foreground=[Rev19Native]::GetForegroundWindow()
    $screenPath=Join-Path $OutputDir ($Stem+'-physical-screen.png')
    Save-PhysicalScreen $screenPath
    Write-Host ('REV19_SCREEN_OCCUPANCY center={0},{1} expected={2}' -f $centerX,$centerY,(Format-WindowEvidence $ExpectedWindow))
    Write-Host ('REV19_SCREEN_OCCUPANCY point={0}' -f (Format-WindowEvidence $atPoint))
    Write-Host ('REV19_SCREEN_OCCUPANCY pointRoot={0}' -f (Format-WindowEvidence $rootAtPoint))
    Write-Host ('REV19_SCREEN_OCCUPANCY foreground={0}' -f (Format-WindowEvidence $foreground))
    Write-Host ('REV19_SCREEN_OCCUPANCY fullScreen={0}' -f $screenPath)
}
function Capture-ScreenChildBitmap($Child,[IntPtr]$ExpectedWindow,[string]$EvidenceStem) {
    $w=[Math]::Max(1,$Child.Right-$Child.Left);$h=[Math]::Max(1,$Child.Bottom-$Child.Top)
    $screen=[Windows.Forms.Screen]::PrimaryScreen.Bounds
    if($Child.Left-lt$screen.Left -or $Child.Top-lt$screen.Top -or $Child.Right-gt$screen.Right -or $Child.Bottom-gt$screen.Bottom){
        throw "screen crop for button id=$($Child.Id) is outside primary screen: child=$($Child.Left),$($Child.Top)-$($Child.Right),$($Child.Bottom) screen=$($screen.Left),$($screen.Top)-$($screen.Right),$($screen.Bottom)"
    }
    $bitmap=[Drawing.Bitmap]::new($w,$h);$graphics=[Drawing.Graphics]::FromImage($bitmap)
    try{$graphics.CopyFromScreen($Child.Left,$Child.Top,0,0,[Drawing.Size]::new($w,$h),[Drawing.CopyPixelOperation]::SourceCopy)}finally{$graphics.Dispose()}
    $colors=Get-BitmapUniqueColorCount $bitmap
    if($colors-lt4){
        $cropPath=Join-Path $OutputDir ($EvidenceStem+'-1702-screen-crop.png')
        try{$bitmap.Save($cropPath,[Drawing.Imaging.ImageFormat]::Png)}catch{}
        Write-ScreenOccupancyEvidence $Child $ExpectedWindow $EvidenceStem
        $bitmap.Dispose()
        throw "real screen crop for button id=$($Child.Id) is blank: uniqueColors=$colors path=$cropPath"
    }
    $bitmap
}
function Capture-CompositeWindow([IntPtr]$Window,[string]$Path,[object[]]$Children,[int]$LauncherPid) {
    $bounds=[Rev19Native]::Bounds($Window);if(-not $bounds){throw 'Could not read rev.19 window bounds.'}
    $w=[Math]::Max(1,$bounds.Right-$bounds.Left);$h=[Math]::Max(1,$bounds.Bottom-$bounds.Top)
    $bitmap=[Drawing.Bitmap]::new($w,$h);$graphics=[Drawing.Graphics]::FromImage($bitmap);$hdc=$graphics.GetHdc()
    try{if(-not [Rev19Native]::PrintWindow($Window,$hdc,2)){throw 'PrintWindow failed for rev.19 screenshot.'}}finally{$graphics.ReleaseHdc($hdc)}
    try{
        $child=$Children|Where-Object{$_.Visible -and $_.ClassName -eq 'Button' -and $_.OwnerPid -eq $LauncherPid -and $_.Id -eq 1702}|Select-Object -First 1
        if(-not$child){throw 'launcher-owned remove-services button 1702 missing during composite capture.'}
        Ensure-CaptureWindowForeground $Window
        $stem=[IO.Path]::GetFileNameWithoutExtension($Path)
        $screenButton=Capture-ScreenChildBitmap $child $Window $stem
        try{$graphics.DrawImageUnscaled($screenButton,$child.Left-$bounds.Left,$child.Top-$bounds.Top)}finally{$screenButton.Dispose()}
    }finally{$graphics.Dispose()}
    try{$bitmap.Save($Path,[Drawing.Imaging.ImageFormat]::Png)}finally{$bitmap.Dispose()}
    Write-Host "REV19_PRIMARY_COMPOSITE_OK proxies=1 source=screen id=1702 path=$Path"
}
function Get-CropColorCount([string]$Path,$Child,$WindowBounds) {
    if(-not(Test-Path -LiteralPath $Path -PathType Leaf)){throw "screenshot missing: $Path"}
    $bitmap=[Drawing.Bitmap]::new($Path);$colors=[Collections.Generic.HashSet[int]]::new()
    try{
        $left=[Math]::Max(0,$Child.Left-$WindowBounds.Left);$top=[Math]::Max(0,$Child.Top-$WindowBounds.Top)
        $right=[Math]::Min($bitmap.Width,$Child.Right-$WindowBounds.Left);$bottom=[Math]::Min($bitmap.Height,$Child.Bottom-$WindowBounds.Top)
        if($right-le$left -or $bottom-le$top){throw "button crop is outside screenshot: $left,$top-$right,$bottom"}
        for($y=$top;$y-lt$bottom;$y++){for($x=$left;$x-lt$right;$x++){[void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb())}}
    }finally{$bitmap.Dispose()}
    $colors.Count
}
function Assert-RemoveServicesCaptured([string]$Path,[object[]]$Children,[int]$LauncherPid,$WindowBounds,[string]$Phase) {
    $button=$Children|Where-Object{$_.Visible -and $_.ClassName -eq 'Button' -and $_.Id -eq 1702 -and $_.OwnerPid -eq $LauncherPid}|Select-Object -First 1
    if(-not$button){throw "${Phase}: launcher remove-services button 1702 missing at screenshot time."}
    $colors=Get-CropColorCount $Path $button $WindowBounds
    if($colors-lt4){throw "${Phase}: remove-services is blank in primary screenshot: uniqueColors=$colors path=$Path"}
    Write-Host "REV19_PRIMARY_SCREENSHOT_PAINT_OK name=$Phase uniqueColors=$colors"
    $colors
}
function Assert-SameRow([object[]]$Buttons,[int[]]$Ids,[string]$Name) {
    $row=@($Buttons|Where-Object{$Ids -contains $_.Id})
    if($row.Count -ne $Ids.Count){throw "$Name row expected $($Ids.Count) buttons, found $($row.Count)."}
    $minTop=($row|Measure-Object Top -Minimum).Minimum;$maxTop=($row|Measure-Object Top -Maximum).Maximum
    $minBottom=($row|Measure-Object Bottom -Minimum).Minimum;$maxBottom=($row|Measure-Object Bottom -Maximum).Maximum
    if(($maxTop-$minTop)-gt 2 -or ($maxBottom-$minBottom)-gt 2){$d=@($row|ForEach-Object{'id='+$_.Id+' y='+$_.Top+'..'+$_.Bottom})-join '; ';throw "$Name row is not aligned: $d"}
    $row
}
function Find-Heading([object[]]$Children,[string[]]$Captions,[int]$Id=0) {
    $Children|Where-Object{$_.Visible -and $_.ClassName -eq 'Static' -and (($Id -gt 0 -and $_.Id -eq $Id) -or $Captions -contains $_.Text)}|Select-Object -First 1
}
function Find-FrozenTrayCheckbox([IntPtr]$Window) {
    $children=@(Get-Children $Window)
    $startup=$children|Where-Object{$_.Id -eq 1409 -and $_.ClassName -eq 'Button'}|Select-Object -First 1
    $admin=$children|Where-Object{$_.Id -eq 1410 -and $_.ClassName -eq 'Button'}|Select-Object -First 1
    if(-not $startup -or -not $admin){return $null}
    $rowStep=$admin.Top-$startup.Top;if($rowStep -lt 10 -or $rowStep -gt 80){return $null}
    $targetTop=$startup.Top-$rowStep
    $candidates=@($children|Where-Object{$_.Id -eq 0 -and $_.ClassName -eq 'Button'}|Sort-Object @{Expression={[Math]::Abs($_.Top-$targetTop)}},@{Expression={[Math]::Abs($_.Left-$startup.Left)}})
    foreach($candidate in $candidates){if([Math]::Abs($candidate.Top-$targetTop)-le[Math]::Max(4,[int]($rowStep/3))-and[Math]::Abs($candidate.Left-$startup.Left)-le[Math]::Max(16,$rowStep)){return $candidate}}
    $null
}
function Find-TrayProxy([IntPtr]$Window) {
    $settingsHost=Get-Children $Window|Where-Object{$_.Id -eq 1492}|Select-Object -First 1
    if(-not $settingsHost){return $null}
    Get-Children $settingsHost.Handle|Where-Object{$_.Id -eq 1503 -and $_.ClassName -eq 'Button'}|Select-Object -First 1
}
function Get-DPopEntries([Diagnostics.Process]$Launcher,[Diagnostics.Process]$Core) {
    @([Rev19Native]::Entries()|Where-Object{$_.OwnerPid -eq $Launcher.Id -or $_.OwnerPid -eq $Core.Id -or $_.Title -like '*DPopCleaner*' -or $_.ButtonText -like '*DPopCleaner*'})
}
function Assert-TrayState([Diagnostics.Process]$Launcher,[Diagnostics.Process]$Core,[int]$ExpectedCanonical,[string]$Phase) {
    $deadline=[DateTime]::UtcNow.AddSeconds(8)
    do{
        Start-Sleep -Milliseconds 250
        $entries=@(Get-DPopEntries $Launcher $Core)
        $canonical=@($entries|Where-Object{$_.OwnerPid -eq $Launcher.Id -and $_.Title -eq 'DPopCleaner.TrayRamBadgeHost' -and $_.IconId -eq 1})
        $extras=@($entries|Where-Object{-not($_.OwnerPid -eq $Launcher.Id -and $_.Title -eq 'DPopCleaner.TrayRamBadgeHost' -and $_.IconId -eq 1)})
        $valid=($canonical.Count -eq $ExpectedCanonical -and $extras.Count -eq 0)
        if($ExpectedCanonical -eq 1){$valid=$valid -and $canonical[0].HIcon -ne [IntPtr]::Zero}
    }while(-not $valid -and [DateTime]::UtcNow -lt $deadline)
    Write-Host "REV19_TRAY_$Phase canonical=$($canonical.Count) extras=$($extras.Count)"
    if($canonical.Count -ne $ExpectedCanonical){throw "${Phase}: expected canonical=$ExpectedCanonical, found $($canonical.Count)."}
    if($extras.Count -ne 0){throw "${Phase}: legacy/duplicate tray entry remains: $($extras -join ' | ')"}
    if($ExpectedCanonical -eq 1 -and $canonical[0].HIcon -eq [IntPtr]::Zero){throw "${Phase}: canonical icon has empty hIcon."}
}

$targetIds=@(1701,1702,1703,1704,1705,1707,1708,1710,1711,1713,1714,1716,1717,1720,1721,1722,1723,1724,1725)
$strategyIds=@(1701,1713,1714)
$updateIds=@(1724,1725,1716,1717)
$actionIds=@(1720,1721,1722,1723)
$additionalIds=@(1704,1705,1707,1708)
$serviceIds=@(1703,1702,1710,1711)
$settingsPath=Join-Path $OutputDir 'rev19-settings.ini'
@('auto_update=0','tray_icon=1')|Set-Content -LiteralPath $settingsPath -Encoding ascii
$screenBounds=[Windows.Forms.Screen]::PrimaryScreen.Bounds

$reports=@();$launcher=$null;$core=$null
try{
    $launcher=Start-Process -FilePath $launcherPath -ArgumentList @('--no-update-check','--settings-path',('"'+$settingsPath+'"')) -WorkingDirectory $RootPath -PassThru
    Wait-Until 18 'rev.19 frozen core window' {$script:core=Get-CoreProcess $corePath;$null -ne $script:core}
    $core=$script:core;$window=$core.MainWindowHandle
    $zapretTab=Get-Children $window|Where-Object{$_.Visible -and $_.ClassName -eq 'Button' -and $_.Id -eq 905}|Select-Object -First 1
    if(-not $zapretTab){throw 'rev.19 Zapret tab id=905 missing.'}
    [void][Rev19Native]::SendMessage($zapretTab.Handle,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
    Wait-Until 8 'rev.19 target buttons and service heading' {
        $c=@(Get-Children $window);@($c|Where-Object{$_.Visible -and $_.ClassName -eq 'Button' -and $targetIds -contains $_.Id}).Count -eq 19 -and @($c|Where-Object{$_.Visible -and $_.ClassName -eq 'Static' -and $_.Id -eq 1726}).Count -eq 1
    }

    foreach($size in @(
        [pscustomobject]@{Name='base';Width=1024;Height=768},
        [pscustomobject]@{Name='wide';Width=1366;Height=800},
        [pscustomobject]@{Name='large';Width=1680;Height=840},
        [pscustomobject]@{Name='user';Width=1908;Height=950}
    )){
        $windowY=if($size.Height -gt $screenBounds.Height){$screenBounds.Bottom-$size.Height-8}else{0}
        if(-not [Rev19Native]::SetWindowPos($window,[IntPtr]::Zero,0,$windowY,$size.Width,$size.Height,0x0002 -bor 0x0004 -bor 0x0010 -bor 0x0400)){throw "$($size.Name): resize failed."}
        Start-Sleep -Milliseconds 1100
        $windowBounds=[Rev19Native]::Bounds($window);$children=@(Get-Children $window)
        $buttons=@($children|Where-Object{$_.Visible -and $_.ClassName -eq 'Button' -and $targetIds -contains $_.Id})
        if($buttons.Count -ne 19){throw "$($size.Name): expected 19 target buttons, found $($buttons.Count)."}
        $edits=@($children|Where-Object{$_.Visible -and $_.ClassName -eq 'Edit'}|Sort-Object Top)
        if($edits.Count -lt 2){throw "$($size.Name): expected two status Edit controls."}
        $detailHeight=$edits[1].Bottom-$edits[1].Top
        if($detailHeight -lt 70 -or $detailHeight -gt 155){throw "$($size.Name): idle status detail is not compact: height=$detailHeight expected 70..155."}
        $combos=@($children|Where-Object{$_.Visible -and $_.ClassName -eq 'ComboBox'}|Sort-Object Top)
        if($combos.Count -lt 2){throw "$($size.Name): strategy/filter ComboBoxes missing."}
        $strategyLabel=Find-Heading $children @('Стратегия','Strategy')
        $updateHeading=Find-Heading $children @('Обновление Zapret','Zapret Update')
        $additionalHeading=Find-Heading $children @('Дополнительно','Additional')
        $serviceHeading=Find-Heading $children @('Сервисные действия','Service actions') 1726
        foreach($named in @(@('strategy',$strategyLabel),@('update',$updateHeading),@('additional',$additionalHeading),@('service',$serviceHeading))){if(-not $named[1]){throw "$($size.Name): $($named[0]) heading missing."}}

        for($i=0;$i -lt $buttons.Count;$i++){
            $b=$buttons[$i]
            if([Rev19Native]::GetWindowTheme($b.Handle) -ne [IntPtr]::Zero){throw "$($size.Name): button id=$($b.Id) still has Windows theme/ghost frame."}
            $needed=[Windows.Forms.TextRenderer]::MeasureText([string]$b.Text,[Drawing.SystemFonts]::MessageBoxFont).Width+8
            if(($b.Right-$b.Left)-lt $needed){throw "$($size.Name): clipped button id=$($b.Id) text='$($b.Text)'."}
            for($j=$i+1;$j -lt $buttons.Count;$j++){if(Test-Overlap $b $buttons[$j]){throw "$($size.Name): buttons $($b.Id) and $($buttons[$j].Id) overlap."}}
            foreach($edit in $edits){if(Test-Overlap $b $edit){throw "$($size.Name): button $($b.Id) overlaps status Edit."}}
            foreach($combo in $combos){if(Test-Overlap $b $combo){throw "$($size.Name): button $($b.Id) overlaps ComboBox."}}
            foreach($heading in @($strategyLabel,$updateHeading,$additionalHeading,$serviceHeading)){if(Test-Overlap $b $heading){throw "$($size.Name): button $($b.Id) overlaps heading '$($heading.Text)'."}}
        }

        $strategy=@(Assert-SameRow $buttons $strategyIds 'strategy')
        $update=@(Assert-SameRow $buttons $updateIds 'update')
        $action=@(Assert-SameRow $buttons $actionIds 'bridge-action')
        $additional=@(Assert-SameRow $buttons $additionalIds 'additional')
        $service=@(Assert-SameRow $buttons $serviceIds 'service')
        $detailBottom=$edits[1].Bottom
        $strategyTop=($strategy|Measure-Object Top -Minimum).Minimum;$strategyBottom=($strategy|Measure-Object Bottom -Maximum).Maximum
        $updateTop=($update|Measure-Object Top -Minimum).Minimum;$updateBottom=($update|Measure-Object Bottom -Maximum).Maximum
        $actionTop=($action|Measure-Object Top -Minimum).Minimum;$actionBottom=($action|Measure-Object Bottom -Maximum).Maximum
        $additionalTop=($additional|Measure-Object Top -Minimum).Minimum;$additionalBottom=($additional|Measure-Object Bottom -Maximum).Maximum
        $serviceTop=($service|Measure-Object Top -Minimum).Minimum;$serviceBottom=($service|Measure-Object Bottom -Maximum).Maximum
        if(-not($detailBottom -lt $strategyTop -and $strategyBottom -lt $updateTop -and $updateBottom -lt $actionTop -and $actionBottom -lt $additionalTop -and $additionalBottom -lt $serviceTop)){
            throw "$($size.Name): semantic vertical order broken detail=$detailBottom strategy=$strategyTop..$strategyBottom update=$updateTop..$updateBottom action=$actionTop..$actionBottom additional=$additionalTop..$additionalBottom service=$serviceTop..$serviceBottom."
        }
        $toggleWidths=@($update|Where-Object{$_.Id -in 1716,1717}|ForEach-Object{$_.Right-$_.Left})
        $primaryWidths=@($update|Where-Object{$_.Id -in 1724,1725}|ForEach-Object{$_.Right-$_.Left})
        if($size.Width -ge 1680){
            if(($toggleWidths|Measure-Object -Maximum).Maximum -gt 250){throw "$($size.Name): auto controls are still giant; max width=$(($toggleWidths|Measure-Object -Maximum).Maximum)."}
            if(($toggleWidths|Measure-Object -Maximum).Maximum -ge ($primaryWidths|Measure-Object -Minimum).Minimum){throw "$($size.Name): auto controls are not narrower than primary update actions."}
        }
        $serviceWidths=@($service|ForEach-Object{$_.Right-$_.Left})
        if($size.Width -ge 1680 -and ($serviceWidths|Measure-Object -Maximum).Maximum -gt 230){throw "$($size.Name): service actions are not compact."}
        $unusedBottom=$windowBounds.Bottom-$serviceBottom

        $shot=Join-Path $OutputDir ("rev19-zapret-{0}-{1}x{2}.png" -f $size.Name,$size.Width,$size.Height)
        Capture-CompositeWindow $window $shot $children $launcher.Id
        $removeColors=Assert-RemoveServicesCaptured $shot $children $launcher.Id $windowBounds $size.Name
        Write-Host "REV19_SIZE_CAPTURE name=$($size.Name) rows=$strategyTop/$updateTop/$actionTop/$additionalTop/$serviceTop serviceBottom=$serviceBottom unusedBottom=$unusedBottom"
        if($size.Width -eq 1908 -and $unusedBottom -gt 210){throw "$($size.Name): too much lower blank space remains: $unusedBottom px."}
        $reports += [pscustomobject]@{name=$size.Name;width=$size.Width;height=$size.Height;detail_height=$detailHeight;strategy_top=$strategyTop;update_top=$updateTop;action_top=$actionTop;additional_top=$additionalTop;service_top=$serviceTop;service_bottom=$serviceBottom;unused_bottom=$unusedBottom;toggle_widths=$toggleWidths;primary_update_widths=$primaryWidths;service_widths=$serviceWidths;remove_services_colors=$removeColors;screenshot=$shot}
        Write-Host "REV19_SIZE_OK name=$($size.Name) size=$($size.Width)x$($size.Height) detail=$detailHeight rows=$strategyTop/$updateTop/$actionTop/$additionalTop/$serviceTop unusedBottom=$unusedBottom"
    }

    if(-not $SkipTray){
        $gear=Get-Children $window|Where-Object{$_.Id -eq 906 -and $_.ClassName -eq 'Button'}|Select-Object -First 1
        if(-not $gear){throw 'rev.19 Settings gear id=906 missing.'}
        [void][Rev19Native]::SendMessage($gear.Handle,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
        Wait-Until 8 'rev.19 tray proxy' {$null -ne (Find-TrayProxy $window)}
        $proxy=Find-TrayProxy $window;$legacy=Find-FrozenTrayCheckbox $window
        if(-not $proxy -or -not $legacy){throw 'rev.19 tray proxy/frozen checkbox could not be identified.'}
        if([Rev19Native]::SendMessage($proxy.Handle,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 1){[void][Rev19Native]::SendMessage($proxy.Handle,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)}
        Wait-Until 5 'tray ON and legacy OFF' {$p=Find-TrayProxy $window;$l=Find-FrozenTrayCheckbox $window;$p -and $l -and [Rev19Native]::SendMessage($p.Handle,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 1 -and [Rev19Native]::SendMessage($l.Handle,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 0}
        Assert-TrayState $launcher $core 1 'ON'
        $proxy=Find-TrayProxy $window;[void][Rev19Native]::SendMessage($proxy.Handle,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
        Wait-Until 5 'tray OFF' {$p=Find-TrayProxy $window;$l=Find-FrozenTrayCheckbox $window;$p -and $l -and [Rev19Native]::SendMessage($p.Handle,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 0 -and [Rev19Native]::SendMessage($l.Handle,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 0}
        Assert-TrayState $launcher $core 0 'OFF'
        $proxy=Find-TrayProxy $window;[void][Rev19Native]::SendMessage($proxy.Handle,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
        Wait-Until 5 'tray restored ON' {$p=Find-TrayProxy $window;$l=Find-FrozenTrayCheckbox $window;$p -and $l -and [Rev19Native]::SendMessage($p.Handle,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 1 -and [Rev19Native]::SendMessage($l.Handle,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 0}
        Assert-TrayState $launcher $core 1 'RESTORED_ON'
    }

    $reports|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $OutputDir 'rev19-zapret-cleanup-report.json') -Encoding utf8
    $reports|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $OutputDir 'rev17-zapret-responsive-report.json') -Encoding utf8
    $userReport=$reports|Where-Object{$_.width -eq 1908}|Select-Object -First 1
    [pscustomobject]@{width=1908;height=950;detail_status_height=$userReport.detail_height;buttons=19;native_theme_handles_zero=$true;canonical_tray_count=if($SkipTray){$null}else{1};screenshot=$userReport.screenshot}|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $OutputDir 'rev18-user-report-smoke.json') -Encoding utf8
    Write-Host 'REV19_ZAPRET_CLEANUP_SMOKE_OK'
}
finally{
    foreach($name in @('DPopCleaner','DPopCleaner.Core','SimpleUpdate')){foreach($p in @(Get-Process -Name $name -ErrorAction SilentlyContinue)){try{if($p.Path -and [IO.Path]::GetFullPath($p.Path).StartsWith($RootPath,[StringComparison]::OrdinalIgnoreCase)){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue}}catch{}}}
}
