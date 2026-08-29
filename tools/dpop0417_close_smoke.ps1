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
$fakeCore = Join-Path $work 'DPopCleaner.Core.exe'

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
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $fakeCore -PathType Leaf)) { throw 'Failed to compile hanging fake DPopCleaner.Core.exe.' }

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
        $candidate = Get-Process -Name 'DPopCleaner.Core' -ErrorAction SilentlyContinue | Where-Object {
            try { [IO.Path]::GetFullPath($_.Path) -eq [IO.Path]::GetFullPath($fakeCore) } catch { $false }
        } | Select-Object -First 1
        if ($candidate) { $core = $candidate; $core.Refresh() }
    } while (($null -eq $core -or $core.MainWindowHandle -eq [IntPtr]::Zero) -and [DateTime]::UtcNow -lt $deadline)

    if ($null -eq $core -or $core.MainWindowHandle -eq [IntPtr]::Zero) { throw 'Fake DPopCleaner.Core.exe did not create a visible window.' }

    # The fake core intentionally turns WM_CLOSE into Hide(), reproducing tray/minimize behavior.
    if (-not [CloseSmokeNative]::PostMessage($core.MainWindowHandle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)) {
        throw 'Failed to post WM_CLOSE to fake core.'
    }
    Start-Sleep -Milliseconds 1400
    $core.Refresh(); $launcher.Refresh()
    if ($core.HasExited) { throw 'Hidden DPopCleaner.Core was incorrectly terminated by the bridge.' }
    if ($launcher.HasExited) { throw 'SimpleUpdate exited while the core was only hidden.' }

    # Real process termination remains the only normal launcher exit signal.
    $sw = [Diagnostics.Stopwatch]::StartNew()
    Stop-Process -Id $core.Id -Force
    $core.WaitForExit(5000) | Out-Null
    if (-not $launcher.WaitForExit(3000)) { throw 'SimpleUpdate stayed alive after the core process actually exited.' }
    $sw.Stop()

    Write-Host "SIMPLEUPDATE_HIDE_RESTORE_LIFECYCLE_OK actual_exit_ms=$($sw.ElapsedMilliseconds)"
}
finally {
    if ($core -and -not $core.HasExited) { Stop-Process -Id $core.Id -Force -ErrorAction SilentlyContinue }
    if ($launcher -and -not $launcher.HasExited) { Stop-Process -Id $launcher.Id -Force -ErrorAction SilentlyContinue }
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
