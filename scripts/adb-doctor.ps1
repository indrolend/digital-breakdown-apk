$ErrorActionPreference = "Continue"

$Repo = Split-Path -Parent $PSScriptRoot
Set-Location $Repo

$Adb = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe"
if (!(Test-Path $Adb)) { $Adb = "adb" }

Write-Host "`n== ADB ==" -ForegroundColor Cyan
& $Adb kill-server | Out-Null
& $Adb start-server
& $Adb devices -l

Write-Host "`n== Active LG / Android / ADB devices ==" -ForegroundColor Cyan
Get-PnpDevice -PresentOnly |
  Where-Object {
    $_.InstanceId -match "VID_1004|PID_633E|PID_61F9|PID_631D|ADB|ANDROID|LGE" -or
    $_.FriendlyName -match "LG|Stylo|Android|ADB|LGE|MTP"
  } |
  Sort-Object Class,FriendlyName |
  Format-Table Status,Class,FriendlyName,InstanceId -AutoSize

Write-Host "`n== Repo ==" -ForegroundColor Cyan
git status --short
git log -1 --oneline
