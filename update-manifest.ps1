param(
  [Parameter(Mandatory=$true)][string]$Installer,
  [Parameter(Mandatory=$true)][string]$Repository,
  [string]$Tag = "v0.2.15-beta"
)
$hash = (Get-FileHash -Algorithm SHA256 $Installer).Hash.ToLowerInvariant()
$size = (Get-Item $Installer).Length
$name = [IO.Path]::GetFileName($Installer)
$url = "https://github.com/$Repository/releases/download/$Tag/$name"
$manifest = [ordered]@{
  product = "DPopCleaner"
  channel = "beta"
  version = "0.2.15"
  version_code = 215
  mandatory = $false
  download_url = $url
  sha256 = $hash
  size = $size
  signed = $false
  notes_url = "https://elesnichenko1-droid.github.io/dpopcleaner-site/"
  install_args = "/SILENT /NORESTART"
}
$manifest | ConvertTo-Json | Set-Content -Encoding utf8 "update/beta.json"
Write-Host "SHA256=$hash"
Write-Host "SIZE=$size"
