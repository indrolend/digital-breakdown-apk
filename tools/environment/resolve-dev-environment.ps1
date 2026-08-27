[CmdletBinding()]
param(
    [switch]$ProvisionCMake,
    [switch]$AsJson
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$CMakeBootstrap = Join-Path $RepoRoot 'tools\bootstrap\ensure-cmake.ps1'
$PrivateCMake = Join-Path $env:LOCALAPPDATA 'DigitalBreakdownDev\tools\cmake-3.31.6\bin\cmake.exe'

function First-ExistingPath {
    param([string[]]$Paths)
    foreach ($path in @($Paths)) {
        if ($path -and (Test-Path $path)) { return $path }
    }
    return $null
}

function Resolve-CommandPath {
    param([string]$Name)
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    return $null
}

function Resolve-CMakePath {
    $direct = First-ExistingPath @(
        (Resolve-CommandPath 'cmake'),
        $PrivateCMake,
        (Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe'),
        $(if (${env:ProgramFiles(x86)}) { Join-Path ${env:ProgramFiles(x86)} 'CMake\bin\cmake.exe' })
    )
    if ($direct) { return $direct }

    $roots = @(
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio'),
        $(if (${env:ProgramFiles(x86)}) { Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio' })
    ) | Where-Object { $_ -and (Test-Path $_) }

    foreach ($root in @($roots)) {
        $match = @(Get-ChildItem -Path $root -Filter cmake.exe -File -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match 'CommonExtensions[\\/]Microsoft[\\/]CMake[\\/]CMake[\\/]bin[\\/]cmake\.exe$' } |
            Select-Object -First 1)
        if ($match.Count -gt 0) { return $match[0].FullName }
    }

    return $null
}

function Resolve-VsWherePath {
    First-ExistingPath @(
        (Resolve-CommandPath 'vswhere'),
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe')
    )
}

function Resolve-VisualStudio {
    $vswhere = Resolve-VsWherePath
    if (-not $vswhere) {
        return [pscustomobject]@{ available = $false; installationPath = $null; generator = $null; version = $null }
    }

    $json = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $json) {
        return [pscustomobject]@{ available = $false; installationPath = $null; generator = $null; version = $null }
    }

    $items = @($json | ConvertFrom-Json | ForEach-Object { $_ })
    if ($items.Count -eq 0) {
        return [pscustomobject]@{ available = $false; installationPath = $null; generator = $null; version = $null }
    }

    $item = $items[0]
    $major = 0
    if ($item.installationVersion -match '^(\d+)\.') { $major = [int]$Matches[1] }
    $year = switch ($major) {
        18 { '2026' }
        17 { '2022' }
        16 { '2019' }
        default { $null }
    }
    $generator = if ($year) { "Visual Studio $major $year" } else { $null }

    [pscustomobject]@{
        available = [bool]$generator
        installationPath = $item.installationPath
        generator = $generator
        version = $item.installationVersion
    }
}

$cmakePath = Resolve-CMakePath
if (-not $cmakePath -and $ProvisionCMake) {
    if (-not (Test-Path $CMakeBootstrap)) { throw 'Portable CMake bootstrap script is missing.' }
    & $CMakeBootstrap | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Portable CMake provisioning failed.' }
    $cmakePath = Resolve-CMakePath
}

$cmakeVersion = $null
if ($cmakePath) {
    $versionLine = @(& $cmakePath --version 2>$null | Select-Object -First 1)
    if ($versionLine.Count -gt 0) { $cmakeVersion = $versionLine[0] }
}

$visualStudio = Resolve-VisualStudio
$adb = Resolve-CommandPath 'adb'
$scrcpy = Resolve-CommandPath 'scrcpy'
$java = Resolve-CommandPath 'java'
$git = Resolve-CommandPath 'git'

$authorizedDevices = @()
if ($adb) {
    $authorizedDevices = @(& $adb devices 2>$null | Select-String '\sdevice$')
}

$result = [pscustomobject]@{
    repository = [pscustomobject]@{ available = (Test-Path (Join-Path $RepoRoot '.git')); path = $RepoRoot }
    git = [pscustomobject]@{ available = [bool]$git; path = $git }
    cmake = [pscustomobject]@{ available = [bool]$cmakePath; path = $cmakePath; version = $cmakeVersion; source = $(if ($cmakePath -eq $PrivateCMake) { 'private' } elseif ($cmakePath) { 'system' } else { 'missing' }) }
    compiler = $visualStudio
    android = [pscustomobject]@{
        adbAvailable = [bool]$adb
        adbPath = $adb
        authorizedDevice = ($authorizedDevices.Count -gt 0)
        scrcpyAvailable = [bool]$scrcpy
        scrcpyPath = $scrcpy
        javaAvailable = [bool]$java
        javaPath = $java
    }
}

if ($AsJson) {
    $result | ConvertTo-Json -Depth 6
} else {
    $result
}
