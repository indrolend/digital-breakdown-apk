[CmdletBinding()]
param([string]$Serial = $env:ANDROID_SERIAL)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Find-Adb {
    $command = Get-Command adb.exe -ErrorAction SilentlyContinue
    if (-not $command) { $command = Get-Command adb -ErrorAction SilentlyContinue }
    if ($command) { return $command.Source }
    $candidate = Join-Path $env:LOCALAPPDATA 'Android/Sdk/platform-tools/adb.exe'
    if (Test-Path $candidate) { return $candidate }
    throw 'ADB was not found.'
}

$Adb = Find-Adb
if (-not $Serial) {
    $devices = @(& $Adb devices | Select-Object -Skip 1 | Where-Object { $_ -match '\sdevice$' })
    if ($devices.Count -ne 1) { throw 'Connect exactly one authorized Android device or pass -Serial.' }
    $Serial = ($devices[0] -split '\s+')[0]
}

$PackageName = 'com.indrolend.digitalbreakdown.native'
$PidText = (& $Adb -s $Serial shell pidof $PackageName 2>$null).Trim()
Write-Host "Digital Breakdown native logs - $Serial" -ForegroundColor Cyan
Write-Host 'Press Ctrl+C to stop.'

if ($PidText -match '^\d+$') {
    & $Adb -s $Serial logcat --pid=$PidText
} else {
    Write-Warning 'Game process is not currently running; filtering by DBNATIVE tag.'
    & $Adb -s $Serial logcat DBNATIVE:I '*:S'
}
