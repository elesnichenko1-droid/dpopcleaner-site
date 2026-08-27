[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$MainExe,
    [Parameter(Mandatory = $true)][string]$UpdaterExe,
    [string]$Installer = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class DPopIconNative {
    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    public static extern uint ExtractIconExW(string szFileName, int nIconIndex, IntPtr[] phiconLarge, IntPtr[] phiconSmall, uint nIcons);
    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool DestroyIcon(IntPtr hIcon);
}
'@

function Assert-EmbeddedIcon([string]$Path, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label not found: $Path"
    }
    $full = (Resolve-Path -LiteralPath $Path).Path
    $count = [DPopIconNative]::ExtractIconExW($full, -1, $null, $null, 0)
    if ($count -lt 1) {
        throw "$Label has no embedded icon resource: $full"
    }

    $large = New-Object IntPtr[] 1
    $small = New-Object IntPtr[] 1
    $extracted = [DPopIconNative]::ExtractIconExW($full, 0, $large, $small, 1)
    if ($extracted -lt 1 -or ($large[0] -eq [IntPtr]::Zero -and $small[0] -eq [IntPtr]::Zero)) {
        throw "$Label icon resource could not be extracted: $full"
    }
    if ($large[0] -ne [IntPtr]::Zero) { [void][DPopIconNative]::DestroyIcon($large[0]) }
    if ($small[0] -ne [IntPtr]::Zero) { [void][DPopIconNative]::DestroyIcon($small[0]) }
    Write-Host "$Label embedded icon: PASS ($count icon group(s))"
}

Assert-EmbeddedIcon $MainExe 'DPopCleaner.exe'
Assert-EmbeddedIcon $UpdaterExe 'DPopUpdater.exe'
if ($Installer) {
    Assert-EmbeddedIcon $Installer 'DPopCleaner installer'
}
