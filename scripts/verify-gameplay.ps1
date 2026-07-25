param(
    [string]$BuildDir = "build/gameplay-checks"
)

$ErrorActionPreference = "Stop"

cmake -S native-desktop -B $BuildDir -DCMAKE_BUILD_TYPE=Release
cmake --build $BuildDir --config Release --target GameplayRoleAndSoulMotionTest GameplayGeometryAndConfigTest TargetLifecycleTest Pass7ParityTest MultiplayerProtocolTest --parallel
ctest --test-dir $BuildDir -C Release --output-on-failure

& (Join-Path $BuildDir "Release/Pass7ParityTest.exe")
if ($LASTEXITCODE -ne 0) { throw "Pass7ParityTest failed with exit code $LASTEXITCODE" }

& (Join-Path $BuildDir "Release/MultiplayerProtocolTest.exe")
if ($LASTEXITCODE -ne 0) { throw "MultiplayerProtocolTest failed with exit code $LASTEXITCODE" }

git diff --check
if ($LASTEXITCODE -ne 0) { throw "git diff --check failed with exit code $LASTEXITCODE" }
