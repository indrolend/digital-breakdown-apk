# Physical humanoid locomotion lab

This is an isolated developer experiment for replacing visually animated enemy
locomotion with an articulated body that must support itself through contacts.
It is not linked into the shipping game.

The mature enemy motor remains the high-level authority for *intent*: desired
travel direction, speed, attack commitment, and bracing. In this lab those
signals are targets only. The body root is never translated or rotated by the
controller. Motion must emerge from force-limited joint motors, gravity,
collision, and foot contact.

## Setup (WSL2)

```bash
~/.local/bin/uv venv --python 3.12 ~/.local/share/digital-breakdown/physical-walker-venv
~/.local/bin/uv pip install \
  --python ~/.local/share/digital-breakdown/physical-walker-venv/bin/python \
  mujoco numpy pillow
```

## Run

From the repository root in WSL:

```bash
PY=~/.local/share/digital-breakdown/physical-walker-venv/bin/python
$PY experiments/physical-humanoid/physical_walker.py --mode fall --seconds 5
$PY experiments/physical-humanoid/physical_walker.py --mode stand --seconds 5
$PY experiments/physical-humanoid/physical_walker.py --mode walk --seconds 8 --desired-speed 0.7
$PY experiments/physical-humanoid/smoke_test.py
```

Add `--render artifacts/physical-walker.png` to preserve the final real
MuJoCo frame. Add `--viewer` for an interactive WSLg window.

## Current boundary

This milestone establishes the embodiment seam and honest failure conditions.
The bootstrap controller is intentionally small and deterministic; it is not
presented as learned locomotion. A later policy can replace `BootstrapPolicy`
while consuming the same observation and producing the same normalized joint
targets. The authored game motor would supply the desired velocity and facing
command, not body transforms.

The copied ideas are standard articulated-control techniques described by
DeepMimic and MuJoCo's public humanoid examples. The model and experiment code
here are original, deliberately compact, and covered by this repository's
license.
