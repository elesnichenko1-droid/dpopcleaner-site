[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
$sourceDir = Join-Path $root 'assets/current-settings-source'
$output = Join-Path $root 'assets/dpopcleaner-current-settings.png'
$expectedSize = [int64]74050
$expectedSha256 = 'ad8dd8dfd5d07312d9ff588f2afcae6d655e1a84cb64e17cb1666dc22dd7a572'
$expectedNames = 0..12 | ForEach-Object { 'part{0:D2}.b64' -f $_ }

if (-not (Test-Path -LiteralPath $sourceDir -PathType Container)) {
    throw "Exact screenshot source directory is missing: $sourceDir"
}

$parts = @(Get-ChildItem -LiteralPath $sourceDir -Filter 'part*.b64' -File | Sort-Object Name)
$actualNames = @($parts | ForEach-Object { $_.Name })
if (($actualNames -join '|') -ne ($expectedNames -join '|')) {
    throw "Exact screenshot source parts are incomplete or unexpected. Expected: $($expectedNames -join ', '); actual: $($actualNames -join ', ')"
}

$encoded = ($parts | ForEach-Object { (Get-Content -Raw -LiteralPath $_.FullName).Trim() }) -join ''
try {
    $bytes = [Convert]::FromBase64String($encoded)
}
catch {
    throw "Exact screenshot source is not valid base64: $($_.Exception.Message)"
}

if ($bytes.LongLength -ne $expectedSize) {
    throw "Exact screenshot size mismatch before materialization. Expected $expectedSize bytes, got $($bytes.LongLength)."
}

$outputDir = Split-Path -Parent $output
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
$tmp = "$output.tmp"

try {
    [IO.File]::WriteAllBytes($tmp, $bytes)
    $actualSize = (Get-Item -LiteralPath $tmp).Length
    if ($actualSize -ne $expectedSize) {
        throw "Materialized screenshot size mismatch. Expected $expectedSize bytes, got $actualSize."
    }

    $actualSha256 = (Get-FileHash -LiteralPath $tmp -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualSha256 -ne $expectedSha256) {
        throw "Materialized screenshot SHA-256 mismatch. Expected $expectedSha256, got $actualSha256."
    }

    Move-Item -LiteralPath $tmp -Destination $output -Force
}
finally {
    Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
}

Write-Host "Materialized exact DPopCleaner settings screenshot: $output ($expectedSize bytes, SHA-256 $expectedSha256)"
