[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Debug',
    [switch]$Reconfigure
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

New-Item -ItemType Directory -Force -Path $BuildDir, $StateRoot, $LogDir | Out-Null

function Resolve-CMake {
    $command = Get-Command cmake -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $candidates = @(
        "$env:ProgramFiles\CMake\bin\cmake.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )

    return $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

$CMake = Resolve-CMake
if (-not $CMake) {
    throw 'CMake was not found. Install CMake or the Visual Studio C++ CMake tools.'
}

if ($Reconfigure -and (Test-Path $BuildDir)) {
    Remove-Item $BuildDir -Recurse -Force
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
}

Write-Host '[1/3] Configure native desktop host' -ForegroundColor Cyan
& $CMake -S $SourceDir -B $BuildDir -A Win32 *> $BuildLog
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
    executable = $Executable
    builtAt = (Get-Date).ToString('o')
} | ConvertTo-Json | Set-Content -Path $StatePath -Encoding UTF8

Write-Host '[3/3] Launch native desktop host' -ForegroundColor Cyan
Start-Process -FilePath $Executable -WorkingDirectory $RepoRoot
Write-Host "SUCCESS  Desktop $BuildId launched" -ForegroundColor Green
Write-Output $BuildId
