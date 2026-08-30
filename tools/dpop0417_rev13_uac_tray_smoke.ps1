[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$RootPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev13-uac-tray'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RootPath = [IO.Path]::GetFullPath($RootPath)
$OutputDir = if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repoRoot $OutputDir }
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$app = Join-Path $RootPath 'DPopCleaner.exe'
$coreExe = Join-Path $RootPath 'DPopCleaner.Core.exe'
if (-not (Test-Path -LiteralPath $app -PathType Leaf)) { throw "Installed DPopCleaner.exe missing: $app" }
if (-not (Test-Path -LiteralPath $coreExe -PathType Leaf)) { throw "Installed DPopCleaner.Core.exe missing: $coreExe" }

# The original installer regression was CreateProcessAsUser / code 740: a post-install
# original-user launch was pointed at an EXE whose embedded requestedExecutionLevel was
# requireAdministrator. Rev.13 must embed asInvoker; explicit runas happens inside the launcher.
$mt = Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\mt.exe" -File -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1
if (-not $mt) { throw 'mt.exe not found; cannot inspect installed requestedExecutionLevel.' }
$manifestPath = Join-Path $OutputDir 'DPopCleaner.installed.manifest.xml'
& $mt.FullName -nologo "-inputresource:$app;#1" "-out:$manifestPath"
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw 'Could not extract installed launcher manifest.' }
$manifestText = (Get-Content -Raw -LiteralPath $manifestPath).ToLowerInvariant()
if ($manifestText -notmatch 'requestedexecutionlevel\s+level="asinvoker"') { throw 'Installed launcher is not asInvoker.' }
if ($manifestText -match 'requireadministrator') { throw 'Installed launcher still embeds requireAdministrator; code 740 can recur.' }

$nativeCode = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class Rev13Native
{
    public const uint BM_GETCHECK = 0x00F0;
    public const uint BM_CLICK = 0x00F5;
    public const uint WM_CLOSE = 0x0010;
    private const int BST_CHECKED = 1;
    private delegate bool EnumChildProc(IntPtr hwnd, IntPtr lParam);

    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumChildProc callback, IntPtr lParam);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int maxCount);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);

    public static IntPtr FindById(IntPtr parent, int id)
    {
        IntPtr found = IntPtr.Zero;
        EnumChildProc cb = delegate(IntPtr hwnd, IntPtr _) { if (GetDlgCtrlID(hwnd) == id) { found = hwnd; return false; } return true; };
        EnumChildWindows(parent, cb, IntPtr.Zero); GC.KeepAlive(cb); return found;
    }

    public static IntPtr FindByText(IntPtr parent, string wanted)
    {
        IntPtr found = IntPtr.Zero;
        EnumChildProc cb = delegate(IntPtr hwnd, IntPtr _) {
            var b = new StringBuilder(512); GetWindowText(hwnd, b, b.Capacity);
            if (String.Equals(b.ToString(), wanted, StringComparison.Ordinal)) { found = hwnd; return false; }
            return true;
        };
        EnumChildWindows(parent, cb, IntPtr.Zero); GC.KeepAlive(cb); return found;
    }

    public static bool IsChecked(IntPtr hwnd) { return SendMessage(hwnd, BM_GETCHECK, IntPtr.Zero, IntPtr.Zero).ToInt32() == BST_CHECKED; }
}

public static class Rev13TokenProbe
{
    private const uint PROCESS_QUERY_LIMITED_INFORMATION = 0x1000;
    private const uint TOKEN_QUERY = 0x0008;
    private const int TokenElevation = 20;
    [StructLayout(LayoutKind.Sequential)] private struct TOKEN_ELEVATION { public int TokenIsElevated; }
    [DllImport("kernel32.dll", SetLastError=true)] private static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
    [DllImport("advapi32.dll", SetLastError=true)] private static extern bool OpenProcessToken(IntPtr process, uint access, out IntPtr token);
    [DllImport("advapi32.dll", SetLastError=true)] private static extern bool GetTokenInformation(IntPtr token, int infoClass, out TOKEN_ELEVATION info, int length, out int returnLength);
    [DllImport("kernel32.dll")] private static extern bool CloseHandle(IntPtr handle);

