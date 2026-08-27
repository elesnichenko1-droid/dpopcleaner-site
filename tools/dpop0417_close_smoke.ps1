[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$helper = Join-Path $root 'v0417/src/SimpleUpdate/bin/Release/net48/SimpleUpdate.exe'
if (-not (Test-Path -LiteralPath $helper -PathType Leaf)) { throw "SimpleUpdate not built: $helper" }

$work = Join-Path ([IO.Path]::GetTempPath()) ('DPopSimpleCloseSmoke-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work -Force | Out-Null
Copy-Item -LiteralPath $helper -Destination (Join-Path $work 'SimpleUpdate.exe') -Force
$settings = Join-Path $work 'SimpleUpdate.ini'
$sourcePath = Join-Path $work 'FakeCore.cs'
$fakeCore = Join-Path $work 'DPopCleaner.exe'

@'
using System;
using System.Windows.Forms;

internal static class FakeCoreProgram
{
    [STAThread]
    private static void Main()
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new HangingCloseForm());
    }
}

internal sealed class HangingCloseForm : Form
{
    internal HangingCloseForm()
    {
        Text = "DPopCleaner";
        Width = 900;
        Height = 600;
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        e.Cancel = true;
        Hide();
    }
}
'@ | Set-Content -LiteralPath $sourcePath -Encoding utf8

$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $csc -PathType Leaf)) { throw "csc.exe not found: $csc" }
& $csc /nologo /target:winexe "/out:$fakeCore" /reference:System.Windows.Forms.dll /reference:System.dll $sourcePath
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $fakeCore -PathType Leaf)) { throw 'Failed to compile hanging fake DPopCleaner core.' }

$native = @'
using System;
using System.Runtime.InteropServices;
public static class CloseSmokeNative
{
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);
}
'@
Add-Type -TypeDefinition $native -Language CSharp

$launcher = $null
$core = $null
try {
    $launcher = Start-Process -FilePath (Join-Path $work 'SimpleUpdate.exe') -ArgumentList @('--no-update-check','--settings-path',('"' + $settings + '"')) -WorkingDirectory $work -PassThru

    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    do {
        Start-Sleep -Milliseconds 100
        $candidate = Get-Process -Name 'DPopCleaner' -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $fakeCore } | Select-Object -First 1
        if ($candidate) { $core = $candidate; $core.Refresh() }
    } while (($null -eq $core -or $core.MainWindowHandle -eq [IntPtr]::Zero) -and [DateTime]::UtcNow -lt $deadline)

    if ($null -eq $core -or $core.MainWindowHandle -eq [IntPtr]::Zero) { throw 'Fake DPopCleaner did not create a visible window.' }

    $sw = [Diagnostics.Stopwatch]::StartNew()
    if (-not [CloseSmokeNative]::PostMessage($core.MainWindowHandle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)) {
        throw 'Failed to post WM_CLOSE to fake core.'
    }

    $deadline = [DateTime]::UtcNow.AddMilliseconds(1400)
    do {
        Start-Sleep -Milliseconds 40
        $core.Refresh()
        $launcher.Refresh()
    } while ((-not $core.HasExited -or -not $launcher.HasExited) -and [DateTime]::UtcNow -lt $deadline)
    $sw.Stop()

    if (-not $core.HasExited) { throw "Hidden DPopCleaner process still alive after close ($($sw.ElapsedMilliseconds) ms)." }
    if (-not $launcher.HasExited) { throw "SimpleUpdate still alive after DPopCleaner close ($($sw.ElapsedMilliseconds) ms)." }
    if ($sw.ElapsedMilliseconds -gt 1400) { throw "Close bridge exceeded 1400 ms: $($sw.ElapsedMilliseconds) ms" }

    Write-Host "SIMPLEUPDATE_FAST_CLOSE_OK elapsed_ms=$($sw.ElapsedMilliseconds)"
}
finally {
    if ($core -and -not $core.HasExited) { Stop-Process -Id $core.Id -Force -ErrorAction SilentlyContinue }
    if ($launcher -and -not $launcher.HasExited) { Stop-Process -Id $launcher.Id -Force -ErrorAction SilentlyContinue }
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
