#!/usr/bin/env python3
"""Verify the native desktop runtime's authoritative assets."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSETS = (
    ("native-models/phone.dbmesh", b"DBM1"),
    ("native-models/human.dbhuman", b"DBH1"),
    ("native-models/flower.dbmesh", b"DBM1"),
    ("native-desktop/audio/game_music.mp3", None),
    ("native-desktop/audio/menu_music.mp3", None),
    ("native-desktop/audio/tv_room_pad.mp3", None),
    ("native-desktop/audio/game_over.mp3", None),
)

for relative, signature in ASSETS:
    path = ROOT / relative
    if not path.is_file() or path.stat().st_size == 0:
        raise SystemExit(f"Missing native runtime asset: {relative}")
    if signature is not None and path.read_bytes()[:4] != signature:
        raise SystemExit(f"Invalid native runtime asset: {relative}")

print(f"ASSETS=PASS count={len(ASSETS)}")