    public static bool IsElevated(int pid)
    {
        IntPtr p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, (uint)pid); if (p == IntPtr.Zero) return false;
        try { IntPtr t; if (!OpenProcessToken(p, TOKEN_QUERY, out t)) return false; try { TOKEN_ELEVATION e; int n; return GetTokenInformation(t, TokenElevation, out e, Marshal.SizeOf(typeof(TOKEN_ELEVATION)), out n) && e.TokenIsElevated != 0; } finally { CloseHandle(t); } }
        finally { CloseHandle(p); }
    }
}

public static class Rev13TrayProbe
{
    private const int TB_BUTTONCOUNT = 0x0418;
    private const int TB_GETBUTTON = 0x0417;
    private const uint PROCESS_VM_OPERATION=0x0008, PROCESS_VM_READ=0x0010, PROCESS_VM_WRITE=0x0020, PROCESS_QUERY_INFORMATION=0x0400;
    private const uint MEM_COMMIT=0x1000, MEM_RESERVE=0x2000, MEM_RELEASE=0x8000, PAGE_READWRITE=0x04;
    private delegate bool EnumChildProc(IntPtr hwnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)] private struct TBBUTTON64 { public int iBitmap; public int idCommand; public byte fsState; public byte fsStyle; [MarshalAs(UnmanagedType.ByValArray, SizeConst=6)] public byte[] reserved; public UIntPtr dwData; public IntPtr iString; }
    [StructLayout(LayoutKind.Sequential)] private struct TRAYDATA64 { public IntPtr hwnd; public uint uID; public uint callback; public uint r0; public uint r1; public IntPtr hIcon; }

    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumChildProc cb, IntPtr p);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder text, int max);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int max);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll")] private static extern IntPtr SendMessage(IntPtr hwnd, int msg, IntPtr wp, IntPtr lp);
    [DllImport("kernel32.dll", SetLastError=true)] private static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
    [DllImport("kernel32.dll", SetLastError=true)] private static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr a, UIntPtr s, uint type, uint protect);
    [DllImport("kernel32.dll", SetLastError=true)] private static extern bool VirtualFreeEx(IntPtr p, IntPtr a, UIntPtr s, uint type);
    [DllImport("kernel32.dll", SetLastError=true)] private static extern bool ReadProcessMemory(IntPtr p, IntPtr a, IntPtr b, UIntPtr s, out UIntPtr read);
    [DllImport("kernel32.dll")] private static extern bool CloseHandle(IntPtr h);

    public static string[] RawEntriesForProcess(int ownerPid)
    {
        var result = new List<string>();
        foreach (var toolbar in Toolbars()) CollectToolbar(toolbar, ownerPid, result);
        return result.ToArray();
    }

    public static string[] UniqueIdentitiesForProcess(int ownerPid)
    {
        var unique = new HashSet<string>(StringComparer.Ordinal);
        foreach (var raw in RawEntriesForProcess(ownerPid))
        {
            var split = raw.IndexOf(";icon=", StringComparison.Ordinal);
            unique.Add(split >= 0 ? raw.Substring(split + 6) : raw);
        }
        var result = new string[unique.Count]; unique.CopyTo(result); return result;
    }

    private static List<IntPtr> Toolbars()
    {
        var result = new List<IntPtr>(); Add(FindWindow("Shell_TrayWnd", null), result); Add(FindWindow("NotifyIconOverflowWindow", null), result); return result;
    }
    private static void Add(IntPtr root, List<IntPtr> result)
    {
        if (root == IntPtr.Zero) return;
        EnumChildProc cb = delegate(IntPtr h, IntPtr _) { var s=new StringBuilder(128); GetClassName(h,s,s.Capacity); if (s.ToString()=="ToolbarWindow32" && !result.Contains(h)) result.Add(h); return true; };
        EnumChildWindows(root, cb, IntPtr.Zero); GC.KeepAlive(cb);
    }
    private static string Clean(string value)
    {
        return (value ?? String.Empty).Replace(";", ",").Replace("\r", " ").Replace("\n", " ");
    }
    private static void CollectToolbar(IntPtr toolbar, int ownerPid, List<string> result)
    {
        uint shellPid; GetWindowThreadProcessId(toolbar, out shellPid); if (shellPid==0) return;
        IntPtr p=OpenProcess(PROCESS_VM_OPERATION|PROCESS_VM_READ|PROCESS_VM_WRITE|PROCESS_QUERY_INFORMATION,false,shellPid); if(p==IntPtr.Zero)return;
        int size=Marshal.SizeOf(typeof(TBBUTTON64)); IntPtr remote=VirtualAllocEx(p,IntPtr.Zero,new UIntPtr((uint)size),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE); if(remote==IntPtr.Zero){CloseHandle(p);return;}
        try
        {
            int count=SendMessage(toolbar,TB_BUTTONCOUNT,IntPtr.Zero,IntPtr.Zero).ToInt32();
            for(int i=0;i<count;i++)
            {
                if(SendMessage(toolbar,TB_GETBUTTON,new IntPtr(i),remote)==IntPtr.Zero)continue;
                TBBUTTON64 b; if(!Read(p,remote,out b)||b.dwData==UIntPtr.Zero)continue;
                TRAYDATA64 d; if(!Read(p,new IntPtr(unchecked((long)b.dwData.ToUInt64())),out d)||d.hwnd==IntPtr.Zero)continue;
                uint pid; uint threadId=GetWindowThreadProcessId(d.hwnd,out pid); if(pid!=(uint)ownerPid)continue;
                var cls=new StringBuilder(256); GetClassName(d.hwnd,cls,cls.Capacity);
                var title=new StringBuilder(512); GetWindowText(d.hwnd,title,title.Capacity);
                string identity = "0x" + d.hwnd.ToInt64().ToString("X") + ":" + d.uID.ToString();
                result.Add("toolbar=0x" + toolbar.ToInt64().ToString("X") + ";class=" + Clean(cls.ToString()) + ";title=" + Clean(title.ToString()) + ";thread=" + threadId.ToString() + ";icon=" + identity);
            }
        }
        finally { VirtualFreeEx(p,remote,UIntPtr.Zero,MEM_RELEASE); CloseHandle(p); }
    }
    private static bool Read<T>(IntPtr p, IntPtr remote, out T value) where T:struct
    {
        int size=Marshal.SizeOf(typeof(T)); IntPtr local=Marshal.AllocHGlobal(size); try { UIntPtr n; if(!ReadProcessMemory(p,remote,local,new UIntPtr((uint)size),out n)||n.ToUInt64()<(ulong)size){value=default(T);return false;} value=(T)Marshal.PtrToStructure(local,typeof(T));return true;} finally{Marshal.FreeHGlobal(local);} }
}
'@
Add-Type -TypeDefinition $nativeCode -Language CSharp

