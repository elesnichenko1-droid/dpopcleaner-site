[CmdletBinding()]
param(
    [string]$RootPath = '',
    [string]$InstallerPath = '',
    [string]$OutputDir = '_release/0.4.17/evidence/rev16-tray'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$hasRoot = -not [string]::IsNullOrWhiteSpace($RootPath)
$hasInstaller = -not [string]::IsNullOrWhiteSpace($InstallerPath)
if ($hasRoot -eq $hasInstaller) { throw 'Pass exactly one of RootPath or InstallerPath.' }
$OutputDir = if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repoRoot $OutputDir }
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$installRoot = $null
$installed = $false
$testExplorerRestart = $false
if ($hasInstaller) {
    $InstallerPath = if ([IO.Path]::IsPathRooted($InstallerPath)) { $InstallerPath } else { Join-Path $repoRoot $InstallerPath }
    $InstallerPath = [IO.Path]::GetFullPath($InstallerPath)
    if (-not (Test-Path -LiteralPath $InstallerPath -PathType Leaf)) { throw "rev.16 installer missing: $InstallerPath" }
    $installRoot = Join-Path ([IO.Path]::GetTempPath()) ('dpop0417-rev16-tray-' + [Guid]::NewGuid().ToString('N'))
    $install = Start-Process -FilePath $InstallerPath -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',"/DIR=$installRoot") -Wait -PassThru
    if ($install.ExitCode -ne 0) { throw "rev.16 silent install failed: $($install.ExitCode)" }
    $installed = $true
    $RootPath = $installRoot
    $testExplorerRestart = $true
}

$RootPath = [IO.Path]::GetFullPath($RootPath)
$launcherPath = Join-Path $RootPath 'DPopCleaner.exe'
$corePath = Join-Path $RootPath 'DPopCleaner.Core.exe'
foreach ($required in @($launcherPath, $corePath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "rev.16 tray smoke prerequisite missing: $required" }
}

$settingsPath = Join-Path $RootPath 'SimpleUpdate-rev16-tray-smoke.ini'

$native = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class Rev16TrayChild {
    public IntPtr Handle;
    public int Id;
    public string Text;
    public string ClassName;
    public bool Visible;
}

public sealed class Rev16TrayEntry {
    public IntPtr Toolbar;
    public IntPtr Hwnd;
    public uint IconId;
    public IntPtr HIcon;
    public int OwnerPid;
    public string Title;
    public string ClassName;
    public string ButtonText;
    public string Identity { get { return "0x" + Hwnd.ToInt64().ToString("X") + ":" + IconId.ToString(); } }
    public override string ToString() {
        return "owner=" + OwnerPid + ";identity=" + Identity + ";hicon=0x" + HIcon.ToInt64().ToString("X") + ";title=" + (Title ?? "") + ";text=" + (ButtonText ?? "");
    }
}

public static class Rev16TrayNative {
    private const int TB_BUTTONCOUNT = 0x0418;
    private const int TB_GETBUTTON = 0x0417;
    private const int TB_GETBUTTONTEXTW = 0x044B;
    private const uint PROCESS_VM_OPERATION=0x0008, PROCESS_VM_READ=0x0010, PROCESS_VM_WRITE=0x0020, PROCESS_QUERY_INFORMATION=0x0400;
    private const uint MEM_COMMIT=0x1000, MEM_RESERVE=0x2000, MEM_RELEASE=0x8000, PAGE_READWRITE=0x04;
    private delegate bool EnumProc(IntPtr hwnd, IntPtr p);

