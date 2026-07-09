#Requires -Version 5.1
<#
.SYNOPSIS
    Capture bounded APK evidence and store it on phone-first paths.
#>

param(
    [Parameter(Mandatory)][string]$Package,
    [string]$PhoneEvidenceDir = "/sdcard/Download/db-control/evidence/latest",
    [string]$WindowsEvidenceDir = "",
    [string]$HostEvidenceDir = "",
    [Alias("PullEvidenceToHost")]
    [switch]$PullEvidenceToWindows,
    [string]$ScreenshotDevice = "/sdcard/Download/db-control/evidence/latest/screen.png",
    [switch]$SkipScreenshot,
    [switch]$SkipLogcat
)

$ErrorActionPreference = "Stop"
$adb = $null
$statusValues = @("pass", "fail", "skipped", "warning", "pending")

function Get-DbHostHome {
    if ($env:USERPROFILE) { return $env:USERPROFILE }
    if ($HOME) { return $HOME }
    return [Environment]::GetFolderPath("UserProfile")
}

function Get-DbHostDownloads {
    return (Join-Path (Get-DbHostHome) "Downloads")
}

function Get-DbHostTemp {
    return [System.IO.Path]::GetTempPath()
}

if (-not $HostEvidenceDir -and $WindowsEvidenceDir) {
    $HostEvidenceDir = $WindowsEvidenceDir
}
if (-not $HostEvidenceDir) {
    $HostEvidenceDir = Join-Path (Join-Path (Get-DbHostDownloads) "db-control") "evidence-latest"
}
# Keep the legacy variable populated for old callers and result readers.
$WindowsEvidenceDir = $HostEvidenceDir

$result = [ordered]@{
    adb                 = "pending"
    package             = $Package
    timestamp           = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    logcat              = "skipped"
    dbnativeLogs        = "skipped"
    crashScan           = "skipped"
    screenshot          = "skipped"
    evidenceDir         = $PhoneEvidenceDir
    evidencePathHost    = if ($PullEvidenceToWindows) { $HostEvidenceDir } else { "" }
    evidencePathWindows = if ($PullEvidenceToWindows) { $WindowsEvidenceDir } else { "" }
    errorSummary        = @()
}

function Add-ResultError {
    param([string]$Message)
    if ($Message) {
        $result.errorSummary = @($result.errorSummary + $Message)
    }
}

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

function Invoke-AdbArgs {
    param([Parameter(Mandatory)][string[]]$AdbArgs)
    $out = & { $ErrorActionPreference = 'Continue'; & $adb @AdbArgs 2>&1 }
    [PSCustomObject]@{ Output = $out; ExitCode = $LASTEXITCODE }
}

function Write-PhoneTextFile {
    param(
        [Parameter(Mandatory)][string]$PhonePath,
        [Parameter(Mandatory)][string[]]$Lines
    )
    $tmp = Join-Path (Get-DbHostTemp) ("db-control-" + [guid]::NewGuid().ToString() + ".txt")
    try {
        $Lines | Set-Content -Encoding UTF8 $tmp
        $push = Invoke-Adb push $tmp $PhonePath
        if ($push.ExitCode -ne 0) {
            throw "adb push failed for $PhonePath"
        }
    } finally {
        if (Test-Path $tmp) { Remove-Item -Force $tmp }
    }
}

function Save-Result {
    try {
        $result.timestamp = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
        $json = $result | ConvertTo-Json -Depth 5
        Write-PhoneTextFile -PhonePath "$PhoneEvidenceDir/result.json" -Lines @($json)
    } catch {
        Write-Host "[warn] Could not write phone result.json: $_"
    }
}

$exitCode = 0

