[CmdletBinding()]
param(
  [string]$Channel = "",
  [string]$Build = "",
  [string]$Platform = "",
  [string]$ApiBaseUrl = "",
  [string]$InstallDir = "",
  [switch]$Force,
  [switch]$NoLaunch,
  [switch]$NoShortcut,
  [switch]$DryRun
)

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

function Default-String([string]$Value, [string]$Fallback) {
  if ([string]::IsNullOrWhiteSpace($Value)) { return $Fallback }
  return $Value.Trim()
}

function Write-Step([string]$Message) {
  Write-Host "[SekaiLink bootstrapper] $Message"
}

function Ensure-Directory([string]$PathValue) {
  if (!(Test-Path -LiteralPath $PathValue)) {
    New-Item -ItemType Directory -Path $PathValue -Force | Out-Null
  }
}

function Read-JsonFile([string]$PathValue) {
  if (!(Test-Path -LiteralPath $PathValue)) { return $null }
  try {
    return Get-Content -LiteralPath $PathValue -Raw | ConvertFrom-Json
  } catch {
    return $null
  }
}

function Get-JsonString($Object, [string]$Name) {
  if ($null -eq $Object) { return "" }
  $prop = $Object.PSObject.Properties[$Name]
  if ($null -eq $prop -or $null -eq $prop.Value) { return "" }
  return ([string]$prop.Value).Trim()
}

function Build-ReleaseUrl([string]$BaseUrl, [string]$ReleaseChannel, [string]$ReleaseBuild, [string]$ReleasePlatform) {
  $base = $BaseUrl.TrimEnd("/")
  $query = "channel=$([uri]::EscapeDataString($ReleaseChannel))&platform=$([uri]::EscapeDataString($ReleasePlatform))"
  if (![string]::IsNullOrWhiteSpace($ReleaseBuild)) {
    $query = "$query&build=$([uri]::EscapeDataString($ReleaseBuild))"
  }
  return "$base/api/client/release-latest?$query"
}

function Get-ReleaseInfo([string]$ReleaseUrl) {
  Write-Step "Checking $ReleaseUrl"
  return Invoke-RestMethod -Uri $ReleaseUrl -Headers @{
    "User-Agent" = "SekaiLink-Bootstrapper/0.1 Windows"
    "Cache-Control" = "no-cache"
  } -TimeoutSec 20
}

function Resolve-BundleRoot([string]$ExtractDir) {
  if (Test-Path -LiteralPath (Join-Path $ExtractDir "resources/app.asar")) { return $ExtractDir }
  $entries = @(Get-ChildItem -LiteralPath $ExtractDir -Force)
  if ($entries.Count -eq 1 -and $entries[0].PSIsContainer) {
    return $entries[0].FullName
  }
  return $ExtractDir
}

function Find-ClientExecutable([string]$BundleRoot) {
  $candidates = @("SekaiLink Client.exe", "SekaiLink-client.exe", "sekailink-client.exe")
  foreach ($name in $candidates) {
    $full = Join-Path $BundleRoot $name
    if (Test-Path -LiteralPath $full) { return $name }
  }
  $match = Get-ChildItem -LiteralPath $BundleRoot -Force -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name.ToLowerInvariant().EndsWith(".exe") -and $_.Name.ToLowerInvariant().Contains("sekailink") } |
    Select-Object -First 1
  if ($null -ne $match) { return $match.Name }
  return ""
}

function Remove-InstallContents([string]$TargetDir) {
  Ensure-Directory $TargetDir
  $keep = @(".sekailink", "SekaiLink-bootstrapper.cmd", "SekaiLink-bootstrapper.ps1", "sekailink-bootstrapper.sh")
  Get-ChildItem -LiteralPath $TargetDir -Force | Where-Object { $keep -notcontains $_.Name } |
    Remove-Item -Recurse -Force
}

function Install-SelfBootstrapper([string]$TargetDir) {
  $sourceDir = Split-Path -LiteralPath $PSCommandPath -Parent
  foreach ($name in @("SekaiLink-bootstrapper.cmd", "SekaiLink-bootstrapper.ps1")) {
    $src = Join-Path $sourceDir $name
    $dst = Join-Path $TargetDir $name
    if (Test-Path -LiteralPath $src) {
      if ([IO.Path]::GetFullPath($src) -ne [IO.Path]::GetFullPath($dst)) {
        Copy-Item -LiteralPath $src -Destination $dst -Force
      }
    }
  }
}