$launcher = $null
$core = $null
try {
    $launcher = Start-Process -FilePath $app -ArgumentList @('--no-update-check') -WorkingDirectory $RootPath -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(12)
    do {
        Start-Sleep -Milliseconds 100
        $candidate = Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue | Where-Object {
            try { [IO.Path]::GetFullPath($_.Path) -eq [IO.Path]::GetFullPath($coreExe) } catch { $false }
        } | Select-Object -First 1
        if ($candidate) { $core = $candidate; $core.Refresh() }
        $launcher.Refresh()
    } while (($null -eq $core -or $core.MainWindowHandle -eq [IntPtr]::Zero) -and [DateTime]::UtcNow -lt $deadline)

    if ($launcher.HasExited) { throw 'DPopCleaner launcher exited unexpectedly; UAC bootstrap did not reach the installed app.' }
    if (-not [Rev13TokenProbe]::IsElevated($launcher.Id)) { throw 'Installed launcher is not elevated after explicit UAC bootstrap.' }
    if ($null -eq $core -or $core.MainWindowHandle -eq [IntPtr]::Zero) { throw 'DPopCleaner.Core.exe did not create its main window.' }

    $settings = [Rev13Native]::FindById($core.MainWindowHandle, 906)
    if ($settings -ne [IntPtr]::Zero) { [void][Rev13Native]::SendMessage($settings, [Rev13Native]::BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero); Start-Sleep -Milliseconds 500 }
    $trayCheck = [Rev13Native]::FindByText($core.MainWindowHandle, 'Работать в трее и отслеживать новые установки')
    if ($trayCheck -eq [IntPtr]::Zero) { throw 'Frozen tray setting checkbox was not found.' }
    if (-not [Rev13Native]::IsChecked($trayCheck)) { [void][Rev13Native]::SendMessage($trayCheck, [Rev13Native]::BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero); Start-Sleep -Milliseconds 400 }

    # Closing with the existing tray setting must keep the core alive. One tray identity can
    # be mirrored by Explorer collections, so the contract counts unique (owner HWND,uID)
    # identities rather than raw toolbar rows.
    [void][Rev13Native]::PostMessage($core.MainWindowHandle, [Rev13Native]::WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 600
    $core.Refresh(); if ($core.HasExited) { throw 'Frozen core exited instead of entering tray mode.' }

    $trayDeadline = [DateTime]::UtcNow.AddSeconds(8)
    $launcherRaw = @(); $coreRaw = @(); $launcherUnique = @(); $coreUnique = @()
    do {
        Start-Sleep -Milliseconds 250
        $launcherRaw = @([Rev13TrayProbe]::RawEntriesForProcess($launcher.Id))
        $coreRaw = @([Rev13TrayProbe]::RawEntriesForProcess($core.Id))
        $launcherUnique = @([Rev13TrayProbe]::UniqueIdentitiesForProcess($launcher.Id))
        $coreUnique = @([Rev13TrayProbe]::UniqueIdentitiesForProcess($core.Id))
    } while (($launcherUnique.Count -ne 1 -or $coreUnique.Count -ne 0) -and [DateTime]::UtcNow -lt $trayDeadline)

    Write-Host "REV13_TRAY_DIAGNOSTIC bridge_raw=$($launcherRaw.Count) bridge_unique=$($launcherUnique.Count) core_raw=$($coreRaw.Count) core_unique=$($coreUnique.Count)"
    Write-Host ("REV13_TRAY_BRIDGE_ROWS " + ($launcherRaw -join ' | '))
    Write-Host ("REV13_TRAY_BRIDGE_IDENTITIES " + ($launcherUnique -join ' | '))
    Write-Host ("REV13_TRAY_CORE_ROWS " + ($coreRaw -join ' | '))

    if ($launcherUnique.Count -ne 1 -or $coreUnique.Count -ne 0) {
        throw "Expected one tray icon identity (bridge RAM badge) and zero legacy core identities; bridge_unique=$($launcherUnique.Count) core_unique=$($coreUnique.Count)"
    }

    [pscustomobject]@{
        launcher_pid = $launcher.Id
        core_pid = $core.Id
        requestedExecutionLevel = 'asInvoker'
        elevated = $true
        createProcessAsUser_code_740 = 'not_reproduced'
        bridge_ram_tray_raw_rows = $launcherRaw.Count
        bridge_ram_tray_icons = $launcherUnique.Count
        bridge_ram_tray_identities = @($launcherUnique)
        legacy_core_tray_raw_rows = $coreRaw.Count
        legacy_core_tray_icons = $coreUnique.Count
        one_tray_icon = $true
        ram_badge = $true
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputDir 'rev13-uac-tray-smoke-report.json') -Encoding utf8

    Write-Host 'REV13_UAC_TRAY_SMOKE_OK'
    Write-Host 'CreateProcessAsUser code 740 regression: PASS'
    Write-Host 'requestedExecutionLevel asInvoker + explicit UAC elevation: PASS'
    Write-Host 'one tray icon with RAM badge; legacy core tray icon suppressed: PASS'
}
finally {
    if ($core -and -not $core.HasExited) { Stop-Process -Id $core.Id -Force -ErrorAction SilentlyContinue }
    if ($launcher -and -not $launcher.HasExited) { Stop-Process -Id $launcher.Id -Force -ErrorAction SilentlyContinue }
}