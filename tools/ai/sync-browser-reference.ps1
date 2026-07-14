param(
    [string]$RepositoryUrl = 'https://github.com/indrolend/digitalbreakdownreference.git',
    [string]$Ref = 'main'
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$referenceRoot = Join-Path $repoRoot 'reference'
$target = Join-Path $referenceRoot 'browser-pass7'

New-Item -ItemType Directory -Force -Path $referenceRoot | Out-Null

if (Test-Path (Join-Path $target '.git')) {
    git -C $target fetch --depth 1 origin $Ref
    if ($LASTEXITCODE -ne 0) { throw 'Failed to fetch authoritative browser reference.' }

    git -C $target checkout --detach FETCH_HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Failed to check out authoritative browser reference.' }
} elseif (Test-Path $target) {
    throw "Reference target exists but is not a Git repository: $target"
} else {
    git clone --depth 1 --branch $Ref $RepositoryUrl $target
    if ($LASTEXITCODE -ne 0) { throw 'Failed to clone authoritative browser reference.' }
}

$runtime = Join-Path $target 'index_module.mjs'
if (-not (Test-Path $runtime)) {
    throw "Authoritative runtime was not found after sync: $runtime"
}

$commit = (git -C $target rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or -not $commit) { throw 'Unable to resolve browser reference commit.' }

Write-Host "Authoritative browser reference ready."
Write-Host "Path: $target"
Write-Host "Commit: $commit"
