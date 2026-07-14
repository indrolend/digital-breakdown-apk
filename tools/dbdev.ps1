param(
    [Parameter(Position = 0)]
    [ValidateSet(
        'ui',
        'status',
        'doctor',
        'repair',
        'sync',
        'desktop-build',
        'desktop-run',
        'desktop-smoke',
        'android-build',
        'android-install',
        'android-stream',
        'release-windows',
        'release-android',
        'release-all',
        'diagnostics'
    )]
    [string]$Command = 'status'
)

$ErrorActionPreference = 'Stop'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$DesktopScript = Join-Path $RepoRoot 'tools\desktop\run-desktop.ps1'
$ReleaseScript = Join-Path $RepoRoot 'tools\release\get-latest-native.ps1'
$AndroidDeployScript = Join-Path $RepoRoot 'tools\device\deploy-local.ps1'
$DevUiBuildScript = Join-Path $RepoRoot 'tools\dev-ui\build-dev-ui.ps1'
$EnvironmentResolver = Join-Path $RepoRoot 'tools\environment\resolve-dev-environment.ps1'
$EnvironmentRepair = Join-Path $RepoRoot 'tools\environment\repair-dev-environment.ps1'
$DesktopExeCandidates = @(
    (Join-Path $RepoRoot 'build\desktop-debug\bin\Debug\DigitalBreakdown.exe'),
    (Join-Path $RepoRoot 'build\desktop-debug\bin\DigitalBreakdown.exe'),
    (Join-Path $RepoRoot 'build\desktop-release\bin\Release\DigitalBreakdown.exe'),
    (Join-Path $RepoRoot 'build\desktop-release\bin\DigitalBreakdown.exe')
)

function Invoke-Checked {
    param(
        [Parameter(Mandatory)] [string]$FilePath,
        [string[]]$ArgumentList = @()
    )

    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath exited with code $LASTEXITCODE."
    }
}

function Get-GitState {
    $commit = (& git -C $RepoRoot rev-parse --short HEAD).Trim()
    $branch = (& git -C $RepoRoot branch --show-current).Trim()
    $dirtyLines = @(& git -C $RepoRoot status --porcelain)
    [pscustomobject]@{
        Branch = $branch
        Commit = $commit
        Dirty = $dirtyLines.Count -gt 0
        DirtyCount = $dirtyLines.Count
    }
}

function Get-DesktopExe {
    $matches = @($DesktopExeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1)
    if ($matches.Count -gt 0) { return $matches[0] }
    return $null
}

function Get-Environment {
    param([switch]$ProvisionCMake)
    if (-not (Test-Path $EnvironmentResolver)) { throw "Missing $EnvironmentResolver" }
    if ($ProvisionCMake) { return & $EnvironmentResolver -ProvisionCMake }
    return & $EnvironmentResolver
}

function Show-Status {
    $git = Get-GitState
    Write-Host 'DIGITAL BREAKDOWN DEV' -ForegroundColor Green
    Write-Host "Repository : $RepoRoot"
    Write-Host "Branch     : $($git.Branch)"
    Write-Host "Commit     : $($git.Commit)$(if ($git.Dirty) { '-dirty' })"
    Write-Host "Local edits: $($git.DirtyCount)"

    $desktopExe = Get-DesktopExe
    Write-Host "Desktop    : $(if ($desktopExe) { $desktopExe } else { 'not built' })"

    $environment = Get-Environment
    Write-Host "Android    : $(if ($environment.android.authorizedDevice) { 'authorized device connected' } elseif ($environment.android.adbAvailable) { 'no authorized device' } else { 'adb not found' })"
}

