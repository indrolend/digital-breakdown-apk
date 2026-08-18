param(
    [string]$BuildDir = "build/gameplay-checks"
)

$ErrorActionPreference = "Stop"
$ReleaseDir = Join-Path $BuildDir "Release"

python tools/check_ownership_boundaries.py
if ($LASTEXITCODE -ne 0) { throw "Ownership boundary check failed with exit code $LASTEXITCODE" }

cmake -S native-desktop -B $BuildDir -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw "Gameplay configure failed with exit code $LASTEXITCODE" }

cmake --build $BuildDir --config Release --target DigitalBreakdown GameplayRoleAndSoulMotionTest GameplayGeometryAndConfigTest TargetLifecycleTest GameplayStateContractsTest PhoneBodyContractTest PhoneMenuLayoutTest MenuNavigationTest PhoneDisplayStateTest EarlyBrowserVisualsTest TraversalCalibrationTest Pass7ParityTest MultiplayerProtocolTest MultiplayerDeterminismTest HostRemotePeerSimulationIsolationTest NetworkInputEdgeCharacterizationTest MobileFramingRemoteAuthorityTest MultiplayerConnectionStateTest GameplayFlowProbe RoomProgressionProbe DeterministicInputSoak --parallel
if ($LASTEXITCODE -ne 0) { throw "Gameplay build failed with exit code $LASTEXITCODE" }

ctest --test-dir $BuildDir -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "CTest failed with exit code $LASTEXITCODE" }

& (Join-Path $ReleaseDir "Pass7ParityTest.exe")
if ($LASTEXITCODE -ne 0) { throw "Pass7ParityTest failed with exit code $LASTEXITCODE" }

& (Join-Path $ReleaseDir "MultiplayerProtocolTest.exe")
if ($LASTEXITCODE -ne 0) { throw "MultiplayerProtocolTest failed with exit code $LASTEXITCODE" }

& (Join-Path $ReleaseDir "MultiplayerDeterminismTest.exe")
if ($LASTEXITCODE -ne 0) { throw "MultiplayerDeterminismTest failed with exit code $LASTEXITCODE" }

& (Join-Path $ReleaseDir "HostRemotePeerSimulationIsolationTest.exe")
if ($LASTEXITCODE -ne 0) { throw "HostRemotePeerSimulationIsolationTest failed with exit code $LASTEXITCODE" }

& (Join-Path $ReleaseDir "NetworkInputEdgeCharacterizationTest.exe")
if ($LASTEXITCODE -ne 0) { throw "NetworkInputEdgeCharacterizationTest failed with exit code $LASTEXITCODE" }

& (Join-Path $ReleaseDir "MobileFramingRemoteAuthorityTest.exe")
if ($LASTEXITCODE -ne 0) { throw "MobileFramingRemoteAuthorityTest failed with exit code $LASTEXITCODE" }

git diff --check
if ($LASTEXITCODE -ne 0) { throw "git diff --check failed with exit code $LASTEXITCODE" }
