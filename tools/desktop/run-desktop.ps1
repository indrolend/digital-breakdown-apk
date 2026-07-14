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
$EnvironmentResolver = Join-Path $RepoRoot 'tools\environment\resolve-dev-environment.ps1'
$BuildEnvironmentPath = Join-Path $BuildDir 'db-build-environment.json'

New-Item -ItemType Directory -Force -Path $StateRoot, $LogDir | Out-Null

function Write-Stage {
    param([string]$Name, [string]$Status)
    Write-Host ("[{0}] {1}" -f $Status.ToUpperInvariant(), $Name) -ForegroundColor $(if ($Status -eq 'ok') { 'Green' } elseif ($Status -eq 'repair') { 'Yellow' } else { 'Cyan' })
}

function Remove-BuildCache {
    if (Test-Path $BuildDir) {
        Write-Stage 'Removing incompatible desktop build cache' 'repair'
        Remove-Item $BuildDir -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
}

if (-not (Test-Path $EnvironmentResolver)) {
    throw 'Development environment resolver is missing.'
}

Write-Stage 'Inspect development environment' 'run'
$environment = & $EnvironmentResolver -ProvisionCMake
if (-not $environment.cmake.available -or -not $environment.cmake.path) {
    throw 'CMAKE_MISSING: CMake could not be resolved or provisioned.'
}
if (-not $environment.compiler.available -or -not $environment.compiler.generator) {
    throw 'CPP_TOOLCHAIN_MISSING: Install the Visual Studio Desktop development with C++ workload.'
}
Write-Stage ("CMake {0}" -f $environment.cmake.version) 'ok'
Write-Stage ("Compiler {0}" -f $environment.compiler.generator) 'ok'

$expectedBuildEnvironment = [ordered]@{
    schemaVersion = 1
    sourcePath = $SourceDir
    generator = $environment.compiler.generator
    architecture = $Architecture
    configuration = $Configuration
    cmakePath = $environment.cmake.path
    cmakeVersion = $environment.cmake.version
}
$expectedJson = $expectedBuildEnvironment | ConvertTo-Json

$cacheCompatible = $false
if (-not $Reconfigure -and (Test-Path $BuildEnvironmentPath)) {
    try {
        $existing = Get-Content $BuildEnvironmentPath -Raw | ConvertFrom-Json
        $cacheCompatible = (
            $existing.sourcePath -eq $expectedBuildEnvironment.sourcePath -and
            $existing.generator -eq $expectedBuildEnvironment.generator -and
            $existing.architecture -eq $expectedBuildEnvironment.architecture -and
            $existing.configuration -eq $expectedBuildEnvironment.configuration -and
            $existing.cmakePath -eq $expectedBuildEnvironment.cmakePath
        )
    } catch {
        $cacheCompatible = $false
    }
}

if ($Reconfigure -or ((Test-Path $BuildDir) -and -not $cacheCompatible)) {
    Remove-BuildCache
} else {
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
}

function Invoke-Configure {
    & $environment.cmake.path -S $SourceDir -B $BuildDir -G $environment.compiler.generator -A $Architecture *> $BuildLog
    return $LASTEXITCODE
}

Write-Stage 'Configure native desktop host' 'run'
$configureCode = Invoke-Configure
if ($configureCode -ne 0) {
    Write-Stage 'Configure failed; performing one clean recovery attempt' 'repair'
    Remove-BuildCache
    $configureCode = Invoke-Configure
}
if ($configureCode -ne 0) {
    Get-Content $BuildLog -Tail 40 -ErrorAction SilentlyContinue
    throw "CMAKE_CONFIGURE_FAILED: See $BuildLog"
}
$expectedJson | Set-Content -Path $BuildEnvironmentPath -Encoding UTF8
Write-Stage 'Configure native desktop host' 'ok'

Write-Stage 'Build native desktop host' 'run'
& $environment.cmake.path --build $BuildDir --config $Configuration *>> $BuildLog
if ($LASTEXITCODE -ne 0) {
    Get-Content $BuildLog -Tail 40 -ErrorAction SilentlyContinue
    throw "CPP_COMPILE_FAILED: See $BuildLog"
}
Write-Stage 'Build native desktop host' 'ok'

$ExecutableCandidates = @(
    (Join-Path $BuildDir "bin\$Configuration\DigitalBreakdown.exe"),
    (Join-Path $BuildDir 'bin\DigitalBreakdown.exe'),
    (Join-Path $BuildDir "$Configuration\DigitalBreakdown.exe")
)
$Executable = @($ExecutableCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1)
if ($Executable.Count -eq 0) {
    throw "DESKTOP_EXE_MISSING: Desktop executable was not produced under $BuildDir"
}
$ExecutablePath = $Executable[0]

$Commit = (& git -C $RepoRoot rev-parse --short HEAD 2>$null).Trim()
if (-not $Commit) { $Commit = 'unknown' }
$DirtyLines = @(& git -C $RepoRoot status --porcelain 2>$null)
$BuildId = if ($DirtyLines.Count -gt 0) { "$Commit-dirty" } else { $Commit }

[pscustomobject]@{
    shortCommit = $BuildId
    configuration = $Configuration
    architecture = $Architecture
    generator = $environment.compiler.generator
    cmake = $environment.cmake.version
    executable = $ExecutablePath
    builtAt = (Get-Date).ToString('o')
} | ConvertTo-Json | Set-Content -Path $StatePath -Encoding UTF8

if ($SmokeTest) {
    Write-Stage 'Run native smoke test' 'run'
    & $ExecutablePath --smoke-test
    if ($LASTEXITCODE -ne 0) {
        throw "DESKTOP_SMOKE_FAILED: code $LASTEXITCODE"
    }
    Write-Stage "Desktop $BuildId smoke test" 'ok'
} elseif ($BuildOnly) {
    Write-Stage "Desktop $BuildId build complete" 'ok'
} else {
    Write-Stage 'Launch native desktop host' 'run'
    Start-Process -FilePath $ExecutablePath -WorkingDirectory $RepoRoot
    Write-Stage "Desktop $BuildId launched" 'ok'
}

Write-Output $BuildId
