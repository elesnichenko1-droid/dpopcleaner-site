Set-StrictMode -Version Latest

$script:ExpectedSourceSha256 = '7d5e0a510189db31ef7ee1aca72dc182332a8020d994c81be40a519c5960515c'

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

Export-ModuleMember -Function Assert-CleanSource, Assert-CleanInstallerDefinition
