[CmdletBinding()]
param(
    [string]$Exe = (Join-Path $PSScriptRoot "..\build\pr27-windows\bin\DigitalBreakdown.exe"),
    [string]$ServiceUrl = "https://digital-breakdown-multiplayer.indrolend.workers.dev",
    [int]$TimeoutSeconds = 20
)

$ErrorActionPreference = "Stop"
$Exe = (Resolve-Path $Exe).Path
$runRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("digital-breakdown-multiplayer-" + [Guid]::NewGuid().ToString("N"))
$hostData = Join-Path $runRoot "host-data"
$guestData = Join-Path $runRoot "guest-data"
$hostLog = Join-Path $runRoot "host.log"
$guestLog = Join-Path $runRoot "guest.log"
New-Item -ItemType Directory -Force $hostData, $guestData | Out-Null
$hostProcess = $null
$guestProcess = $null

function Start-Game([string]$LocalData, [string]$Log, [string]$Arguments) {
    $command = 'set "LOCALAPPDATA={0}" && "{1}" {2} --service-url "{3}" > "{4}" 2>&1' -f $LocalData, $Exe, $Arguments, $ServiceUrl, $Log
    Start-Process -FilePath $env:ComSpec -ArgumentList "/d", "/s", "/c", "`"$command`"" -PassThru -WindowStyle Minimized
}

try {
    $hostProcess = Start-Game $hostData $hostLog "--host-room"
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $room = $null
    while ((Get-Date) -lt $deadline) {
        if (Test-Path $hostLog) {
            $match = Select-String -Path $hostLog -Pattern 'MULTIPLAYER_ROOM_CODE ([A-Z2-9]{6})' | Select-Object -Last 1
            if ($match) { $room = $match.Matches[0].Groups[1].Value; break }
            if (Select-String -Path $hostLog -Pattern 'MULTIPLAYER_FAILED' -Quiet) { throw "Host failed before creating a room. See $hostLog" }
        }
        Start-Sleep -Milliseconds 100
    }
    if (-not $room) { throw "Timed out waiting for a room code. See $hostLog" }

    $guestProcess = Start-Game $guestData $guestLog "--join-room $room"
    $hostPattern = "MULTIPLAYER_CONNECTED role=host player=0 room=$room"
    $guestPattern = "MULTIPLAYER_CONNECTED role=guest player=1 room=$room"
    while ((Get-Date) -lt $deadline) {
        $hostConnected = (Test-Path $hostLog) -and (Select-String -Path $hostLog -SimpleMatch $hostPattern -Quiet)
        $guestConnected = (Test-Path $guestLog) -and (Select-String -Path $guestLog -SimpleMatch $guestPattern -Quiet)
        if ($hostConnected -and $guestConnected) {
            Write-Host "MULTIPLAYER_TWO_INSTANCE_OK room=$room"
            Write-Host "Host log: $hostLog"
            Write-Host "Guest log: $guestLog"
            exit 0
        }
        if ((Test-Path $guestLog) -and (Select-String -Path $guestLog -Pattern 'ROOM NOT FOUND|VERSION MISMATCH|CONNECTION FAILED|MULTIPLAYER_FAILED' -Quiet)) {
            throw "Guest connection failed. See $guestLog"
        }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for both welcome messages. Host: $hostLog Guest: $guestLog"
}
finally {
    foreach ($process in @($guestProcess, $hostProcess)) {
        if ($process -and -not $process.HasExited) {
            & "$env:SystemRoot\System32\taskkill.exe" /PID $process.Id /T /F 2>$null | Out-Null
        }
    }
}
