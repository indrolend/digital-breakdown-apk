[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Host", "Guest", "Compare", "LocalProof")]
    [string]$Role,
    [string]$Exe,
    [string]$ServiceUrl,
    [string]$RoomCode,
    [string]$ExpectedCommit,
    [int]$ExpectedProtocol = 7,
    [int]$ExpectedGameplay = 5,
    [string]$OutputDirectory,
    [string]$HostResults,
    [string]$GuestResults,
    [ValidateRange(5, 3600)]
    [int]$MaximumDurationSeconds = 180,
    [switch]$Scripted,
    [switch]$ManualCheckpoints,
    [switch]$AllowCommitMismatch
)

$ErrorActionPreference = "Stop"

function Require-Value([string]$Name, [string]$Value) {
    if ([string]::IsNullOrWhiteSpace($Value)) { throw "$Name is required for role $Role" }
}

function Read-Json([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { throw "Missing result file: $Path" }
    Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function Write-Result([string]$Directory, [hashtable]$Summary, [object]$Identity, [string[]]$Markers) {
    New-Item -ItemType Directory -Force -Path $Directory | Out-Null
    $Identity | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Directory "identity.json")
    [pscustomobject]$Summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Directory "summary.json")
    $Markers | Set-Content -LiteralPath (Join-Path $Directory "markers.txt")
    $Summary.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Value)" } |
        Set-Content -LiteralPath (Join-Path $Directory "summary.txt")
}

function Compare-Results([string]$HostDirectory, [string]$GuestDirectory) {
    $hostIdentity = Read-Json (Join-Path $HostDirectory "identity.json")
    $guestIdentity = Read-Json (Join-Path $GuestDirectory "identity.json")
    $hostSummary = Read-Json (Join-Path $HostDirectory "summary.json")
    $guestSummary = Read-Json (Join-Path $GuestDirectory "summary.json")
    $hostMarkers = Get-Content -LiteralPath (Join-Path $HostDirectory "markers.txt")
    $guestMarkers = Get-Content -LiteralPath (Join-Path $GuestDirectory "markers.txt")
    $failures = [System.Collections.Generic.List[string]]::new()
    if (-not $AllowCommitMismatch -and $hostIdentity.commit -ne $guestIdentity.commit) { $failures.Add("commit_mismatch") }
    if ($hostIdentity.protocol -ne $guestIdentity.protocol) { $failures.Add("protocol_mismatch") }
    if ($hostIdentity.gameplay -ne $guestIdentity.gameplay) { $failures.Add("gameplay_mismatch") }
    if ($hostSummary.room -ne $guestSummary.room) { $failures.Add("room_mismatch") }
    if ($hostSummary.session -and $guestSummary.session -and $hostSummary.session -ne $guestSummary.session) { $failures.Add("session_mismatch") }
    if ($hostSummary.durable -ne "ok" -or $guestSummary.durable -ne "ok") { $failures.Add("durable_outcome") }
    if ($hostSummary.convergence -ne "ok" -or $guestSummary.convergence -ne "ok") { $failures.Add("convergence") }
    if ($hostSummary.manual_checkpoints -eq "failed" -or $guestSummary.manual_checkpoints -eq "failed") { $failures.Add("manual_checkpoint") }
    $hostHashes = @{}
    foreach ($line in ($hostMarkers | Select-String -Pattern 'MULTIPLAYER_AUTH_STATE_HASH tick=(\d+) hash=(\d+)')) {
        $hostHashes[$line.Matches[0].Groups[1].Value] = $line.Matches[0].Groups[2].Value
    }
    $commonHashes = 0
    foreach ($line in ($guestMarkers | Select-String -Pattern 'MULTIPLAYER_AUTH_STATE_HASH tick=(\d+) hash=(\d+)')) {
        $tick = $line.Matches[0].Groups[1].Value
        if ($hostHashes.ContainsKey($tick) -and $hostHashes[$tick] -eq $line.Matches[0].Groups[2].Value) { ++$commonHashes }
    }
    if ($commonHashes -eq 0) { $failures.Add("no_common_authoritative_hash") }
    if ([int]$hostSummary.exit_code -ne 0 -or [int]$guestSummary.exit_code -ne 0) { $failures.Add("process_exit") }
    if ($failures.Count -gt 0) {
        Write-Host "MULTIPLAYER_MULTI_DEVICE_ACCEPTANCE_FAILED reason=$($failures -join ',')"
        return 1
    }
    Write-Host "MULTIPLAYER_MULTI_DEVICE_ACCEPTANCE_OK room=$($hostSummary.room) commit=$($hostIdentity.commit)"
    return 0
}

if ($Role -eq "Compare") {
    Require-Value "HostResults" $HostResults
    Require-Value "GuestResults" $GuestResults
    exit (Compare-Results (Resolve-Path $HostResults).Path (Resolve-Path $GuestResults).Path)
}

