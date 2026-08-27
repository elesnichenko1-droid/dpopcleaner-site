[CmdletBinding()]
param(
    [string]$OutputRoot = '_release/0.4.18/third-party/Zapret',
    [string]$ArchivePath = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$PinnedVersion = '1.10.2'
$PinnedArchiveName = 'zapret-discord-youtube-1.10.2.zip'
$PinnedUrl = 'https://github.com/Flowseal/zapret-discord-youtube/releases/download/1.10.2/zapret-discord-youtube-1.10.2.zip'
$PinnedSize = [int64]1508077
$PinnedSha256 = '5eaac9fb2e4b1abd693487452a3ff3f4dfe9578a45f9ddddfa4bc1f5a6bb62d5'
$DiscordToken = '--hostlist-domains=discord.media'
$FilterTcpRegex = [regex]::new('--filter-tcp=(?<ports>\d+(?:,\d+)*)', [Text.RegularExpressions.RegexOptions]::IgnoreCase -bor [Text.RegularExpressions.RegexOptions]::CultureInvariant)

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$output = if ([IO.Path]::IsPathRooted($OutputRoot)) { $OutputRoot } else { Join-Path $root $OutputRoot }
$work = Join-Path $root '_release/0.4.18/third-party/work'
$downloads = Join-Path $root '_release/0.4.18/third-party/downloads'
$archive = if ($ArchivePath) {
    if ([IO.Path]::IsPathRooted($ArchivePath)) { $ArchivePath } else { Join-Path $root $ArchivePath }
} else {
    Join-Path $downloads $PinnedArchiveName
}

function New-Directory([string]$Path) {
    New-Item -ItemType Directory -Path $Path -Force | Out-Null
}

function Test-BytePrefix([byte[]]$Bytes, [byte[]]$Prefix) {
    if ($null -eq $Bytes -or $null -eq $Prefix -or $Bytes.Length -lt $Prefix.Length) { return $false }
    for ($i = 0; $i -lt $Prefix.Length; $i++) {
        if ($Bytes[$i] -ne $Prefix[$i]) { return $false }
    }
    return $true
}

function Read-TextPreservingEncoding([string]$Path) {
    [byte[]]$bytes = [IO.File]::ReadAllBytes($Path)
    $encoding = $null
    [byte[]]$preamble = @()
    $offset = 0

    if (Test-BytePrefix $bytes ([byte[]](0xEF, 0xBB, 0xBF))) {
        $encoding = [Text.UTF8Encoding]::new($false, $true)
        $preamble = [byte[]](0xEF, 0xBB, 0xBF)
        $offset = 3
    } elseif (Test-BytePrefix $bytes ([byte[]](0xFF, 0xFE))) {
        $encoding = [Text.UnicodeEncoding]::new($false, $false, $true)
        $preamble = [byte[]](0xFF, 0xFE)
        $offset = 2
    } elseif (Test-BytePrefix $bytes ([byte[]](0xFE, 0xFF))) {
        $encoding = [Text.UnicodeEncoding]::new($true, $false, $true)
        $preamble = [byte[]](0xFE, 0xFF)
        $offset = 2
    } else {
        try {
            $encoding = [Text.UTF8Encoding]::new($false, $true)
            $null = $encoding.GetString($bytes)
        } catch [Text.DecoderFallbackException] {
            $encoding = [Text.Encoding]::Default
        }
    }

    $text = if ($bytes.Length -eq $offset) { '' } else { $encoding.GetString($bytes, $offset, $bytes.Length - $offset) }
    return [pscustomobject]@{ Text = $text; Encoding = $encoding; Preamble = $preamble }
}

function Write-TextPreservingEncoding([string]$Path, [string]$Text, $Encoding, [byte[]]$Preamble) {
    [byte[]]$body = $Encoding.GetBytes($Text)
    [byte[]]$result = New-Object byte[] ($Preamble.Length + $body.Length)
    if ($Preamble.Length -gt 0) { [Buffer]::BlockCopy($Preamble, 0, $result, 0, $Preamble.Length) }
    if ($body.Length -gt 0) { [Buffer]::BlockCopy($body, 0, $result, $Preamble.Length, $body.Length) }
    [IO.File]::WriteAllBytes($Path, $result)
}

function Get-DiscordFilterMatch([string]$Line) {
    $domainIndex = $Line.IndexOf($DiscordToken, [StringComparison]::OrdinalIgnoreCase)
    if ($domainIndex -lt 0) { return $null }

    $sectionStart = $Line.LastIndexOf('--new', $domainIndex, [StringComparison]::OrdinalIgnoreCase)
    if ($sectionStart -lt 0) { $sectionStart = 0 }
    $searchAfter = $domainIndex + $DiscordToken.Length
    $sectionEnd = $Line.IndexOf('--new', $searchAfter, [StringComparison]::OrdinalIgnoreCase)
    if ($sectionEnd -lt 0) { $sectionEnd = $Line.Length }

    $selected = $null
    foreach ($match in $FilterTcpRegex.Matches($Line)) {
        if (-not $match.Success -or $match.Index -lt $sectionStart -or $match.Index -ge $sectionEnd) { continue }
        if ($match.Index -le $domainIndex) {
            $selected = $match
        } elseif ($null -eq $selected) {
            $selected = $match
            break
        }
    }
    return $selected
}

function Patch-DiscordMediaTcp443([string]$Path) {
    $decoded = Read-TextPreservingEncoding $Path
    [string[]]$parts = [regex]::Split($decoded.Text, '(\r\n|\n|\r)')
    $changed = $false
    $domainLines = 0

    for ($i = 0; $i -lt $parts.Length; $i += 2) {
        $line = $parts[$i]
        if ($line.IndexOf($DiscordToken, [StringComparison]::OrdinalIgnoreCase) -lt 0) { continue }
        $domainLines++
        $match = Get-DiscordFilterMatch $line
        if ($null -eq $match) {
            throw "discord.media rule has no --filter-tcp in its strategy section: $Path"
        }
        $portsGroup = $match.Groups['ports']
        $ports = @($portsGroup.Value.Split(',') | ForEach-Object { $_.Trim() })
        if ($ports -contains '443') { continue }
        $parts[$i] = $line.Substring(0, $portsGroup.Index) + '443,' + $portsGroup.Value + $line.Substring($portsGroup.Index + $portsGroup.Length)
        $changed = $true
    }

    if ($changed) {
        Write-TextPreservingEncoding $Path ([string]::Concat($parts)) $decoded.Encoding $decoded.Preamble
    }
    return [pscustomobject]@{ Changed = $changed; DomainLines = $domainLines }
}

function Assert-DiscordMediaTcp443([string]$Path) {
    $decoded = Read-TextPreservingEncoding $Path
    [string[]]$parts = [regex]::Split($decoded.Text, '(\r\n|\n|\r)')
    for ($i = 0; $i -lt $parts.Length; $i += 2) {
        $line = $parts[$i]
        if ($line.IndexOf($DiscordToken, [StringComparison]::OrdinalIgnoreCase) -lt 0) { continue }
        $match = Get-DiscordFilterMatch $line
        if ($null -eq $match) { throw "discord.media rule has no --filter-tcp after patch: $Path" }
        $ports = @($match.Groups['ports'].Value.Split(',') | ForEach-Object { $_.Trim() })
        if ($ports -notcontains '443') { throw "discord.media TCP filter still lacks port 443 after patch: $Path" }
    }
}

if (-not $ArchivePath) {
    New-Directory $downloads
    if (Test-Path -LiteralPath $archive -PathType Leaf) { Remove-Item -LiteralPath $archive -Force }
    Write-Host "Downloading pinned Flowseal Zapret $PinnedVersion from $PinnedUrl"
    Invoke-WebRequest -Uri $PinnedUrl -OutFile $archive -UseBasicParsing -MaximumRedirection 10
}

if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) {
    throw "Pinned Zapret archive not found: $archive"
}

