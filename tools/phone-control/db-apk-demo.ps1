#Requires -Version 5.1
<#
.SYNOPSIS
    Phone-first APK demo runner: download -> install -> launch -> evidence.

.DESCRIPTION
    Uses the phone as the runtime source of truth under:
      /sdcard/Download/db-control/
        apks/current.apk
        evidence/latest/
        evidence/archive/YYYYMMDD-HHMMSS/

    The host computer, Windows or macOS, acts as controller and optional
    evidence mirror only.
#>

param(
    [Parameter(Mandatory)][string]$RunId,
    [Parameter(Mandatory)][string]$ArtifactName,
    [Parameter(Mandatory)][string]$Package,
    [string]$OutName = "current.apk",
    [string]$DeviceApkDir = "/sdcard/Download/db-control/apks",
    [switch]$SkipDownload,
    [Alias("SkipInstall")]
    [switch]$EvidenceOnly,
    [string]$LocalApkPath = "",
    [Alias("PullEvidenceToHost")]
    [switch]$PullEvidenceToWindows,
    [switch]$NoPullEvidence,
    [switch]$ArchivePreviousEvidence,
    [switch]$CleanBeforeRun,
    [int]$KeepEvidenceCount = 10
)

$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot
$adb = $null

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

$PhoneRoot = "/sdcard/Download/db-control"
$PhoneApkPath = "$DeviceApkDir/$OutName"
$PhoneEvidenceRoot = "$PhoneRoot/evidence"
$PhoneEvidenceLatest = "$PhoneEvidenceRoot/latest"
$PhoneEvidenceArchive = "$PhoneEvidenceRoot/archive"
$HostControlRoot = Join-Path (Get-DbHostDownloads) "db-control"
$HostEvidenceLatest = Join-Path $HostControlRoot "evidence-latest"
$ShouldPullEvidence = $PullEvidenceToWindows -and (-not $NoPullEvidence)

$statusValues = @("pass", "fail", "skipped", "warning", "pending")
$result = [ordered]@{
    adb                 = "pending"
    ssh                 = "skipped"
    ghAuth              = "skipped"
    download            = "skipped"
    pull                = "skipped"
    install             = "skipped"
    launch              = "skipped"
    evidence            = "pending"
    dbnativeLogs        = "skipped"
    crashScan           = "skipped"
    screenshot          = "skipped"
    apkPathPhone        = $PhoneApkPath
    apkPathWindows      = ""
    apkPathHost         = ""
    evidencePathPhone   = $PhoneEvidenceLatest
    evidencePathWindows = if ($ShouldPullEvidence) { $HostEvidenceLatest } else { "" }
    evidencePathHost    = if ($ShouldPullEvidence) { $HostEvidenceLatest } else { "" }
    package             = $Package
    runId               = $RunId
    artifactName        = $ArtifactName
    timestamp           = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    logcatCleared       = "skipped"
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
        $json = $result | ConvertTo-Json -Depth 5
        Write-PhoneTextFile -PhonePath "$PhoneEvidenceLatest/result.json" -Lines @($json)
        if ($ShouldPullEvidence) {
            New-Item -ItemType Directory -Force -Path $HostEvidenceLatest | Out-Null
            $null = Invoke-Adb pull "$PhoneEvidenceLatest/result.json" (Join-Path $HostEvidenceLatest "result.json")
        }
    } catch {
        Write-Host "[warn] Could not write result.json to phone: $_"
    }
}

function Ensure-PhoneLayout {
    $mkdir = Invoke-Adb shell "mkdir -p '$PhoneRoot/apks' '$PhoneEvidenceLatest' '$PhoneEvidenceArchive' '$PhoneRoot/tools'"
    if ($mkdir.ExitCode -ne 0) {
        throw "Failed to create phone layout under $PhoneRoot"
    }
}

