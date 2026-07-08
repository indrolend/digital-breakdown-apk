#Requires -Version 5.1
<#
.SYNOPSIS
    Full APK demo runner: download artifact -> pull -> install -> launch -> evidence.

.DESCRIPTION
    Orchestrates the end-to-end smoke test workflow:
      1. (Optional) Ask Termux to download the APK from GitHub Actions via gh.
      2. Pull the APK from phone shared storage to Windows.
      3. Install the APK via adb.
      4. Launch the package.
      5. Capture logcat and screenshot evidence.
      6. Write a result.json summary.

    This script is meant to be called from phone-session.ps1 via DbApkDemo,
    or directly as a standalone script.

.PARAMETER RunId
    GitHub Actions run ID containing the artifact.

.PARAMETER ArtifactName
    Name of the artifact (e.g. digital-breakdown-native-debug-apk).

.PARAMETER Package
    Android package name (e.g. com.indrolend.digitalbreakdown.native).

.PARAMETER OutName
    APK filename to use on the device and locally (default: app-debug.apk).

.PARAMETER DeviceApkDir
    Device-side directory for APKs (default: /sdcard/Download/db-apks).

.PARAMETER SkipDownload
    Skip the Termux gh download step (use if APK is already on the phone).

.PARAMETER SkipInstall
    Skip install/launch; only capture evidence.

.PARAMETER LocalApkPath
    Provide a local APK path to skip both Termux download and adb pull.

.EXAMPLE
    # Full run from phone-session.ps1:
    DbApkDemo -RunId 28961104004 `
              -ArtifactName digital-breakdown-native-debug-apk `
              -Package com.indrolend.digitalbreakdown.native

