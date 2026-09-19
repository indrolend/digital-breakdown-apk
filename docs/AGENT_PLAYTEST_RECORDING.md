# Agent playtest recording

The developer-only `--agent-playtest` mode can preserve a short video demonstration while an agent plays the authoritative native game. Recording does not advance the game on its own and does not accept gameplay mutations. It captures the real `DesktopRenderer` output produced by ordinary `step` commands.

Launch from the repository root:

```powershell
.\build\desktop-release\bin\Release\DigitalBreakdown.exe --agent-playtest --capture-width 1280 --capture-height 720 --agent-frame .\artifacts\agent-playtest\current-frame.ppm
```

The interactive recording commands are:

```text
record start name=enemy-demo fps=30
record status
record stop
```

While recording, use normal `step ...` commands. The bridge renders and captures at the requested 15, 20, 30, or 60 frames per simulated second. The initial state is included as frame zero. `record stop` reports the frame directory, duration, and encoder tool. Recording names may contain letters, digits, hyphens, and underscores; an existing non-empty recording is never overwritten.

Encode the resulting silent MP4 with the repository tool:

```powershell
.\tools\encode-agent-playtest-video.ps1 -FramesDirectory .\artifacts\agent-playtest\recordings\enemy-demo -Framerate 30
```

The video is intentionally silent because frame-stepped agent decisions include arbitrary pauses between commands. It demonstrates rendered gameplay and physical consequences faithfully, but it should not be used to judge real-time reaction difficulty or audio timing.