function Write-InstallState([string]$StateDir, [string]$TargetDir, $Release, [string]$ReleaseChannel, [string]$ReleaseBuild) {
  Ensure-Directory $StateDir
  $version = Get-JsonString $Release "version"
  $state = [ordered]@{
    version = $version
    manifestVersion = $version
    channel = $ReleaseChannel
    build = $ReleaseBuild
    platform = "win32"
    arch = "x64"
    artifactType = "client-bundle"
    installDir = $TargetDir
    updatedAt = (Get-Date).ToUniversalTime().ToString("o")
  }
  $state | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $StateDir "install-state.json") -Encoding UTF8
}

function Ensure-LauncherSecret([string]$StateDir) {
  Ensure-Directory $StateDir
  $pathValue = Join-Path $StateDir "launcher-secret.key"
  if (Test-Path -LiteralPath $pathValue) {
    $existing = (Get-Content -LiteralPath $pathValue -Raw).Trim()
    if ($existing.Length -ge 32) { return $existing }
  }
  $bytes = New-Object byte[] 32
  $rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
  try {
    $rng.GetBytes($bytes)
  } finally {
    $rng.Dispose()
  }
  $secret = [Convert]::ToBase64String($bytes)
  Set-Content -LiteralPath $pathValue -Value $secret -Encoding ASCII
  return $secret
}

function Convert-ToBase64Url([byte[]]$Bytes) {
  return [Convert]::ToBase64String($Bytes).TrimEnd("=").Replace("+", "-").Replace("/", "_")
}