Require-Value "Exe" $Exe
Require-Value "ServiceUrl" $ServiceUrl
$Exe = (Resolve-Path $Exe).Path

if ($Role -eq "LocalProof") {
    Require-Value "OutputDirectory" $OutputDirectory
    $OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    $parity = Join-Path $PSScriptRoot "test_multiplayer_parity_windows.ps1"
    $cases = @(
        @{ Name = "baseline"; Latency = 0; Jitter = 0; DropSnapshot = 0; DropInput = 0; Seed = 701; Termination = "Host" },
        @{ Name = "moderate"; Latency = 90; Jitter = 35; DropSnapshot = 7; DropInput = 9; Seed = 702; Termination = "Host" },
        @{ Name = "guest-disconnect"; Latency = 0; Jitter = 0; DropSnapshot = 0; DropInput = 0; Seed = 703; Termination = "Guest" },
        @{ Name = "host-disconnect"; Latency = 0; Jitter = 0; DropSnapshot = 0; DropInput = 0; Seed = 704; Termination = "Host" }
    )
    $completed = 0
    foreach ($case in $cases) {
        $caseOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $parity `
            -Exe $Exe -ServiceUrl $ServiceUrl -TimeoutSeconds $MaximumDurationSeconds `
            -NetLatencyMs $case.Latency -NetJitterMs $case.Jitter `
            -DropSnapshotEvery $case.DropSnapshot -DropInputEvery $case.DropInput `
            -NetSeed $case.Seed -TerminationRole $case.Termination 2>&1
        $caseOutput | Set-Content -LiteralPath (Join-Path $OutputDirectory "$($case.Name).txt")
        $caseOutput | ForEach-Object { Write-Host $_ }
        if ($LASTEXITCODE -ne 0) {
            Write-Host "MULTIPLAYER_LOCAL_ACCEPTANCE_FAILED case=$($case.Name)"
            exit $LASTEXITCODE
        }
        ++$completed
    }
    @{ attempted = $cases.Count; completed = $completed; failures = 0 } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory "local-proof.json")
    Write-Host "MULTIPLAYER_LOCAL_ACCEPTANCE_OK attempted=$($cases.Count) completed=$completed"
    exit 0
}

Require-Value "OutputDirectory" $OutputDirectory
if ($Role -eq "Guest") { Require-Value "RoomCode" $RoomCode }
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$identityText = (& $Exe --build-identity-json 2>&1 | Out-String).Trim()
try { $identity = $identityText | ConvertFrom-Json }
catch { throw "Native build identity is unavailable: $identityText" }
if ($ExpectedCommit -and -not $AllowCommitMismatch -and $identity.commit -ne $ExpectedCommit) {
    throw "Commit mismatch: expected $ExpectedCommit, got $($identity.commit)"
}
if ([int]$identity.protocol -ne $ExpectedProtocol) { throw "Protocol mismatch: expected $ExpectedProtocol, got $($identity.protocol)" }
if ([int]$identity.gameplay -ne $ExpectedGameplay) { throw "Gameplay mismatch: expected $ExpectedGameplay, got $($identity.gameplay)" }
$service = [Uri]$ServiceUrl
if ($service.Scheme -notin @("http", "https")) { throw "ServiceUrl must use http or https" }
$healthUri = "{0}://{1}/health" -f $service.Scheme, $service.Authority
$health = Invoke-RestMethod -Uri $healthUri -TimeoutSec 10
if ([int]$health.protocolVersion -ne $ExpectedProtocol -or
    ($null -ne $health.gameplayVersion -and [int]$health.gameplayVersion -ne $ExpectedGameplay)) {
    throw "Service version mismatch at $healthUri"
}
Write-Host "MULTIPLAYER_ACCEPTANCE_IDENTITY role=$($Role.ToLowerInvariant()) commit=$($identity.commit) protocol=$($identity.protocol) gameplay=$($identity.gameplay) platform=$($identity.platform) architecture=$($identity.architecture) configuration=$($identity.configuration)"

