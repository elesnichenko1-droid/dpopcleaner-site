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

    $installerFixture = Join-Path $tempRoot 'DPopCleaner_Setup_0.2.14_BETA_CLEAN_R1.exe'
    [IO.File]::WriteAllBytes($installerFixture, [Text.Encoding]::UTF8.GetBytes('clean-installer-fixture'))
    $manifest = New-CleanReleaseManifest -InstallerPath $installerFixture -Signed:$false

    if ($manifest.version -ne '0.2.14' -or $manifest.version_code -ne 214 -or $manifest.revision -ne 1) {
        throw 'creates exact 0.2.14 R1 version metadata: wrong version fields.'
    }
    if ($manifest.download_url -ne 'https://github.com/elesnichenko1-droid/dpopcleaner-site/releases/download/v0.2.14-clean-r1/DPopCleaner_Setup_0.2.14_BETA_CLEAN_R1.exe') {
        throw 'creates exact 0.2.14 R1 version metadata: wrong release URL.'
    }
    if ($manifest.sha256 -ne '5da46342f1223410621a498a3f0e7da461f78708d1c61c65375635f8f7bd9fcb' -or $manifest.size -ne 23) {
        throw 'creates exact 0.2.14 R1 version metadata: wrong artifact metadata.'
    }
    if ($manifest.available -ne $true -or $manifest.signed -ne $false) {
        throw 'creates exact 0.2.14 R1 version metadata: wrong publication flags.'
    }
    Write-Host 'PASS: creates exact 0.2.14 R1 published manifest'

    Assert-DoesNotThrow 'accepts the exact published manifest contract' {
        Assert-CleanReleaseManifest -Manifest $manifest -Published
    }

    $mutations = @(
        @{ Name = 'wrong manifest version'; Field = 'version'; Value = '0.3.1' },
        @{ Name = 'wrong manifest revision'; Field = 'revision'; Value = 2 },
        @{ Name = 'wrong manifest URL'; Field = 'download_url'; Value = 'https://example.com/setup.exe' },
        @{ Name = 'empty manifest hash'; Field = 'sha256'; Value = '' },
        @{ Name = 'zero manifest size'; Field = 'size'; Value = 0 },
        @{ Name = 'unavailable published manifest'; Field = 'available'; Value = $false }
    )
    foreach ($mutation in $mutations) {
        $copy = [ordered]@{}
        foreach ($property in $manifest.PSObject.Properties) {
            $copy[$property.Name] = $property.Value
        }
        $copy[$mutation.Field] = $mutation.Value
        $badManifest = [pscustomobject]$copy
        Assert-Throws "rejects $($mutation.Name)" {
            Assert-CleanReleaseManifest -Manifest $badManifest -Published
        } 'Invalid clean release manifest'
    }

    $siteFixture = Join-Path $tempRoot 'site-fixture'
    $siteOutput = Join-Path $siteFixture '_site'
    New-Item -ItemType Directory -Path (Join-Path $siteFixture 'update') | Out-Null
    $allowedSiteFiles = @(
        '.nojekyll',
        'dpopcleaner-icon.png',
        'index.html',
        'release-manifest.js',
        'script.js',
        'styles.css',
        'version.json'
    )
    foreach ($file in $allowedSiteFiles) {
        Set-Content -LiteralPath (Join-Path $siteFixture $file) -Value $file -Encoding utf8
    }
    Set-Content -LiteralPath (Join-Path $siteFixture 'update/beta.json') -Value '{}' -Encoding utf8
    Set-Content -LiteralPath (Join-Path $siteFixture 'update/stable.json') -Value '{}' -Encoding utf8
    Set-Content -LiteralPath (Join-Path $siteFixture 'secret.exe') -Value 'must not deploy' -Encoding utf8
    Set-Content -LiteralPath (Join-Path $siteFixture 'README.md') -Value 'must not deploy' -Encoding utf8

    $stageScript = Join-Path $root 'scripts/Stage-Site.ps1'
    Assert-Throws 'rejects a staging destination outside the site root' {
        & $stageScript -Root $siteFixture -Destination (Join-Path $tempRoot 'outside-site')
    } 'Unsafe website staging destination'
    & $stageScript -Root $siteFixture -Destination $siteOutput
    $actualFiles = Get-ChildItem -LiteralPath $siteOutput -File -Recurse |
        ForEach-Object { [IO.Path]::GetRelativePath($siteOutput, $_.FullName).Replace('\', '/') } |
        Sort-Object
    $expectedFiles = @(
        '.nojekyll',
        'dpopcleaner-icon.png',
        'index.html',
        'release-manifest.js',
        'script.js',
        'styles.css',
        'update/beta.json',
        'update/stable.json',
        'version.json'
    ) | Sort-Object
    if (Compare-Object -ReferenceObject $expectedFiles -DifferenceObject $actualFiles) {
        throw "stages only public website files: got $($actualFiles -join ', ')."
    }
    Write-Host 'PASS: stages only public website files'
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force
}

Write-Host 'All clean release policy tests passed.'