.EXAMPLE
    # Skip download (APK already on phone):
    DbApkDemo -RunId 28961104004 `
              -ArtifactName digital-breakdown-native-debug-apk `
              -Package com.indrolend.digitalbreakdown.native `
              -SkipDownload

.EXAMPLE
    # Local APK only (no Termux needed):
    .\db-apk-demo.ps1 `
        -RunId 0 `
        -ArtifactName unused `
        -Package com.indrolend.digitalbreakdown.native `
        -LocalApkPath "$env:USERPROFILE\Downloads\app-debug.apk"
#>

param(
    [Parameter(Mandatory)][string]$RunId,
    [Parameter(Mandatory)][string]$ArtifactName,
    [Parameter(Mandatory)][string]$Package,
    [string]$OutName          = "app-debug.apk",
    [string]$DeviceApkDir     = "/sdcard/Download/db-apks",
    [switch]$SkipDownload,
    [switch]$SkipInstall,
    [string]$LocalApkPath     = ""
)

$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot

# ---------------------------------------------------------------------------
# Load phone-session if not already loaded
# ---------------------------------------------------------------------------

$sessionScript = Join-Path $ScriptDir "phone-session.ps1"
if ((Test-Path $sessionScript) -and (-not (Get-Command Require-Adb -ErrorAction SilentlyContinue))) {
    . $sessionScript
}

function Get-AdbExe {
    if ($env:DB_ADB_PATH -and (Test-Path $env:DB_ADB_PATH)) { return $env:DB_ADB_PATH }
    $onPath = Get-Command adb -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    throw 'ADB not found. Run phone-session.ps1 first or set $env:DB_ADB_PATH.'
}

# ---------------------------------------------------------------------------
# Evidence directory (shared across sub-scripts)
# ---------------------------------------------------------------------------

$ts = Get-Date -Format "yyyyMMdd-HHmmss"
$pkgShort = $Package -replace "com\.indrolend\.", "" -replace "\.", "-"
$EvidenceDir = Join-Path $env:USERPROFILE "Downloads\db-demo-evidence\$pkgShort-$ts"
New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null

# Accumulate results
$result = [ordered]@{
    adb          = "pending"
    ssh          = "skipped"
    ghAuth       = "skipped"
    download     = "skipped"
    pull         = "skipped"
    install      = "skipped"
    launch       = "skipped"
    dbnativeLogs = "skipped"
    crashScan    = "skipped"
    screenshot   = "skipped"
    apkPath      = ""
    package      = $Package
    runId        = $RunId
    artifactName = $ArtifactName
    evidenceDir  = $EvidenceDir
    timestamp    = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
}

function Save-Result {
    $result | ConvertTo-Json -Depth 3 | Set-Content -Encoding UTF8 (Join-Path $EvidenceDir "result.json")
}

Write-Host ""
Write-Host "=== Digital Breakdown APK Demo ==="
Write-Host "[evidence] $EvidenceDir"

# ---------------------------------------------------------------------------
# Step 1: ADB check
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "--- Step 1: ADB check ---"
try {
    $adb = Get-AdbExe
    $deviceLines = & $adb devices 2>&1 | Select-String "device$"
    if ($deviceLines) {
        $deviceId = ($deviceLines | Select-Object -First 1).ToString().Trim().Split()[0]
        Write-Host "[ok] ADB device: $deviceId"
        $result["adb"] = "pass"
    } else {
        Write-Host "[fail] No ADB device connected."
        $result["adb"] = "fail"
        Save-Result
        exit 1
    }
} catch {
    Write-Host "[fail] ADB error: $_"
    $result["adb"] = "fail"
    Save-Result
    exit 1
}

# ---------------------------------------------------------------------------
# Step 2: Termux download via gh (unless skipped or local APK provided)
# ---------------------------------------------------------------------------

$DeviceApkPath = "$DeviceApkDir/$OutName"

if ($LocalApkPath) {
    Write-Host ""
    Write-Host "--- Step 2: Using local APK (skipping Termux download and pull) ---"
    if (-not (Test-Path $LocalApkPath)) {
        Write-Host "[fail] Local APK not found at: $LocalApkPath"
        $result["download"] = "fail"
        Save-Result
        exit 1
    }
    Write-Host "[ok] Using local APK: $LocalApkPath"
    $result["download"] = "skipped"
    $result["pull"] = "skipped"
    $result["apkPath"] = $LocalApkPath

} elseif ($SkipDownload) {
    Write-Host ""
    Write-Host "--- Step 2: Skipping Termux download (APK should already be on phone) ---"
    $result["download"] = "skipped"

    # Step 3: Pull from device
    Write-Host ""
    Write-Host "--- Step 3: Pulling APK from phone ---"
    $LocalApkPath = Join-Path $env:USERPROFILE "Downloads\$OutName"
    try {
        $deviceCheck = & $adb shell "ls '$DeviceApkPath' 2>/dev/null" 2>&1
        if (-not $deviceCheck -or $deviceCheck -match "No such file") {
            Write-Host "[fail] APK not found on device at: $DeviceApkPath"
            Write-Host "  Run in Termux: db-apk-artifact-download --repo indrolend/digital-breakdown-apk --run $RunId --artifact $ArtifactName --out $DeviceApkPath"
            $result["pull"] = "fail"
            Save-Result
            exit 1
        }
        & $adb pull $DeviceApkPath $LocalApkPath
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[ok] Pulled: $LocalApkPath"
            $result["pull"] = "pass"
            $result["apkPath"] = $LocalApkPath
        } else {
            Write-Host "[fail] adb pull failed."
            $result["pull"] = "fail"
            Save-Result
            exit 1
        }
    } catch {
        Write-Host "[fail] Pull error: $_"
        $result["pull"] = "fail"
        Save-Result
        exit 1
    }

} else {
    # Full path: Termux download -> pull
    Write-Host ""
    Write-Host "--- Step 2: Termux gh download ---"

    # Check SSH reachability
    $sshReady = $false
    try {
        $user = Get-TermuxUser
        if ($user) {
            Start-SshForward
            $sshTest = ssh -p 8022 -o StrictHostKeyChecking=no -o ConnectTimeout=5 "$user@127.0.0.1" "echo ssh_ok" 2>&1
            if ($sshTest -match "ssh_ok") {
                $sshReady = $true
                $result["ssh"] = "pass"
                Write-Host "[ok] SSH reachable as $user"
            } else {
                $result["ssh"] = "fail"
                Write-Host "[warn] SSH not responding."
            }
        }
    } catch {
        $result["ssh"] = "fail"
        Write-Host "[warn] SSH error: $_"
    }

    if ($sshReady) {
        # Check gh auth
        try {
            $ghStatus = ssh -p 8022 -o StrictHostKeyChecking=no "$user@127.0.0.1" "gh auth status 2>&1" 2>&1
            if ($ghStatus -match "Logged in") {
                $result["ghAuth"] = "pass"
                Write-Host "[ok] gh auth active in Termux"
            } else {
                $result["ghAuth"] = "fail"
                Write-Host "[warn] gh not authenticated in Termux."
                Write-Host "  Run in Termux: gh auth login"
            }
        } catch {
            $result["ghAuth"] = "fail"
        }

        if ($result["ghAuth"] -eq "pass") {
            # Check if APK already on device
            $deviceHasApk = $false
            try {
                $deviceCheck = & $adb shell "ls '$DeviceApkPath' 2>/dev/null" 2>&1
                if ($deviceCheck -and $deviceCheck -notmatch "No such file") {
                    Write-Host "[info] APK already exists on device: $DeviceApkPath"
                    $read = Read-Host "Re-download? [y/N]"
                    if ($read -notmatch "^[Yy]$") {
                        $deviceHasApk = $true
                        $result["download"] = "skipped"
                        Write-Host "[skip] Using existing APK on device."
                    }
                }
            } catch { }

            if (-not $deviceHasApk) {
                # Run download in Termux
                $downloadCmd = "db-apk-artifact-download --repo indrolend/digital-breakdown-apk --run $RunId --artifact $ArtifactName --out $DeviceApkPath"
                Write-Host "[ssh] Running in Termux: $downloadCmd"
                ssh -p 8022 -o StrictHostKeyChecking=no "$user@127.0.0.1" $downloadCmd
                if ($LASTEXITCODE -eq 0) {
                    $result["download"] = "pass"
                    Write-Host "[ok] Termux download complete."
                } else {
                    $result["download"] = "fail"
                    Write-Host "[fail] Termux download failed."
                    Write-Host "  Try manually: PhoneCmd 'db-apk-artifact-download --repo ... --run $RunId --artifact $ArtifactName --out $DeviceApkPath'"
                    Save-Result
                    exit 1
                }
            }
        } else {
            Write-Host "[warn] Skipping Termux download due to gh auth failure."
            Write-Host "  Fallback: provide local APK with -LocalApkPath or authenticate gh in Termux."
            $result["download"] = "fail"
            Save-Result
            exit 1
        }
    } else {
        Write-Host "[warn] SSH not available. Cannot trigger Termux download."
        Write-Host "  Fallbacks:"
        Write-Host "    1. Run in Termux manually: db-apk-artifact-download --run $RunId --artifact $ArtifactName --out $DeviceApkPath"
        Write-Host "    2. Or re-run with -SkipDownload once the APK is on the phone."
        Write-Host "    3. Or provide -LocalApkPath with a Windows-side APK."
        $result["download"] = "fail"
        Save-Result
        exit 1
    }

    # Step 3: Pull
    Write-Host ""
    Write-Host "--- Step 3: Pulling APK from phone ---"
    $LocalApkPath = Join-Path $env:USERPROFILE "Downloads\$OutName"
    try {
        & $adb pull $DeviceApkPath $LocalApkPath
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[ok] Pulled: $LocalApkPath"
            $result["pull"] = "pass"
            $result["apkPath"] = $LocalApkPath
        } else {
            Write-Host "[fail] adb pull failed."
            $result["pull"] = "fail"
            Save-Result
            exit 1
        }
    } catch {
        Write-Host "[fail] Pull error: $_"
        $result["pull"] = "fail"
        Save-Result
        exit 1
    }
}

# Write apk-source.txt
"Run ID: $RunId`nArtifact: $ArtifactName`nDevice path: $DeviceApkPath`nLocal path: $LocalApkPath" |
    Set-Content -Encoding UTF8 (Join-Path $EvidenceDir "apk-source.txt")

