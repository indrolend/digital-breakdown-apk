[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Exe,
    [Parameter(Mandatory = $true)]
    [string]$ServiceUrl,
    [int]$TimeoutSeconds = 90,
    [int]$NetLatencyMs = 0,
    [int]$NetJitterMs = 0,
    [int]$DropSnapshotEvery = 0,
    [int]$DropInputEvery = 0,
    [int]$NetSeed = 1
)

$ErrorActionPreference = "Stop"
$Exe = (Resolve-Path $Exe).Path
$runRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("digital-breakdown-parity-" + [Guid]::NewGuid().ToString("N"))
$hostData = Join-Path $runRoot "host-data"
$guestData = Join-Path $runRoot "guest-data"
$hostLog = Join-Path $runRoot "host.log"
$guestLog = Join-Path $runRoot "guest.log"
New-Item -ItemType Directory -Force $hostData, $guestData | Out-Null
$hostProcess = $null
$guestProcess = $null
$stage = "startup"

function Fail-Parity([string]$Reason) {
    Write-Host "MULTIPLAYER_PARITY_FAILED stage=$stage reason=$Reason"
    Write-Host "Host log: $hostLog"
    Write-Host "Guest log: $guestLog"
    throw "Multiplayer parity failed at $stage`: $Reason"
}

