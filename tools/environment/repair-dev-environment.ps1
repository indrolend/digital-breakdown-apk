[CmdletBinding()]
param(
    [switch]$ClearDesktopCache
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Resolver = Join-Path $PSScriptRoot 'resolve-dev-environment.ps1'
$StateRoot = Join-Path $env:LOCALAPPDATA 'DigitalBreakdownDev'
$ToolRoot = Join-Path $StateRoot 'tools'
$LogRoot = Join-Path $StateRoot 'logs'

New-Item -ItemType Directory -Force -Path $StateRoot, $ToolRoot, $LogRoot | Out-Null

Write-Host 'Repairing project-owned development state...' -ForegroundColor Cyan

Get-ChildItem -Path $ToolRoot -Directory -Filter 'cmake-expand-*' -ErrorAction SilentlyContinue |
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue

if ($ClearDesktopCache) {
    foreach ($path in @(
        (Join-Path $RepoRoot 'build\desktop-debug'),
        (Join-Path $RepoRoot 'build\desktop-release')
    )) {
        if (Test-Path $path) {
            Write-Host "Removing disposable build cache: $path" -ForegroundColor DarkGray
            Remove-Item $path -Recurse -Force
        }
    }
}

$environment = & $Resolver -ProvisionCMake
if (-not $environment.cmake.available) {
    throw 'CMake could not be provisioned.'
}

Write-Host 'SAFE_REPAIR_OK' -ForegroundColor Green
$environment