# ---------------------------------------------------------------------------
# Step 4: Install + Launch
# ---------------------------------------------------------------------------

if (-not $SkipInstall) {
    Write-Host ""
    Write-Host "--- Step 4: Install ---"
    $installScript = Join-Path $ScriptDir "db-apk-install.ps1"
    if (Test-Path $installScript) {
        try {
            & $installScript -DevicePath $DeviceApkPath -LocalPath $LocalApkPath -Package $Package -SkipPull -ErrorAction Stop
            $result["install"] = "pass"
            $result["launch"] = "pass"
            Write-Host "[ok] Install and launch complete."
        } catch {
            Write-Host "[warn] Install/launch step had an error: $_"
            $result["install"] = "fail"
        }
    } else {
        # Inline install if script not available
        Write-Host "[info] Running inline install ..."
        & $adb install -r $LocalApkPath 2>&1 | Tee-Object -Variable installOut
        if ($LASTEXITCODE -eq 0) {
            $result["install"] = "pass"
            $installOut | Set-Content -Encoding UTF8 (Join-Path $EvidenceDir "install.txt")
        } else {
            $result["install"] = "fail"
        }

        if ($result["install"] -eq "pass") {
            Write-Host "[adb] Launching $Package ..."
            & $adb shell monkey -p $Package 1 2>&1 | Tee-Object -Variable launchOut
            $result["launch"] = if ($LASTEXITCODE -eq 0) { "pass" } else { "fail" }
            $launchOut | Set-Content -Encoding UTF8 (Join-Path $EvidenceDir "launch.txt")
        }
    }
}

