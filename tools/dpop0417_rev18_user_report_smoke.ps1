[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$RootPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev18-user-report'
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
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "rev.18 smoke prerequisite missing: $required" }
}

$native = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class Rev18Child {
    public IntPtr Handle; public int Id; public string Text; public string ClassName; public bool Visible;
    public int Left; public int Top; public int Right; public int Bottom;
}
public sealed class Rev18TrayEntry {
    public IntPtr Toolbar; public IntPtr Hwnd; public uint IconId; public IntPtr HIcon; public int OwnerPid;
    public string Title; public string ClassName; public string ButtonText;
    public string Identity { get { return "0x" + Hwnd.ToInt64().ToString("X") + ":" + IconId.ToString(); } }
    public override string ToString() { return "owner="+OwnerPid+";identity="+Identity+";hicon=0x"+HIcon.ToInt64().ToString("X")+";title="+(Title??"")+";text="+(ButtonText??""); }
}
public static class Rev18Native {
    private const int TB_BUTTONCOUNT=0x0418, TB_GETBUTTON=0x0417, TB_GETBUTTONTEXTW=0x044B;
    private const uint PROCESS_VM_OPERATION=0x0008, PROCESS_VM_READ=0x0010, PROCESS_VM_WRITE=0x0020, PROCESS_QUERY_INFORMATION=0x0400;
    private const uint MEM_COMMIT=0x1000, MEM_RESERVE=0x2000, MEM_RELEASE=0x8000, PAGE_READWRITE=0x04;
    private delegate bool EnumProc(IntPtr hwnd, IntPtr p);
    [StructLayout(LayoutKind.Sequential)] private struct RECT { public int Left,Top,Right,Bottom; }
    [StructLayout(LayoutKind.Sequential)] private struct TBBUTTON64 { public int iBitmap; public int idCommand; public byte fsState; public byte fsStyle; [MarshalAs(UnmanagedType.ByValArray,SizeConst=6)] public byte[] reserved; public UIntPtr dwData; public IntPtr iString; }
    [StructLayout(LayoutKind.Sequential)] private struct TRAYDATA64 { public IntPtr hwnd; public uint uID; public uint callback; public uint r0; public uint r1; public IntPtr hIcon; }

    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent,EnumProc cb,IntPtr p);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumProc cb,IntPtr p);
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
    [DllImport("uxtheme.dll")] public static extern IntPtr GetWindowTheme(IntPtr hwnd);
    [DllImport("kernel32.dll",SetLastError=true)] private static extern IntPtr OpenProcess(uint access,bool inherit,uint pid);
    [DllImport("kernel32.dll",SetLastError=true)] private static extern IntPtr VirtualAllocEx(IntPtr p,IntPtr a,UIntPtr s,uint type,uint protect);
    [DllImport("kernel32.dll",SetLastError=true)] private static extern bool VirtualFreeEx(IntPtr p,IntPtr a,UIntPtr s,uint type);
    [DllImport("kernel32.dll",SetLastError=true)] private static extern bool ReadProcessMemory(IntPtr p,IntPtr a,IntPtr b,UIntPtr s,out UIntPtr read);
    [DllImport("kernel32.dll")] private static extern bool CloseHandle(IntPtr h);

    public static Rev18Child[] Children(IntPtr parent) {
        var list=new List<Rev18Child>(); EnumProc cb=delegate(IntPtr h,IntPtr _) {
            var t=new StringBuilder(1024); var c=new StringBuilder(128); RECT r;
            GetWindowText(h,t,t.Capacity); GetClassName(h,c,c.Capacity); GetWindowRect(h,out r);
            list.Add(new Rev18Child{Handle=h,Id=GetDlgCtrlID(h),Text=t.ToString(),ClassName=c.ToString(),Visible=IsWindowVisible(h),Left=r.Left,Top=r.Top,Right=r.Right,Bottom=r.Bottom}); return true;
        }; EnumChildWindows(parent,cb,IntPtr.Zero); GC.KeepAlive(cb); return list.ToArray();
    }
    public static Rev18Child Bounds(IntPtr hwnd) {
        RECT r; if(!GetWindowRect(hwnd,out r))return null; var t=new StringBuilder(1024); var c=new StringBuilder(128);
        GetWindowText(hwnd,t,t.Capacity); GetClassName(hwnd,c,c.Capacity);
        return new Rev18Child{Handle=hwnd,Id=GetDlgCtrlID(hwnd),Text=t.ToString(),ClassName=c.ToString(),Visible=IsWindowVisible(hwnd),Left=r.Left,Top=r.Top,Right=r.Right,Bottom=r.Bottom};
    }
    public static Rev18TrayEntry[] Entries() {
        var result=new List<Rev18TrayEntry>(); foreach(var toolbar in Toolbars())Collect(toolbar,result); return result.ToArray();
    }
    private static List<IntPtr> Toolbars() { var result=new List<IntPtr>(); Add(FindWindow("Shell_TrayWnd",null),result); Add(FindWindow("NotifyIconOverflowWindow",null),result); return result; }
    private static void Add(IntPtr root,List<IntPtr> result) {
        if(root==IntPtr.Zero)return; EnumProc cb=delegate(IntPtr h,IntPtr _) { var s=new StringBuilder(128); GetClassName(h,s,s.Capacity); if(s.ToString()=="ToolbarWindow32"&&!result.Contains(h))result.Add(h); return true; }; EnumChildWindows(root,cb,IntPtr.Zero); GC.KeepAlive(cb);
    }
    private static void Collect(IntPtr toolbar,List<Rev18TrayEntry> result) {
        uint shellPid; GetWindowThreadProcessId(toolbar,out shellPid); if(shellPid==0)return;
        IntPtr p=OpenProcess(PROCESS_VM_OPERATION|PROCESS_VM_READ|PROCESS_VM_WRITE|PROCESS_QUERY_INFORMATION,false,shellPid); if(p==IntPtr.Zero)return;
        int buttonSize=Marshal.SizeOf(typeof(TBBUTTON64)); IntPtr rb=VirtualAllocEx(p,IntPtr.Zero,new UIntPtr((uint)buttonSize),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE); IntPtr rt=VirtualAllocEx(p,IntPtr.Zero,new UIntPtr(2048),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        if(rb==IntPtr.Zero||rt==IntPtr.Zero){if(rb!=IntPtr.Zero)VirtualFreeEx(p,rb,UIntPtr.Zero,MEM_RELEASE);if(rt!=IntPtr.Zero)VirtualFreeEx(p,rt,UIntPtr.Zero,MEM_RELEASE);CloseHandle(p);return;}
        try { int count=SendMessage(toolbar,TB_BUTTONCOUNT,IntPtr.Zero,IntPtr.Zero).ToInt32(); for(int i=0;i<count;i++) {
            if(SendMessage(toolbar,TB_GETBUTTON,new IntPtr(i),rb)==IntPtr.Zero)continue; TBBUTTON64 b; if(!Read(p,rb,out b)||b.dwData==UIntPtr.Zero)continue; TRAYDATA64 d; if(!Read(p,new IntPtr(unchecked((long)b.dwData.ToUInt64())),out d)||d.hwnd==IntPtr.Zero)continue;
            uint owner; GetWindowThreadProcessId(d.hwnd,out owner); var title=new StringBuilder(512); GetWindowText(d.hwnd,title,title.Capacity); var cls=new StringBuilder(128); GetClassName(d.hwnd,cls,cls.Capacity); string text="";
            int n=SendMessage(toolbar,TB_GETBUTTONTEXTW,new IntPtr(b.idCommand),rt).ToInt32(); if(n>0)text=ReadUtf16(p,rt,Math.Min(1023,n+1)); result.Add(new Rev18TrayEntry{Toolbar=toolbar,Hwnd=d.hwnd,IconId=d.uID,HIcon=d.hIcon,OwnerPid=(int)owner,Title=title.ToString(),ClassName=cls.ToString(),ButtonText=text});
        }} finally { VirtualFreeEx(p,rb,UIntPtr.Zero,MEM_RELEASE); VirtualFreeEx(p,rt,UIntPtr.Zero,MEM_RELEASE); CloseHandle(p); }
    }
    private static string ReadUtf16(IntPtr p,IntPtr remote,int chars){int bytes=Math.Max(2,chars*2);IntPtr local=Marshal.AllocHGlobal(bytes);try{UIntPtr read;if(!ReadProcessMemory(p,remote,local,new UIntPtr((uint)bytes),out read))return "";int usable=(int)Math.Min((ulong)bytes,read.ToUInt64());return usable<=0?"":Marshal.PtrToStringUni(local,usable/2).TrimEnd('\0');}finally{Marshal.FreeHGlobal(local);}}
    private static bool Read<T>(IntPtr p,IntPtr remote,out T value) where T:struct {int size=Marshal.SizeOf(typeof(T));IntPtr local=Marshal.AllocHGlobal(size);try{UIntPtr n;if(!ReadProcessMemory(p,remote,local,new UIntPtr((uint)size),out n)||n.ToUInt64()<(ulong)size){value=default(T);return false;}value=(T)Marshal.PtrToStructure(local,typeof(T));return true;}finally{Marshal.FreeHGlobal(local);}}
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Get-Children([IntPtr]$Window) { @([Rev18Native]::Children($Window)) }
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
function Capture-Window([IntPtr]$Window,[string]$Path) {
    $bounds=[Rev18Native]::Bounds($Window); if(-not $bounds){throw 'Could not read rev.18 window bounds.'}
    $w=[Math]::Max(1,$bounds.Right-$bounds.Left);$h=[Math]::Max(1,$bounds.Bottom-$bounds.Top);$bitmap=[Drawing.Bitmap]::new($w,$h);$graphics=[Drawing.Graphics]::FromImage($bitmap);$hdc=$graphics.GetHdc()
    try { if(-not [Rev18Native]::PrintWindow($Window,$hdc,2)){throw 'PrintWindow failed for rev.18 screenshot.'} } finally { $graphics.ReleaseHdc($hdc);$graphics.Dispose() }
    try{$bitmap.Save($Path,[Drawing.Imaging.ImageFormat]::Png)}finally{$bitmap.Dispose()}
}
function Find-FrozenTrayCheckbox([IntPtr]$Window) {
    $children=@(Get-Children $Window)
    $startup=$children|Where-Object{$_.Id -eq 1409 -and $_.ClassName -eq 'Button'}|Select-Object -First 1
    $admin=$children|Where-Object{$_.Id -eq 1410 -and $_.ClassName -eq 'Button'}|Select-Object -First 1
    if(-not $startup -or -not $admin){return $null}
    $rowStep=$admin.Top-$startup.Top;if($rowStep -lt 10 -or $rowStep -gt 80){return $null}
    $targetTop=$startup.Top-$rowStep
    $candidates=@($children|Where-Object{$_.Id -eq 0 -and $_.ClassName -eq 'Button'}|Sort-Object @{Expression={[Math]::Abs($_.Top-$targetTop)}},@{Expression={[Math]::Abs($_.Left-$startup.Left)}})
    foreach($candidate in $candidates){if([Math]::Abs($candidate.Top-$targetTop) -le [Math]::Max(4,[int]($rowStep/3)) -and [Math]::Abs($candidate.Left-$startup.Left) -le [Math]::Max(16,$rowStep)){return $candidate}}
    $null
}
function Get-DPopEntries([Diagnostics.Process]$Launcher,[Diagnostics.Process]$Core) {
    @([Rev18Native]::Entries()|Where-Object{$_.OwnerPid -eq $Launcher.Id -or $_.OwnerPid -eq $Core.Id -or $_.Title -like '*DPopCleaner*' -or $_.ButtonText -like '*DPopCleaner*'})
}
function Assert-TrayState([Diagnostics.Process]$Launcher,[Diagnostics.Process]$Core,[int]$ExpectedCanonical,[string]$Phase) {
    $deadline=[DateTime]::UtcNow.AddSeconds(8)
    do {
        Start-Sleep -Milliseconds 250
        $entries=@(Get-DPopEntries $Launcher $Core)
        $canonical=@($entries|Where-Object{$_.OwnerPid -eq $Launcher.Id -and $_.Title -eq 'DPopCleaner.TrayRamBadgeHost' -and $_.IconId -eq 1})
        $extras=@($entries|Where-Object{-not($_.OwnerPid -eq $Launcher.Id -and $_.Title -eq 'DPopCleaner.TrayRamBadgeHost' -and $_.IconId -eq 1)})
        $valid=($canonical.Count -eq $ExpectedCanonical -and $extras.Count -eq 0)
        if($ExpectedCanonical -eq 1){$valid=$valid -and $canonical[0].HIcon -ne [IntPtr]::Zero}
    } while(-not $valid -and [DateTime]::UtcNow -lt $deadline)
    Write-Host "REV18_TRAY_$Phase canonical=$($canonical.Count) extras=$($extras.Count)"
    foreach($entry in $entries){Write-Host "REV18_TRAY_ROW $entry"}
    if($canonical.Count -ne $ExpectedCanonical){throw "${Phase}: expected canonical=$ExpectedCanonical, found $($canonical.Count)."}
    if($extras.Count -ne 0){throw "${Phase}: legacy/duplicate DPopCleaner tray entry remains: $($extras -join ' | ')"}
    if($ExpectedCanonical -eq 1 -and $canonical[0].HIcon -eq [IntPtr]::Zero){throw "${Phase}: canonical icon has empty hIcon."}
}

$targetIds=@(1701,1702,1703,1704,1705,1707,1708,1710,1711,1713,1714,1716,1717,1720,1721,1722,1723,1724,1725)
$settingsPath=Join-Path $OutputDir 'rev18-user-settings.ini'
@('auto_update=0','tray_icon=0')|Set-Content -LiteralPath $settingsPath -Encoding ascii
$launcher=$null;$core=$null
try {
    $launcher=Start-Process -FilePath $launcherPath -ArgumentList @('--no-update-check','--settings-path',('"'+$settingsPath+'"')) -WorkingDirectory $RootPath -PassThru
    Wait-Until 18 'rev.18 installed frozen core window' {$script:core=Get-CoreProcess $corePath;$null -ne $script:core}
    $core=$script:core;$window=$core.MainWindowHandle

    $zapretTab=Get-Children $window|Where-Object{$_.Visible -and $_.ClassName -eq 'Button' -and $_.Id -eq 905}|Select-Object -First 1
    if(-not $zapretTab){throw 'rev.18 Zapret tab id=905 missing.'}
    [void][Rev18Native]::SendMessage($zapretTab.Handle,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
    Wait-Until 8 'rev.18 Zapret target buttons' {@(Get-Children $window|Where-Object{$_.Visible -and $_.ClassName -eq 'Button' -and $targetIds -contains $_.Id}).Count -eq 19}

    if(-not [Rev18Native]::SetWindowPos($window,[IntPtr]::Zero,0,0,1908,950,0x0002 -bor 0x0004 -bor 0x0010 -bor 0x0400)){throw 'Could not force 1908x950 rev.18 window.'}
    Start-Sleep -Milliseconds 1100
    $windowBounds=[Rev18Native]::Bounds($window);$children=@(Get-Children $window);$buttons=@($children|Where-Object{$_.Visible -and $_.ClassName -eq 'Button' -and $targetIds -contains $_.Id})
    if($buttons.Count -ne 19){throw "rev.18 1908x950 expected 19 buttons, found $($buttons.Count)."}
    $edits=@($children|Where-Object{$_.Visible -and $_.ClassName -eq 'Edit'}|Sort-Object Top)
    if($edits.Count -lt 2){throw "rev.18 expected two status Edit controls, found $($edits.Count)."}
    $detailHeight=$edits[1].Bottom-$edits[1].Top
    if($detailHeight -lt 140){throw "rev.18 1908x950 detail status did not consume vertical space: height=$detailHeight."}
    foreach($button in $buttons){
        $theme=[Rev18Native]::GetWindowTheme($button.Handle)
        if($theme -ne [IntPtr]::Zero){throw "rev.18 ghost-theme regression: button id=$($button.Id) still has native theme handle=0x$($theme.ToInt64().ToString('X'))."}
    }
    $bottommost=($buttons|Measure-Object Bottom -Maximum).Maximum
    $unusedBottom=$windowBounds.Bottom-$bottommost
    if($unusedBottom -gt 90){throw "rev.18 1908x950 leaves too much unused lower space: windowBottom=$($windowBounds.Bottom) bottommost=$bottommost unused=$unusedBottom."}
    $shot=Join-Path $OutputDir 'rev18-zapret-1908x950.png';Capture-Window $window $shot
    Write-Host "REV18_1908X950_LAYOUT_OK detailHeight=$detailHeight unusedBottom=$unusedBottom"
    Write-Host 'REV18_GHOST_BUTTON_THEME_OK'

    $gear=Get-Children $window|Where-Object{$_.Id -eq 906 -and $_.ClassName -eq 'Button'}|Select-Object -First 1
    if(-not $gear){throw 'Settings gear id=906 missing.'}
    [void][Rev18Native]::SendMessage($gear.Handle,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
    Wait-Until 8 'rev.18 tray proxy' {@(Get-Children $window|Where-Object{$_.Id -eq 1503}).Count -ge 1}
    $proxy=Get-Children $window|Where-Object{$_.Id -eq 1503}|Select-Object -First 1
    if(-not $proxy){throw 'Tray proxy id=1503 missing.'}
    $legacy=Find-FrozenTrayCheckbox $window
    if(-not $legacy){throw 'Could not identify frozen-core tray checkbox.'}

    if([Rev18Native]::SendMessage($proxy.Handle,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 1){[void][Rev18Native]::SendMessage($proxy.Handle,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)}
    Wait-Until 5 'canonical tray preference ON with frozen tray OFF' {
        $script:p=Get-Children $window|Where-Object{$_.Id -eq 1503}|Select-Object -First 1
        $script:l=Find-FrozenTrayCheckbox $window
        $script:p -and $script:l -and [Rev18Native]::SendMessage($script:p.Handle,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 1 -and [Rev18Native]::SendMessage($script:l.Handle,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 0
    }
    Assert-TrayState $launcher $core 1 'ON'

    $proxy=Get-Children $window|Where-Object{$_.Id -eq 1503}|Select-Object -First 1
    [void][Rev18Native]::SendMessage($proxy.Handle,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
    Wait-Until 5 'canonical tray preference OFF' {
        $script:p=Get-Children $window|Where-Object{$_.Id -eq 1503}|Select-Object -First 1
        $script:l=Find-FrozenTrayCheckbox $window
        $script:p -and $script:l -and [Rev18Native]::SendMessage($script:p.Handle,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 0 -and [Rev18Native]::SendMessage($script:l.Handle,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 0
    }
    Assert-TrayState $launcher $core 0 'OFF'

    [void][Rev18Native]::SendMessage($proxy.Handle,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
    Wait-Until 5 'canonical tray preference restored ON' {
        $script:p=Get-Children $window|Where-Object{$_.Id -eq 1503}|Select-Object -First 1
        $script:l=Find-FrozenTrayCheckbox $window
        $script:p -and $script:l -and [Rev18Native]::SendMessage($script:p.Handle,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 1 -and [Rev18Native]::SendMessage($script:l.Handle,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 0
    }
    Assert-TrayState $launcher $core 1 'RESTORED_ON'

    [pscustomobject]@{width=1908;height=950;detail_status_height=$detailHeight;unused_bottom=$unusedBottom;buttons=19;native_theme_handles_zero=$true;proxy_tray_on=$true;frozen_tray_off=$true;canonical_tray_count=1;screenshot=$shot} |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputDir 'rev18-user-report-smoke.json') -Encoding utf8
    Write-Host 'REV18_USER_REPORT_SMOKE_OK'
}
finally {
    foreach($name in @('DPopCleaner','DPopCleaner.Core','SimpleUpdate')){
        foreach($p in @(Get-Process -Name $name -ErrorAction SilentlyContinue)){try{if($p.Path -and [IO.Path]::GetFullPath($p.Path).StartsWith($RootPath,[StringComparison]::OrdinalIgnoreCase)){Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue}}catch{}}
    }
}
