[CmdletBinding()]
param(
    [string]$ExePath = '_release/0.4.17/stage/DPopCleaner.exe'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$native = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public sealed class Rev7Child {
    public IntPtr Handle;
    public int Id;
    public string Text;
    public string ClassName;
    public bool Visible;
    public int Left;
    public int Top;
    public int Right;
    public int Bottom;
}
public static class Rev7Native {
    private delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
    [StructLayout(LayoutKind.Sequential)] private struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc proc, IntPtr lParam);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int max);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder text, int max);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, StringBuilder lp);
    public static Rev7Child[] Children(IntPtr parent) {
        var list = new List<Rev7Child>();
        EnumProc cb = delegate(IntPtr h, IntPtr _) {
            var t = new StringBuilder(512); var c = new StringBuilder(128); RECT r;
            GetWindowText(h,t,t.Capacity); GetClassName(h,c,c.Capacity); GetWindowRect(h,out r);
            list.Add(new Rev7Child { Handle=h, Id=GetDlgCtrlID(h), Text=t.ToString(), ClassName=c.ToString(), Visible=IsWindowVisible(h), Left=r.Left, Top=r.Top, Right=r.Right, Bottom=r.Bottom });
            return true;
        };
        EnumChildWindows(parent, cb, IntPtr.Zero); GC.KeepAlive(cb); return list.ToArray();
    }
    public static string[] ComboItems(IntPtr combo) {
        const uint CB_GETCOUNT = 0x0146, CB_GETLBTEXT = 0x0148, CB_GETLBTEXTLEN = 0x0149;
        int count = SendMessage(combo, CB_GETCOUNT, IntPtr.Zero, IntPtr.Zero).ToInt32();
        var values = new List<string>();
        for (int i=0;i<count;i++) {
            int len = SendMessage(combo, CB_GETLBTEXTLEN, (IntPtr)i, IntPtr.Zero).ToInt32();
            var text = new StringBuilder(Math.Max(1,len+1));
            SendMessage(combo, CB_GETLBTEXT, (IntPtr)i, text);
            values.Add(text.ToString());
        }
        return values.ToArray();
    }
}
'@
Add-Type -TypeDefinition $native -Language CSharp

function Get-Children([IntPtr]$Window) { @([Rev7Native]::Children($Window)) }
function Dump-Page([string]$Name, [IntPtr]$Window) {
    Write-Host "===== REV7_UI_PAGE=$Name ====="
    Get-Children $Window | Where-Object { $_.Visible } | Sort-Object Top,Left | Select-Object Id,ClassName,Text,Left,Top,Right,Bottom | Format-Table -AutoSize | Out-String -Width 320 | Write-Host
}
function Click-ByText([IntPtr]$Window, [string]$Text) {
    $item = Get-Children $Window | Where-Object { $_.Visible -and $_.ClassName -eq 'Button' -and $_.Text -eq $Text } | Select-Object -First 1
    if (-not $item) { throw "Button '$Text' not found." }
    [void][Rev7Native]::SendMessage($item.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 500
}

$exe = (Resolve-Path -LiteralPath $ExePath).Path
$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path -Parent $exe) -PassThru
try {
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    do { Start-Sleep -Milliseconds 200; $p.Refresh() } while ($p.MainWindowHandle -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)
    if ($p.MainWindowHandle -eq [IntPtr]::Zero) { throw 'DPopCleaner window did not appear.' }
    $window = $p.MainWindowHandle

    Dump-Page 'overview' $window
    Click-ByText $window 'ОЗУ'
    Dump-Page 'ram' $window
    $ramCombo = Get-Children $window | Where-Object { $_.Id -eq 1956 -and $_.ClassName -eq 'ComboBox' } | Select-Object -First 1
    if (-not $ramCombo) { throw 'RAM threshold ComboBox id=1956 not found.' }
    $ramItems = @([Rev7Native]::ComboItems($ramCombo.Handle))
    Write-Host "REV7_RAM_THRESHOLD_ITEMS=$($ramItems -join '|')"

    Click-ByText $window 'Zapret'
    Dump-Page 'zapret' $window

    $settings = Get-Children $window | Where-Object { $_.Id -eq 906 -and $_.ClassName -eq 'Button' } | Select-Object -First 1
    if (-not $settings) { throw 'Settings gear id=906 not found.' }
    [void][Rev7Native]::SendMessage($settings.Handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 500
    Dump-Page 'settings' $window

    Write-Host 'REV7_UI_DIAGNOSTIC_OK'
}
finally {
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
}
