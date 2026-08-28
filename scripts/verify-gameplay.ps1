param(
    [string]$BuildDir = "build/gameplay-checks"
)

$ErrorActionPreference = "Stop"

cmake -S native-desktop -B $BuildDir -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }

cmake --build $BuildDir --config Release --target GameplayRoleAndSoulMotionTest GameplayGeometryAndConfigTest TargetLifecycleTest GameplayStateContractsTest PhoneBodyContractTest PhoneMenuLayoutTest PhoneDisplayStateTest Pass7ParityTest MultiplayerProtocolTest MultiplayerConnectionStateTest --parallel
if ($LASTEXITCODE -ne 0) { throw "Gameplay test build failed with exit code $LASTEXITCODE" }

ctest --test-dir $BuildDir -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Gameplay tests failed with exit code $LASTEXITCODE" }

& (Join-Path $BuildDir "Release/Pass7ParityTest.exe")
if ($LASTEXITCODE -ne 0) { throw "Pass7ParityTest failed with exit code $LASTEXITCODE" }

& (Join-Path $BuildDir "Release/MultiplayerProtocolTest.exe")
if ($LASTEXITCODE -ne 0) { throw "MultiplayerProtocolTest failed with exit code $LASTEXITCODE" }

git diff --check
if ($LASTEXITCODE -ne 0) { throw "git diff --check failed with exit code $LASTEXITCODE" }