try {
    $adb = Get-AdbExe

    $dev = Invoke-Adb devices
    $deviceLines = $dev.Output | Select-String "device$"
    if (-not $deviceLines) {
        throw "No ADB device connected."
    }
    $result["adb"] = "pass"

    $mk = Invoke-Adb shell "mkdir -p '$PhoneEvidenceDir'"
    if ($mk.ExitCode -ne 0) {
        throw "Could not create phone evidence directory: $PhoneEvidenceDir"
    }

    if (-not $SkipLogcat) {
        Write-Host "[logcat] Capturing bounded dump with adb logcat -d ..."
        $log = Invoke-Adb logcat -d -v time
        if ($log.ExitCode -ne 0) {
            $result["logcat"] = "fail"
            Add-ResultError "adb logcat -d failed."
        } else {
            $fullLines = @($log.Output | ForEach-Object { "$_" })
            Write-PhoneTextFile -PhonePath "$PhoneEvidenceDir/logcat-full.txt" -Lines $fullLines

            $filterPattern = "DBNATIVE|AndroidRuntime|FATAL|crash|$Package"
            $filtered = $fullLines | Select-String -Pattern $filterPattern -AllMatches | ForEach-Object { $_.Line }
            if (-not $filtered) { $filtered = @() }
            Write-PhoneTextFile -PhonePath "$PhoneEvidenceDir/logcat-filtered.txt" -Lines $filtered
            $result["logcat"] = "pass"

            $dbnative = $fullLines | Select-String -Pattern "DBNATIVE" | ForEach-Object { $_.Line }
            if ($dbnative -and $dbnative.Count -gt 0) {
                Write-PhoneTextFile -PhonePath "$PhoneEvidenceDir/logcat-dbnative.txt" -Lines $dbnative
                $result["dbnativeLogs"] = "pass"
            } else {
                $result["dbnativeLogs"] = "warning"
                Write-PhoneTextFile -PhonePath "$PhoneEvidenceDir/logcat-dbnative.txt" -Lines @("[warning] No DBNATIVE lines found.")
            }

            $crashPattern = "FATAL EXCEPTION|AndroidRuntime.*FATAL|ANR in|am_crash|Process.*has died|Fatal signal|Force finishing"
            $crashLines = $fullLines | Select-String -Pattern $crashPattern -AllMatches | ForEach-Object { $_.Line }
            # Exclude known browser-process noise so package-under-test crash signals stay actionable.
            $appCrashLines = $crashLines | Where-Object { $_ -notmatch "com\.android\.chrome|cr_CrashFileManager" }
            if ($appCrashLines -and $appCrashLines.Count -gt 0) {
                Write-PhoneTextFile -PhonePath "$PhoneEvidenceDir/logcat-crashes.txt" -Lines $appCrashLines
                $result["crashScan"] = "fail"
            } else {
                Write-PhoneTextFile -PhonePath "$PhoneEvidenceDir/logcat-crashes.txt" -Lines @()
                $result["crashScan"] = "pass"
            }
        }
    }

    if (-not $SkipScreenshot) {
        # Pass -p through an explicit string array so PowerShell never treats it as a script parameter.
        $shot = Invoke-AdbArgs -AdbArgs @("shell", "screencap", "-p", $ScreenshotDevice)
        if ($shot.ExitCode -eq 0) {
            $check = Invoke-Adb shell "ls '$ScreenshotDevice' 2>/dev/null"
            if ($check.ExitCode -eq 0) {
                $result["screenshot"] = "pass"
            } else {
                $result["screenshot"] = "fail"
                Add-ResultError "Screenshot file missing on phone after screencap."
            }
        } else {
            $result["screenshot"] = "fail"
            Add-ResultError "adb shell screencap failed."
        }
    }

    if ($PullEvidenceToWindows) {
        New-Item -ItemType Directory -Force -Path $HostEvidenceDir | Out-Null
        Remove-Item -Recurse -Force -ErrorAction SilentlyContinue (Join-Path $HostEvidenceDir "*")
        $pull = Invoke-Adb pull "$PhoneEvidenceDir/." $HostEvidenceDir
        if ($pull.ExitCode -ne 0) {
            Add-ResultError "Failed to mirror evidence to host."
        }
    }

} catch {
    $exitCode = 1
    Add-ResultError $_.Exception.Message
    if ($result["adb"] -eq "pending") { $result["adb"] = "fail" }
    if ($result["logcat"] -eq "skipped" -and -not $SkipLogcat) { $result["logcat"] = "fail" }
    if ($result["screenshot"] -eq "skipped" -and -not $SkipScreenshot) { $result["screenshot"] = "fail" }
} finally {
    foreach ($k in @("adb", "logcat", "dbnativeLogs", "crashScan", "screenshot")) {
        if ($result[$k] -eq "pending") { $result[$k] = "warning" }
        if ($result[$k] -notin $statusValues) { $result[$k] = "warning" }
    }
    Save-Result
}

Write-Host ($result | ConvertTo-Json -Depth 5)
if ($exitCode -ne 0) { exit $exitCode }
if ($result["logcat"] -eq "fail" -or $result["screenshot"] -eq "fail") { exit 1 }
exit 0
