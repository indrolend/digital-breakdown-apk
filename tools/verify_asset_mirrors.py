#!/usr/bin/env python3
"""Fail when platform-specific runtime asset mirrors drift apart."""

from hashlib import sha256
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PAIRS = (
    ("native-models/phone.dbmesh", "native-android/app/src/main/res/raw/phone.dbmesh"),
    ("native-models/human.dbhuman", "native-android/app/src/main/res/raw/human.dbhuman"),
    ("native-models/flower.dbmesh", "native-android/app/src/main/res/raw/flower.dbmesh"),
    ("native-desktop/audio/game_music.mp3", "native-android/app/src/main/res/raw/game_music.mp3"),
    ("native-desktop/audio/menu_music.mp3", "native-android/app/src/main/res/raw/menu_music.mp3"),
    ("native-desktop/audio/tv_room_pad.mp3", "native-android/app/src/main/res/raw/tv_room_pad.mp3"),
    ("native-desktop/audio/game_over.mp3", "native-android/app/src/main/res/raw/game_over.mp3"),
)


def digest(relative: str) -> str:
    path = ROOT / relative
    if not path.is_file():
        raise SystemExit(f"Missing mirrored runtime asset: {relative}")
    return sha256(path.read_bytes()).hexdigest()


drift = [(source, mirror) for source, mirror in PAIRS if digest(source) != digest(mirror)]
if drift:
    for source, mirror in drift:
        print(f"ASSET_MIRROR_DRIFT source={source} mirror={mirror}")
    raise SystemExit(1)

print(f"ASSET_MIRRORS=PASS count={len(PAIRS)}")
