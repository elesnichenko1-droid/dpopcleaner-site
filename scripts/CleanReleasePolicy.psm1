Set-StrictMode -Version Latest

$script:ExpectedSourceSha256 = '7d5e0a510189db31ef7ee1aca72dc182332a8020d994c81be40a519c5960515c'
$script:ReleaseAssetName = 'DPopCleaner_Setup_0.2.14_BETA_CLEAN_R1.exe'
$script:ReleaseUrl = "https://github.com/elesnichenko1-droid/dpopcleaner-site/releases/download/v0.2.14-clean-r1/$($script:ReleaseAssetName)"

function Assert-CleanSource {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Standalone EXE does not exist: $Path"
    }

    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
    if ($actual -ne $script:ExpectedSourceSha256) {
        throw "Unexpected standalone EXE hash: $actual"
    }
}

function Assert-CleanInstallerDefinition {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Installer definition does not exist: $Path"
    }

    $inFilesSection = $false
    $payloads = @()
    foreach ($line in Get-Content -LiteralPath $Path) {
        if ($line -match '^\s*\[Files\]\s*$') {
            $inFilesSection = $true
            continue
        }
        if ($inFilesSection -and $line -match '^\s*\[[^]]+\]\s*$') {
            break
        }
        if ($inFilesSection -and $line -match '^\s*Source\s*:') {
            $payloads += $line
        }
    }

    if ($payloads.Count -ne 1) {
        throw "Clean installer must contain exactly one payload; found $($payloads.Count)."
    }

    $payload = $payloads[0]
    if ($payload -match '(?i)WinDivert|winws|Zapret|\.bat(?:[";\s]|$)|\.cmd(?:[";\s]|$)|\.ps1(?:[";\s]|$)') {
        throw "Forbidden installer payload: $payload"
    }
    if ($payload -notmatch 'Source\s*:\s*"\{#SourceExe\}"') {
        throw 'Clean installer payload must use {#SourceExe}.'
    }
    if ($payload -notmatch 'DestName\s*:\s*"DPopCleaner\.exe"') {
        throw 'Clean installer payload destination must be DPopCleaner.exe.'
    }
}

function New-CleanReleaseManifest {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$InstallerPath,
        [Parameter(Mandatory)][bool]$Signed
    )

    if (-not (Test-Path -LiteralPath $InstallerPath -PathType Leaf)) {
        throw "Installer does not exist: $InstallerPath"
    }
    if ([IO.Path]::GetFileName($InstallerPath) -ne $script:ReleaseAssetName) {
        throw "Unexpected installer filename: $([IO.Path]::GetFileName($InstallerPath))"
    }

    $item = Get-Item -LiteralPath $InstallerPath
    if ($item.Length -le 0) {
        throw 'Installer must not be empty.'
    }

    [pscustomobject][ordered]@{
        product = 'DPopCleaner'
        channel = 'beta'
        version = '0.2.14'
        version_code = 214
        revision = 1
        mandatory = $false
        download_url = $script:ReleaseUrl
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $InstallerPath).Hash.ToLowerInvariant()
        size = [int64]$item.Length
        signed = $Signed
        available = $true
        notes_url = 'https://elesnichenko1-droid.github.io/dpopcleaner-site/'
        install_args = '/SILENT /NORESTART'
    }
}

function Assert-CleanReleaseManifest {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][object]$Manifest,
        [switch]$Published
    )

    function Read-Field([string]$Name) {
        $property = $Manifest.PSObject.Properties[$Name]
        if ($null -eq $property) { return $null }
        return $property.Value
    }

    $valid =
        (Read-Field 'product') -eq 'DPopCleaner' -and
        (Read-Field 'channel') -eq 'beta' -and
        (Read-Field 'version') -eq '0.2.14' -and
        [int](Read-Field 'version_code') -eq 214 -and
        [int](Read-Field 'revision') -eq 1

    if ($Published) {
        $valid = $valid -and
            (Read-Field 'available') -eq $true -and
            (Read-Field 'download_url') -eq $script:ReleaseUrl -and
            (Read-Field 'sha256') -is [string] -and
            (Read-Field 'sha256') -match '^[a-f0-9]{64}$' -and
            [int64](Read-Field 'size') -gt 0 -and
            (Read-Field 'signed') -is [bool]
    } else {
        $valid = $valid -and
            (Read-Field 'available') -eq $false -and
            [string]::IsNullOrEmpty([string](Read-Field 'download_url')) -and
            [string]::IsNullOrEmpty([string](Read-Field 'sha256')) -and
            [int64](Read-Field 'size') -eq 0
    }

    if (-not $valid) {
        throw 'Invalid clean release manifest.'
    }
}

Export-ModuleMember -Function Assert-CleanSource, Assert-CleanInstallerDefinition, New-CleanReleaseManifest, Assert-CleanReleaseManifest
