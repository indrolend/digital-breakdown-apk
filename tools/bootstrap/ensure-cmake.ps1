[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ToolRoot = Join-Path $env:LOCALAPPDATA 'DigitalBreakdownDev\tools'
$Version = '3.31.6'
$ArchiveName = "cmake-$Version-windows-x86_64.zip"
$DownloadUrl = "https://github.com/Kitware/CMake/releases/download/v$Version/$ArchiveName"
$ArchivePath = Join-Path $ToolRoot $ArchiveName
$InstallRoot = Join-Path $ToolRoot "cmake-$Version"
$ExpectedExe = Join-Path $InstallRoot 'bin\cmake.exe'

New-Item -ItemType Directory -Force -Path $ToolRoot | Out-Null

if (Test-Path $ExpectedExe) {
    Write-Output $ExpectedExe
    exit 0
}

Write-Host "Preparing portable CMake $Version..." -ForegroundColor Cyan

if (-not (Test-Path $ArchivePath)) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $DownloadUrl -OutFile $ArchivePath -UseBasicParsing
}

$TempRoot = Join-Path $ToolRoot "cmake-expand-$([Guid]::NewGuid().ToString('N'))"
try {
    Expand-Archive -Path $ArchivePath -DestinationPath $TempRoot -Force
    $CMakeExe = Get-ChildItem -Path $TempRoot -Filter cmake.exe -File -Recurse |
        Where-Object { $_.FullName -match '[\\/]bin[\\/]cmake\.exe$' } |
        Select-Object -First 1

    if (-not $CMakeExe) {
        throw 'The portable CMake archive did not contain bin\cmake.exe.'
    }

    $ExtractedRoot = Split-Path (Split-Path $CMakeExe.FullName -Parent) -Parent
    if (Test-Path $InstallRoot) {
        Remove-Item $InstallRoot -Recurse -Force
    }
    Move-Item -Path $ExtractedRoot -Destination $InstallRoot
}
finally {
    if (Test-Path $TempRoot) {
        Remove-Item $TempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

if (-not (Test-Path $ExpectedExe)) {
    throw "Portable CMake installation failed: $ExpectedExe was not created."
}

Write-Host "Portable CMake ready: $ExpectedExe" -ForegroundColor Green
Write-Output $ExpectedExe