$data = Join-Path $OutputDirectory "client-data"
$log = Join-Path $OutputDirectory "client.log"
New-Item -ItemType Directory -Force -Path $data | Out-Null
$nativeRoleArgs = if ($Role -eq "Host") { "--host-room --auto-start-multiplayer" } else { "--join-room $RoomCode" }
if ($Scripted) { $nativeRoleArgs += " --multiplayer-parity-test" }
$command = 'set "LOCALAPPDATA={0}" && "{1}" {2} --service-url "{3}" > "{4}" 2>&1' -f $data, $Exe, $nativeRoleArgs, $ServiceUrl, $log
$started = [DateTime]::UtcNow
$process = Start-Process -FilePath $env:ComSpec -ArgumentList "/d", "/s", "/c", "`"$command`"" -PassThru -WindowStyle Hidden
$timedOut = $false
$reportedRoom = $false
try {
    $deadline = (Get-Date).AddSeconds($MaximumDurationSeconds)
    while ((Get-Date) -lt $deadline -and -not $process.HasExited) {
        if ($Role -eq "Host" -and (Test-Path $log)) {
            $roomMatch = Select-String -Path $log -Pattern 'MULTIPLAYER_ROOM_CODE ([A-Z2-9]{6})' | Select-Object -Last 1
            if ($roomMatch -and -not $reportedRoom) {
                Write-Host "MULTIPLAYER_ACCEPTANCE_ROOM_CODE $($roomMatch.Matches[0].Groups[1].Value)"
                $reportedRoom = $true
            }
        }
        Start-Sleep -Milliseconds 200
    }
    if (-not $process.HasExited) {
        $timedOut = $true
        & "$env:SystemRoot\System32\taskkill.exe" /PID $process.Id /T /F 2>$null | Out-Null
        $process.WaitForExit(5000) | Out-Null
    }
} finally {
    if (-not $process.HasExited) {
        & "$env:SystemRoot\System32\taskkill.exe" /PID $process.Id /T /F 2>$null | Out-Null
        $process.WaitForExit(5000) | Out-Null
    }
}

$lines = if (Test-Path $log) { @(Get-Content -LiteralPath $log) } else { @() }
$markers = @($lines | Where-Object { $_ -match '^(MULTIPLAYER_|Build identity:)' })
$roomMatch = $markers | Select-String -Pattern 'MULTIPLAYER_(?:ROOM_CODE |PLAYING role=\w+ room=)([A-Z2-9]{6})' | Select-Object -First 1
$sessionMatch = $markers | Select-String -Pattern 'session=(\d+)' | Select-Object -Last 1
$hashMatch = $markers | Select-String -Pattern 'MULTIPLAYER_AUTH_STATE_HASH tick=\d+ hash=(\d+)' | Select-Object -Last 1
$durableEvidence = ($markers -match 'MULTIPLAYER_DURABLE_SOUL') -and
                   ($markers -match 'MULTIPLAYER_DURABLE_PROJECTILE .*state=terminal')
if ($Role -eq "Host") { $durableEvidence = $durableEvidence -and ($markers -match 'MULTIPLAYER_DURABLE_MELEE') }
$durable = if ($durableEvidence) { "ok" } else { "incomplete" }
$convergence = if (($markers -match 'MULTIPLAYER_METRICS .*hash_matches=[1-9]') -or $hashMatch) { "ok" } else { "incomplete" }
$manualStatus = "not_requested"
if ($ManualCheckpoints) {
    $checks = @(
        "Remote player motion is smooth",
        "Remote melee pose is visible",
        "Vacuum attraction and ingestion are coherent",
        "Projectile ownership and termination are coherent",
        "Pause remains local",
        "Disconnect returns the peer to a stable status"
    )
    $manualStatus = "ok"
    foreach ($check in $checks) {
        if ((Read-Host "$check [y/N]") -notmatch '^(?i)y(?:es)?$') { $manualStatus = "failed" }
    }
}
$exitCode = if ($timedOut) { 0 } else { $process.ExitCode }
$summary = @{
    role = $Role.ToLowerInvariant()
    room = if ($roomMatch) { $roomMatch.Matches[0].Groups[1].Value } else { $RoomCode }
    session = if ($sessionMatch) { $sessionMatch.Matches[0].Groups[1].Value } else { "" }
    durable = $durable
    convergence = $convergence
    manual_checkpoints = $manualStatus
    final_hash = if ($hashMatch) { $hashMatch.Matches[0].Groups[1].Value } else { "" }
    exit_code = $exitCode
    timed_shutdown = $timedOut
    service_host = $service.Host
    started_utc = $started.ToString("o")
    finished_utc = [DateTime]::UtcNow.ToString("o")
    duration_seconds = [Math]::Round(([DateTime]::UtcNow - $started).TotalSeconds, 3)
}
Write-Result $OutputDirectory $summary $identity $markers
if ($exitCode -eq 0 -and $durable -eq "ok" -and $convergence -eq "ok" -and $manualStatus -ne "failed") {
    Remove-Item -LiteralPath $log -Force -ErrorAction SilentlyContinue
    Write-Host "MULTIPLAYER_ACCEPTANCE_RESULT role=$($summary.role) durable=ok convergence=ok room=$($summary.room)"
    exit 0
}
Write-Host "MULTIPLAYER_ACCEPTANCE_FAILED role=$($summary.role) durable=$durable convergence=$convergence exit=$exitCode log=$log"
exit 1
