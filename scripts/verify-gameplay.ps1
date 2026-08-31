param(
    [string]$BuildDir = "build/gameplay-checks"
)

$ErrorActionPreference = "Stop"

$timer = [System.Diagnostics.Stopwatch]::StartNew()
$cachePath = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path -LiteralPath $cachePath) {
    $timer.Stop()
    Write-Output ("NATIVE_STAGE=PASS name=configure durationSeconds={0:F3} cached=true" -f $timer.Elapsed.TotalSeconds)
} else {
    cmake -S native-desktop -B $BuildDir -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { throw "Gameplay configure failed with exit code $LASTEXITCODE" }
    $timer.Stop()
    Write-Output ("NATIVE_STAGE=PASS name=configure durationSeconds={0:F3} cached=false" -f $timer.Elapsed.TotalSeconds)
}

$timer.Restart()
cmake --build $BuildDir --config Release --target DigitalBreakdown GameplayRoleAndSoulMotionTest GameplayGeometryAndConfigTest TargetLifecycleTest GameplayStateContractsTest PhoneBodyContractTest PhoneMenuLayoutTest MenuNavigationTest PhoneDisplayStateTest EarlyBrowserVisualsTest RenderContractsTest DesktopPlaytestPolicyTest DeveloperCodecTest SoulEconomyTest SoulProjectileLifecycleTest TraversalCalibrationTest Pass7ParityTest MultiplayerProtocolTest MultiplayerDeterminismTest HostRemotePeerSimulationIsolationTest MultiplayerConnectionStateTest GameplayFlowProbe RoomProgressionProbe DeterministicInputSoak --parallel
if ($LASTEXITCODE -ne 0) { throw "Gameplay build failed with exit code $LASTEXITCODE" }
$timer.Stop()
Write-Output ("NATIVE_STAGE=PASS name=build durationSeconds={0:F3}" -f $timer.Elapsed.TotalSeconds)

$timer.Restart()
ctest --test-dir $BuildDir -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "CTest failed with exit code $LASTEXITCODE" }
$timer.Stop()
Write-Output ("NATIVE_STAGE=PASS name=ctest durationSeconds={0:F3}" -f $timer.Elapsed.TotalSeconds)

$timer.Restart()
& (Join-Path $BuildDir "Release/Pass7ParityTest.exe")
if ($LASTEXITCODE -ne 0) { throw "Pass7ParityTest failed with exit code $LASTEXITCODE" }

& (Join-Path $BuildDir "Release/MultiplayerProtocolTest.exe")
if ($LASTEXITCODE -ne 0) { throw "MultiplayerProtocolTest failed with exit code $LASTEXITCODE" }

& (Join-Path $BuildDir "Release/MultiplayerDeterminismTest.exe")
if ($LASTEXITCODE -ne 0) { throw "MultiplayerDeterminismTest failed with exit code $LASTEXITCODE" }

& (Join-Path $BuildDir "Release/HostRemotePeerSimulationIsolationTest.exe")
if ($LASTEXITCODE -ne 0) { throw "HostRemotePeerSimulationIsolationTest failed with exit code $LASTEXITCODE" }
$timer.Stop()
Write-Output ("NATIVE_STAGE=PASS name=evidence durationSeconds={0:F3}" -f $timer.Elapsed.TotalSeconds)

$timer.Restart()
git diff --check
if ($LASTEXITCODE -ne 0) { throw "git diff --check failed with exit code $LASTEXITCODE" }
$timer.Stop()
Write-Output ("NATIVE_STAGE=PASS name=diff durationSeconds={0:F3}" -f $timer.Elapsed.TotalSeconds)

Write-Output "NATIVE_VERIFICATION=PASS suite=gameplay"
