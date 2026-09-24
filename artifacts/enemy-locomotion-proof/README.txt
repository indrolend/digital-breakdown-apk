Digital Breakdown physical enemy locomotion proof

Build:
  cmake -S native-desktop -B C:\db-build\enemy-locomotion -A x64
  cmake --build C:\db-build\enemy-locomotion --config Release --target EnemyLocomotionProbe DigitalBreakdown --parallel

Deterministic contract run:
  C:\db-build\enemy-locomotion\Release\EnemyLocomotionProbe.exe

Regenerate telemetry:
  C:\db-build\enemy-locomotion\Release\EnemyLocomotionProbe.exe --telemetry artifacts\enemy-locomotion-proof\telemetry.csv

Playable smoke:
  C:\db-build\enemy-locomotion\bin\Release\DigitalBreakdown.exe --smoke-test

The CSV contains deterministic walk, 90-degree turn, moderate-shove, and
strong-shove scenarios. Foot phases are: 0 Planted, 1 Unloading, 2 Swing,
3 SeekingContact, 4 Loading. The strong-shove trace includes failed support,
fall, gathered feet, re-established two-foot support, and supported rise.