function Start-Game([string]$LocalData, [string]$Log, [string]$Arguments) {
    $impairment = "--net-latency-ms $NetLatencyMs --net-jitter-ms $NetJitterMs --net-drop-snapshot-every $DropSnapshotEvery --net-drop-input-every $DropInputEvery --net-seed $NetSeed"
    $command = 'set "LOCALAPPDATA={0}" && "{1}" {2} {3} --service-url "{4}" > "{5}" 2>&1' -f $LocalData, $Exe, $Arguments, $impairment, $ServiceUrl, $Log
    Start-Process -FilePath $env:ComSpec -ArgumentList "/d", "/s", "/c", "`"$command`"" -PassThru -WindowStyle Hidden
}

function Log-Contains([string]$Path, [string]$Pattern) {
    (Test-Path $Path) -and (Select-String -Path $Path -Pattern $Pattern -Quiet)
}

function Common-HashCount {
    if (-not (Test-Path $hostLog) -or -not (Test-Path $guestLog)) { return 0 }
    $hostHashes = @{}
    foreach ($line in (Select-String -Path $hostLog -Pattern 'MULTIPLAYER_AUTH_STATE_HASH tick=(\d+) hash=(\d+)')) {
        $hostHashes[$line.Matches[0].Groups[1].Value] = $line.Matches[0].Groups[2].Value
    }
    $count = 0
    foreach ($line in (Select-String -Path $guestLog -Pattern 'MULTIPLAYER_AUTH_STATE_HASH tick=(\d+) hash=(\d+)')) {
        $tick = $line.Matches[0].Groups[1].Value
        if ($hostHashes.ContainsKey($tick) -and $hostHashes[$tick] -eq $line.Matches[0].Groups[2].Value) { $count++ }
    }
    return $count
}

try {
    $stage = "create-room"
    $hostProcess = Start-Game $hostData $hostLog "--host-room --auto-start-multiplayer --multiplayer-parity-test"
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $room = $null
    while ((Get-Date) -lt $deadline) {
        if (Test-Path $hostLog) {
            $match = Select-String -Path $hostLog -Pattern 'MULTIPLAYER_ROOM_CODE ([A-Z2-9]{6})' | Select-Object -Last 1
            if ($match) { $room = $match.Matches[0].Groups[1].Value; break }
            if (Log-Contains $hostLog 'MULTIPLAYER_FAILED|VERSION MISMATCH') { Fail-Parity "host connection failed" }
        }
        Start-Sleep -Milliseconds 100
    }
    if (-not $room) { Fail-Parity "room code timeout" }

    $stage = "join-room"
    $guestProcess = Start-Game $guestData $guestLog "--join-room $room --multiplayer-parity-test"
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        if ((Log-Contains $hostLog "MULTIPLAYER_PLAYING role=host room=$room") -and
            (Log-Contains $guestLog "MULTIPLAYER_PLAYING role=guest room=$room")) { break }
        if (Log-Contains $guestLog 'MULTIPLAYER_FAILED|ROOM NOT FOUND|VERSION MISMATCH') { Fail-Parity "guest connection failed" }
        Start-Sleep -Milliseconds 100
    }
    if (-not (Log-Contains $guestLog "MULTIPLAYER_PLAYING role=guest room=$room")) { Fail-Parity "playing-state timeout" }

    $stage = "snapshot-hashes"
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline -and (Common-HashCount) -lt 3) { Start-Sleep -Milliseconds 100 }
    if ((Common-HashCount) -lt 3) { Fail-Parity "fewer than three matching authoritative snapshot hashes" }

    $stage = "guest-input"
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline -and
           (-not (Log-Contains $hostLog 'MULTIPLAYER_INPUT_RECEIVED player=1') -or
            -not (Log-Contains $guestLog 'MULTIPLAYER_TEST_GUEST_MOVEMENT') -or
            -not (Log-Contains $guestLog 'MULTIPLAYER_TEST_GUEST_JUMP .*kind=double_jump') -or
            -not (Log-Contains $guestLog 'MULTIPLAYER_TEST_GUEST_JUMP_PREDICTED'))) {
        Start-Sleep -Milliseconds 100
    }
    if (-not (Log-Contains $hostLog 'MULTIPLAYER_INPUT_RECEIVED player=1')) { Fail-Parity "guest input was not received by host" }
    if (-not (Log-Contains $guestLog 'MULTIPLAYER_TEST_GUEST_MOVEMENT')) { Fail-Parity "scripted guest movement was not observed" }
    if (-not (Log-Contains $guestLog 'MULTIPLAYER_TEST_GUEST_JUMP .*kind=jump') -or
        -not (Log-Contains $guestLog 'MULTIPLAYER_TEST_GUEST_JUMP .*kind=double_jump')) { Fail-Parity "jump or double-jump initiation missing" }
    if (-not (Log-Contains $guestLog 'MULTIPLAYER_TEST_GUEST_JUMP_PREDICTED .*jump_vel=[1-9]')) { Fail-Parity "guest jump did not begin locally before reconciliation" }

    $stage = "visual-state"
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline -and
           (-not (Log-Contains $hostLog 'MULTIPLAYER_VISUAL_STATE entity=player id=1') -or
            -not (Log-Contains $guestLog 'MULTIPLAYER_VISUAL_STATE entity=player id=0') -or
            -not (Log-Contains $guestLog 'MULTIPLAYER_ENEMY_VISUAL_TRANSITION'))) {
        Start-Sleep -Milliseconds 100
    }
    if (-not (Log-Contains $hostLog 'MULTIPLAYER_VISUAL_STATE entity=player id=1') -or
        -not (Log-Contains $guestLog 'MULTIPLAYER_VISUAL_STATE entity=player id=0')) { Fail-Parity "structured remote-player render state missing" }
    if (-not (Log-Contains $guestLog 'MULTIPLAYER_ENEMY_VISUAL_TRANSITION')) { Fail-Parity "enemy visual transition missing" }

    $stage = "combat-events"
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline -and
           (-not (Log-Contains $hostLog 'MULTIPLAYER_ACTION_STARTED') -or
            -not (Log-Contains $hostLog 'MULTIPLAYER_HIT_CONFIRMED') -or
            -not (Log-Contains $guestLog 'MULTIPLAYER_ACTION_CONFIRMED') -or
            -not (Log-Contains $guestLog 'MULTIPLAYER_HIT_CONFIRMED') -or
            -not (Log-Contains $guestLog 'MULTIPLAYER_SHELL_BROKEN'))) {
        Start-Sleep -Milliseconds 100
    }
    if (-not (Log-Contains $hostLog 'MULTIPLAYER_ACTION_STARTED')) { Fail-Parity "host never authored guest action" }
    if (-not (Log-Contains $hostLog 'MULTIPLAYER_HIT_CONFIRMED')) { Fail-Parity "host never confirmed enemy hit" }
    if (-not (Log-Contains $guestLog 'MULTIPLAYER_ACTION_CONFIRMED')) { Fail-Parity "guest action remained unconfirmed" }
    if (-not (Log-Contains $guestLog 'MULTIPLAYER_HIT_CONFIRMED')) { Fail-Parity "guest missed authoritative hit event" }
    if (-not (Log-Contains $guestLog 'MULTIPLAYER_SHELL_BROKEN')) { Fail-Parity "guest missed shell-break event" }

    $stage = "pause-resume"
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline -and
           (-not (Log-Contains $hostLog 'MULTIPLAYER_MENU_CLOSED') -or
            -not (Log-Contains $guestLog 'MULTIPLAYER_MENU_CLOSED'))) {
        Start-Sleep -Milliseconds 100
    }
    if (-not (Log-Contains $hostLog 'MULTIPLAYER_MENU_OPENED') -or
        -not (Log-Contains $hostLog 'MULTIPLAYER_MENU_CLOSED') -or
        -not (Log-Contains $guestLog 'MULTIPLAYER_MENU_OPENED') -or
        -not (Log-Contains $guestLog 'MULTIPLAYER_MENU_CLOSED') -or
        -not (Log-Contains $guestLog 'MULTIPLAYER_MOUSE_CAPTURE_RESTORED')) { Fail-Parity "independent pause/resume diagnostics missing" }
    if ($hostProcess.HasExited -or $guestProcess.HasExited) { Fail-Parity "process exited before host-departure stage" }

    $stage = "host-departure"
    & "$env:SystemRoot\System32\taskkill.exe" /PID $hostProcess.Id /T /F 2>$null | Out-Null
    $hostProcess = $null
    $deadline = (Get-Date).AddSeconds(8)
    while ((Get-Date) -lt $deadline -and -not (Log-Contains $guestLog 'MULTIPLAYER_HOST_LEFT')) {
        Start-Sleep -Milliseconds 100
    }
    if (-not (Log-Contains $guestLog 'MULTIPLAYER_HOST_LEFT')) { Fail-Parity "guest did not reach stable host-left state" }

    Write-Host "MULTIPLAYER_PARITY_OK room=$room"
    Write-Host "MULTIPLAYER_COMBAT_PARITY_OK room=$room action=confirmed enemy=0"
    Write-Host "Host log: $hostLog"
    Write-Host "Guest log: $guestLog"
}
finally {
    foreach ($process in @($guestProcess, $hostProcess)) {
        if ($process -and -not $process.HasExited) {
            & "$env:SystemRoot\System32\taskkill.exe" /PID $process.Id /T /F 2>$null | Out-Null
        }
    }
}
