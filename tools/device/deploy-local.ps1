[CmdletBinding()]
param(
    [switch]$NoBuild,
    [switch]$NoMirror,
    [switch]$NoLogs
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$NativeRoot = Join-Path $RepoRoot 'native-android'
$PackageName = 'com.indrolend.digitalbreakdown.native'
$ApkPath = Join-Path $NativeRoot 'app/build/outputs/apk/debug/app-debug.apk'

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

$adbFallbacks = @(
    (Join-Path $env:LOCALAPPDATA 'Android/Sdk/platform-tools/adb.exe'),
    (Join-Path $env:USERPROFILE 'AppData/Local/Android/Sdk/platform-tools/adb.exe')
)
$Adb = Find-CommandPath @('adb.exe', 'adb') $adbFallbacks
if (-not $Adb) {
    throw 'ADB was not found. Install Android platform-tools or add adb.exe to PATH.'
}

$Scrcpy = Find-CommandPath @('scrcpy.exe', 'scrcpy') @(
    (Join-Path $env:USERPROFILE 'scrcpy/scrcpy.exe'),
    (Join-Path $RepoRoot 'tools/bin/scrcpy/scrcpy.exe')
)

Write-Host 'DIGITAL BREAKDOWN - LOCAL DEVICE DEPLOY' -ForegroundColor Cyan
Write-Host "Repository  $RepoRoot"
Write-Host "ADB         $Adb"

$deviceLines = & $Adb devices | Select-Object -Skip 1 | Where-Object { $_ -match '\S' }
$authorized = @($deviceLines | Where-Object { $_ -match '\sdevice$' })
$unauthorized = @($deviceLines | Where-Object { $_ -match '\sunauthorized$' })

if ($unauthorized.Count -gt 0) {
    throw 'The phone is connected but not authorized. Unlock it and accept the USB debugging prompt.'
}
if ($authorized.Count -eq 0) {
    throw 'No authorized Android device is connected.'
}
if ($authorized.Count -gt 1) {
    throw "More than one Android device is connected. Disconnect extras or set ANDROID_SERIAL."
}

$Serial = ($authorized[0] -split '\s+')[0]
$Model = (& $Adb -s $Serial shell getprop ro.product.model).Trim()
$AndroidVersion = (& $Adb -s $Serial shell getprop ro.build.version.release).Trim()
Write-Host "Device      $Model ($Serial), Android $AndroidVersion" -ForegroundColor Green

if (-not $NoBuild) {
    $Gradle = Join-Path $RepoRoot 'android/gradlew.bat'
    if (-not (Test-Path $Gradle)) {
        $Gradle = Join-Path $NativeRoot 'gradlew.bat'
    }
    if (-not (Test-Path $Gradle)) {
        throw 'Gradle wrapper was not found at android/gradlew.bat or native-android/gradlew.bat.'
    }

    Write-Host 'Building native Android debug APK...' -ForegroundColor Cyan
    & $Gradle -p $NativeRoot assembleDebug --stacktrace
    if ($LASTEXITCODE -ne 0) { throw "Gradle build failed with exit code $LASTEXITCODE." }
}

if (-not (Test-Path $ApkPath)) {
    throw "APK not found: $ApkPath"
}

$GitCommit = 'unknown'
$GitDirty = $false
if (Get-Command git -ErrorAction SilentlyContinue) {
    $GitCommit = (& git -C $RepoRoot rev-parse --short HEAD 2>$null).Trim()
    $GitDirty = [bool](& git -C $RepoRoot status --porcelain 2>$null)
}
$BuildId = if ($GitDirty) { "$GitCommit-dirty" } else { $GitCommit }
Write-Host "Build       $BuildId"
Write-Host "APK         $ApkPath"

Write-Host 'Installing/replacing APK...' -ForegroundColor Cyan
$installOutput = & $Adb -s $Serial install -r -d $ApkPath 2>&1
$installOutput | ForEach-Object { Write-Host $_ }
if ($LASTEXITCODE -ne 0) {
    if (($installOutput -join "`n") -match 'INSTALL_FAILED_UPDATE_INCOMPATIBLE') {
        throw 'APK signature mismatch. The installed build was signed with a different key. Uninstall it once, or normalize local and published development signing.'
    }
    throw "ADB install failed with exit code $LASTEXITCODE."
}

Write-Host 'Launching Digital Breakdown...' -ForegroundColor Cyan
& $Adb -s $Serial shell am force-stop $PackageName | Out-Null
& $Adb -s $Serial shell monkey -p $PackageName -c android.intent.category.LAUNCHER 1 | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'The APK installed, but the app could not be launched.' }

if (-not $NoMirror) {
    if ($Scrcpy) {
        Write-Host "Starting scrcpy: $Scrcpy" -ForegroundColor Cyan
        Start-Process -FilePath $Scrcpy -ArgumentList @('-s', $Serial, '--max-size=1024', '--no-audio', '--window-title=Digital Breakdown - Stylo 4')
    } else {
        Write-Warning 'scrcpy was not found. Installation succeeded; mirroring was skipped.'
    }
}

if (-not $NoLogs) {
    Write-Host 'Opening filtered native log window...' -ForegroundColor Cyan
    $logScript = Join-Path $PSScriptRoot 'logs.ps1'
    Start-Process powershell.exe -ArgumentList @('-NoExit', '-ExecutionPolicy', 'Bypass', '-File', "`"$logScript`"", '-Serial', $Serial)
}

Write-Host "DEPLOY COMPLETE: $BuildId on $Model" -ForegroundColor Green
