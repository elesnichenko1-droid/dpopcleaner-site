[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Get-Location).Path,
    [string]$Output = (Join-Path (Get-Location).Path 'dpop033-output')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$Output = [IO.Path]::GetFullPath($Output)
$workspace = Join-Path ([IO.Path]::GetTempPath()) ('dpopcleaner-0.3.3-' + [guid]::NewGuid().ToString('N'))
$migrator = Join-Path $RepositoryRoot 'tools/dpop033_migrate.py'
$tests = Join-Path $RepositoryRoot 'tests/test_dpop033_migrate.py'

if (-not (Test-Path -LiteralPath $migrator -PathType Leaf)) {
    throw "Migration tool is missing: $migrator"
}
if (-not (Test-Path -LiteralPath $tests -PathType Leaf)) {
    throw "Migration tests are missing: $tests"
}

Push-Location $RepositoryRoot
try {
    Write-Host '=== DPopCleaner 0.3.3: tests ==='
    python $tests -v
    if ($LASTEXITCODE -ne 0) {
        throw 'Migration tests failed.'
    }

    Write-Host '=== DPopCleaner 0.3.3: isolated migration + Windows Release build ==='
    python $migrator `
        --repository $RepositoryRoot `
        --output $Output `
        --workspace $workspace `
        --build
    if ($LASTEXITCODE -ne 0) {
        throw 'Migration/build failed.'
    }

    Write-Host ''
    Write-Host "READY: $Output"
    Write-Host (Join-Path $Output 'artifacts/DPopCleaner.exe')
    Write-Host (Join-Path $Output 'migration-report.json')
}
finally {
    Pop-Location
}
