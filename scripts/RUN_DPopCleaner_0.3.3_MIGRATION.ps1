[CmdletBinding()]
param([string]$RepositoryRoot=(Get-Location).Path,[string]$Output=(Join-Path (Get-Location).Path 'dpop033-output'))
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
$RepositoryRoot=[IO.Path]::GetFullPath($RepositoryRoot);$Output=[IO.Path]::GetFullPath($Output);$workspace=Join-Path ([IO.Path]::GetTempPath()) ('dpopcleaner-0.3.3-'+[guid]::NewGuid().ToString('N'))
Push-Location $RepositoryRoot
try { python tests/test_dpop033_migrate.py -v; if($LASTEXITCODE-ne 0){throw 'Migration tests failed.'}; python tests/test_dpop033_recovery_controls.py -v; if($LASTEXITCODE-ne 0){throw 'RecoveryControls regression test failed.'}; python tools/dpop033_migrate.py --repository $RepositoryRoot --output $Output --workspace $workspace --build; if($LASTEXITCODE-ne 0){throw 'Migration/build failed.'}; Write-Host "READY: $Output" } finally { Pop-Location }