# ---------------------------------------------------------------------------
# Step 5: Evidence
# ---------------------------------------------------------------------------

Write-Host ""
Write-Host "--- Step 5: Evidence capture ---"
$evidenceScript = Join-Path $ScriptDir "db-apk-evidence.ps1"
if (Test-Path $evidenceScript) {
    try {
        & $evidenceScript -Package $Package -EvidenceDir $EvidenceDir -ErrorAction Stop
        # Merge evidence results
        $evidenceResultPath = Join-Path $EvidenceDir "result.json"
        if (Test-Path $evidenceResultPath) {
            $evidenceResult = Get-Content $evidenceResultPath | ConvertFrom-Json
            $result["dbnativeLogs"] = $evidenceResult.dbnativeLogs
            $result["crashScan"]    = $evidenceResult.crashScan
            $result["screenshot"]   = $evidenceResult.screenshot
            $result["logcat"]       = $evidenceResult.logcat
        }
    } catch {
        Write-Host "[warn] Evidence capture error: $_"
    }
}

# ---------------------------------------------------------------------------
# Final result.json
# ---------------------------------------------------------------------------

Save-Result
Write-Host ""
Write-Host "=== Demo Run Complete ==="
Write-Host "[result] $EvidenceDir"
Write-Host ""

$result | ConvertTo-Json | Write-Host

$failures = @($result.Keys | Where-Object {
    $result[$_] -notin @("pass", "skipped", "pending") -and
    $_ -notin @("apkPath", "package", "runId", "artifactName", "evidenceDir", "timestamp")
})
if ($failures.Count -gt 0) {
    Write-Host ""
    Write-Host "[warn] Failed checks: $($failures -join ', ')"
    exit 1
}

Write-Host "[ok] All checks passed."
