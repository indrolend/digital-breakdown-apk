#Requires -Version 5.1
<#
.SYNOPSIS
    Digital Breakdown phone-control session launcher.

.DESCRIPTION
    Locates ADB, verifies device connection, opens Termux, forwards SSH port,
    and loads helper commands into the current PowerShell session.

    Dot-source this file to load the helpers:
        . .\tools\phone-control\phone-session.ps1

    Config is persisted to $env:USERPROFILE\.db-phone-config.ps1 so you only
    need to supply your ADB path once.

.EXAMPLE
    # First-time or any session
    . .\tools\phone-control\phone-session.ps1

    # Show Termux terminal interactively
    OpenTermux

    # Run a Termux command
    PhoneCmd "db-menu"

    # Full APK demo (requires db-apk-demo.ps1 in same directory)
    DbApkDemo -RunId 28961104004 -ArtifactName digital-breakdown-native-debug-apk -Package com.indrolend.digitalbreakdown.native
#>

$ErrorActionPreference = "Stop"
$script:PhoneSessionDir = $PSScriptRoot

# ---------------------------------------------------------------------------
# Config persistence
# ---------------------------------------------------------------------------

$script:ConfigPath = Join-Path $env:USERPROFILE ".db-phone-config.ps1"

function Read-PhoneConfig {
    if (Test-Path $script:ConfigPath) {
        . $script:ConfigPath
    }
}

function Save-PhoneConfig {
    param(
        [string]$AdbPath,
        [string]$ScrcpyPath = ""
    )
    $lines = @(
        "# Digital Breakdown phone-control config — auto-generated",
        "`$env:DB_ADB_PATH = '$AdbPath'"
    )
    if ($ScrcpyPath) {
        $lines += "`$env:DB_SCRCPY_PATH = '$ScrcpyPath'"
    }
    $lines | Set-Content -Encoding UTF8 $script:ConfigPath
    Write-Host "[config] Saved to $script:ConfigPath"
}

Read-PhoneConfig

# ---------------------------------------------------------------------------
# ADB discovery
# ---------------------------------------------------------------------------

$script:AdbExe = $null

function Find-Adb {
    # 1. Already known from config
    if ($env:DB_ADB_PATH -and (Test-Path $env:DB_ADB_PATH)) {
        return $env:DB_ADB_PATH
    }

    # 2. On PATH
    $onPath = Get-Command adb -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }

    # 3. Common download/install locations
    $candidates = @(
        "$env:USERPROFILE\Downloads\platform-tools-latest-windows\platform-tools\adb.exe",
        "$env:USERPROFILE\Downloads\platform-tools\adb.exe",
        "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe",
        "C:\Android\platform-tools\adb.exe",
        "C:\Program Files\Android\platform-tools\adb.exe",
        "C:\Program Files (x86)\Android\android-sdk\platform-tools\adb.exe",
        "${env:ProgramFiles(x86)}\Android\android-sdk\platform-tools\adb.exe"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { return $c }
    }

    # 4. Glob search under Downloads and LOCALAPPDATA
    $searchRoots = @($env:USERPROFILE, "$env:USERPROFILE\Downloads", $env:LOCALAPPDATA)
    foreach ($root in $searchRoots) {
        if (-not (Test-Path $root)) { continue }
        $found = Get-ChildItem -Path $root -Filter adb.exe -Recurse -ErrorAction SilentlyContinue |
                 Select-Object -First 1
        if ($found) { return $found.FullName }
    }

    return $null
}

