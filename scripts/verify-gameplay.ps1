param(
    [string]$BuildDir = "build/gameplay-checks"
)

$ErrorActionPreference = "Stop"
$GameCpp = "native-android/app/src/main/cpp/game/Game.cpp"
$ReleaseDir = Join-Path $BuildDir "Release"

python tools/apply_local_settings_ownership.py
if ($LASTEXITCODE -ne 0) { throw "Local settings ownership plan check failed with exit code $LASTEXITCODE" }

python tools/apply_network_input_edge_latching.py
if ($LASTEXITCODE -ne 0) { throw "Network input edge latch plan check failed with exit code $LASTEXITCODE" }

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

python tools/apply_network_input_edge_latching.py --apply
if ($LASTEXITCODE -ne 0) { throw "Network input edge repair apply failed with exit code $LASTEXITCODE" }

try {
    cmake --build $BuildDir --config Release --target NetworkInputEdgeRepairValidationTest MultiplayerDeterminismTest HostRemotePeerSimulationIsolationTest --parallel
    if ($LASTEXITCODE -ne 0) { throw "Network input edge repair build failed with exit code $LASTEXITCODE" }

    & (Join-Path $ReleaseDir "NetworkInputEdgeRepairValidationTest.exe")
    if ($LASTEXITCODE -ne 0) { throw "NetworkInputEdgeRepairValidationTest failed with exit code $LASTEXITCODE" }

    & (Join-Path $ReleaseDir "MultiplayerDeterminismTest.exe")
    if ($LASTEXITCODE -ne 0) { throw "Patched MultiplayerDeterminismTest failed with exit code $LASTEXITCODE" }

    & (Join-Path $ReleaseDir "HostRemotePeerSimulationIsolationTest.exe")
    if ($LASTEXITCODE -ne 0) { throw "Patched HostRemotePeerSimulationIsolationTest failed with exit code $LASTEXITCODE" }
}
finally {
    git checkout -- $GameCpp
    if ($LASTEXITCODE -ne 0) { throw "Failed to restore Game.cpp after repair validation with exit code $LASTEXITCODE" }
}

git diff --check
if ($LASTEXITCODE -ne 0) { throw "git diff --check failed with exit code $LASTEXITCODE" }
