$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

New-Item -ItemType Directory -Force ".\logs" | Out-Null

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
adb install -r .\app\build\outputs\apk\debug\app-debug.apk
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== Clear, launch, capture =="
adb shell pm clear com.indrolend.digitalbreakdown | Out-Null
adb logcat -c
adb shell monkey -p com.indrolend.digitalbreakdown 1 | Out-Null

Start-Sleep -Seconds 5

Set-Location $Root

adb logcat -d |
  Select-String -Pattern "STYLO|DBTERM|GAMEPAD|ReferenceError|TypeError|SyntaxError|WebGL|OutOfMemory|F DEBUG|digitalbreakdown" |
  Out-File ".\logs\latest-logcat.txt" -Encoding utf8

adb exec-out screencap -p > ".\logs\latest-screen.png"

Write-Host "Done."
Write-Host "Log: .\logs\latest-logcat.txt"
Write-Host "Screen: .\logs\latest-screen.png"