function Require-Adb {
    if ($script:AdbExe) { return $script:AdbExe }

    $found = Find-Adb
    if ($found) {
        $script:AdbExe = $found
        Write-Host "[adb] Found: $found"
        # Persist for next session if not already saved
        if (-not $env:DB_ADB_PATH) {
            $env:DB_ADB_PATH = $found
            Save-PhoneConfig -AdbPath $found
        }
        return $found
    }

    Write-Host ""
    Write-Host "[adb] ADB not found automatically."
    Write-Host "  Download platform-tools from:"
    Write-Host "  https://developer.android.com/tools/releases/platform-tools"
    Write-Host ""
    $userPath = Read-Host "Paste the full path to adb.exe (or press Enter to skip)"
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

    $candidates = @(
        "$env:USERPROFILE\Downloads\scrcpy-win64-v3.2\scrcpy.exe",
        "$env:USERPROFILE\Downloads\scrcpy-win64\scrcpy.exe",
        "$env:USERPROFILE\Downloads\scrcpy\scrcpy.exe",
        "C:\scrcpy\scrcpy.exe"
    )
    foreach ($c in $candidates) { if (Test-Path $c) { return $c } }

    $searchRoots = @("$env:USERPROFILE\Downloads")
    foreach ($root in $searchRoots) {
        if (-not (Test-Path $root)) { continue }
        $found = Get-ChildItem -Path $root -Filter scrcpy.exe -Recurse -ErrorAction SilentlyContinue |
                 Select-Object -First 1
        if ($found) { return $found.FullName }
    }

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
    Write-Host "[adb] Device: $device"
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

    # Read user from file written by bootstrap script
    $raw = & $adb shell "cat /sdcard/Download/termux-user.txt 2>/dev/null" 2>$null
    if ($raw -and $raw -notmatch "No such file|is a directory") {
        $script:TermuxUser = $raw.Trim()
        return $script:TermuxUser
    }

    # Fallback: ask adb who owns the Termux home
    $stat = & $adb shell "stat -c '%U' /data/data/com.termux/files/home 2>/dev/null" 2>$null
    if ($stat -and $stat -notmatch "No such file") {
        $script:TermuxUser = $stat.Trim()
        return $script:TermuxUser
    }

    Write-Host "[phone] Could not read Termux username automatically."
    Write-Host "  Run the bootstrap on the phone first:"
    Write-Host "    PhoneCmd 'bash /sdcard/Download/termux-control-bootstrap.sh'"
    Write-Host "  Or set manually:"
    $u = Read-Host "  Enter Termux username (e.g. u0_a234)"
    if ($u) {
        $script:TermuxUser = $u.Trim()
    }
    return $script:TermuxUser
}

# ---------------------------------------------------------------------------
# SSH forwarding
# ---------------------------------------------------------------------------

function Start-SshForward {
    $adb = Require-Adb
    Assert-DeviceConnected | Out-Null
    Write-Host "[adb] Forwarding tcp:8022 -> tcp:8022 ..."
    & $adb forward tcp:8022 tcp:8022 | Out-Null
    Write-Host "[ssh] Forward active. Connect with: ssh -p 8022 <user>@127.0.0.1"
}

# ---------------------------------------------------------------------------
# Exported helper commands
# ---------------------------------------------------------------------------

function global:OpenTermux {
    <#
    .SYNOPSIS Opens the Termux app on the phone.
    #>
    $adb = Require-Adb
    Assert-DeviceConnected | Out-Null
    & $adb shell "am start -n com.termux/.HomeActivity" | Out-Null
    Write-Host "[termux] Opened."
}

function global:Phone {
    <#
    .SYNOPSIS Opens an SSH session into Termux.
    .EXAMPLE Phone
    #>
    $user = Get-TermuxUser
    if (-not $user) { throw "Termux user unknown. Run Get-TermuxUser." }
    Start-SshForward
    Write-Host "[ssh] Connecting as $user ..."
    ssh -p 8022 "$user@127.0.0.1"
}

function global:PhoneCmd {
    <#
    .SYNOPSIS Runs a single command in Termux over SSH.
    .PARAMETER Cmd Command string to execute.
    .EXAMPLE PhoneCmd "db-menu"
    #>
    param([Parameter(Mandatory)][string]$Cmd)
    $user = Get-TermuxUser
    if (-not $user) { throw "Termux user unknown." }
    Start-SshForward
    ssh -p 8022 -o StrictHostKeyChecking=no "$user@127.0.0.1" "$Cmd"
}

function global:DbMenu {
    <#
    .SYNOPSIS Opens the db-menu inside Termux over SSH.
    #>
    PhoneCmd "db-menu"
}

function global:PhoneClipSet {
    <#
    .SYNOPSIS Sets the Android clipboard via Termux.
    .PARAMETER Text Text to put on the clipboard.
    #>
    param([Parameter(Mandatory)][string]$Text)
    $user = Get-TermuxUser
    if (-not $user) { throw "Termux user unknown." }
    Start-SshForward
    # Pipe text to db-clip-set; avoid shell injection by piping stdin
    $Text | ssh -p 8022 -o StrictHostKeyChecking=no "$user@127.0.0.1" "db-clip-set"
}

function global:PhoneClipGet {
    <#
    .SYNOPSIS Reads the Android clipboard from Termux.
    #>
    $user = Get-TermuxUser
    if (-not $user) { throw "Termux user unknown." }
    Start-SshForward
    ssh -p 8022 -o StrictHostKeyChecking=no "$user@127.0.0.1" "db-clip-get"
}