function Show-Doctor {
    $environment = Get-Environment
    $gitState = Get-GitState
    $desktopExe = Get-DesktopExe

    Write-Host 'DIGITAL BREAKDOWN DOCTOR' -ForegroundColor Cyan
    Write-Host ("{0,-18} {1}" -f 'Repository', $(if ($environment.repository.available) { 'ready' } else { 'missing' }))
    Write-Host ("{0,-18} {1}" -f 'Git', $(if ($environment.git.available) { 'ready' } else { 'missing' }))
    Write-Host ("{0,-18} {1}" -f 'CMake', $(if ($environment.cmake.available) { "$($environment.cmake.version) [$($environment.cmake.source)]" } else { 'missing; repairable' }))
    Write-Host ("{0,-18} {1}" -f 'C++ compiler', $(if ($environment.compiler.available) { $environment.compiler.generator } else { 'missing; manual install required' }))
    Write-Host ("{0,-18} {1}" -f 'Desktop build', $(if ($desktopExe) { 'ready' } else { 'not built' }))
    Write-Host ("{0,-18} {1}" -f 'ADB', $(if ($environment.android.adbAvailable) { 'ready' } else { 'missing' }))
    Write-Host ("{0,-18} {1}" -f 'Stylo 4', $(if ($environment.android.authorizedDevice) { 'authorized' } else { 'not authorized/connected' }))
    Write-Host ("{0,-18} {1}" -f 'scrcpy', $(if ($environment.android.scrcpyAvailable) { 'ready' } else { 'missing' }))
    Write-Host ("{0,-18} {1}" -f 'Java', $(if ($environment.android.javaAvailable) { 'ready' } else { 'missing' }))
    Write-Host ("{0,-18} {1}" -f 'Working tree', $(if ($gitState.Dirty) { "$($gitState.DirtyCount) local changes" } else { 'clean' }))
}

switch ($Command) {
    'ui' {
        if (-not (Test-Path $DevUiBuildScript)) { throw "Missing $DevUiBuildScript" }
        & $DevUiBuildScript -Launch
        if ($LASTEXITCODE -ne 0) { throw 'Developer UI build failed.' }
    }
    'status' { Show-Status }
    'doctor' { Show-Doctor }
    'repair' {
        if (-not (Test-Path $EnvironmentRepair)) { throw "Missing $EnvironmentRepair" }
        & $EnvironmentRepair -ClearDesktopCache
        if ($LASTEXITCODE -ne 0) { throw 'Safe environment repair failed.' }
        Show-Doctor
    }
    'sync' {
        $git = Get-GitState
        if ($git.Dirty) {
            throw 'Local edits are present. Sync stopped without overwriting them.'
        }
        Invoke-Checked git @('-C', $RepoRoot, 'fetch', 'origin', 'main', '--prune')
        Invoke-Checked git @('-C', $RepoRoot, 'checkout', 'main')
        Invoke-Checked git @('-C', $RepoRoot, 'pull', '--ff-only', 'origin', 'main')
        Show-Status
    }
    'desktop-build' {
        if (-not (Test-Path $DesktopScript)) { throw "Missing $DesktopScript" }
        & $DesktopScript -BuildOnly
        if ($LASTEXITCODE -ne 0) { throw 'Desktop build failed.' }
    }
    'desktop-run' {
        if (-not (Test-Path $DesktopScript)) { throw "Missing $DesktopScript" }
        & $DesktopScript
        if ($LASTEXITCODE -ne 0) { throw 'Desktop build/run failed.' }
    }
    'desktop-smoke' {
        $exe = Get-DesktopExe
        if (-not $exe) {
            & $DesktopScript -BuildOnly
            if ($LASTEXITCODE -ne 0) { throw 'Desktop build failed.' }
            $exe = Get-DesktopExe
        }
        if (-not $exe) { throw 'Desktop executable was not produced.' }
        Invoke-Checked $exe @('--smoke-test')
    }
    'android-build' {
        Invoke-Checked (Join-Path $RepoRoot 'android\gradlew.bat') @('-p', (Join-Path $RepoRoot 'native-android'), 'assembleDebug', '--stacktrace')
    }
    'android-install' {
        if (-not (Test-Path $AndroidDeployScript)) { throw "Missing $AndroidDeployScript" }
        & $AndroidDeployScript
        if ($LASTEXITCODE -ne 0) { throw 'Android deploy failed.' }
    }
    'android-stream' {
        if (-not (Test-Path $AndroidDeployScript)) { throw "Missing $AndroidDeployScript" }
        & $AndroidDeployScript
        if ($LASTEXITCODE -ne 0) { throw 'Android deploy failed.' }
        $environment = Get-Environment
        if (-not $environment.android.scrcpyAvailable) { throw 'SCRCPY_MISSING: scrcpy was not found.' }
        Invoke-Checked $environment.android.scrcpyPath @('--window-title=Digital Breakdown')
    }
    'release-windows' { & $ReleaseScript -Platform Windows -Launch }
    'release-android' { & $ReleaseScript -Platform Android }
    'release-all' { & $ReleaseScript -Platform All }
    'diagnostics' {
        Show-Status
        Write-Host ''
        Show-Doctor
    }
}
