param(
  [string]$AdbPath = "$env:USERPROFILE\Downloads\platform-tools-latest-windows\platform-tools\adb.exe",
  [string]$ScrcpyPath = "$env:USERPROFILE\Downloads\scrcpy-win64-v3.1\scrcpy.exe",
  [int]$SshPort = 8022
)

$ErrorActionPreference = "Stop"

function Say($msg) {
  Write-Host ""
  Write-Host "== $msg ==" -ForegroundColor Cyan
}

function OK($msg) {
  Write-Host "[OK] $msg" -ForegroundColor Green
}

function Warn($msg) {
  Write-Host "[WARN] $msg" -ForegroundColor Yellow
}

function Stop-Fail($msg) {
  Write-Host "[FAIL] $msg" -ForegroundColor Red
  exit 1
}

Say "ADB setup"

if (!(Test-Path $AdbPath)) {
  Stop-Fail "adb.exe not found: $AdbPath"
}

$PlatformTools = Split-Path $AdbPath
$env:Path = "$PlatformTools;$env:Path"
OK "ADB path loaded: $AdbPath"

Say "Device check"
$devices = & $AdbPath devices
$devices | ForEach-Object { Write-Host $_ }
$deviceLine = $devices | Where-Object { $_ -match "`tdevice$" } | Select-Object -First 1

if (-not $deviceLine) {
  Stop-Fail "No authorized phone found. Check USB debugging / authorization prompt."
}

$DeviceId = ($deviceLine -split "`t")[0]
OK "Phone connected: $DeviceId"

Say "Open Termux"
& $AdbPath shell monkey -p com.termux 1 | Out-Null
Start-Sleep -Seconds 1
OK "Termux opened or focused"

Say "Forward SSH"
& $AdbPath forward "tcp:$SshPort" "tcp:$SshPort" | Out-Null
OK "Forwarded localhost:$SshPort to phone:$SshPort"

Say "Find Termux username"
$user = ""
try {
  $rawUser = & $AdbPath shell cat /sdcard/Download/termux-user.txt 2>$null
  if ($rawUser) {
    $user = ($rawUser | Select-Object -First 1).Trim()
  }
} catch {
  $user = ""
}

if ([string]::IsNullOrWhiteSpace($user) -or $user -match "Is a directory") {
  Warn "Could not read /sdcard/Download/termux-user.txt as a normal file."
  Warn "Attempting to repair known bad directory state."
  & $AdbPath shell "rm -rf /sdcard/Download/termux-user.txt" 2>$null | Out-Null
  $user = Read-Host "Enter Termux username from phone command: whoami"
  if (-not [string]::IsNullOrWhiteSpace($user)) {
    & $AdbPath shell "printf '$user\n' > /sdcard/Download/termux-user.txt" 2>$null | Out-Null
  }
}

if ([string]::IsNullOrWhiteSpace($user)) {
  Stop-Fail "No Termux username available. Run whoami in Termux and rerun with the answer."
}

OK "Termux user: $user"

Say "SSH check"
$sshTarget = "$($user)@127.0.0.1"
$sshWorks = $false
try {
  $test = ssh -o ConnectTimeout=3 -o StrictHostKeyChecking=no -p $SshPort $sshTarget "echo SSH_OK" 2>$null
  if ($test -match "SSH_OK") {
    $sshWorks = $true
    OK "SSH works"
  }
} catch {
  $sshWorks = $false
}

if (-not $sshWorks) {
  Warn "SSH is not responding yet."
  Warn "On the phone in Termux, run:"
  Write-Host "  pkg install -y openssh"
  Write-Host "  passwd"
  Write-Host "  sshd"
  Write-Host ""
  Warn "Then rerun this session script."
}

Say "scrcpy check"
if (Test-Path $ScrcpyPath) {
  OK "scrcpy found: $ScrcpyPath"
} else {
  Warn "scrcpy not found at:"
  Write-Host "  $ScrcpyPath"
}

$global:PhoneUser = $user
$global:SshPort = $SshPort
$global:ScrcpyExe = $ScrcpyPath

function global:OpenTermux {
  adb shell monkey -p com.termux 1
}

function global:Phone {
  adb forward "tcp:$global:SshPort" "tcp:$global:SshPort" | Out-Null
  $target = "$($global:PhoneUser)@127.0.0.1"
  ssh -o StrictHostKeyChecking=no -p $global:SshPort $target
}

function global:PhoneCmd {
  param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$Command
  )

  adb forward "tcp:$global:SshPort" "tcp:$global:SshPort" | Out-Null
  $target = "$($global:PhoneUser)@127.0.0.1"
  ssh -o StrictHostKeyChecking=no -p $global:SshPort $target ($Command -join " ")
}

function global:DbMenu {
  adb forward "tcp:$global:SshPort" "tcp:$global:SshPort" | Out-Null
  $target = "$($global:PhoneUser)@127.0.0.1"
  ssh -o StrictHostKeyChecking=no -p $global:SshPort $target "db-menu"
}

function global:PhoneClipSet {
  adb forward "tcp:$global:SshPort" "tcp:$global:SshPort" | Out-Null
  $target = "$($global:PhoneUser)@127.0.0.1"
  Get-Clipboard -Raw | ssh -o StrictHostKeyChecking=no -p $global:SshPort $target "db-clip-set"
}

function global:PhoneClipGet {
  adb forward "tcp:$global:SshPort" "tcp:$global:SshPort" | Out-Null
  $target = "$($global:PhoneUser)@127.0.0.1"
  ssh -o StrictHostKeyChecking=no -p $global:SshPort $target "db-clip-get" | Set-Clipboard
}

function global:StartScrcpy {
  if (!(Test-Path $global:ScrcpyExe)) {
    Write-Host "scrcpy not found: $global:ScrcpyExe" -ForegroundColor Yellow
    return
  }

  & $global:ScrcpyExe
}

Say "Session ready"
Write-Host "Commands loaded:" -ForegroundColor Cyan
Write-Host "  OpenTermux"
Write-Host "  Phone"
Write-Host "  PhoneCmd whoami"
Write-Host "  DbMenu"
Write-Host "  PhoneClipSet"
Write-Host "  PhoneClipGet"
Write-Host "  StartScrcpy"
Write-Host ""
Write-Host "[PC:PowerShell] phone session initialized." -ForegroundColor Green