function Archive-LatestEvidence {
    param([string]$ArchiveStamp)

    if ($PhoneEvidenceArchive -notlike "/sdcard/Download/db-control/evidence/archive*") {
        throw "Unsafe archive path detected: $PhoneEvidenceArchive"
    }
    if ($PhoneEvidenceLatest -notlike "/sdcard/Download/db-control/evidence/latest*") {
        throw "Unsafe latest evidence path detected: $PhoneEvidenceLatest"
    }

    $hasFiles = Invoke-Adb shell "ls -A '$PhoneEvidenceLatest' 2>/dev/null"
    if ($hasFiles.ExitCode -ne 0) { return }
    if (-not ($hasFiles.Output | Out-String).Trim()) { return }

    $archiveDir = "$PhoneEvidenceArchive/$ArchiveStamp"
    $mk = Invoke-Adb shell "mkdir -p '$archiveDir'"
    if ($mk.ExitCode -eq 0) {
        $move = Invoke-Adb shell "cp -a '$PhoneEvidenceLatest'/.' '$archiveDir' && rm -rf '$PhoneEvidenceLatest'/*"
        if ($move.ExitCode -ne 0) {
            Add-ResultError "Could not archive latest evidence on phone."
        }
    }

    if ($KeepEvidenceCount -gt 0) {
        $prune = Invoke-Adb shell "ls -1dt '$PhoneEvidenceArchive'/* 2>/dev/null | tail -n +$($KeepEvidenceCount + 1) | xargs -r rm -rf"
        if ($prune.ExitCode -ne 0) {
            Add-ResultError "Could not prune old evidence archives."
        }
    }
}

function Clear-LatestEvidence {
    if ($PhoneEvidenceLatest -notlike "/sdcard/Download/db-control/evidence/latest*") {
        throw "Unsafe latest evidence path detected: $PhoneEvidenceLatest"
    }
    $clear = Invoke-Adb shell "rm -rf '$PhoneEvidenceLatest'/*"
    if ($clear.ExitCode -ne 0) {
        Add-ResultError "Could not clear latest evidence folder on phone."
    }
}

function Capture-Evidence {
    $evidenceScript = Join-Path $ScriptDir "db-apk-evidence.ps1"
    if (-not (Test-Path $evidenceScript)) {
        $result["evidence"] = "fail"
        Add-ResultError "db-apk-evidence.ps1 not found."
        return
    }

    $args = @(
        "-Package", $Package,
        "-PhoneEvidenceDir", $PhoneEvidenceLatest
    )

    if ($ShouldPullEvidence) {
        $args += @("-PullEvidenceToWindows", "-WindowsEvidenceDir", $HostEvidenceLatest)
    }

    & $evidenceScript @args
    $evExit = $LASTEXITCODE

    $tmpJson = Join-Path (Get-DbHostTemp) ("db-control-evidence-result-" + [guid]::NewGuid().ToString() + ".json")
    $pull = Invoke-Adb pull "$PhoneEvidenceLatest/result.json" $tmpJson
    if ($pull.ExitCode -eq 0 -and (Test-Path $tmpJson)) {
        try {
            $ev = Get-Content $tmpJson -Raw | ConvertFrom-Json
            $result["evidence"] = if ($ev.logcat -eq "pass" -and $ev.screenshot -eq "pass") { "pass" } elseif ($ev.logcat -in @("warning", "skipped") -or $ev.screenshot -in @("warning", "skipped")) { "warning" } else { "fail" }
            $result["dbnativeLogs"] = if ($ev.dbnativeLogs) { $ev.dbnativeLogs } else { "warning" }
            $result["crashScan"] = if ($ev.crashScan) { $ev.crashScan } else { "warning" }
            $result["screenshot"] = if ($ev.screenshot) { $ev.screenshot } else { "warning" }
        } catch {
            $result["evidence"] = "fail"
            Add-ResultError "Could not parse evidence result.json."
        }
        Remove-Item -Force $tmpJson
    } else {
        $result["evidence"] = "fail"
        Add-ResultError "Evidence result.json not found on phone."
    }

    if ($evExit -ne 0 -and $result["evidence"] -ne "fail") {
        $result["evidence"] = "warning"
    }
}

$exitCode = 0

