param(
  [Parameter(Position = 0)]
  [string]$Command = "status",

  [Parameter(Position = 1)]
  [string]$Arg1 = "",

  [Parameter(Position = 2)]
  [string]$Arg2 = ""
)

$ErrorActionPreference = "Stop"

$Root = $env:SEKAILINK_WORKBENCH_ROOT
if (-not $Root) { $Root = "D:\SekaiLink" }

$Repo = Join-Path $Root "repos\sekailink-canonical"
$BareRepo = Join-Path $Root "git\sekailink-canonical.git"
$Logs = Join-Path $Root "logs\worker"
$Artifacts = Join-Path $Root "artifacts"
$MsysBash = "C:\msys64\usr\bin\bash.exe"
$UcrtPath = "/ucrt64/bin:/usr/bin:/bin"

New-Item -ItemType Directory -Force -Path $Logs, $Artifacts | Out-Null

function New-LogPath {
  param([string]$Name)
  $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
  return (Join-Path $Logs "$stamp-$Name.log")
}

function Invoke-Logged {
  param(
    [string]$Name,
    [scriptblock]$Body
  )

  $log = New-LogPath $Name
  $started = Get-Date
  "sekai-worker command=$Name started=$($started.ToString('o'))" | Tee-Object -FilePath $log
  try {
    & $Body 2>&1 | Tee-Object -FilePath $log -Append
    $status = "ok"
  } catch {
    $status = "failed"
    "ERROR: $($_.Exception.Message)" | Tee-Object -FilePath $log -Append
    throw
  } finally {
    $ended = Get-Date
    $result = [ordered]@{
      command = $Name
      status = $status
      started_at = $started.ToString("o")
      ended_at = $ended.ToString("o")
      log = $log
      repo = $Repo
      artifacts = $Artifacts
    }
    $resultPath = Join-Path $Artifacts "last-build-result.json"
    ($result | ConvertTo-Json -Depth 4) | Set-Content -Encoding UTF8 -Path $resultPath
    "result=$resultPath" | Tee-Object -FilePath $log -Append
  }
}

function Invoke-Msys {
  param([string]$Script)
  if (-not (Test-Path $MsysBash)) { throw "missing_msys2_bash:$MsysBash" }
  New-Item -ItemType Directory -Force -Path (Join-Path $Root "tmp") | Out-Null
  $scriptName = "sekai-worker-$PID-$([guid]::NewGuid().ToString('N')).sh"
  $scriptWin = Join-Path (Join-Path $Root "tmp") $scriptName
  $scriptMsys = "/d/SekaiLink/tmp/$scriptName"
  $body = "set -euo pipefail`nexport PATH=${UcrtPath}:`$PATH`n$Script`n"
  $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
  [System.IO.File]::WriteAllText($scriptWin, $body, $utf8NoBom)
  $oldPreference = $ErrorActionPreference
  $ErrorActionPreference = "Continue"
  & $MsysBash -lc "bash '$scriptMsys'" 2>&1 | ForEach-Object { Write-Output $_ }
  $code = $LASTEXITCODE
  $ErrorActionPreference = $oldPreference
  Remove-Item -Force -ErrorAction SilentlyContinue -Path $scriptWin
  if ($code -ne 0) { throw "msys_failed:$code" }
}

function Assert-Repo {
  if (-not (Test-Path $Repo)) { throw "missing_worktree:$Repo" }
}

function Show-Status {
  Write-Host "SekaiLink Windows Workbench"
  Write-Host "root=$Root"
  Write-Host "repo=$Repo"
  Write-Host "bare=$BareRepo"
  hostname
  whoami
  Get-Volume D | Select-Object DriveLetter, FileSystemLabel, SizeRemaining, Size | Format-Table -Auto
  Get-Service sshd | Select-Object Name, Status, StartType | Format-Table -Auto
  if (Test-Path $Repo) {
    Invoke-Msys "cd /d/SekaiLink/repos/sekailink-canonical && git status --short --branch && git rev-parse --short HEAD"
  }
}

function Invoke-Doctor {
  Show-Status
  Invoke-Msys @'
for c in git cmake ninja gcc g++ node npm python zip unzip jq curl openssl; do
  printf "%-10s " "$c"
  command -v "$c" || exit 1
done
git --version
cmake --version | head -1
ninja --version
gcc --version | head -1
node --version
npm --version
python --version
'@
  wsl.exe --status 2>&1
}

function Sync-Repo {
  param([string]$Branch)
  if (-not $Branch) { $Branch = "main" }
  if (-not (Test-Path $Repo)) {
    Invoke-Msys "git clone /d/SekaiLink/git/sekailink-canonical.git /d/SekaiLink/repos/sekailink-canonical"
  } else {
    Invoke-Msys "cd /d/SekaiLink/repos/sekailink-canonical && git remote set-url origin /d/SekaiLink/git/sekailink-canonical.git"
  }
  Invoke-Msys "cd /d/SekaiLink/repos/sekailink-canonical && git fetch --prune origin && git checkout $Branch && git reset --hard origin/$Branch && git clean -fdx"
}

function Build-Bootloader {
  Assert-Repo
  Invoke-Msys "cd /d/SekaiLink/repos/sekailink-canonical/apps/client-core && npm run bootstrapper:pack"
}

function Build-Client {
  Assert-Repo
  Invoke-Msys "cd /d/SekaiLink/repos/sekailink-canonical/apps/client-core && npm ci && npm run electron:pack:win"
}

function Build-Sekaiemu {
  Assert-Repo
  Invoke-Msys "cd /d/SekaiLink/repos/sekailink-canonical && cmake -S apps/sekaiemu -B /d/SekaiLink/build/sekaiemu-ucrt64 -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build /d/SekaiLink/build/sekaiemu-ucrt64 -j4"
}

function Run-Tests {
  Assert-Repo
  Invoke-Msys "cd /d/SekaiLink/repos/sekailink-canonical/apps/client-core && npm test -- --runInBand"
}

function Package-Release {
  Assert-Repo
  Invoke-Msys "cd /d/SekaiLink/repos/sekailink-canonical/apps/client-core && npm run electron:pack:update-bundles && npm run bootstrapper:pack"
}

function Show-Artifacts {
  Get-ChildItem $Artifacts -Recurse -File -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 50 FullName, Length, LastWriteTime |
    Format-Table -Auto
}

function Show-Logs {
  Get-ChildItem $Logs -File -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 20 FullName, Length, LastWriteTime |
    Format-Table -Auto
}

switch ($Command.ToLowerInvariant()) {
  "status" { Invoke-Logged "status" { Show-Status } }
  "doctor" { Invoke-Logged "doctor" { Invoke-Doctor } }
  "sync" { Invoke-Logged "sync" { Sync-Repo $Arg1 } }
  "build" {
    switch ($Arg1.ToLowerInvariant()) {
      "bootloader" { Invoke-Logged "build-bootloader" { Build-Bootloader } }
      "client" { Invoke-Logged "build-client" { Build-Client } }
      "sekaiemu" { Invoke-Logged "build-sekaiemu" { Build-Sekaiemu } }
      default { throw "unknown_build_target:$Arg1" }
    }
  }
  "test" { Invoke-Logged "test" { Run-Tests } }
  "package" { Invoke-Logged "package" { Package-Release } }
  "artifacts" { Invoke-Logged "artifacts" { Show-Artifacts } }
  "logs" { Invoke-Logged "logs" { Show-Logs } }
  default { throw "unknown_command:$Command" }
}
