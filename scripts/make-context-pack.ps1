$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$OutDir = ".\context-pack"
$OutFile = ".\context-pack\mobile-port-context.txt"

Remove-Item $OutDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $OutDir | Out-Null

"" | Set-Content $OutFile -Encoding utf8

$Files = @(
  ".\package.json",
  ".\package-lock.json",
  ".\capacitor.config.json",
  ".\capacitor.config.ts",
  ".\www\index.html",
  ".\www\android-entry.mjs",
  ".\android\app\build.gradle",
  ".\android\build.gradle",
  ".\android\settings.gradle",
  ".\android\app\src\main\AndroidManifest.xml",
  ".\scripts\build-install-run.ps1",
  ".\scripts\copy-debug-log.ps1",
  ".\scripts\use-runtime.ps1",
  ".\logs\latest-logcat.txt",
  ".\logs\latest-device.txt",
  ".\logs\latest-source-markers.txt",
  ".\docs\MOBILE_PORT_STATUS.md"
)

foreach ($File in $Files) {
  if (Test-Path $File) {
    Add-Content $OutFile "`n`n===== FILE: $File =====`n" -Encoding utf8
    Get-Content $File -Raw | Add-Content $OutFile -Encoding utf8
  }
}

if (Test-Path ".\www\runtimes") {
  Get-ChildItem ".\www\runtimes" -Filter "*.mjs" | ForEach-Object {
    Add-Content $OutFile "`n`n===== FILE: $($_.FullName) =====`n" -Encoding utf8
    Get-Content $_.FullName -Raw | Add-Content $OutFile -Encoding utf8
  }
}

Write-Host "Wrote $OutFile"