$actualSize = (Get-Item -LiteralPath $archive).Length
if ($actualSize -ne $PinnedSize) {
    throw "Pinned Zapret ZIP size mismatch. Expected $PinnedSize, got $actualSize."
}

$actualSha = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualSha -ne $PinnedSha256) {
    throw "Pinned Zapret ZIP SHA-256 mismatch. Expected $PinnedSha256, got $actualSha."
}

# Extraction is intentionally after exact byte-length and SHA-256 verification.
if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
New-Directory $work
$extractRoot = Join-Path $work 'extract'
New-Directory $extractRoot
Expand-Archive -LiteralPath $archive -DestinationPath $extractRoot -Force

$payloadRoot = $extractRoot
if (-not (Test-Path -LiteralPath (Join-Path $payloadRoot 'service.bat') -PathType Leaf)) {
    $topDirectories = @(Get-ChildItem -LiteralPath $extractRoot -Directory -Force)
    if ($topDirectories.Count -eq 1 -and (Test-Path -LiteralPath (Join-Path $topDirectories[0].FullName 'service.bat') -PathType Leaf)) {
        $payloadRoot = $topDirectories[0].FullName
    } else {
        $candidates = @(Get-ChildItem -LiteralPath $extractRoot -Filter 'service.bat' -File -Recurse -Force | Where-Object {
            Test-Path -LiteralPath (Join-Path $_.Directory.FullName 'general.bat') -PathType Leaf
        })
        if ($candidates.Count -ne 1) { throw 'Could not resolve one exact Flowseal payload root after extraction.' }
        $payloadRoot = $candidates[0].Directory.FullName
    }
}

