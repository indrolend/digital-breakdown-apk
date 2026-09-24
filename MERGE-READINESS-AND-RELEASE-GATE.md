# Merge Readiness and Release Gate

## Purpose
Convert the accumulated physical-enemy and environment authority into a branch that can be reviewed and merged without relying on hidden manual checks.

## Cleanup performed
- Registered twenty existing focused regression programs in the default native CTest suite.
- Kept assertions active for those contracts in Release builds.
- Gave the two source-inspection contracts an explicit repository-root working directory.
- Advanced the relay protocol constant from 9 to 10 to match the native replicated snapshot expansion. Native gameplay version remains 8 and save format remains 4.

No gameplay manager, recovery system, rendering pipeline, or compatibility shim was added. The accumulated gameplay and presentation behavior is unchanged by this stabilization pass.

## Verification
- Windows storefront Release build: PASS.
- Native CTest: 59/59 PASS.
- Multiplayer relay tests: 13/13 PASS.
- Protocol consistency: PASS (`protocol=10`, `gameplay=8`).
- Strict MSVC C++20 `Game.cpp` compile with `/W4 /WX`: PASS.
- Staged storefront identity: PASS (`storefront_release=true`, `developer_console=false`).
- Staged executable smoke: PASS.
- Room-inspector smoke: PASS.
- Production dependency audit: 0 known vulnerabilities.

## Release recommendation
Do not publish the public release from this Windows-only verification result. Merge the branch, run the required Windows/macOS/Linux release workflow and aggregate manifest verification, then perform real-hardware graphics and controller acceptance. Complete the asset-license and attribution review before publication. If those gates pass without a release-critical defect, no broad gameplay rewrite is warranted; otherwise move the release date rather than waiving a failed gate.

## Known limitations
- macOS and Linux builds were not run locally.
- Full interactive visual, audio, controller, multiplayer-service, and long-session acceptance were not performed.
- The original `digital-breakdown-apk` checkout had unrelated uncommitted renderer-contract changes and was intentionally not modified.