try {
    Write-Host ""
    Write-Host "=== Digital Breakdown APK Demo (Phone-First) ==="
    Write-Host "[phone apk] $PhoneApkPath"
    Write-Host "[phone evidence] $PhoneEvidenceLatest"
    if ($ShouldPullEvidence) {
        Write-Host "[host mirror] $HostEvidenceLatest"
    }

    $adb = Get-AdbExe
    $dev = Invoke-Adb devices
    $deviceLines = $dev.Output | Select-String "device$"
    if (-not $deviceLines) {
        throw "No authorized ADB device connected."
    }
    $result["adb"] = "pass"

    Ensure-PhoneLayout

    $archiveStamp = Get-Date -Format "yyyyMMdd-HHmmss"
    if ($ArchivePreviousEvidence) {
        Archive-LatestEvidence -ArchiveStamp $archiveStamp
    }
    if ($CleanBeforeRun -or $ArchivePreviousEvidence) {
        Clear-LatestEvidence
    }

    # Placeholder files are always present
    Write-PhoneTextFile -PhonePath "$PhoneEvidenceLatest/install.txt" -Lines @("[pending] install not run yet")
    Write-PhoneTextFile -PhonePath "$PhoneEvidenceLatest/launch.txt" -Lines @("[pending] launch not run yet")

    if ($EvidenceOnly) {
        $result["download"] = "skipped"
        $result["pull"] = "skipped"
        $result["install"] = "skipped"
        $result["launch"] = "skipped"
        $result["logcatCleared"] = if ($CleanBeforeRun) { "pass" } else { "skipped" }
    } elseif ($LocalApkPath) {
        if (-not (Test-Path $LocalApkPath)) {
            throw "Local APK not found at: $LocalApkPath"
        }
        $push = Invoke-Adb push $LocalApkPath $PhoneApkPath
        if ($push.ExitCode -ne 0) {
            throw "Failed to push local APK to phone path: $PhoneApkPath"
        }
        $result["download"] = "skipped"
        $result["pull"] = "skipped"
        $result["apkPathWindows"] = $LocalApkPath
        $result["apkPathHost"] = $LocalApkPath
    } elseif ($SkipDownload) {
        $exists = Invoke-Adb shell "ls '$PhoneApkPath' 2>/dev/null"
        if ($exists.ExitCode -ne 0 -or ($exists.Output -join "`n") -match "No such file") {
            throw "APK not found on phone at: $PhoneApkPath"
        }
        $result["download"] = "skipped"
    } else {
        $sessionScript = Join-Path $ScriptDir "phone-session.ps1"
        if ((Test-Path $sessionScript) -and (-not (Get-Command Get-TermuxUser -ErrorAction SilentlyContinue))) {
            . $sessionScript
        }

        $user = $null
        try {
            $user = Get-TermuxUser
            if ($user) {
                Start-SshForward
                $sshTest = ssh -p 8022 -o StrictHostKeyChecking=no -o ConnectTimeout=5 "$user@127.0.0.1" "echo ssh_ok" 2>&1
                if ($sshTest -match "ssh_ok") {
                    $result["ssh"] = "pass"
                } else {
                    $result["ssh"] = "fail"
                    throw "SSH connection to Termux failed."
                }
            } else {
                $result["ssh"] = "fail"
                throw "Could not determine Termux username."
            }

            $ghStatus = ssh -p 8022 -o StrictHostKeyChecking=no "$user@127.0.0.1" "gh auth status 2>&1" 2>&1
            if ($ghStatus -match "Logged in") {
                $result["ghAuth"] = "pass"
            } else {
                $result["ghAuth"] = "fail"
                throw "gh auth is not active in Termux."
            }

            $downloadCmd = "db-apk-artifact-download --repo indrolend/digital-breakdown-apk --run $RunId --artifact $ArtifactName --out $PhoneApkPath"
            ssh -p 8022 -o StrictHostKeyChecking=no "$user@127.0.0.1" $downloadCmd
            if ($LASTEXITCODE -ne 0) {
                throw "Termux artifact download failed."
            }
            $result["download"] = "pass"
        } catch {
            if ($result["ssh"] -eq "skipped") { $result["ssh"] = "fail" }
            if ($result["ghAuth"] -eq "skipped") { $result["ghAuth"] = "fail" }
            throw
        }
    }

    if (-not $EvidenceOnly) {
        if ($CleanBeforeRun) {
            $clear = Invoke-Adb logcat -c
            $result["logcatCleared"] = if ($clear.ExitCode -eq 0) { "pass" } else { "warning" }
            if ($clear.ExitCode -ne 0) {
                Add-ResultError "adb logcat -c returned non-zero."
            }
        } else {
            $result["logcatCleared"] = "skipped"
        }

        $install = Invoke-Adb shell pm install -r "$PhoneApkPath"
        $installLines = @($install.Output | ForEach-Object { "$_" })
        if (-not $installLines) { $installLines = @("[warn] install command produced no output") }
        Write-PhoneTextFile -PhonePath "$PhoneEvidenceLatest/install.txt" -Lines $installLines

        if ($install.ExitCode -ne 0 -and ($installLines -join "`n") -match "INSTALL_FAILED_UPDATE_INCOMPATIBLE") {
            if ($Package) {
                $null = Invoke-Adb uninstall $Package
                $install = Invoke-Adb shell pm install -r "$PhoneApkPath"
                $installLines = @($install.Output | ForEach-Object { "$_" })
                if (-not $installLines) { $installLines = @("[warn] install retry produced no output") }
                Write-PhoneTextFile -PhonePath "$PhoneEvidenceLatest/install.txt" -Lines $installLines
            }
        }

        if ($install.ExitCode -eq 0 -or (($installLines -join "`n") -match "Success")) {
            $result["install"] = "pass"
        } else {
            $result["install"] = "fail"
            $result["launch"] = "skipped"
            Write-PhoneTextFile -PhonePath "$PhoneEvidenceLatest/launch.txt" -Lines @("[skipped] Launch skipped because install failed.")
        }

        if ($result["install"] -eq "pass") {
            $launch = Invoke-Adb shell monkey -p $Package 1
            $launchLines = @($launch.Output | ForEach-Object { "$_" })
            if (-not $launchLines) { $launchLines = @("[warn] launch command produced no output") }
            Write-PhoneTextFile -PhonePath "$PhoneEvidenceLatest/launch.txt" -Lines $launchLines
            $result["launch"] = if ($launch.ExitCode -eq 0) { "pass" } else { "warning" }
            if ($launch.ExitCode -ne 0) {
                Add-ResultError "Launch command returned non-zero exit code."
            }
        }
    } else {
        Write-PhoneTextFile -PhonePath "$PhoneEvidenceLatest/install.txt" -Lines @("[skipped] EvidenceOnly mode requested.")
        Write-PhoneTextFile -PhonePath "$PhoneEvidenceLatest/launch.txt" -Lines @("[skipped] EvidenceOnly mode requested.")
    }

    Capture-Evidence

    if ($ShouldPullEvidence) {
        New-Item -ItemType Directory -Force -Path $HostEvidenceLatest | Out-Null
        Remove-Item -Recurse -Force -ErrorAction SilentlyContinue (Join-Path $HostEvidenceLatest "*")
        $pullLatest = Invoke-Adb pull "$PhoneEvidenceLatest/." $HostEvidenceLatest
        if ($pullLatest.ExitCode -ne 0) {
            $result["evidencePathWindows"] = ""
            $result["evidencePathHost"] = ""
            Add-ResultError "Failed to mirror latest evidence to host."
        }
    }

} catch {
    $msg = $_.Exception.Message
    if (-not $msg) { $msg = "Unhandled error." }
    Add-ResultError $msg
    $exitCode = 1
    if ($result["adb"] -eq "pending") { $result["adb"] = "fail" }
    foreach ($stage in @("ssh", "ghAuth", "download", "pull", "install", "launch", "dbnativeLogs", "crashScan", "screenshot")) {
        if ($result[$stage] -eq "pending") { $result[$stage] = "fail" }
    }
    if ($result["evidence"] -eq "pending") { $result["evidence"] = "fail" }
} finally {
    # Normalize any unset/pending statuses before writing final result
    foreach ($key in @("adb", "ssh", "ghAuth", "download", "pull", "install", "launch", "evidence", "dbnativeLogs", "crashScan", "screenshot", "logcatCleared")) {
        if (-not $result.Contains($key)) { $result[$key] = "warning" }
        if ($result[$key] -eq "pending") { $result[$key] = "warning" }
        if ($result[$key] -notin $statusValues) { $result[$key] = "warning" }
    }
    $result.timestamp = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    Save-Result
}

Write-Host ""
Write-Host "=== Demo Run Complete ==="
Write-Host "[phone evidence] $PhoneEvidenceLatest"
if ($ShouldPullEvidence) {
    Write-Host "[host mirror] $HostEvidenceLatest"
}
Write-Host ($result | ConvertTo-Json -Depth 5)

$failed = @($result.Keys | Where-Object {
    $_ -in @("adb", "ssh", "ghAuth", "download", "pull", "install", "launch", "evidence", "dbnativeLogs", "crashScan", "screenshot") -and $result[$_] -eq "fail"
})
if ($failed.Count -gt 0) {
    Write-Host "[warn] Failed checks: $($failed -join ', ')"
    exit 1
}
if ($exitCode -ne 0) { exit $exitCode }
exit 0