$requiredFiles = @(
    'LICENSE.txt',
    'service.bat',
    'general.bat',
    '.service/version.txt',
    'bin/winws.exe',
    'bin/WinDivert.dll',
    'bin/WinDivert64.sys'
)
foreach ($relative in $requiredFiles) {
    $candidate = Join-Path $payloadRoot ($relative -replace '/', [IO.Path]::DirectorySeparatorChar)
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) { throw "Verified Zapret payload missing required file: $relative" }
}
if (-not (Test-Path -LiteralPath (Join-Path $payloadRoot 'lists') -PathType Container)) {
    throw 'Verified Zapret payload missing required directory: lists'
}

$upstreamVersion = (Get-Content -LiteralPath (Join-Path $payloadRoot '.service/version.txt') -Raw).Trim()
if ($upstreamVersion -ne $PinnedVersion) {
    throw "Extracted Zapret version mismatch. Expected $PinnedVersion, got $upstreamVersion."
}

$strategies = @(Get-ChildItem -LiteralPath $payloadRoot -Filter '*.bat' -File | Where-Object { $_.Name -notlike 'service*' } | Sort-Object Name)
if ($strategies.Count -eq 0) { throw 'Verified Zapret payload contains no top-level strategies.' }
$patchedFiles = 0
foreach ($strategy in $strategies) {
    $patch = Patch-DiscordMediaTcp443 $strategy.FullName
    if ($patch.Changed) { $patchedFiles++ }
    Assert-DiscordMediaTcp443 $strategy.FullName
}

# Idempotency: a second patch pass must not change any strategy.
foreach ($strategy in $strategies) {
    $secondPass = Patch-DiscordMediaTcp443 $strategy.FullName
    if ($secondPass.Changed) { throw "Discord screen-share patch is not idempotent: $($strategy.Name)" }
}

if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output -Recurse -Force }
New-Directory (Split-Path -Parent $output)
Copy-Item -LiteralPath $payloadRoot -Destination $output -Recurse -Force

foreach ($relative in $requiredFiles) {
    $candidate = Join-Path $output ($relative -replace '/', [IO.Path]::DirectorySeparatorChar)
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) { throw "Prepared Zapret tree missing required file: $relative" }
}
if (-not (Test-Path -LiteralPath (Join-Path $output 'lists') -PathType Container)) {
    throw 'Prepared Zapret tree missing lists directory.'
}

Write-Host "Pinned Zapret archive verified: $actualSize bytes, SHA-256 $actualSha"
Write-Host "Flowseal Zapret version verified: $upstreamVersion"
Write-Host "Discord media strategies patched: $patchedFiles"
Write-Host "Prepared bundled Zapret tree: $output"