    [StructLayout(LayoutKind.Sequential)] private struct TBBUTTON64 { public int iBitmap; public int idCommand; public byte fsState; public byte fsStyle; [MarshalAs(UnmanagedType.ByValArray, SizeConst=6)] public byte[] reserved; public UIntPtr dwData; public IntPtr iString; }
    [StructLayout(LayoutKind.Sequential)] private struct TRAYDATA64 { public IntPtr hwnd; public uint uID; public uint callback; public uint r0; public uint r1; public IntPtr hIcon; }

    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc cb, IntPtr p);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumProc cb, IntPtr p);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int max);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder text, int max);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("kernel32.dll", SetLastError=true)] private static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
    [DllImport("kernel32.dll", SetLastError=true)] private static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr a, UIntPtr s, uint type, uint protect);
    [DllImport("kernel32.dll", SetLastError=true)] private static extern bool VirtualFreeEx(IntPtr p, IntPtr a, UIntPtr s, uint type);
    [DllImport("kernel32.dll", SetLastError=true)] private static extern bool ReadProcessMemory(IntPtr p, IntPtr a, IntPtr b, UIntPtr s, out UIntPtr read);
    [DllImport("kernel32.dll")] private static extern bool CloseHandle(IntPtr h);

    public static Rev16TrayChild[] Children(IntPtr parent) {
        var list = new List<Rev16TrayChild>();
        EnumProc cb = delegate(IntPtr h, IntPtr _) {
            var t = new StringBuilder(512); var c = new StringBuilder(128);
            GetWindowText(h,t,t.Capacity); GetClassName(h,c,c.Capacity);
            list.Add(new Rev16TrayChild { Handle=h, Id=GetDlgCtrlID(h), Text=t.ToString(), ClassName=c.ToString(), Visible=IsWindowVisible(h) });
            return true;
        };
        EnumChildWindows(parent,cb,IntPtr.Zero); GC.KeepAlive(cb); return list.ToArray();
    }

    public static IntPtr FindTopWindowForProcess(int processId, string title) {
        IntPtr found = IntPtr.Zero;
        EnumProc cb = delegate(IntPtr h, IntPtr _) {
            uint pid; GetWindowThreadProcessId(h, out pid); if (pid != (uint)processId) return true;
            var t = new StringBuilder(512); GetWindowText(h,t,t.Capacity);
            if (String.Equals(t.ToString(), title, StringComparison.Ordinal)) { found = h; return false; }
            return true;
        };
        EnumWindows(cb,IntPtr.Zero); GC.KeepAlive(cb); return found;
    }

    public static Rev16TrayEntry[] Entries() {
        var result = new List<Rev16TrayEntry>();
        foreach (var toolbar in Toolbars()) Collect(toolbar, result);
        return result.ToArray();
    }

    private static List<IntPtr> Toolbars() {
        var result = new List<IntPtr>(); Add(FindWindow("Shell_TrayWnd", null), result); Add(FindWindow("NotifyIconOverflowWindow", null), result); return result;
    }
    private static void Add(IntPtr root, List<IntPtr> result) {
        if (root == IntPtr.Zero) return;
        EnumProc cb = delegate(IntPtr h, IntPtr _) { var s=new StringBuilder(128); GetClassName(h,s,s.Capacity); if (s.ToString()=="ToolbarWindow32" && !result.Contains(h)) result.Add(h); return true; };
        EnumChildWindows(root, cb, IntPtr.Zero); GC.KeepAlive(cb);
    }

    private static void Collect(IntPtr toolbar, List<Rev16TrayEntry> result) {
        uint shellPid; GetWindowThreadProcessId(toolbar, out shellPid); if(shellPid==0)return;
        IntPtr p=OpenProcess(PROCESS_VM_OPERATION|PROCESS_VM_READ|PROCESS_VM_WRITE|PROCESS_QUERY_INFORMATION,false,shellPid); if(p==IntPtr.Zero)return;
        int buttonSize=Marshal.SizeOf(typeof(TBBUTTON64));
        IntPtr remoteButton=VirtualAllocEx(p,IntPtr.Zero,new UIntPtr((uint)buttonSize),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        IntPtr remoteText=VirtualAllocEx(p,IntPtr.Zero,new UIntPtr(2048),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        if(remoteButton==IntPtr.Zero || remoteText==IntPtr.Zero) { if(remoteButton!=IntPtr.Zero)VirtualFreeEx(p,remoteButton,UIntPtr.Zero,MEM_RELEASE); if(remoteText!=IntPtr.Zero)VirtualFreeEx(p,remoteText,UIntPtr.Zero,MEM_RELEASE); CloseHandle(p); return; }
        try {
            int count=SendMessage(toolbar,TB_BUTTONCOUNT,IntPtr.Zero,IntPtr.Zero).ToInt32();
            for(int i=0;i<count;i++) {
                if(SendMessage(toolbar,TB_GETBUTTON,new IntPtr(i),remoteButton)==IntPtr.Zero)continue;
                TBBUTTON64 b; if(!Read(p,remoteButton,out b)||b.dwData==UIntPtr.Zero)continue;
                TRAYDATA64 d; if(!Read(p,new IntPtr(unchecked((long)b.dwData.ToUInt64())),out d)||d.hwnd==IntPtr.Zero)continue;
                uint owner; GetWindowThreadProcessId(d.hwnd,out owner);
                var title=new StringBuilder(512); GetWindowText(d.hwnd,title,title.Capacity);
                var cls=new StringBuilder(128); GetClassName(d.hwnd,cls,cls.Capacity);
                string buttonText="";
                var textLength=SendMessage(toolbar,TB_GETBUTTONTEXTW,new IntPtr(b.idCommand),remoteText).ToInt32();
                if(textLength>0) buttonText=ReadUtf16(p,remoteText,Math.Min(1023,textLength+1));
                result.Add(new Rev16TrayEntry { Toolbar=toolbar, Hwnd=d.hwnd, IconId=d.uID, HIcon=d.hIcon, OwnerPid=(int)owner, Title=title.ToString(), ClassName=cls.ToString(), ButtonText=buttonText });
            }
        } finally { VirtualFreeEx(p,remoteButton,UIntPtr.Zero,MEM_RELEASE); VirtualFreeEx(p,remoteText,UIntPtr.Zero,MEM_RELEASE); CloseHandle(p); }
    }

    private static string ReadUtf16(IntPtr p, IntPtr remote, int chars) {
        int bytes=Math.Max(2,chars*2); IntPtr local=Marshal.AllocHGlobal(bytes);
        try { UIntPtr read; if(!ReadProcessMemory(p,remote,local,new UIntPtr((uint)bytes),out read))return ""; int usable=(int)Math.Min((ulong)bytes,read.ToUInt64()); if(usable<=0)return ""; return Marshal.PtrToStringUni(local,usable/2).TrimEnd('\0'); }
        finally { Marshal.FreeHGlobal(local); }
    }
    private static bool Read<T>(IntPtr p, IntPtr remote, out T value) where T:struct {
        int size=Marshal.SizeOf(typeof(T)); IntPtr local=Marshal.AllocHGlobal(size); try { UIntPtr n; if(!ReadProcessMemory(p,remote,local,new UIntPtr((uint)size),out n)||n.ToUInt64()<(ulong)size){value=default(T);return false;} value=(T)Marshal.PtrToStructure(local,typeof(T));return true;} finally{Marshal.FreeHGlobal(local);} }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Get-Core([int]$ExcludeId = -1) {
    foreach ($candidate in @(Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue)) {
        try {
            if ($candidate.Id -eq $ExcludeId) { continue }
            if ([IO.Path]::GetFullPath($candidate.Path) -eq [IO.Path]::GetFullPath($corePath)) { return $candidate }
        } catch { }
    }
    return $null
}

function Wait-Core([int]$ExcludeId = -1, [int]$Seconds = 12) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    do {
        Start-Sleep -Milliseconds 100
        $candidate = Get-Core -ExcludeId $ExcludeId
        if ($candidate) { $candidate.Refresh(); if (-not $candidate.HasExited -and $candidate.MainWindowHandle -ne [IntPtr]::Zero) { return $candidate } }
    } while ([DateTime]::UtcNow -lt $deadline)
    return $null
}

function Open-SettingsAndEnableTray([Diagnostics.Process]$Core) {
    $children = @([Rev16TrayNative]::Children($Core.MainWindowHandle))
    $gear = $children | Where-Object { $_.Id -eq 906 -and $_.ClassName -eq 'Button' } | Select-Object -First 1
    if (-not $gear) { throw "Settings gear id=906 missing for core pid=$($Core.Id)." }
    [void][Rev16TrayNative]::SendMessage($gear.Handle,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
    $deadline=[DateTime]::UtcNow.AddSeconds(8)
    do {
        Start-Sleep -Milliseconds 120
        $children=@([Rev16TrayNative]::Children($Core.MainWindowHandle))
        $settingsHost=$children | Where-Object { $_.Id -eq 1492 -and $_.Visible } | Select-Object -First 1
    } while (-not $settingsHost -and [DateTime]::UtcNow -lt $deadline)
    if (-not $settingsHost) { throw "Enhanced Settings host id=1492 missing for core pid=$($Core.Id)." }
    $trayProxy=$children | Where-Object { $_.Id -eq 1503 } | Select-Object -First 1
    if (-not $trayProxy) { throw 'Tray proxy id=1503 missing.' }
    if ([Rev16TrayNative]::SendMessage($trayProxy.Handle,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 1) {
        [void][Rev16TrayNative]::SendMessage($trayProxy.Handle,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)
        Start-Sleep -Milliseconds 300
    }
}

function Get-CanonicalEntry([Diagnostics.Process]$Launcher) {
    $entries=@([Rev16TrayNative]::Entries())
    @($entries | Where-Object { $_.OwnerPid -eq $Launcher.Id -and $_.Title -eq 'DPopCleaner.TrayRamBadgeHost' -and $_.IconId -eq 1 }) | Select-Object -First 1
}

function Assert-SingleTray([Diagnostics.Process]$Launcher,[Diagnostics.Process]$Core,[string]$Phase) {
    $deadline=[DateTime]::UtcNow.AddSeconds(8)
    do {
        Start-Sleep -Milliseconds 250
        $entries=@([Rev16TrayNative]::Entries())
        $canonical=@($entries | Where-Object { $_.OwnerPid -eq $Launcher.Id -and $_.Title -eq 'DPopCleaner.TrayRamBadgeHost' -and $_.IconId -eq 1 })
        $dpop=@($entries | Where-Object {
            $_.OwnerPid -eq $Core.Id -or
            $_.OwnerPid -eq $Launcher.Id -or
            $_.Title -like '*DPopCleaner*' -or
            $_.ButtonText -like '*DPopCleaner*'
        })
        $extras=@($dpop | Where-Object { -not ($_.OwnerPid -eq $Launcher.Id -and $_.Title -eq 'DPopCleaner.TrayRamBadgeHost' -and $_.IconId -eq 1) })
    } while (($canonical.Count -ne 1 -or $extras.Count -ne 0 -or $canonical[0].HIcon -eq [IntPtr]::Zero) -and [DateTime]::UtcNow -lt $deadline)

    Write-Host "REV16_TRAY_$Phase canonical=$($canonical.Count) extras=$($extras.Count)"
    foreach($entry in $dpop){ Write-Host "REV16_TRAY_ROW $entry" }
    if($canonical.Count -ne 1){ throw "${Phase}: expected exactly one canonical DPopCleaner tray identity, found $($canonical.Count)." }
    if($canonical[0].HIcon -eq [IntPtr]::Zero){ throw "${Phase}: canonical tray identity has empty hIcon." }
    if($extras.Count -ne 0){ throw "${Phase}: legacy/duplicate/ghost DPopCleaner tray entries remain: $($extras -join ' | ')" }
    return $canonical[0]
}

$launcher=$null
$core1=$null
$core2=$null
$explorerRestarted=$false
try {
    $launcher=Start-Process -FilePath $launcherPath -ArgumentList @('--no-update-check','--settings-path',('"'+$settingsPath+'"')) -WorkingDirectory $RootPath -PassThru
    $core1=Wait-Core
    if(-not $core1){ throw 'Initial frozen core did not create a window.' }
    Open-SettingsAndEnableTray -Core $core1
    $initial=Assert-SingleTray -Launcher $launcher -Core $core1 -Phase 'INITIAL'
    Write-Host 'REV16_SINGLE_TRAY_INITIAL_OK'

    $oldCoreId=$core1.Id
    Stop-Process -Id $oldCoreId -Force
    $core1.WaitForExit(5000) | Out-Null
    $core1=$null
    $core2=Start-Process -FilePath $corePath -WorkingDirectory $RootPath -PassThru
    $deadline=[DateTime]::UtcNow.AddSeconds(12)
    do { Start-Sleep -Milliseconds 100; $core2.Refresh(); $launcher.Refresh() } while (($core2.MainWindowHandle -eq [IntPtr]::Zero -or $launcher.HasExited) -and [DateTime]::UtcNow -lt $deadline)
    if($launcher.HasExited){ throw 'Launcher exited during core language-restart simulation.' }
    if($core2.HasExited -or $core2.MainWindowHandle -eq [IntPtr]::Zero){ throw 'Successor core did not create a window.' }
    Open-SettingsAndEnableTray -Core $core2
    $afterRestart=Assert-SingleTray -Launcher $launcher -Core $core2 -Phase 'LANGUAGE_RESTART'
    if($afterRestart.Hwnd -ne $initial.Hwnd -or $afterRestart.IconId -ne $initial.IconId){
        throw "Tray identity changed across core restart: initial=$($initial.Identity) after=$($afterRestart.Identity)"
    }
    Write-Host 'REV16_SINGLE_TRAY_LANGUAGE_RESTART_OK'

    if($testExplorerRestart){
        Get-Process explorer -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 800
        Start-Process explorer.exe | Out-Null
        $explorerRestarted=$true
        $afterExplorer=Assert-SingleTray -Launcher $launcher -Core $core2 -Phase 'EXPLORER_RESTART'
        if($afterExplorer.Hwnd -ne $initial.Hwnd -or $afterExplorer.IconId -ne $initial.IconId){
            throw "Tray identity changed after Explorer restart: initial=$($initial.Identity) after=$($afterExplorer.Identity)"
        }
        Write-Host 'REV16_SINGLE_TRAY_EXPLORER_RESTART_OK'
    } else {
        Write-Host 'REV16_SINGLE_TRAY_EXPLORER_RESTART_SKIPPED_STAGE'
    }

    [pscustomobject]@{
        root=$RootPath
        installed=$installed
        launcher_pid=$launcher.Id
        old_core_pid=$oldCoreId
        new_core_pid=$core2.Id
        canonical_hwnd=('0x'+$initial.Hwnd.ToInt64().ToString('X'))
        canonical_uid=$initial.IconId
        explorer_restart=$testExplorerRestart
        no_legacy_or_ghost=$true
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputDir 'rev16-tray-report.json') -Encoding utf8
}
finally {
    if($core1 -and -not $core1.HasExited){ Stop-Process -Id $core1.Id -Force -ErrorAction SilentlyContinue }
    if($core2 -and -not $core2.HasExited){ Stop-Process -Id $core2.Id -Force -ErrorAction SilentlyContinue }
    if($launcher -and -not $launcher.HasExited){ Stop-Process -Id $launcher.Id -Force -ErrorAction SilentlyContinue }
    Remove-Item -LiteralPath $settingsPath -Force -ErrorAction SilentlyContinue
    if($installed){
        $uninstaller=Join-Path $installRoot 'unins000.exe'
        if(Test-Path -LiteralPath $uninstaller -PathType Leaf){ try{ Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait | Out-Null }catch{} }
        if(Test-Path -LiteralPath $installRoot){ Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue }
    }
}
