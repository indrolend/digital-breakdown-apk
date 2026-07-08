#Requires -Version 5.1
<#
.SYNOPSIS
    Install APK from stable phone path with optional Windows pull.
#>

param(
    [string]$DevicePath = "/sdcard/Download/db-control/apks/current.apk",
    [string]$LocalPath = "",
    [string]$Package = "",
    [switch]$SkipPull,
    [switch]$SkipLaunch
)

$ErrorActionPreference = "Stop"

function Get-AdbExe {
    if ($env:DB_ADB_PATH -and (Test-Path $env:DB_ADB_PATH)) { return $env:DB_ADB_PATH }
    $onPath = Get-Command adb -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    throw 'ADB not found. Run phone-session.ps1 first or set $env:DB_ADB_PATH.'
}

function Invoke-Adb {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$AdbArgs)
    $out = & { $ErrorActionPreference = 'Continue'; & $adb @AdbArgs 2>&1 }
    [PSCustomObject]@{ Output = $out; ExitCode = $LASTEXITCODE }
}

$adb = Get-AdbExe

Write-Host ""
Write-Host "=== APK Install Helper (Phone-First) ==="

$r = Invoke-Adb devices
$deviceLines = $r.Output | Select-String "device$"
if (-not $deviceLines) {
    Write-Host "[fail] No authorized ADB device detected."
    exit 1
}

$exists = Invoke-Adb shell "ls '$DevicePath' 2>/dev/null"
if ($exists.ExitCode -ne 0 -or ($exists.Output -join "`n") -match "No such file") {
    Write-Host "[fail] APK not found at phone path: $DevicePath"
    exit 1
}

if (-not $SkipPull) {
    if (-not $LocalPath) {
        $LocalPath = Join-Path $env:TEMP ("db-control-" + (Split-Path $DevicePath -Leaf))
    }
    $pull = Invoke-Adb pull $DevicePath $LocalPath
    if ($pull.ExitCode -ne 0) {
        Write-Host "[warn] adb pull failed; continuing with phone-side install."
    } else {
        Write-Host "[ok] Optional local mirror: $LocalPath"
    }
}

Write-Host "[adb] Installing from phone path: $DevicePath"
$install = Invoke-Adb shell pm install -r "$DevicePath"
$installText = $install.Output -join "`n"
Write-Host $installText

if ($install.ExitCode -ne 0 -and $installText -match "INSTALL_FAILED_UPDATE_INCOMPATIBLE" -and $Package) {
    Invoke-Adb uninstall $Package | Out-Null
    $install = Invoke-Adb shell pm install -r "$DevicePath"
    $installText = $install.Output -join "`n"
    Write-Host $installText
}

if ($install.ExitCode -ne 0 -and $installText -notmatch "Success") {
    Write-Host "[fail] Install failed."
    exit 1
}

Write-Host "[ok] Install complete."

if (-not $SkipLaunch -and $Package) {
    $launch = Invoke-Adb shell monkey -p $Package 1
    if ($launch.ExitCode -eq 0) {
        Write-Host "[ok] Launch command sent."
    } else {
        Write-Host "[warn] Launch returned non-zero."
    }
}

Write-Host "=== Install complete ==="
