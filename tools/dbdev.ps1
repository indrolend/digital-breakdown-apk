param(
    [Parameter(Position = 0)]
    [ValidateSet(
        'help',
        'ui',
        'status',
        'doctor',
        'repair',
        'sync',
        'desktop-build',
        'desktop-run',
        'desktop-test',
        'desktop-smoke',
        'playtest',
        'room-smoke',
        'android-build',
        'android-install',
        'android-stream',
        'release-windows',
        'release-android',
        'release-all',
        'diagnostics'
    )]
    [string]$Command = 'status',
    [ValidateSet('game','rally','traversal','rooms','tv-room','tv-enter')]
    [string]$Mode = 'game',
    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Release',
    [switch]$Automation,
    [switch]$Reconfigure
)

$ErrorActionPreference = 'Stop'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$DesktopScript = Join-Path $RepoRoot 'tools\desktop\run-desktop.ps1'
$ReleaseScript = Join-Path $RepoRoot 'tools\release\get-latest-native.ps1'
$AndroidDeployScript = Join-Path $RepoRoot 'tools\device\deploy-local.ps1'
$DevUiBuildScript = Join-Path $RepoRoot 'tools\dev-ui\build-dev-ui.ps1'
$EnvironmentResolver = Join-Path $RepoRoot 'tools\environment\resolve-dev-environment.ps1'
$EnvironmentRepair = Join-Path $RepoRoot 'tools\environment\repair-dev-environment.ps1'
$GameplayVerifier = Join-Path $RepoRoot 'scripts\verify-gameplay.ps1'
$NativeAndroidRoot = Join-Path $RepoRoot 'native-android'

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
    param([ValidateSet('Debug','Release')][string]$RequestedConfiguration = 'Release')
    $buildName = $RequestedConfiguration.ToLowerInvariant()
    $candidates = @(
        (Join-Path $RepoRoot "build\desktop-$buildName\bin\$RequestedConfiguration\DigitalBreakdown.exe"),
        (Join-Path $RepoRoot "build\desktop-$buildName\bin\DigitalBreakdown.exe")
    )
    $matches = @($candidates | Where-Object { Test-Path $_ } | Select-Object -First 1)
    if ($matches.Count -gt 0) { return $matches[0] }
    return $null
}

function Invoke-DesktopBuild {
    if (-not (Test-Path $DesktopScript)) { throw "Missing $DesktopScript" }
    if ($Reconfigure) { & $DesktopScript -Configuration $Configuration -BuildOnly -Reconfigure }
    else { & $DesktopScript -Configuration $Configuration -BuildOnly }
    if ($LASTEXITCODE -ne 0) { throw 'Desktop build failed.' }
}

function Start-DesktopGame {
    param([string[]]$GameArguments = @())
    $exe = Get-DesktopExe $Configuration
    if (-not $exe) { throw "Desktop $Configuration executable was not produced." }
    $existing = @(Get-Process -Name DigitalBreakdown -ErrorAction SilentlyContinue)
    if ($existing.Count -gt 0) { throw 'DESKTOP_ALREADY_RUNNING: Close the existing game before starting a playtest.' }
    # Windows PowerShell 5.1 can concatenate a string array passed through a
    # function boundary. These are closed-set, repo-owned flags, so construct
    # one explicit command line for Start-Process.
    Start-Process -FilePath $exe -WorkingDirectory $RepoRoot -ArgumentList ($GameArguments -join ' ')
    Write-Host "PLAYTEST_READY mode=$Mode automation=$($Automation.IsPresent) executable=$exe" -ForegroundColor Green
    if ($Automation) {
        $capture = Join-Path $env:LOCALAPPDATA 'DigitalBreakdownDev\captures\automation-latest.ppm'
        Write-Host "PLAYTEST_CAPTURE path=$capture" -ForegroundColor Green
    }
}

function Show-Help {
    Write-Host 'DIGITAL BREAKDOWN DEVELOPMENT COMMANDS' -ForegroundColor Cyan
    Write-Host '  status | doctor | repair | sync'
    Write-Host '  desktop-build [-Configuration Debug|Release] [-Reconfigure]'
    Write-Host '  desktop-run   [-Configuration Debug|Release] [-Reconfigure]'
    Write-Host '  desktop-test | desktop-smoke | room-smoke'
    Write-Host '  playtest -Mode game|rally|traversal|rooms|tv-room|tv-enter [-Automation]'
    Write-Host '  android-build | android-install | android-stream'
    Write-Host '  ui | release-windows | release-android | release-all | diagnostics'
}

function Get-Environment {
    param([switch]$ProvisionCMake)
    if (-not (Test-Path $EnvironmentResolver)) { throw "Missing $EnvironmentResolver" }
    if ($ProvisionCMake) { return & $EnvironmentResolver -ProvisionCMake }
    return & $EnvironmentResolver
}

