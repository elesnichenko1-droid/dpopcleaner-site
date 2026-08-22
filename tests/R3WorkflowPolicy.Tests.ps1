$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-Throws {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][scriptblock]$Action,
        [Parameter(Mandatory)][string]$MessagePattern
    )
    try { & $Action } catch {
        if ($_.Exception.Message -notmatch $MessagePattern) {
            throw "${Name}: expected '$MessagePattern', got '$($_.Exception.Message)'."
        }
        Write-Host "PASS: $Name"
        return
    }
    throw "${Name}: expected an exception."
}

$root = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $root 'scripts/R3ReleasePolicy.psm1') -Force
$workflow = Join-Path $root '.github/workflows/build-dpopcleaner-0.3.1-r3.yml'
$pagesWorkflow = Join-Path $root '.github/workflows/static.yml'

Assert-R3WorkflowDefinition -Path $workflow
Write-Host 'PASS: accepts the gated R3 build and release workflow'
$workflowContent = Get-Content -Raw -LiteralPath $workflow
if ($workflowContent.IndexOf('Upload verified Pages site', [StringComparison]::Ordinal) -gt
    $workflowContent.IndexOf('Commit verified site metadata', [StringComparison]::Ordinal)) {
    throw 'Verified Pages site must be uploaded before main is advanced by the metadata commit.'
}
Write-Host 'PASS: uploads the retryable Pages artifact before advancing main'
Assert-R3PagesWorkflowDefinition -Path $pagesWorkflow
Write-Host 'PASS: accepts Pages deployment only from the verified release artifact'

Assert-LegacyReleaseWorkflowsManualOnly -Paths @(
    (Join-Path $root '.github/workflows/build-clean-0.2.14-r1.yml'),
    (Join-Path $root '.github/workflows/build-dpopcleaner-release.yml')
)
Write-Host 'PASS: legacy release workflows cannot run automatically on main'

$captureScript = Get-Content -Raw -LiteralPath (Join-Path $root 'scripts/Capture-AppScreenshot.ps1')
foreach ($requiredCloseEvidence in @(
    'Register-CimIndicationEvent',
    'DPopCleaner did not exit gracefully',
    'DPopUpdater.exe',
    'DPopCleaner_Setup_0.3.1_BETA_R3.exe'
)) {
    if (-not $captureScript.Contains($requiredCloseEvidence)) {
        throw "Close verification is missing required evidence: $requiredCloseEvidence"
    }
}
Write-Host 'PASS: close verification fails closed and watches relaunch processes'

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('dpop-r3-workflow-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $tempRoot | Out-Null
    $original = Get-Content -Raw -LiteralPath $workflow

    $wrongHash = Join-Path $tempRoot 'wrong-hash.yml'
    $original.Replace('F748D61FEC75E4EDC992CB5B09D554E914197C68C690384ACEB61F143D8F76C9', ('0' * 64)) |
        Set-Content -LiteralPath $wrongHash -Encoding utf8
    Assert-Throws 'rejects a workflow with an unpinned Zapret archive' {
        Assert-R3WorkflowDefinition -Path $wrongHash
    } 'pinned Zapret archive hash'

    $generatedCpp = Join-Path $tempRoot 'generated-cpp.yml'
    ($original + "`r`n# Set-Content generated.cpp`r`n") | Set-Content -LiteralPath $generatedCpp -Encoding utf8
    Assert-Throws 'rejects workflow-side C++ generation' {
        Assert-R3WorkflowDefinition -Path $generatedCpp
    } 'must not generate C\+\+ source'

    $missingDefender = Join-Path $tempRoot 'missing-defender.yml'
    $original.Replace('Run Defender scans', 'Skip security scan') |
        Set-Content -LiteralPath $missingDefender -Encoding utf8
    Assert-Throws 'rejects a release without the Defender gate' {
        Assert-R3WorkflowDefinition -Path $missingDefender
    } 'required gate: Run Defender scans'

    $invalidPowerShellInterpolation = Join-Path $tempRoot 'invalid-powershell-interpolation.yml'
    $original.Replace('${binary}:', '$binary:') |
        Set-Content -LiteralPath $invalidPowerShellInterpolation -Encoding utf8
    Assert-Throws 'rejects an ambiguous PowerShell variable before a colon' {
        Assert-R3WorkflowDefinition -Path $invalidPowerShellInterpolation
    } 'ambiguous PowerShell variable interpolation'

    $excludedDefenderTargets = Join-Path $tempRoot 'excluded-defender-targets.yml'
    $original.Replace('Remove-MpPreference -ExclusionPath $originalExclusions', '# exclusions left active') |
        Set-Content -LiteralPath $excludedDefenderTargets -Encoding utf8
    Assert-Throws 'rejects Defender scans that leave hosted-runner exclusions active' {
        Assert-R3WorkflowDefinition -Path $excludedDefenderTargets
    } 'required release policy text: Remove-MpPreference -ExclusionPath'

    $allowsSkippedScan = Join-Path $tempRoot 'allows-skipped-defender-scan.yml'
    $original.Replace("if (`$outputText -match 'was skipped')", "if (`$false)") |
        Set-Content -LiteralPath $allowsSkippedScan -Encoding utf8
    Assert-Throws 'rejects a Defender gate that permits skipped scans' {
        Assert-R3WorkflowDefinition -Path $allowsSkippedScan
    } 'required release policy text: if \(\$outputText -match ''was skipped''\)'

    $movingReleaseCheckout = Join-Path $tempRoot 'moving-release-checkout.yml'
    $original.Replace('ref: ${{ github.sha }}', 'ref: main') |
        Set-Content -LiteralPath $movingReleaseCheckout -Encoding utf8
    Assert-Throws 'rejects release jobs that check out moving main' {
        Assert-R3WorkflowDefinition -Path $movingReleaseCheckout
    } 'required release policy text: ref: \$\{\{ github.sha \}\}'

    $untargetedRelease = Join-Path $tempRoot 'untargeted-release.yml'
    $original.Replace('--target $env:GITHUB_SHA', '') |
        Set-Content -LiteralPath $untargetedRelease -Encoding utf8
    Assert-Throws 'rejects a release tag not targeted to the verified commit' {
        Assert-R3WorkflowDefinition -Path $untargetedRelease
    } 'required release policy text: --target \$env:GITHUB_SHA'

    $pagesOriginal = Get-Content -Raw -LiteralPath $pagesWorkflow
    $manualPages = Join-Path $tempRoot 'manual-pages.yml'
    ($pagesOriginal + "`r`non:`r`n  workflow_dispatch:`r`n") |
        Set-Content -LiteralPath $manualPages -Encoding utf8
    Assert-Throws 'rejects a manual Pages deployment bypass' {
        Assert-R3PagesWorkflowDefinition -Path $manualPages
    } 'must not expose workflow_dispatch'

    $movingPages = Join-Path $tempRoot 'moving-pages.yml'
    ($pagesOriginal + "`r`n      - uses: actions/checkout@v5`r`n") |
        Set-Content -LiteralPath $movingPages -Encoding utf8
    Assert-Throws 'rejects Pages deployment from a moving checkout' {
        Assert-R3PagesWorkflowDefinition -Path $movingPages
    } 'must deploy the verified artifact without checkout'
} finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}

Write-Host 'All R3 workflow policy tests passed.'
