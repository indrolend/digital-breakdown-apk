#Requires -Version 5.1
<#
.SYNOPSIS
    Windows-side logcat and screenshot evidence capture.

.DESCRIPTION
    After installing and launching an APK, captures:
      - filtered logcat (DBNATIVE + crash markers)
      - full logcat
      - screenshot
      - result.json summary
    All saved to a timestamped evidence folder under Downloads\db-demo-evidence\.

.PARAMETER Package
    Android package name (e.g. com.indrolend.digitalbreakdown.native).

.PARAMETER EvidenceDir
    Override evidence output directory. Default: Downloads\db-demo-evidence\TIMESTAMP\.

.PARAMETER ScreenshotDevice
    Device-side path to save the screenshot. Default: /sdcard/Download/db-apks/screen.png

.PARAMETER SkipScreenshot
    Skip screenshot capture.

.PARAMETER SkipLogcat
    Skip logcat capture.

.EXAMPLE
    .\db-apk-evidence.ps1 -Package com.indrolend.digitalbreakdown.native

.EXAMPLE
    # Called from db-apk-demo.ps1 with an evidence directory already set
    .\db-apk-evidence.ps1 -Package com.indrolend.digitalbreakdown.native -EvidenceDir C:\...\evidence\pr2-20250101-120000
#>

param(
    [Parameter(Mandatory)][string]$Package,
    [string]$EvidenceDir = "",
    [string]$ScreenshotDevice = "/sdcard/Download/db-apks/screen.png",
    [switch]$SkipScreenshot,
    [switch]$SkipLogcat
)

$ErrorActionPreference = "Stop"

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
# Evidence directory
# ---------------------------------------------------------------------------

if (-not $EvidenceDir) {
    $ts = Get-Date -Format "yyyyMMdd-HHmmss"
    $pkgSafe = $Package -replace "[^a-zA-Z0-9\.\-_]", "-"
    $EvidenceDir = Join-Path $env:USERPROFILE "Downloads\db-demo-evidence\$pkgSafe-$ts"
}

New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null
Write-Host ""
Write-Host "=== APK Evidence Capture ==="
Write-Host "[evidence] Saving to: $EvidenceDir"

# ---------------------------------------------------------------------------
# Check device
# ---------------------------------------------------------------------------

$deviceLines = & $adb devices 2>&1 | Select-String "device$"
if (-not $deviceLines) {
    Write-Host "[fail] No ADB device. Connect phone."
    exit 1
}
$deviceId = ($deviceLines | Select-Object -First 1).ToString().Trim().Split()[0]
Write-Host "[adb] Device: $deviceId"

# Result accumulator
$result = [ordered]@{
    adb           = "pass"
    package       = $Package
    timestamp     = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    logcat        = "skipped"
    dbnativeLogs  = "skipped"
    crashScan     = "skipped"
    screenshot    = "skipped"
    evidenceDir   = $EvidenceDir
}
$validResultStates = @("pass", "fail", "skipped", "pending")
$nonStatusKeys = @("package", "timestamp", "evidenceDir")

# ---------------------------------------------------------------------------
# Logcat capture
# ---------------------------------------------------------------------------

