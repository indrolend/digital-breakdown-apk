#!/usr/bin/env python3
"""Apply the first narrow ownership migration: LocalSettingsState.

Default mode is a dry-run contract check. Pass --apply to rewrite the three
known production seams. Every replacement is exact and count-checked so this
script refuses to operate if the source has drifted.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]

GAME_HPP = ROOT / "native-android/app/src/main/cpp/game/Game.hpp"
DESKTOP_MAIN = ROOT / "native-desktop/main.cpp"
ANDROID_BRIDGE = ROOT / "native-android/app/src/main/cpp/native_bridge.cpp"

REPLACEMENTS = {
    GAME_HPP: [
        (
            "    void setPersistentProgression(std::int64_t tokens, int shotLevel, int lungeLevel, int attackLevel);\n",
            "    void setPersistentProgression(std::int64_t tokens, int shotLevel, int lungeLevel, int attackLevel);\n"
            "    void setLocalSettings(const LocalSettingsState& settings) { state_.localSettings = settings; }\n",
            "declare explicit local-settings owner API",
        ),
    ],
    DESKTOP_MAIN: [
        (
            "    if(version>=2)game.networkMutableState().localSettings=settings;\n",
            "    if(version>=2)game.setLocalSettings(settings);\n",
            "route desktop persistence through settings API",
        ),
    ],
    ANDROID_BRIDGE: [
        (
            "extern \"C\" JNIEXPORT void JNICALL Java_com_indrolend_digitalbreakdown_NativeBridge_setLocalSettings(JNIEnv*,jclass,jfloat music,jfloat sfx,jboolean musicMuted,jboolean sfxMuted,jint preset,jboolean shadows,jboolean portal,jboolean particles,jboolean fps){auto& settings=gGame.networkMutableState().localSettings;settings.musicVolume=clampf(music,0,1);settings.sfxVolume=clampf(sfx,0,1);settings.musicMuted=musicMuted==JNI_TRUE;settings.sfxMuted=sfxMuted==JNI_TRUE;settings.graphicsPreset=std::max(0,std::min(2,static_cast<int>(preset)));settings.shadows=shadows==JNI_TRUE;settings.portalWindow=portal==JNI_TRUE;settings.particles=particles==JNI_TRUE;settings.fpsCounter=fps==JNI_TRUE;settings.mobileFraming=true;}\n",
            "extern \"C\" JNIEXPORT void JNICALL Java_com_indrolend_digitalbreakdown_NativeBridge_setLocalSettings(JNIEnv*,jclass,jfloat music,jfloat sfx,jboolean musicMuted,jboolean sfxMuted,jint preset,jboolean shadows,jboolean portal,jboolean particles,jboolean fps){LocalSettingsState settings=gGame.state().localSettings;settings.musicVolume=clampf(music,0,1);settings.sfxVolume=clampf(sfx,0,1);settings.musicMuted=musicMuted==JNI_TRUE;settings.sfxMuted=sfxMuted==JNI_TRUE;settings.graphicsPreset=std::max(0,std::min(2,static_cast<int>(preset)));settings.shadows=shadows==JNI_TRUE;settings.portalWindow=portal==JNI_TRUE;settings.particles=particles==JNI_TRUE;settings.fpsCounter=fps==JNI_TRUE;settings.mobileFraming=true;gGame.setLocalSettings(settings);}\n",
            "route Android JNI settings through settings API",
        ),
    ],
}


def apply_exact(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one source match, found {count}")
    return text.replace(old, new, 1)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true", help="write the ownership migration")
    args = parser.parse_args()

    proposed: dict[Path, str] = {}
    try:
        for path, replacements in REPLACEMENTS.items():
            text = path.read_text(encoding="utf-8")
            for old, new, label in replacements:
                text = apply_exact(text, old, new, label)
                print(f"LOCAL_SETTINGS_OWNERSHIP_READY {path.relative_to(ROOT)} {label}")
            proposed[path] = text
    except (OSError, RuntimeError) as exc:
        print(f"LOCAL_SETTINGS_OWNERSHIP_FAIL {exc}", file=sys.stderr)
        return 1

    if not args.apply:
        print("LOCAL_SETTINGS_OWNERSHIP_CHECK=PASS")
        return 0

    for path, text in proposed.items():
        path.write_text(text, encoding="utf-8")
    print("LOCAL_SETTINGS_OWNERSHIP_APPLIED=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
