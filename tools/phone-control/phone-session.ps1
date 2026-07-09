#Requires -Version 5.1
<#
.SYNOPSIS
    Digital Breakdown phone-control session launcher.

.DESCRIPTION
    Locates ADB, verifies device connection, opens Termux, forwards SSH port,
    and loads helper commands into the current PowerShell session.

    Dot-source this file to load the helpers:
        . ./tools/phone-control/phone-session.ps1

    Config is persisted to the host user home as .db-phone-config.ps1 so you
    only need to supply host paths once.
#>

$ErrorActionPreference = "Stop"
$script:PhoneSessionDir = $PSScriptRoot
$script:DbQuiet = ($env:DB_PHONE_QUIET -eq "1")

# ---------------------------------------------------------------------------
# Host compatibility helpers
# ---------------------------------------------------------------------------

function Get-DbHostHome {
    if ($env:USERPROFILE) { return $env:USERPROFILE }
    if ($HOME) { return $HOME }
    return [Environment]::GetFolderPath("UserProfile")
}

function Get-DbHostDownloads {
    return (Join-Path (Get-DbHostHome) "Downloads")
}

function Add-DbPathEntry {
    param([Parameter(Mandatory)][string]$Path)
    if (-not $Path) { return }
    $sep = [IO.Path]::PathSeparator
    $parts = @($env:PATH -split [regex]::Escape([string]$sep) | Where-Object { $_ })
    if ($parts -notcontains $Path) {
        $env:PATH = "$Path$sep$env:PATH"
    }
}

function Write-DbStatus {
    param([string]$Message)
    if (-not $script:DbQuiet) { Write-Host $Message }
}

function Quote-Sh {
    param([Parameter(Mandatory)][string]$Text)
    return "'" + $Text.Replace("'", "'\"'\"'") + "'"
}

# ---------------------------------------------------------------------------
# Config persistence
# ---------------------------------------------------------------------------

$script:ConfigPath = Join-Path (Get-DbHostHome) ".db-phone-config.ps1"

function Read-PhoneConfig {
    if (Test-Path $script:ConfigPath) {
        . $script:ConfigPath
    }
}

function Save-PhoneConfig {
    param(
        [string]$AdbPath = $env:DB_ADB_PATH,
        [string]$ScrcpyPath = $env:DB_SCRCPY_PATH,
        [string]$SshKeyPath = $env:DB_TERMUX_SSH_KEY
    )
    $lines = @("# Digital Breakdown phone-control config - auto-generated")
    if ($AdbPath) { $lines += "`$env:DB_ADB_PATH = '$AdbPath'" }
    if ($ScrcpyPath) { $lines += "`$env:DB_SCRCPY_PATH = '$ScrcpyPath'" }
    if ($SshKeyPath) { $lines += "`$env:DB_TERMUX_SSH_KEY = '$SshKeyPath'" }
    $lines | Set-Content -Encoding UTF8 $script:ConfigPath
    Write-DbStatus "[config] Saved to $script:ConfigPath"
}

Read-PhoneConfig

# ---------------------------------------------------------------------------
# ADB discovery
# ---------------------------------------------------------------------------

$script:AdbExe = $null

function Find-Adb {
    if ($env:DB_ADB_PATH -and (Test-Path $env:DB_ADB_PATH)) {
        return $env:DB_ADB_PATH
    }

    $onPath = Get-Command adb -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }

    $hostHome = Get-DbHostHome
    $downloads = Get-DbHostDownloads
    $localAppData = $env:LOCALAPPDATA
    $candidates = @(
        (Join-Path $downloads "platform-tools-latest-windows/platform-tools/adb.exe"),
        (Join-Path $downloads "platform-tools/adb.exe"),
        (Join-Path $downloads "platform-tools/adb"),
        (Join-Path $hostHome "Library/Android/sdk/platform-tools/adb"),
        "/opt/homebrew/bin/adb",
        "/usr/local/bin/adb",
        "C:\Android\platform-tools\adb.exe",
        "C:\Program Files\Android\platform-tools\adb.exe",
        "C:\Program Files (x86)\Android\android-sdk\platform-tools\adb.exe"
    )
    if ($localAppData) {
        $candidates += (Join-Path $localAppData "Android/Sdk/platform-tools/adb.exe")
    }
    if (${env:ProgramFiles(x86)}) {
        $candidates += (Join-Path ${env:ProgramFiles(x86)} "Android/android-sdk/platform-tools/adb.exe")
    }

    foreach ($c in $candidates) {
        if ($c -and (Test-Path $c)) { return $c }
    }

    return $null
}

