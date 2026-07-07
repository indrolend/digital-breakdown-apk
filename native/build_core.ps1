$ErrorActionPreference = "Stop"

$BuildDir = Join-Path $PSScriptRoot "build"
New-Item -ItemType Directory -Force $BuildDir | Out-Null

$cxx = if (Get-Command clang++ -ErrorAction SilentlyContinue) { "clang++" } else { "g++" }

& $cxx `
  -std=c++17 `
  -Wall `
  -Wextra `
  -pedantic `
  -I "$PSScriptRoot/core" `
  "$PSScriptRoot/core/simulation.cpp" `
  "$PSScriptRoot/core/render_state.cpp" `
  "$PSScriptRoot/tests/sim_smoke_test.cpp" `
  -o "$BuildDir/db_sim_smoke.exe"

& "$BuildDir/db_sim_smoke.exe"
