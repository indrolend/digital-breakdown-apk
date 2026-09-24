param(
    [int]$Rooms = 100,
    [int]$MaxSteps = 1200,
    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$DbDev = Join-Path $RepoRoot 'tools\dbdev.ps1'

& $DbDev desktop-build -Configuration $Configuration
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }

& $DbDev room-smoke -Configuration $Configuration
if ($LASTEXITCODE -ne 0) { throw 'Room smoke failed.' }

$exe = @(
    "$RepoRoot\build\desktop-$($Configuration.ToLower())\bin\$Configuration\DigitalBreakdown.exe"
    "$RepoRoot\build\desktop-$($Configuration.ToLower())\bin\DigitalBreakdown.exe"
    "$RepoRoot\build-enemy-runtime\bin\$Configuration\DigitalBreakdown.exe"
) | Where-Object { Test-Path $_ } |
    Select-Object -First 1

if (-not $exe) {
    throw 'DigitalBreakdown.exe missing.'
}

$outRoot = Join-Path $env:LOCALAPPDATA 'DigitalBreakdownDev\autonomous'
New-Item -ItemType Directory -Force -Path $outRoot | Out-Null

$summary = Join-Path $outRoot 'latest.csv'

"run,step,frame,room,seed,environment,x,y,z,action,clear,dead" |
    Set-Content $summary

function Parse-State([string]$line) {
    if ($line -notmatch '^AGENT_STATE ') { return $null }

    $values = @{}
    foreach ($token in ($line -split ' ' | Select-Object -Skip 1)) {
        $pair = $token -split '=',2
        if ($pair.Count -eq 2) {
            $values[$pair[0]] = $pair[1]
        }
    }

    return $values
}

for ($run = 0; $run -lt $Rooms; $run++) {
    Write-Host "AUTONOMOUS_RUN run=$run"

    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $exe
    $psi.WorkingDirectory = $RepoRoot
    $psi.Arguments = '--agent-playtest'
    $psi.UseShellExecute = $false
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $false

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $psi

    if (-not $process.Start()) {
        throw 'Agent launch failed.'
    }

    $state = $null

    while (-not $process.StandardOutput.EndOfStream) {
        $line = $process.StandardOutput.ReadLine()
        Write-Host $line

        $candidate = Parse-State $line
        if ($candidate) {
            $state = $candidate
            break
        }
    }

    for ($step = 0; $step -lt $MaxSteps; $step++) {
        if (-not $state) { break }

        $position = $state.pos -split ','
        $action = $state.action

        "$run,$step,$($state.frame),$($state.room),$($state.seed),$($state.environment),$($position[0]),$($position[1]),$($position[2]),$action,$($state.clear),$($state.dead)" |
            Add-Content $summary

        if ($state.dead -eq '1') {
            Write-Host "AUTONOMOUS_DEAD run=$run step=$step"
            break
        }

        if ($action -eq 'ledge_hang') {
            $command = 'step frames=20 moveZ=1'
        }
        elseif ($step % 90 -eq 70) {
            $command = 'step frames=30 moveZ=1 jump=1 sprint=1'
        }
        elseif ($step % 150 -gt 125) {
            $command = 'step frames=24 moveZ=1 lookX=18 sprint=1'
        }
        elseif ($step % 110 -gt 95) {
            $command = 'step frames=20 moveX=0.6 moveZ=1 sprint=1'
        }
        else {
            $command = 'step frames=24 moveZ=1 sprint=1'
        }

        $process.StandardInput.WriteLine($command)
        $process.StandardInput.Flush()

        $state = $null

        while (-not $process.StandardOutput.EndOfStream) {
            $line = $process.StandardOutput.ReadLine()
            Write-Host $line

            $candidate = Parse-State $line
            if ($candidate) {
                $state = $candidate
                break
            }
        }
    }

    if (-not $process.HasExited) {
        $process.StandardInput.WriteLine('quit')
        $process.StandardInput.Flush()

        if (-not $process.WaitForExit(3000)) {
            $process.Kill($true)
        }
    }
}

Write-Host ''
Write-Host "AUTONOMOUS_RESULTS=$summary"

& $DbDev desktop-test
