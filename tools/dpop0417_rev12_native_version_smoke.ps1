[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$InstallerPath,
    [string]$OutputDir = '_release/0.4.17/evidence/rev12-native-version'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$InstallerPath = if ([IO.Path]::IsPathRooted($InstallerPath)) { $InstallerPath } else { Join-Path $repoRoot $InstallerPath }
$OutputDir = if ([IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repoRoot $OutputDir }
$InstallerPath = [IO.Path]::GetFullPath($InstallerPath)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
if (-not (Test-Path -LiteralPath $InstallerPath -PathType Leaf)) { throw "Installer missing: $InstallerPath" }

Add-Type -AssemblyName System.Drawing
$native = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class Rev12Child {
    public IntPtr Handle;
    public int Id;
    public string Text;
    public string ClassName;
    public bool Visible;
    public long Style;
}
public static class Rev12Native {
    private delegate bool EnumProc(IntPtr hwnd, IntPtr p);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc proc, IntPtr p);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll", EntryPoint="GetWindowLongPtrW")] private static extern IntPtr GetWindowLongPtr64(IntPtr hwnd, int index);
    [DllImport("user32.dll", EntryPoint="GetWindowLongW")] private static extern int GetWindowLong32(IntPtr hwnd, int index);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll", SetLastError=true)] public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll", SetLastError=true)] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hwnd, int command);
    private static long StyleOf(IntPtr hwnd) { return IntPtr.Size == 8 ? GetWindowLongPtr64(hwnd, -16).ToInt64() : GetWindowLong32(hwnd, -16); }
    public static Rev12Child[] Children(IntPtr parent) {
        var list = new List<Rev12Child>();
        EnumProc cb = delegate(IntPtr h, IntPtr _) {
            var t = new StringBuilder(512); var c = new StringBuilder(128);
            GetWindowText(h,t,t.Capacity); GetClassName(h,c,c.Capacity);
            list.Add(new Rev12Child { Handle=h, Id=GetDlgCtrlID(h), Text=t.ToString(), ClassName=c.ToString(), Visible=IsWindowVisible(h), Style=StyleOf(h) });
            return true;
        };
        EnumChildWindows(parent, cb, IntPtr.Zero); GC.KeepAlive(cb); return list.ToArray();
    }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Get-Children([IntPtr]$Window) { @([Rev12Native]::Children($Window)) }
