$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

python tools/apply_gameplay_role_adoption.py --check
python tools/apply_gameplay_role_adoption.py --write
& "$PSScriptRoot\verify-gameplay.ps1"

Write-Host "Gameplay helper adoption applied and verified."