function New-LaunchToken([string]$Secret) {
  $now = [int64]([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds())
  $payload = @{
    iss = "sekailink-bootstrapper"
    aud = "sekailink-client"
    iat = $now
    exp = $now + (5 * 60 * 1000)
  } | ConvertTo-Json -Compress
  $body = Convert-ToBase64Url ([Text.Encoding]::UTF8.GetBytes($payload))
  $hmac = New-Object System.Security.Cryptography.HMACSHA256
  try {
    $hmac.Key = [Text.Encoding]::UTF8.GetBytes($Secret)
    $sigBytes = $hmac.ComputeHash([Text.Encoding]::UTF8.GetBytes($body))
  } finally {
    $hmac.Dispose()
  }
  $sig = -join ($sigBytes | ForEach-Object { $_.ToString("x2") })
  return "$body.$sig"
}

function New-WindowsShortcuts([string]$TargetDir, [string]$ExePath) {
  $cmdPath = Join-Path $TargetDir "SekaiLink-bootstrapper.cmd"
  if (!(Test-Path -LiteralPath $cmdPath)) { return }
  try {
    $wsh = New-Object -ComObject WScript.Shell
    $desktop = [Environment]::GetFolderPath("DesktopDirectory")
    $programs = [Environment]::GetFolderPath("Programs")
    foreach ($shortcutPath in @(
      (Join-Path $desktop "SekaiLink.lnk"),
      (Join-Path $programs "SekaiLink.lnk")
    )) {
      $shortcut = $wsh.CreateShortcut($shortcutPath)
      $shortcut.TargetPath = $cmdPath
      $shortcut.WorkingDirectory = $TargetDir
      if (Test-Path -LiteralPath $ExePath) { $shortcut.IconLocation = "$ExePath,0" }
      $shortcut.Save()
    }
  } catch {
    Write-Step "Shortcut creation skipped: $($_.Exception.Message)"
  }
}

function Launch-Client([string]$TargetDir, [string]$StateDir, [string]$ExecutableRel) {
  $exePath = Join-Path $TargetDir $ExecutableRel
  if (!(Test-Path -LiteralPath $exePath)) {
    throw "client_executable_missing:$exePath"
  }
  $secret = Ensure-LauncherSecret $StateDir
  $env:SEKAILINK_BOOTSTRAP_INSTALL_DIR = $TargetDir
  $env:SEKAILINK_BOOTSTRAP_STATE_DIR = $StateDir
  $env:SEKAILINK_BOOTSTRAP_TOKEN = New-LaunchToken $secret
  Write-Step "Launching $exePath"
  Start-Process -FilePath $exePath -WorkingDirectory $TargetDir | Out-Null
}

$Channel = Default-String $Channel (Default-String $env:SEKAILINK_CHANNEL "test")
$Build = Default-String $Build (Default-String $env:SEKAILINK_BUILD "release")
$Platform = Default-String $Platform (Default-String $env:SEKAILINK_PLATFORM "win32-x64")
$ApiBaseUrl = Default-String $ApiBaseUrl (Default-String $env:SEKAILINK_API_BASE_URL "https://sekailink.com")
$InstallDir = Default-String $InstallDir (Default-String $env:SEKAILINK_INSTALL_DIR (Join-Path $env:LOCALAPPDATA "Programs/sekailink-client"))
$InstallDir = [IO.Path]::GetFullPath($InstallDir)
$StateDir = Join-Path $InstallDir ".sekailink"
$StatePath = Join-Path $StateDir "install-state.json"
$releaseUrl = Build-ReleaseUrl $ApiBaseUrl $Channel $Build $Platform

$release = $null
try {
  $release = Get-ReleaseInfo $releaseUrl
} catch {
  Write-Step "Update check failed: $($_.Exception.Message)"
  $currentExeRel = Find-ClientExecutable $InstallDir
  if (!$NoLaunch -and ![string]::IsNullOrWhiteSpace($currentExeRel)) {
    Launch-Client $InstallDir $StateDir $currentExeRel
    exit 0
  }
  throw
}

$targetVersion = Get-JsonString $release "version"
$downloadUrl = Get-JsonString $release "download_url"
$sha256 = (Get-JsonString $release "sha256").ToLowerInvariant()
$artifactType = Get-JsonString $release "artifact_type"
if ([string]::IsNullOrWhiteSpace($artifactType)) { $artifactType = "client-bundle" }
if ($release.available -eq $false) { throw "release_not_available" }
if ([string]::IsNullOrWhiteSpace($targetVersion) -or [string]::IsNullOrWhiteSpace($downloadUrl) -or [string]::IsNullOrWhiteSpace($sha256)) {
  throw "release_manifest_incomplete"
}
if ($artifactType -ne "client-bundle") {
  throw "unsupported_artifact_type:$artifactType"
}

$state = Read-JsonFile $StatePath
$installedVersion = Get-JsonString $state "version"
$needsUpdate = $Force -or [string]::IsNullOrWhiteSpace($installedVersion) -or ($installedVersion -ne $targetVersion)
$exeRel = "SekaiLink Client.exe"

if ($DryRun) {
  Write-Step "dry-run installed=$installedVersion latest=$targetVersion needsUpdate=$needsUpdate url=$downloadUrl"
  exit 0
}

if ($needsUpdate) {
  Write-Step "Installing version $targetVersion into $InstallDir"
  $workRoot = Join-Path ([IO.Path]::GetTempPath()) ("sekailink-bootstrapper-" + [Guid]::NewGuid().ToString("N"))
  $downloadDir = Join-Path $workRoot "downloads"
  $extractDir = Join-Path $workRoot "extract"
  Ensure-Directory $downloadDir
  Ensure-Directory $extractDir
  try {
    $fileName = [IO.Path]::GetFileName(([uri]$downloadUrl).AbsolutePath)
    if ([string]::IsNullOrWhiteSpace($fileName)) { $fileName = "sekailink-client-bundle.zip" }
    $zipPath = Join-Path $downloadDir $fileName
    Write-Step "Downloading $downloadUrl"
    Invoke-WebRequest -Uri $downloadUrl -OutFile $zipPath -UseBasicParsing -Headers @{
      "User-Agent" = "SekaiLink-Bootstrapper/0.1 Windows"
      "Accept" = "application/zip,application/octet-stream,*/*"
    }
    $actualSha = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualSha -ne $sha256) {
      throw "checksum_mismatch expected=$sha256 actual=$actualSha"
    }
    Write-Step "Extracting bundle"
    Expand-Archive -LiteralPath $zipPath -DestinationPath $extractDir -Force
    $bundleRoot = Resolve-BundleRoot $extractDir
    if (!(Test-Path -LiteralPath (Join-Path $bundleRoot "resources/app.asar"))) {
      throw "bundle_app_missing"
    }
    $exeRel = Find-ClientExecutable $bundleRoot
    if ([string]::IsNullOrWhiteSpace($exeRel)) { throw "bundle_executable_missing" }
    Remove-InstallContents $InstallDir
    Copy-Item -Path (Join-Path $bundleRoot "*") -Destination $InstallDir -Recurse -Force
    Install-SelfBootstrapper $InstallDir
    Write-InstallState $StateDir $InstallDir $release $Channel $Build
    if (!$NoShortcut) {
      New-WindowsShortcuts $InstallDir (Join-Path $InstallDir $exeRel)
    }
    Write-Step "Installed $targetVersion"
  } finally {
    Remove-Item -LiteralPath $workRoot -Recurse -Force -ErrorAction SilentlyContinue
  }
} else {
  Write-Step "Already up to date: $installedVersion"
}

if (!$NoLaunch) {
  Launch-Client $InstallDir $StateDir $exeRel
}