function Require-Adb {
    if ($script:AdbExe) { return $script:AdbExe }

    $found = Find-Adb
    if ($found) {
        $script:AdbExe = $found
        Write-DbStatus "[adb] Found: $found"
        if (-not $env:DB_ADB_PATH) {
            $env:DB_ADB_PATH = $found
            Save-PhoneConfig -AdbPath $found
        }
        return $found
    }

    Write-Host ""
    Write-Host "[adb] ADB not found automatically."
    Write-Host "  Install Android platform-tools or set DB_ADB_PATH."
    Write-Host "  macOS/Homebrew: brew install android-platform-tools"
    Write-Host "  Windows: download platform-tools from https://developer.android.com/tools/releases/platform-tools"
    Write-Host ""
    $userPath = Read-Host "Paste the full path to adb/adb.exe (or press Enter to skip)"
    if ($userPath -and (Test-Path $userPath)) {
        $script:AdbExe = $userPath
        $env:DB_ADB_PATH = $userPath
        Save-PhoneConfig -AdbPath $userPath
        return $userPath
    }

    throw "ADB not available. Cannot continue."
}

# ---------------------------------------------------------------------------
# scrcpy discovery
# ---------------------------------------------------------------------------

function Find-Scrcpy {
    if ($env:DB_SCRCPY_PATH -and (Test-Path $env:DB_SCRCPY_PATH)) {
        return $env:DB_SCRCPY_PATH
    }

    $onPath = Get-Command scrcpy -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }

    $downloads = Get-DbHostDownloads
    $candidates = @(
        (Join-Path $downloads "scrcpy-win64-v3.2/scrcpy.exe"),
        (Join-Path $downloads "scrcpy-win64/scrcpy.exe"),
        (Join-Path $downloads "scrcpy/scrcpy.exe"),
        "/opt/homebrew/bin/scrcpy",
        "/usr/local/bin/scrcpy",
        "C:\scrcpy\scrcpy.exe"
    )
    foreach ($c in $candidates) { if ($c -and (Test-Path $c)) { return $c } }
    return $null
}

# ---------------------------------------------------------------------------
# Device helpers
# ---------------------------------------------------------------------------

function Get-AttachedDevice {
    $adb = Require-Adb
    $lines = & $adb devices 2>&1 | Select-String "device$"
    if ($lines) {
        return ($lines | Select-Object -First 1).ToString().Trim().Split()[0]
    }
    return $null
}

function Assert-DeviceConnected {
    $device = Get-AttachedDevice
    if (-not $device) {
        Write-Host "[adb] No authorized device detected."
        Write-Host "  1. Connect the phone via USB."
        Write-Host "  2. Unlock the phone and tap 'Allow' on the USB debugging prompt."
        Write-Host "  3. Run: adb devices"
        throw "No device connected."
    }
    Write-DbStatus "[adb] Device: $device"
    return $device
}

# ---------------------------------------------------------------------------
# Termux username helper
# ---------------------------------------------------------------------------

$script:TermuxUser = $null

function Get-TermuxUser {
    if ($script:TermuxUser) { return $script:TermuxUser }

    $adb = Require-Adb
    $deviceId = Get-AttachedDevice
    if (-not $deviceId) { return $null }

    $raw = & $adb shell "cat /sdcard/Download/db-control/termux-user.txt 2>/dev/null || cat /sdcard/Download/termux-user.txt 2>/dev/null" 2>$null
    if ($raw -and $raw -notmatch "No such file|is a directory") {
        $script:TermuxUser = $raw.Trim()
        return $script:TermuxUser
    }

    $stat = & $adb shell "stat -c '%U' /data/data/com.termux/files/home 2>/dev/null" 2>$null
    if ($stat -and $stat -notmatch "No such file") {
        $script:TermuxUser = $stat.Trim()
        return $script:TermuxUser
    }

    Write-Host "[phone] Could not read Termux username automatically."
    Write-Host "  Run the bootstrap on the phone first:"
    Write-Host "    PhoneCmd 'bash /sdcard/Download/db-control/tools/termux-control-bootstrap.sh'"
    $u = Read-Host "  Enter Termux username (e.g. u0_a234)"
    if ($u) { $script:TermuxUser = $u.Trim() }
    return $script:TermuxUser
}