function Ensure-NativeAndroidSdk {
    $localProperties = Join-Path $NativeAndroidRoot 'local.properties'
    if (Test-Path $localProperties) { return }

    $sdkCandidates = @(
        $env:ANDROID_HOME,
        $env:ANDROID_SDK_ROOT,
        $(if ($env:LOCALAPPDATA) { Join-Path $env:LOCALAPPDATA 'Android\Sdk' })
    ) | Where-Object { $_ -and (Test-Path $_) }
    $sdk = @($sdkCandidates | Select-Object -First 1)
    if ($sdk.Count -eq 0) {
        throw 'Android SDK not found. Set ANDROID_HOME or ANDROID_SDK_ROOT.'
    }

    $escapedSdk = $sdk[0].Replace('\', '\\')
    "sdk.dir=$escapedSdk" | Set-Content -LiteralPath $localProperties -Encoding ASCII
    Write-Host "Created machine-local Android SDK configuration: $localProperties"
}

function Show-Status {
    $git = Get-GitState
    Write-Host 'DIGITAL BREAKDOWN DEV' -ForegroundColor Green
    Write-Host "Repository : $RepoRoot"
    Write-Host "Branch     : $($git.Branch)"
    Write-Host "Commit     : $($git.Commit)$(if ($git.Dirty) { '-dirty' })"
    Write-Host "Local edits: $($git.DirtyCount)"

    $desktopExe = Get-DesktopExe 'Release'
    Write-Host "Desktop    : $(if ($desktopExe) { $desktopExe } else { 'not built' })"

    $environment = Get-Environment
    Write-Host "Android    : $(if ($environment.android.authorizedDevice) { 'authorized device connected' } elseif ($environment.android.adbAvailable) { 'no authorized device' } else { 'adb not found' })"
}

function Show-Doctor {
    $environment = Get-Environment
    $gitState = Get-GitState
    $desktopExe = Get-DesktopExe 'Release'

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
    'help' {
        Show-Help
    }
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
        if ($git.Dirty) { throw 'Local edits are present. Sync stopped without overwriting them.' }
        Invoke-Checked git @('-C', $RepoRoot, 'fetch', 'origin', 'main', '--prune')
        Invoke-Checked git @('-C', $RepoRoot, 'checkout', 'main')
        Invoke-Checked git @('-C', $RepoRoot, 'pull', '--ff-only', 'origin', 'main')
        Show-Status
    }
    'desktop-build' {
        Invoke-DesktopBuild
    }
    'desktop-run' {
        Invoke-DesktopBuild
        Start-DesktopGame
    }
    'desktop-test' {
        if (-not (Test-Path $GameplayVerifier)) { throw "Missing $GameplayVerifier" }
        & $GameplayVerifier
        if ($LASTEXITCODE -ne 0) { throw 'Desktop gameplay verification failed.' }
    }
    'desktop-smoke' {
        $exe = Get-DesktopExe $Configuration
        if (-not $exe) {
            Invoke-DesktopBuild
            $exe = Get-DesktopExe $Configuration
        }
        if (-not $exe) { throw 'Desktop executable was not produced.' }
        Invoke-Checked $exe @('--smoke-test')
    }
    'playtest' {
        Invoke-DesktopBuild
        [string[]]$arguments = @(switch ($Mode) {
            'rally' { @('--rally-lab') }
            'traversal' { @('--traversal-lab') }
            'rooms' { @('--room-inspector') }
            'tv-room' { @('--tv-room-test') }
            'tv-enter' { @('--tv-room-enter') }
            default { @() }
        })
        if ($Automation) { $arguments += '--automation-playtest' }
        Start-DesktopGame $arguments
    }
    'room-smoke' {
        Invoke-DesktopBuild
        $exe = Get-DesktopExe $Configuration
        Invoke-Checked $exe @('--room-inspector-smoke')
    }
    'android-build' {
        Ensure-NativeAndroidSdk
        Invoke-Checked (Join-Path $RepoRoot 'android\gradlew.bat') @('-p', (Join-Path $RepoRoot 'native-android'), 'assembleDebug', '--stacktrace')
    }
    'android-install' {
        Ensure-NativeAndroidSdk
        if (-not (Test-Path $AndroidDeployScript)) { throw "Missing $AndroidDeployScript" }
        & $AndroidDeployScript -NoMirror -NoLogs
        if ($LASTEXITCODE -ne 0) { throw 'Android deploy failed.' }
    }
    'android-stream' {
        Ensure-NativeAndroidSdk
        if (-not (Test-Path $AndroidDeployScript)) { throw "Missing $AndroidDeployScript" }
        & $AndroidDeployScript
        if ($LASTEXITCODE -ne 0) { throw 'Android deploy failed.' }
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
