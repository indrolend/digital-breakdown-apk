[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Debug',
    [ValidateSet('x64','Win32')]
    [string]$Architecture = 'x64',
    [switch]$Reconfigure,
    [switch]$BuildOnly,
    [switch]$SmokeTest
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$SourceDir = Join-Path $RepoRoot 'native-desktop'
$BuildDir = Join-Path $RepoRoot "build\desktop-$($Configuration.ToLowerInvariant())"
$StateRoot = Join-Path $env:LOCALAPPDATA 'DigitalBreakdownDev'
$StatePath = Join-Path $StateRoot 'desktop-build.json'
$LogDir = Join-Path $StateRoot 'logs'
$BuildLog = Join-Path $LogDir 'last-desktop-build.log'
$CMakeBootstrap = Join-Path $RepoRoot 'tools\bootstrap\ensure-cmake.ps1'

New-Item -ItemType Directory -Force -Path $BuildDir, $StateRoot, $LogDir | Out-Null

function Resolve-CMake {
    $command = Get-Command cmake -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $portable = Join-Path $env:LOCALAPPDATA 'DigitalBreakdownDev\tools\cmake-3.31.6\bin\cmake.exe'
    if (Test-Path $portable) { return $portable }

    $candidates = @(
        "$env:ProgramFiles\CMake\bin\cmake.exe",
        "${env:ProgramFiles(x86)}\CMake\bin\cmake.exe"
    ) | Where-Object { $_ -and (Test-Path $_) }

    if ($candidates.Count -gt 0) { return $candidates[0] }

    $visualStudioRoots = @(
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio'),
        $(if (${env:ProgramFiles(x86)}) { Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio' })
    ) | Where-Object { $_ -and (Test-Path $_) }

    foreach ($root in $visualStudioRoots) {
        $matches = Get-ChildItem -Path $root -Filter cmake.exe -File -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match 'CommonExtensions[\\/]Microsoft[\\/]CMake[\\/]CMake[\\/]bin[\\/]cmake\.exe$' } |
            Select-Object -First 1
        if ($matches) { return $matches.FullName }
    }

    return $null
}

$CMake = Resolve-CMake
if (-not $CMake) {
    if (-not (Test-Path $CMakeBootstrap)) {
        throw 'CMake was not found and the portable CMake bootstrap script is missing.'
    }

    $bootstrapOutput = @(& $CMakeBootstrap)
    if ($LASTEXITCODE -ne 0) {
        throw 'Portable CMake setup failed.'
    }

    $CMake = $bootstrapOutput |
        Where-Object { $_ -is [string] -and $_ -match 'cmake\.exe$' -and (Test-Path $_) } |
        Select-Object -Last 1

    if (-not $CMake) {
        $CMake = Resolve-CMake
    }
}

if (-not $CMake -or -not (Test-Path $CMake)) {
    throw 'CMake could not be resolved after automatic setup.'
}

Write-Host "Using CMake: $CMake" -ForegroundColor DarkGray

if ($Reconfigure -and (Test-Path $BuildDir)) {
    Remove-Item $BuildDir -Recurse -Force
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
}

Write-Host '[1/3] Configure native desktop host' -ForegroundColor Cyan
& $CMake -S $SourceDir -B $BuildDir -A $Architecture *> $BuildLog
if ($LASTEXITCODE -ne 0) {
    Get-Content $BuildLog -Tail 30 -ErrorAction SilentlyContinue
    throw "Desktop configure failed. See $BuildLog"
}

Write-Host '[2/3] Build native desktop host' -ForegroundColor Cyan
& $CMake --build $BuildDir --config $Configuration *>> $BuildLog
if ($LASTEXITCODE -ne 0) {
    Get-Content $BuildLog -Tail 30 -ErrorAction SilentlyContinue
    throw "Desktop build failed. See $BuildLog"
}

$ExecutableCandidates = @(
    (Join-Path $BuildDir "bin\$Configuration\DigitalBreakdown.exe"),
    (Join-Path $BuildDir 'bin\DigitalBreakdown.exe'),
    (Join-Path $BuildDir "$Configuration\DigitalBreakdown.exe")
)
$Executable = $ExecutableCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Executable) {
    throw "Desktop executable was not produced under $BuildDir"
}

$Commit = (& git -C $RepoRoot rev-parse --short HEAD 2>$null).Trim()
if (-not $Commit) { $Commit = 'unknown' }
$Dirty = [bool](& git -C $RepoRoot status --porcelain 2>$null)
$BuildId = if ($Dirty) { "$Commit-dirty" } else { $Commit }

[pscustomobject]@{
    shortCommit = $BuildId
    configuration = $Configuration
    architecture = $Architecture
    executable = $Executable
    builtAt = (Get-Date).ToString('o')
} | ConvertTo-Json | Set-Content -Path $StatePath -Encoding UTF8

if ($SmokeTest) {
    Write-Host '[3/3] Run native smoke test' -ForegroundColor Cyan
    & $Executable --smoke-test
    if ($LASTEXITCODE -ne 0) {
        throw "Desktop smoke test failed with code $LASTEXITCODE."
    }
    Write-Host "SUCCESS  Desktop $BuildId smoke test passed" -ForegroundColor Green
} elseif ($BuildOnly) {
    Write-Host '[3/3] Build complete' -ForegroundColor Cyan
    Write-Host "SUCCESS  Desktop $BuildId built" -ForegroundColor Green
} else {
    Write-Host '[3/3] Launch native desktop host' -ForegroundColor Cyan
    Start-Process -FilePath $Executable -WorkingDirectory $RepoRoot
    Write-Host "SUCCESS  Desktop $BuildId launched" -ForegroundColor Green
}

Write-Output $BuildId