function Find-Child([IntPtr]$Window, [int]$Id, [switch]$Visible) {
    Get-Children $Window | Where-Object { $_.Id -eq $Id -and ((-not $Visible) -or $_.Visible) } | Select-Object -First 1
}
function Click-Id([IntPtr]$Window, [int]$Id) {
    $child = Find-Child $Window $Id
    if (-not $child) { throw "Control id=$Id not found." }
    [void][Rev12Native]::SendMessage($child.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
}
function Wait-Visible([IntPtr]$Window, [int]$Id, [int]$TimeoutMs = 6000) {
    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMs)
    do {
        Start-Sleep -Milliseconds 100
        $child = Find-Child $Window $Id -Visible
        if ($child) { return $child }
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Visible control id=$Id did not appear."
}
function Wait-OwnerDraw([IntPtr]$Window, [int]$Id, [int]$TimeoutMs = 6000) {
    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMs)
    $lastStyle = 0
    do {
        Start-Sleep -Milliseconds 100
        $child = Find-Child $Window $Id -Visible
        if ($child) {
            $lastStyle = $child.Style
            if (($child.Style -band 0xF) -eq 0xB) { return $child }
        }
    } while ([DateTime]::UtcNow -lt $deadline)
    throw ("Visible bridge button id={0} did not settle to BS_OWNERDRAW; lastStyle=0x{1:X}." -f $Id,$lastStyle)
}
function Assert-NoForbiddenVersionProxy1726([IntPtr]$Window) {
    foreach ($child in @(Get-Children $Window | Where-Object { $_.Id -eq 1726 })) {
        $isRev19ServiceHeading = $child.ClassName -eq 'Static' -and
            ($child.Text -eq 'Сервисные действия' -or $child.Text -eq 'Service actions')
        if (-not $isRev19ServiceHeading) {
            throw 'Forbidden rev.10 version proxy id=1726 exists.'
        }
    }
}
function Capture-Window([IntPtr]$Window, [string]$Target) {
    [void][Rev12Native]::ShowWindow($Window, 9)
    [void][Rev12Native]::SetForegroundWindow($Window)
    Start-Sleep -Milliseconds 300
    $rect = New-Object Rev12Native+RECT
    if (-not [Rev12Native]::GetWindowRect($Window, [ref]$rect)) { throw 'GetWindowRect failed.' }
    $width = $rect.Right - $rect.Left; $height = $rect.Bottom - $rect.Top
    if ($width -lt 800 -or $height -lt 500) { throw "Unexpected DPopCleaner viewport: ${width}x${height}" }
    $bitmap = New-Object System.Drawing.Bitmap($width,$height,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $hdc = $graphics.GetHdc()
    try {
        if (-not [Rev12Native]::PrintWindow($Window,$hdc,2)) {
            if (-not [Rev12Native]::PrintWindow($Window,$hdc,0)) { throw 'PrintWindow failed.' }
        }
    }
    finally { $graphics.ReleaseHdc($hdc); $graphics.Dispose() }
    $bitmap.Save($Target,[System.Drawing.Imaging.ImageFormat]::Png); $bitmap.Dispose()
    if ((Get-Item -LiteralPath $Target).Length -lt 10000) { throw "Captured Zapret PNG is unexpectedly small: $Target" }
}

$installRoot = Join-Path $env:TEMP 'dpop0417-rev12-native-version'
$settingsPath = Join-Path $OutputDir 'SimpleUpdate-rev12.ini'
$reportPath = Join-Path $OutputDir 'rev12-native-version-smoke-report.json'
$screenshotPath = Join-Path $OutputDir 'rev12-zapret-native-version.png'
$launcher = $null; $core = $null; $installed = $false
try {
    Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue
    $setup = Start-Process -FilePath $InstallerPath -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',('/DIR=' + $installRoot)) -Wait -PassThru
    if ($setup.ExitCode -ne 0) { throw "Installer failed: $($setup.ExitCode)" }
    $installed = $true

    $app = Join-Path $installRoot 'DPopCleaner.exe'
    $corePath = Join-Path $installRoot 'DPopCleaner.Core.exe'
    $serviceVersionPath = Join-Path $installRoot 'Zapret\.service\version.txt'
    $nativeVersionPath = Join-Path $installRoot 'Zapret\utils\dpop_version.txt'
    foreach ($required in @($app,$corePath,$serviceVersionPath,$nativeVersionPath)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Installed rev.12 prerequisite missing: $required" }
    }
    $serviceVersion = (Get-Content -Raw -LiteralPath $serviceVersionPath).Trim()
    $nativeVersion = (Get-Content -Raw -LiteralPath $nativeVersionPath).Trim()
    if ($serviceVersion -ne '1.10.2' -or $nativeVersion -ne '1.10.2') { throw "Version metadata mismatch: service=$serviceVersion native=$nativeVersion" }
    $serviceHash = (Get-FileHash -LiteralPath $serviceVersionPath -Algorithm SHA256).Hash
    $nativeHash = (Get-FileHash -LiteralPath $nativeVersionPath -Algorithm SHA256).Hash
    if ($serviceHash -ne $nativeHash) { throw 'Native dpop_version.txt is not byte-identical to pinned .service/version.txt.' }

    $launcher = Start-Process -FilePath $app -ArgumentList @('--no-update-check','--settings-path',('"' + $settingsPath + '"')) -WorkingDirectory $installRoot -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    do {
        Start-Sleep -Milliseconds 200
        foreach ($candidate in @(Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue)) {
            try { if ([IO.Path]::GetFullPath($candidate.Path) -eq [IO.Path]::GetFullPath($corePath)) { $core=$candidate; $core.Refresh(); break } } catch {}
        }
    } while (($null -eq $core -or $core.MainWindowHandle -eq [IntPtr]::Zero) -and [DateTime]::UtcNow -lt $deadline)
    if ($null -eq $core -or $core.MainWindowHandle -eq [IntPtr]::Zero) { throw 'Installed frozen-core window did not appear.' }
    $window = $core.MainWindowHandle

    Click-Id $window 905
    [void](Wait-Visible $window 1703 6000)
    foreach ($id in @(1720,1721,1722,1723,1724,1725)) {
        $button = Wait-OwnerDraw $window $id 6000
        if (($button.Style -band 0xF) -ne 0xB) { throw "Bridge button id=$id is not owner-draw." }
    }
    Assert-NoForbiddenVersionProxy1726 $window

    # The visual line reported by the user is custom/native rendering and is not a
    # trustworthy GetWindowText target. Save the actual installed Zapret page as
    # release evidence so the rendered version can be inspected, not inferred.
    Start-Sleep -Milliseconds 1200
    Capture-Window $window $screenshotPath

    [ordered]@{
        installed_native_version_file = $nativeVersionPath
        native_version = $nativeVersion
        service_version = $serviceVersion
        version_metadata_byte_identical = $true
        version_proxy_1726_absent = $true
        bridge_buttons_owner_draw = $true
        screenshot = $screenshotPath
    } | ConvertTo-Json | Set-Content -LiteralPath $reportPath -Encoding utf8

    Write-Host 'REV12_NATIVE_ZAPRET_VERSION_SMOKE_OK'
    Write-Host 'Frozen-core native version source utils/dpop_version.txt=1.10.2: PASS'
    Write-Host 'No version HWND rewrite/proxy required: PASS'
    Write-Host "Native Zapret screenshot evidence: $screenshotPath"
}
finally {
    if ($core -and -not $core.HasExited) { Stop-Process -Id $core.Id -Force -ErrorAction SilentlyContinue }
    if ($launcher -and -not $launcher.HasExited) { Stop-Process -Id $launcher.Id -Force -ErrorAction SilentlyContinue }
    if ($installed) {
        $uninstaller = Join-Path $installRoot 'unins000.exe'
        if (Test-Path -LiteralPath $uninstaller -PathType Leaf) { try { Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait | Out-Null } catch {} }
    }
    Remove-Item -LiteralPath $installRoot -Recurse -Force -ErrorAction SilentlyContinue
}