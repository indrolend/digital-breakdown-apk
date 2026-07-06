param(
  [Parameter(Mandatory=$true)]
  [string]$Runtime
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$RuntimePath = ".\www\runtimes\$Runtime"

if (!(Test-Path $RuntimePath)) {
  throw "Runtime not found: $RuntimePath"
}

Copy-Item ".\www\android-entry.mjs" ".\www\android-entry.previous.mjs" -Force
Copy-Item $RuntimePath ".\www\android-entry.mjs" -Force

Write-Host "android-entry.mjs now uses: $Runtime"
