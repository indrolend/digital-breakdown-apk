#Requires -Version 5.1
<#
.SYNOPSIS
    Windows-side APK install and launch helper.

.DESCRIPTION
    Pulls an APK from Android shared storage, installs it, and optionally launches
    the package. Part of the Digital Breakdown phone-control workflow.

.PARAMETER DevicePath
    Path of the APK on the Android device (e.g. /sdcard/Download/db-apks/app.apk).

.PARAMETER LocalPath
    Where to save the pulled APK on Windows. Defaults to Downloads folder.

.PARAMETER Package
    Android package name to launch after install (optional).

.PARAMETER SkipPull
    If set, assumes LocalPath already contains the APK (skips adb pull).

.PARAMETER SkipLaunch
    If set, installs but does not launch.

.EXAMPLE
    # Pull from phone and install
    .\db-apk-install.ps1 `
        -DevicePath /sdcard/Download/db-apks/pr2-native-debug.apk `
        -Package com.indrolend.digitalbreakdown.native

.EXAMPLE
    # Install from local APK already on Windows
    .\db-apk-install.ps1 `
        -DevicePath /sdcard/Download/db-apks/pr2-native-debug.apk `
        -LocalPath "$env:USERPROFILE\Downloads\pr2-native-debug.apk" `
        -SkipPull `
        -Package com.indrolend.digitalbreakdown.native
#>

param(
    [Parameter(Mandatory)][string]$DevicePath,
    [string]$LocalPath = "",
    [string]$Package = "",
    [switch]$SkipPull,
    [switch]$SkipLaunch
)

$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Load phone-session helpers if available
# ---------------------------------------------------------------------------

$sessionScript = Join-Path $PSScriptRoot "phone-session.ps1"
if (Test-Path $sessionScript) {
    # Already dot-sourced if running through DbApkInstall
    if (-not (Get-Command Require-Adb -ErrorAction SilentlyContinue)) {
        . $sessionScript
    }
}

# ---------------------------------------------------------------------------
# Resolve ADB
# ---------------------------------------------------------------------------

function Get-AdbExe {
    if ($env:DB_ADB_PATH -and (Test-Path $env:DB_ADB_PATH)) { return $env:DB_ADB_PATH }
    $onPath = Get-Command adb -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    throw 'ADB not found. Run phone-session.ps1 first or set $env:DB_ADB_PATH.'
}

$adb = Get-AdbExe

# ---------------------------------------------------------------------------
# Check device
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "=== APK Install Helper ==="

$deviceLines = & $adb devices 2>&1 | Select-String "device$"
if (-not $deviceLines) {
    Write-Host "[fail] No authorized ADB device detected."
    Write-Host "  Connect phone, unlock, and tap Allow on USB debugging prompt."
    Write-Host "  Then run: adb devices"
    exit 1
}
$deviceId = ($deviceLines | Select-Object -First 1).ToString().Trim().Split()[0]
Write-Host "[adb] Device: $deviceId"

# ---------------------------------------------------------------------------
# Resolve local path
# ---------------------------------------------------------------------------

if (-not $LocalPath) {
    $apkName = Split-Path $DevicePath -Leaf
    $LocalPath = Join-Path $env:USERPROFILE "Downloads\$apkName"
}

# ---------------------------------------------------------------------------
# Pull APK from device
# ---------------------------------------------------------------------------

if (-not $SkipPull) {
    # Check if APK exists on device
    $deviceCheck = & $adb shell "ls '$DevicePath' 2>/dev/null" 2>&1
    if (-not $deviceCheck -or $deviceCheck -match "No such file") {
        Write-Host "[fail] APK not found on device at: $DevicePath"
        Write-Host "  Run db-apk-artifact-download on the phone first:"
        Write-Host "    PhoneCmd 'db-apk-artifact-download --repo indrolend/digital-breakdown-apk --run <id> --artifact <name> --out $DevicePath'"
        exit 1
    }

    Write-Host "[adb] Pulling APK from device: $DevicePath"
    Write-Host "      -> $LocalPath"
    & $adb pull $DevicePath $LocalPath
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[fail] adb pull failed."
        exit 1
    }
    Write-Host "[ok] APK pulled."
} else {
    if (-not (Test-Path $LocalPath)) {
        Write-Host "[fail] Local APK not found at: $LocalPath"
        exit 1
    }
    Write-Host "[skip] Using existing local APK: $LocalPath"
}

$apkSize = (Get-Item $LocalPath).Length
Write-Host "[info] APK size: $([math]::Round($apkSize / 1MB, 2)) MB"

# ---------------------------------------------------------------------------
# Install APK
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "[adb] Installing APK ..."
$installOutput = & $adb install -r $LocalPath 2>&1
$installCode = $LASTEXITCODE
Write-Host $installOutput

if ($installCode -ne 0) {
    if ($installOutput -match "INSTALL_FAILED_UPDATE_INCOMPATIBLE") {
        Write-Host "[warn] Install failed due to signature mismatch. Uninstalling existing app ..."
        if ($Package) {
            & $adb uninstall $Package | Out-Null
            Write-Host "[adb] Re-installing after uninstall ..."
            $installOutput = & $adb install -r $LocalPath 2>&1
            $installCode = $LASTEXITCODE
            Write-Host $installOutput
        } else {
            Write-Host "[fail] Cannot uninstall: -Package not specified."
            exit 1
        }
    }
    if ($installCode -ne 0) {
        Write-Host "[fail] Install failed. See output above."
        Write-Host "  Next steps:"
        Write-Host "    adb install -r '$LocalPath'"
        exit 1
    }
}

Write-Host "[ok] APK installed successfully."

# ---------------------------------------------------------------------------
# Launch app
# ---------------------------------------------------------------------------

if (-not $SkipLaunch -and $Package) {
    Write-Host ""
    Write-Host "[adb] Launching $Package ..."
    $launchOutput = & $adb shell monkey -p $Package 1 2>&1
    Write-Host $launchOutput
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[ok] Launch command sent."
        Write-Host "  Wait a few seconds, then capture evidence:"
        Write-Host "    . .\db-apk-evidence.ps1 -Package $Package"
    } else {
        Write-Host "[warn] Launch command returned non-zero. App may still have launched."
    }
} elseif (-not $Package) {
    Write-Host "[skip] No -Package specified; skipping launch."
}

Write-Host ""
Write-Host "=== Install complete ==="
