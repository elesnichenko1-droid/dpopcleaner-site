$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-Throws {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][scriptblock]$Action,
        [Parameter(Mandatory)][string]$MessagePattern
    )

    try {
        & $Action
    } catch {
        if ($_.Exception.Message -notmatch $MessagePattern) {
            throw "${Name}: expected error matching '$MessagePattern', got '$($_.Exception.Message)'."
        }
        Write-Host "PASS: $Name"
        return
    }

    throw "${Name}: expected an exception."
}

function Assert-DoesNotThrow {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][scriptblock]$Action
    )

    try {
        & $Action
    } catch {
        throw "${Name}: unexpected error '$($_.Exception.Message)'."
    }
    Write-Host "PASS: $Name"
}

$root = Split-Path -Parent $PSScriptRoot
$modulePath = Join-Path $root 'scripts/CleanReleasePolicy.psm1'
Import-Module $modulePath -Force

$sourcePath = Join-Path $root 'downloads/DPopCleaner_0.2.14_BETA.exe'
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ("dpop-release-policy-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempRoot | Out-Null

try {
    Assert-DoesNotThrow 'accepts the known standalone executable' {
        Assert-CleanSource -Path $sourcePath
    }

    $corruptPath = Join-Path $tempRoot 'corrupt.exe'
    Copy-Item -LiteralPath $sourcePath -Destination $corruptPath
    $bytes = [IO.File]::ReadAllBytes($corruptPath)
    $bytes[$bytes.Length - 1] = $bytes[$bytes.Length - 1] -bxor 0xff
    [IO.File]::WriteAllBytes($corruptPath, $bytes)

    Assert-Throws 'rejects a modified standalone executable' {
        Assert-CleanSource -Path $corruptPath
    } 'Unexpected standalone EXE hash'

    $goodIss = Join-Path $tempRoot 'good.iss'
    @'
[Files]
Source: "{#SourceExe}"; DestDir: "{app}"; DestName: "DPopCleaner.exe"; Flags: ignoreversion
'@ | Set-Content -LiteralPath $goodIss -Encoding utf8

    Assert-DoesNotThrow 'accepts one DPopCleaner executable payload' {
        Assert-CleanInstallerDefinition -Path $goodIss
    }

    $twoPayloadsIss = Join-Path $tempRoot 'two-payloads.iss'
    @'
[Files]
Source: "{#SourceExe}"; DestDir: "{app}"; DestName: "DPopCleaner.exe"; Flags: ignoreversion
Source: "helper.dll"; DestDir: "{app}"
'@ | Set-Content -LiteralPath $twoPayloadsIss -Encoding utf8

    Assert-Throws 'rejects a second installer payload' {
        Assert-CleanInstallerDefinition -Path $twoPayloadsIss
    } 'exactly one payload'

    $scriptPayloadIss = Join-Path $tempRoot 'script-payload.iss'
    @'
[Files]
Source: "setup.ps1"; DestDir: "{app}"; DestName: "DPopCleaner.exe"
'@ | Set-Content -LiteralPath $scriptPayloadIss -Encoding utf8

    Assert-Throws 'rejects a script payload' {
        Assert-CleanInstallerDefinition -Path $scriptPayloadIss
    } 'Forbidden installer payload'
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force
}

Write-Host 'All clean release policy tests passed.'
