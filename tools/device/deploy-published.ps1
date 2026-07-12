[CmdletBinding()]
param(
    [switch]$WaitForMain,
    [int]$PollSeconds = 15,
    [int]$TimeoutMinutes = 20,
    [switch]$NoMirror,
    [switch]$NoLogs
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ManifestUrl = 'https://indrolend.github.io/Digital-breakdown-dev/build-info.json'
$MainApiUrl = 'https://api.github.com/repos/indrolend/digital-breakdown-apk/commits/main'
$DefaultApkUrl = 'https://github.com/indrolend/Digital-breakdown-dev/releases/download/latest-dev/DigitalBreakdown-Android.apk'
$PackageName = 'com.indrolend.digitalbreakdown.native'
$DownloadRoot = Join-Path $env:TEMP 'digital-breakdown-device-deploy'
$ApkPath = Join-Path $DownloadRoot 'DigitalBreakdown-Android.apk'

function Find-CommandPath([string[]]$Names, [string[]]$Fallbacks = @()) {
    foreach ($name in $Names) {
        $command = Get-Command $name -ErrorAction SilentlyContinue
        if ($command) { return $command.Source }
    }
    foreach ($candidate in $Fallbacks) {
        if ($candidate -and (Test-Path $candidate)) { return (Resolve-Path $candidate).Path }
    }
    return $null
}

$Adb = Find-CommandPath @('adb.exe', 'adb') @((Join-Path $env:LOCALAPPDATA 'Android/Sdk/platform-tools/adb.exe'))
if (-not $Adb) { throw 'ADB was not found. Install Android platform-tools or add adb.exe to PATH.' }
$Scrcpy = Find-CommandPath @('scrcpy.exe', 'scrcpy') @((Join-Path $env:USERPROFILE 'scrcpy/scrcpy.exe'))

function Read-Json([string]$Url) {
    return Invoke-RestMethod -Uri "$Url?t=$([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds())" -Headers @{ 'Cache-Control' = 'no-cache'; 'User-Agent' = 'DigitalBreakdownDeviceDeploy' }
}

function Get-MainCommit {
    $response = Invoke-RestMethod -Uri $MainApiUrl -Headers @{ 'User-Agent' = 'DigitalBreakdownDeviceDeploy' }
    return [string]$response.sha
}

function Get-PublishedManifest {
    $manifest = Read-Json $ManifestUrl
    if (-not $manifest.commit) { throw 'The portal manifest does not identify a published commit.' }
    if (-not $manifest.android.available) { throw 'The portal manifest says the Android APK is unavailable.' }
    return $manifest
}

Write-Host 'DIGITAL BREAKDOWN - PUBLISHED DEVICE DEPLOY' -ForegroundColor Cyan
$targetMain = Get-MainCommit
$deadline = (Get-Date).AddMinutes($TimeoutMinutes)

while ($true) {
    $manifest = Get-PublishedManifest
    $published = [string]$manifest.commit
    Write-Host "SOURCE MAIN  $($targetMain.Substring(0,7))"
    Write-Host "PUBLISHED    $($published.Substring(0,7))"

    if (-not $WaitForMain -or $published -eq $targetMain) { break }
    if ((Get-Date) -ge $deadline) {
        throw "Timed out waiting for main commit $targetMain to become the complete published build."
    }

    Write-Host "Build/publication is not complete for $($targetMain.Substring(0,7)); checking again in $PollSeconds seconds..." -ForegroundColor Yellow
    Start-Sleep -Seconds $PollSeconds
}

$deviceLines = & $Adb devices | Select-Object -Skip 1 | Where-Object { $_ -match '\S' }
$authorized = @($deviceLines | Where-Object { $_ -match '\sdevice$' })
$unauthorized = @($deviceLines | Where-Object { $_ -match '\sunauthorized$' })
if ($unauthorized.Count -gt 0) { throw 'Unlock the phone and accept the USB debugging authorization prompt.' }
if ($authorized.Count -ne 1) { throw 'Connect exactly one authorized Android device.' }

$Serial = ($authorized[0] -split '\s+')[0]
$Model = (& $Adb -s $Serial shell getprop ro.product.model).Trim()
Write-Host "DEVICE       $Model ($Serial)" -ForegroundColor Green

New-Item -ItemType Directory -Force -Path $DownloadRoot | Out-Null
$ApkUrl = if ($manifest.android.url) { [string]$manifest.android.url } else { $DefaultApkUrl }
Write-Host "Downloading exact published APK for $($published.Substring(0,7))..." -ForegroundColor Cyan
Invoke-WebRequest -Uri $ApkUrl -OutFile $ApkPath -Headers @{ 'Cache-Control' = 'no-cache'; 'User-Agent' = 'DigitalBreakdownDeviceDeploy' }

$actualHash = (Get-FileHash -Algorithm SHA256 -Path $ApkPath).Hash.ToLowerInvariant()
$expectedHash = if ($manifest.android.sha256) { ([string]$manifest.android.sha256).ToLowerInvariant() } else { $null }
if ($expectedHash -and $actualHash -ne $expectedHash) {
    throw "APK checksum mismatch. Expected $expectedHash but downloaded $actualHash. Nothing was installed."
}
Write-Host "CHECKSUM     $actualHash" -ForegroundColor Green

$installOutput = & $Adb -s $Serial install -r -d $ApkPath 2>&1
$installOutput | ForEach-Object { Write-Host $_ }
if ($LASTEXITCODE -ne 0) {
    if (($installOutput -join "`n") -match 'INSTALL_FAILED_UPDATE_INCOMPATIBLE') {
        throw 'APK signature mismatch. The installed copy uses a different signing key. Uninstall it once or normalize development signing.'
    }
    throw "ADB install failed with exit code $LASTEXITCODE."
}

& $Adb -s $Serial shell am force-stop $PackageName | Out-Null
& $Adb -s $Serial shell monkey -p $PackageName -c android.intent.category.LAUNCHER 1 | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'The APK installed, but the app could not be launched.' }

if (-not $NoMirror) {
    if ($Scrcpy) {
        Start-Process -FilePath $Scrcpy -ArgumentList @('-s', $Serial, '--max-size=1024', '--no-audio', '--window-title=Digital Breakdown - Published')
    } else {
        Write-Warning 'scrcpy was not found; mirroring was skipped.'
    }
}

if (-not $NoLogs) {
    $logScript = Join-Path $PSScriptRoot 'logs.ps1'
    Start-Process powershell.exe -ArgumentList @('-NoExit', '-ExecutionPolicy', 'Bypass', '-File', "`"$logScript`"", '-Serial', $Serial)
}

Write-Host "DEPLOY COMPLETE: published $($published.Substring(0,7)) on $Model" -ForegroundColor Green