# ---------------------------------------------------------------------------
# SSH forwarding and credential helpers
# ---------------------------------------------------------------------------

function Start-SshForward {
    param([switch]$Quiet)
    $adb = Require-Adb
    Assert-DeviceConnected | Out-Null
    if (-not $Quiet) { Write-DbStatus "[adb] Forwarding tcp:8022 -> tcp:8022 ..." }
    & $adb forward tcp:8022 tcp:8022 | Out-Null
    if (-not $Quiet) { Write-DbStatus "[ssh] Forward active. Connect with: ssh -p 8022 USER@127.0.0.1" }
}

function Get-TermuxSshArgs {
    $args = @("-p", "8022", "-o", "StrictHostKeyChecking=no")
    if ($env:DB_TERMUX_SSH_KEY -and (Test-Path $env:DB_TERMUX_SSH_KEY)) {
        $args += @("-i", $env:DB_TERMUX_SSH_KEY, "-o", "IdentitiesOnly=yes")
    }
    return $args
}

function Invoke-TermuxSsh {
    param(
        [Parameter(Mandatory)][string]$Cmd,
        [switch]$Quiet
    )
    $user = Get-TermuxUser
    if (-not $user) { throw "Termux user unknown." }
    Start-SshForward -Quiet:$Quiet
    $sshArgs = @(Get-TermuxSshArgs) + @("$user@127.0.0.1", $Cmd)
    & ssh @sshArgs
}

function global:SetupTermuxSshKey {
    <#
    .SYNOPSIS Creates or installs a host SSH key for passwordless Termux SSH.
    #>
    param([string]$KeyPath = (Join-Path (Join-Path (Get-DbHostHome) ".ssh") "db_termux_lg_stylo4"))

    $user = Get-TermuxUser
    if (-not $user) { throw "Termux user unknown." }
    Start-SshForward

    $sshDir = Split-Path $KeyPath
    if (-not (Test-Path $sshDir)) { New-Item -ItemType Directory -Force -Path $sshDir | Out-Null }

    if (-not (Test-Path $KeyPath)) {
        Write-Host "[ssh] Creating key: $KeyPath"
        & ssh-keygen -t ed25519 -f $KeyPath -N "" | Out-Host
    }

    $pub = "$KeyPath.pub"
    if (-not (Test-Path $pub)) { throw "Public key missing: $pub" }

    Write-Host "[ssh] Installing public key into Termux. You may need the Termux password once."
    $pubText = Get-Content $pub -Raw
    $installCmd = "mkdir -p ~/.ssh && chmod 700 ~/.ssh && touch ~/.ssh/authorized_keys && grep -qxF '$($pubText.Trim())' ~/.ssh/authorized_keys || echo '$($pubText.Trim())' >> ~/.ssh/authorized_keys; chmod 600 ~/.ssh/authorized_keys"
    $sshArgs = @("-p", "8022", "-o", "StrictHostKeyChecking=no", "$user@127.0.0.1", $installCmd)
    & ssh @sshArgs
    if ($LASTEXITCODE -ne 0) { throw "Failed to install SSH key in Termux." }

    $env:DB_TERMUX_SSH_KEY = $KeyPath
    Save-PhoneConfig -SshKeyPath $KeyPath

    Write-Host "[ssh] Testing key login ..."
    $testArgs = @(Get-TermuxSshArgs) + @("$user@127.0.0.1", "echo ssh_key_ok")
    & ssh @testArgs
}

function global:TermuxGhTokenLogin {
    <#
    .SYNOPSIS Logs Termux gh into GitHub using a hidden token prompt.
    .DESCRIPTION Does not store the token in the repo or /sdcard. gh stores its own auth config in Termux home.
    #>
    $user = Get-TermuxUser
    if (-not $user) { throw "Termux user unknown." }
    Start-SshForward -Quiet

    $secure = Read-Host "Paste GitHub token for Termux gh" -AsSecureString
    $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    try {
        $token = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
        if (-not $token) { throw "Empty token." }
        $sshArgs = @(Get-TermuxSshArgs) + @("$user@127.0.0.1", "rm -f ~/.config/gh/hosts.yml; gh auth login --with-token >/dev/null && gh auth status")
        $token | & ssh @sshArgs
    } finally {
        if ($bstr -ne [IntPtr]::Zero) { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr) }
        Remove-Variable token -ErrorAction SilentlyContinue
    }
}

# ---------------------------------------------------------------------------
# Exported helper commands
# ---------------------------------------------------------------------------

