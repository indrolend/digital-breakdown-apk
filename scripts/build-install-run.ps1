$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

New-Item -ItemType Directory -Force ".\logs" | Out-Null

$SdkAdb = Join-Path $env:LOCALAPPDATA "Android\Sdk\platform-tools\adb.exe"
$Adb = if (Test-Path $SdkAdb) { $SdkAdb } else { "adb" }

function Invoke-Adb {
  & $Adb @args
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Get-AdbDeviceLines {
  & $Adb devices -l | Select-Object -Skip 1 | Where-Object { $_.Trim().Length -gt 0 }
}

Write-Host "== ADB preflight =="
Write-Host "ADB: $Adb"
& $Adb kill-server | Out-Null
& $Adb start-server | Out-Null
$DeviceLines = @(Get-AdbDeviceLines)

if ($DeviceLines.Count -eq 0) {
  Write-Host "No ADB devices found. Windows may see the phone as MTP without binding the ADB interface." -ForegroundColor Yellow
  Write-Host "Device entries matching LG/Android/ADB/MTP/USB:" -ForegroundColor Yellow
  Get-PnpDevice -PresentOnly |
    Where-Object { $_.FriendlyName -match "LG|Android|ADB|MTP|Stylo|USB|LGE" } |
    Sort-Object Class, FriendlyName |
    Format-Table Status, Class, FriendlyName, InstanceId -AutoSize
  Write-Host "Fix on phone/Windows, then re-run: .\scripts\build-install-run.ps1" -ForegroundColor Yellow
  exit 2
}

$Authorized = @($DeviceLines | Where-Object { $_ -match "\sdevice\s" })
if ($Authorized.Count -eq 0) {
  Write-Host "ADB sees a device, but it is not authorized yet:" -ForegroundColor Yellow
  $DeviceLines | ForEach-Object { Write-Host $_ }
  Write-Host "Unlock the phone, accept the USB debugging prompt, then re-run." -ForegroundColor Yellow
  exit 3
}

$DeviceLines | ForEach-Object { Write-Host $_ }

Write-Host "== Build web bundle =="
npx esbuild .\www\android-entry.mjs --bundle --platform=browser --format=iife --target=chrome61 --outfile=.\www\android-bundle.js --log-level=info
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== Capacitor sync =="
npx cap sync android
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== Gradle build =="
Set-Location ".\android"
.\gradlew.bat assembleDebug
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== Install APK =="
Invoke-Adb install -r .\app\build\outputs\apk\debug\app-debug.apk

Write-Host "== Clear, launch, capture =="
Invoke-Adb shell pm clear com.indrolend.digitalbreakdown | Out-Null
Invoke-Adb logcat -c
Invoke-Adb shell monkey -p com.indrolend.digitalbreakdown 1 | Out-Null

Start-Sleep -Seconds 5

Set-Location $Root

& $Adb logcat -d |
  Select-String -Pattern "STYLO|DBTERM|GAMEPAD|ReferenceError|TypeError|SyntaxError|WebGL|OutOfMemory|F DEBUG|digitalbreakdown" |
  Out-File ".\logs\latest-logcat.txt" -Encoding utf8

& $Adb exec-out screencap -p > ".\logs\latest-screen.png"

Write-Host "Done."
Write-Host "Log: .\logs\latest-logcat.txt"
Write-Host "Screen: .\logs\latest-screen.png"
