param(
    [Parameter(Position = 0)]
    [ValidateSet(
        'ui',
        'status',
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
    $DesktopExeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
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

    $adb = Get-Command adb -ErrorAction SilentlyContinue
    if ($adb) {
        $devices = @(& $adb.Source devices | Select-String '\sdevice$')
        Write-Host "Android    : $(if ($devices.Count) { 'authorized device connected' } else { 'no authorized device' })"
    } else {
        Write-Host 'Android    : adb not found'
    }
}

switch ($Command) {
    'ui' {
        if (-not (Test-Path $DevUiBuildScript)) { throw "Missing $DevUiBuildScript" }
        & $DevUiBuildScript -Launch
        if ($LASTEXITCODE -ne 0) { throw 'Developer UI build failed.' }
    }
    'status' {
        Show-Status
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
        $scrcpy = Get-Command scrcpy -ErrorAction SilentlyContinue
        if (-not $scrcpy) { throw 'scrcpy was not found.' }
        Invoke-Checked $scrcpy.Source @('--window-title=Digital Breakdown')
    }
    'release-windows' {
        & $ReleaseScript -Platform Windows -Launch
    }
    'release-android' {
        & $ReleaseScript -Platform Android
    }
    'release-all' {
        & $ReleaseScript -Platform All
    }
    'diagnostics' {
        Show-Status
        Write-Host ''
        Write-Host 'Tool availability:' -ForegroundColor Cyan
        foreach ($name in 'git','cmake','adb','scrcpy','java') {
            $tool = Get-Command $name -ErrorAction SilentlyContinue
            Write-Host ("{0,-8} {1}" -f $name, $(if ($tool) { $tool.Source } else { 'missing' }))
        }
    }
}