function global:OpenTermux {
    $adb = Require-Adb
    Assert-DeviceConnected | Out-Null
    & $adb shell "am start -n com.termux/.HomeActivity" | Out-Null
    Write-Host "[termux] Opened."
}

function global:Phone {
    $user = Get-TermuxUser
    if (-not $user) { throw "Termux user unknown. Run Get-TermuxUser." }
    Start-SshForward -Quiet
    Write-Host "[ssh] Connecting as $user ..."
    $sshArgs = @(Get-TermuxSshArgs) + @("$user@127.0.0.1")
    ssh @sshArgs
}

function global:PhoneCmd {
    param(
        [Parameter(Mandatory)][string]$Cmd,
        [switch]$Quiet
    )
    Invoke-TermuxSsh -Cmd $Cmd -Quiet:($Quiet -or $script:DbQuiet)
}

function global:DbMenu {
    PhoneCmd "db-menu" -Quiet
}

function global:PhoneClipSet {
    param([Parameter(Mandatory)][string]$Text)
    $user = Get-TermuxUser
    if (-not $user) { throw "Termux user unknown." }
    Start-SshForward -Quiet
    $sshArgs = @(Get-TermuxSshArgs) + @("$user@127.0.0.1", "db-clip-set")
    $Text | ssh @sshArgs
}

function global:PhoneClipGet {
    PhoneCmd "db-clip-get" -Quiet
}

function global:StartScrcpy {
    <#
    .SYNOPSIS Starts scrcpy in a separate terminal by default so the current shell stays usable.
    .PARAMETER CurrentTerminal Run scrcpy in the current PowerShell terminal.
    .PARAMETER Detached Launch without a terminal window when supported.
    #>
    param(
        [switch]$CurrentTerminal,
        [switch]$Detached,
        [int]$MaxSize = 1024
    )

    Assert-DeviceConnected | Out-Null
    $exe = Find-Scrcpy
    if (-not $exe) {
        Write-Host "[scrcpy] Not found. Install scrcpy or save its path with:"
        Write-Host "  Save-PhoneConfig -AdbPath (Find-Adb) -ScrcpyPath '/path/to/scrcpy'"
        return
    }

    $argList = @("--stay-awake", "--max-size", "$MaxSize")

    if ($CurrentTerminal) {
        Write-Host "[scrcpy] Running in current terminal: $exe"
        & $exe @argList
        return
    }

    if ($Detached) {
        Write-Host "[scrcpy] Starting detached: $exe"
        Start-Process -FilePath $exe -ArgumentList $argList | Out-Null
        return
    }

    if ($IsMacOS) {
        $cmd = "cd " + (Quote-Sh (Get-Location).Path) + "; " + (Quote-Sh $exe) + " --stay-awake --max-size $MaxSize; echo; echo '[scrcpy] exited. You can close this window.'"
        $osa = @(
            "tell application \"Terminal\"",
            "activate",
            "do script " + ('"' + $cmd.Replace('\\', '\\\\').Replace('"', '\"') + '"'),
            "end tell"
        )
        Write-Host "[scrcpy] Opening separate macOS Terminal window."
        & osascript -e $osa[0] -e $osa[1] -e $osa[2] -e $osa[3] | Out-Null
        return
    }

    if ($IsWindows) {
        $pwsh = (Get-Command pwsh -ErrorAction SilentlyContinue)
        $shell = if ($pwsh) { $pwsh.Source } else { "powershell.exe" }
        $cmd = "& '$exe' --stay-awake --max-size $MaxSize; Write-Host ''; Read-Host '[scrcpy] exited. Press Enter to close'"
        Write-Host "[scrcpy] Opening separate PowerShell window."
        Start-Process -FilePath $shell -ArgumentList @("-NoExit", "-Command", $cmd) | Out-Null
        return
    }

    $terminal = Get-Command x-terminal-emulator -ErrorAction SilentlyContinue
    if (-not $terminal) { $terminal = Get-Command gnome-terminal -ErrorAction SilentlyContinue }
    if (-not $terminal) { $terminal = Get-Command konsole -ErrorAction SilentlyContinue }
    if ($terminal) {
        $cmd = (Quote-Sh $exe) + " --stay-awake --max-size $MaxSize; echo; read -r -p '[scrcpy] exited. Press Enter to close.'"
        Write-Host "[scrcpy] Opening separate terminal window."
        Start-Process -FilePath $terminal.Source -ArgumentList @("-e", "bash", "-lc", $cmd) | Out-Null
    } else {
        Write-Host "[scrcpy] No terminal launcher found; starting detached."
        Start-Process -FilePath $exe -ArgumentList $argList | Out-Null
    }
}