function global:StartScrcpy {
    <#
    .SYNOPSIS Starts a scrcpy mirror of the phone screen.
    #>
    Assert-DeviceConnected | Out-Null
    $exe = Find-Scrcpy
    if (-not $exe) {
        Write-Host "[scrcpy] Not found. Download from https://github.com/Genymobile/scrcpy/releases"
        Write-Host "  Then save path with: Save-PhoneConfig -AdbPath (Find-Adb) -ScrcpyPath '<path>'"
        return
    }
    Write-Host "[scrcpy] Starting with: $exe"
    Start-Process $exe -ArgumentList "--stay-awake", "--max-size", "1024"
}

function global:DbApkDemo {
    <#
    .SYNOPSIS Full APK demo: download artifact -> pull -> install -> launch -> evidence.
    .PARAMETER RunId     GitHub Actions run ID.
    .PARAMETER ArtifactName  Name of the artifact to download.
    .PARAMETER Package   Android package name.
    .PARAMETER OutName   Output APK filename (default: app-debug.apk).
    .PARAMETER SkipDownload  Skip Termux gh download (use if APK already on phone).
    .PARAMETER EvidenceOnly  Skip download/pull/install/launch; capture evidence only.
    .PARAMETER SkipInstall   Deprecated compatibility alias for EvidenceOnly.
    #>
    param(
        [Parameter(Mandatory)][string]$RunId,
        [Parameter(Mandatory)][string]$ArtifactName,
        [Parameter(Mandatory)][string]$Package,
        [string]$OutName = "app-debug.apk",
        [switch]$SkipDownload,
        [Alias("SkipInstall")]
        [switch]$EvidenceOnly
    )
    $demoScript = Join-Path $script:PhoneSessionDir "db-apk-demo.ps1"
    if (-not (Test-Path $demoScript)) {
        throw "db-apk-demo.ps1 not found at $demoScript"
    }
    & $demoScript `
        -RunId $RunId `
        -ArtifactName $ArtifactName `
        -Package $Package `
        -OutName $OutName `
        -SkipDownload:$SkipDownload `
        -EvidenceOnly:$EvidenceOnly
}

function global:DbApkInstall {
    <#
    .SYNOPSIS Pull APK from phone and install it.
    .PARAMETER DevicePath  Path on device, e.g. /sdcard/Download/db-apks/app.apk
    .PARAMETER LocalPath   Where to save on Windows (default: Downloads).
    .PARAMETER Package     Android package name.
    #>
    param(
        [Parameter(Mandatory)][string]$DevicePath,
        [string]$LocalPath = (Join-Path $env:USERPROFILE "Downloads\$(Split-Path $DevicePath -Leaf)"),
        [string]$Package = ""
    )
    $installScript = Join-Path $script:PhoneSessionDir "db-apk-install.ps1"
    if (-not (Test-Path $installScript)) {
        throw "db-apk-install.ps1 not found at $installScript"
    }
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

    # Add ADB directory to PATH for this session
    $adbDir = Split-Path $adbExe
    if ($env:PATH -notlike "*$adbDir*") {
        $env:PATH = "$adbDir;$env:PATH"
    }

    $device = Get-AttachedDevice
    if ($device) {
        Write-Host "[adb] Device connected: $device"
        Start-SshForward
        $user = Get-TermuxUser
        if ($user) {
            Write-Host "[ssh] Termux user: $user"
        }
    } else {
        Write-Host "[adb] No device connected. Connect phone and run: Assert-DeviceConnected"
    }

    $scrcpy = Find-Scrcpy
    if ($scrcpy) {
        Write-Host "[scrcpy] Found: $scrcpy"
    } else {
        Write-Host "[scrcpy] Not found (optional)."
    }

} catch {
    Write-Host "[warn] Session init: $_"
}

Write-Host ""
Write-Host "Loaded commands:"
Write-Host "  OpenTermux           - Open Termux app on phone"
Write-Host "  Phone                - SSH into Termux"
Write-Host "  PhoneCmd <cmd>       - Run command in Termux over SSH"
Write-Host "  DbMenu               - Open db-menu in Termux"
Write-Host "  PhoneClipSet <text>  - Set Android clipboard"
Write-Host "  PhoneClipGet         - Read Android clipboard"
Write-Host "  StartScrcpy          - Start screen mirror"
Write-Host "  DbApkDemo ...        - Full APK download/install/evidence run"
Write-Host "  DbApkInstall ...     - Pull + install APK from phone storage"
Write-Host ""
