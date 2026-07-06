$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

New-Item -ItemType Directory -Force ".\logs" | Out-Null

$Lines = adb logcat -d |
  Select-String -Pattern "STYLO|DBTERM|GAMEPAD|ReferenceError|TypeError|SyntaxError|WebGL|OutOfMemory|digitalbreakdown"

$Lines | Out-File ".\logs\latest-debug-output.txt" -Encoding utf8
$Lines | Set-Clipboard

Write-Host "Copied filtered debug log to clipboard."
Write-Host "Saved: logs/latest-debug-output.txt"