function global:DbApkDemo {
    param(
        [Parameter(Mandatory)][string]$RunId,
        [Parameter(Mandatory)][string]$ArtifactName,
        [Parameter(Mandatory)][string]$Package,
        [string]$OutName = "current.apk",
        [switch]$SkipDownload,
        [Alias("SkipInstall")]
        [switch]$EvidenceOnly,
        [switch]$PullEvidenceToWindows,
        [switch]$NoPullEvidence,
        [switch]$ArchivePreviousEvidence,
        [switch]$CleanBeforeRun,
        [int]$KeepEvidenceCount = 10
    )
    $demoScript = Join-Path $script:PhoneSessionDir "db-apk-demo.ps1"
    if (-not (Test-Path $demoScript)) { throw "db-apk-demo.ps1 not found at $demoScript" }
    & $demoScript `
        -RunId $RunId `
        -ArtifactName $ArtifactName `
        -Package $Package `
        -OutName $OutName `
        -SkipDownload:$SkipDownload `
        -EvidenceOnly:$EvidenceOnly `
        -PullEvidenceToWindows:$PullEvidenceToWindows `
        -NoPullEvidence:$NoPullEvidence `
        -ArchivePreviousEvidence:$ArchivePreviousEvidence `
        -CleanBeforeRun:$CleanBeforeRun `
        -KeepEvidenceCount $KeepEvidenceCount
}

function global:DbApkInstall {
    param(
        [string]$DevicePath = "/sdcard/Download/db-control/apks/current.apk",
        [string]$LocalPath = "",
        [string]$Package = ""
    )
    $installScript = Join-Path $script:PhoneSessionDir "db-apk-install.ps1"
    if (-not (Test-Path $installScript)) { throw "db-apk-install.ps1 not found at $installScript" }
    & $installScript -DevicePath $DevicePath -LocalPath $LocalPath -Package $Package
}

# ---------------------------------------------------------------------------
# Session startup
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "=== Digital Breakdown Phone Session ==="

try {
    $adbExe = Require-Adb
    $script:AdbExe = $adbExe
    Add-DbPathEntry -Path (Split-Path $adbExe)

    $device = Get-AttachedDevice
    if ($device) {
        Write-Host "[adb] Device connected: $device"
        Start-SshForward -Quiet
        $user = Get-TermuxUser
        if ($user) { Write-Host "[ssh] Termux user: $user" }
        if ($env:DB_TERMUX_SSH_KEY -and (Test-Path $env:DB_TERMUX_SSH_KEY)) {
            Write-Host "[ssh] Key configured: $env:DB_TERMUX_SSH_KEY"
        } else {
            Write-Host "[ssh] Password auth active. Run: SetupTermuxSshKey"
        }
    } else {
        Write-Host "[adb] No device connected. Connect phone and run: Assert-DeviceConnected"
    }

    $scrcpy = Find-Scrcpy
    if ($scrcpy) { Write-Host "[scrcpy] Found: $scrcpy" } else { Write-Host "[scrcpy] Not found (optional)." }
} catch {
    Write-Host "[warn] Session init: $_"
}

Write-Host ""
Write-Host "Loaded commands:"
Write-Host "  OpenTermux           - Open Termux app on phone"
Write-Host "  Phone                - SSH into Termux"
Write-Host "  PhoneCmd CMD         - Run command in Termux over SSH"
Write-Host "  SetupTermuxSshKey    - Configure passwordless Termux SSH"
Write-Host "  TermuxGhTokenLogin   - Hidden GitHub token login for gh in Termux"
Write-Host "  DbMenu               - Open db-menu in Termux"
Write-Host "  PhoneClipSet TEXT    - Set Android clipboard"
Write-Host "  PhoneClipGet         - Read Android clipboard"
Write-Host "  StartScrcpy          - Start screen mirror in separate terminal"
Write-Host "  StartScrcpy -Detached - Start screen mirror detached/no terminal"
Write-Host "  StartScrcpy -CurrentTerminal - Start screen mirror here"
Write-Host "  DbApkDemo ...        - Full phone-first APK + evidence run"
Write-Host "  DbApkInstall ...     - Install APK from phone stable path"
Write-Host ""