if (-not $SkipLogcat) {
    Write-Host ""
    Write-Host "[logcat] Capturing logcat ..."

    # Give app time to run/crash if just launched
    Start-Sleep -Seconds 3

    $logcatFull = & $adb logcat -d -v time 2>&1
    $logcatFullPath = Join-Path $EvidenceDir "logcat-full.txt"
    $logcatFull | Set-Content -Encoding UTF8 $logcatFullPath
    Write-Host "[ok] Full logcat: $logcatFullPath ($($logcatFull.Count) lines)"

    # Filtered: DBNATIVE + crash markers
    $filterPattern = "DBNATIVE|AndroidRuntime|FATAL|crash|$Package"
    $logcatFiltered = $logcatFull | Select-String -Pattern $filterPattern -AllMatches
    $logcatFilteredPath = Join-Path $EvidenceDir "logcat-filtered.txt"
    $logcatFiltered | ForEach-Object { $_.Line } | Set-Content -Encoding UTF8 $logcatFilteredPath
    Write-Host "[ok] Filtered logcat: $logcatFilteredPath ($($logcatFiltered.Count) lines)"

    $result["logcat"] = "pass"

    # DBNATIVE frames check
    $dbnativeLines = $logcatFull | Select-String "DBNATIVE"
    if ($dbnativeLines) {
        Write-Host "[ok] DBNATIVE log entries found: $($dbnativeLines.Count)"
        $result["dbnativeLogs"] = "pass"
        $dbnativeLines | ForEach-Object { $_.Line } | Set-Content -Encoding UTF8 (Join-Path $EvidenceDir "logcat-dbnative.txt")
    } else {
        Write-Host "[warn] No DBNATIVE log entries found."
        $result["dbnativeLogs"] = "fail"
    }

    # Crash scan
    $crashPattern = "FATAL EXCEPTION|AndroidRuntime.*FATAL|ANR in|am_crash|Process.*has died|Fatal signal|Force finishing"
    $crashLines = $logcatFull | Select-String -Pattern $crashPattern -AllMatches
    # Exclude known browser/system noise
    $appCrashLines = $crashLines | Where-Object { $_.Line -notmatch "com\.android\.chrome|cr_CrashFileManager" }
    if ($appCrashLines) {
        Write-Host "[warn] Crash/fatal markers found in logcat:"
        $appCrashLines | ForEach-Object { Write-Host "  $($_.Line)" }
        $appCrashLines | ForEach-Object { $_.Line } | Set-Content -Encoding UTF8 (Join-Path $EvidenceDir "logcat-crashes.txt")
        $result["crashScan"] = "fail"
    } else {
        Write-Host "[ok] No app crash markers found."
        $result["crashScan"] = "pass"
    }
}

# ---------------------------------------------------------------------------
# Screenshot
# ---------------------------------------------------------------------------

if (-not $SkipScreenshot) {
    Write-Host ""
    Write-Host "[screenshot] Capturing screen ..."

    & $adb shell screencap -p $ScreenshotDevice 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[warn] screencap failed."
        $result["screenshot"] = "fail"
    } else {
        $localScreen = Join-Path $EvidenceDir "screen.png"
        & $adb pull $ScreenshotDevice $localScreen 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0 -and (Test-Path $localScreen)) {
            $screenSize = (Get-Item $localScreen).Length
            Write-Host "[ok] Screenshot saved: $localScreen ($([math]::Round($screenSize / 1KB, 1)) KB)"
            $result["screenshot"] = "pass"
            # Clean up device-side screenshot
            & $adb shell rm -f $ScreenshotDevice 2>&1 | Out-Null
        } else {
            Write-Host "[warn] Failed to pull screenshot."
            $result["screenshot"] = "fail"
        }
    }
}

# ---------------------------------------------------------------------------
# Write result.json
# ---------------------------------------------------------------------------

$resultJson = $result | ConvertTo-Json -Depth 3
$resultJson | Set-Content -Encoding UTF8 (Join-Path $EvidenceDir "result.json")
Write-Host ""
Write-Host "[evidence] result.json:"
Write-Host $resultJson

Write-Host ""
Write-Host "=== Evidence capture complete ==="
Write-Host "  Folder: $EvidenceDir"

# Return overall pass/fail
$invalidStates = @($result.Keys | Where-Object {
    ($_ -notin $nonStatusKeys) -and ($result[$_] -notin $validResultStates)
})
$failures = @($result.Keys | Where-Object {
    ($_ -notin $nonStatusKeys) -and ($result[$_] -eq "fail")
})
if ($invalidStates.Count -gt 0) {
    Write-Host "[warn] Invalid result states: $($invalidStates -join ', ')"
    exit 1
}
if ($failures.Count -gt 0) {
    Write-Host "[warn] Some checks failed: $($failures -join ', ')"
    exit 1
}
