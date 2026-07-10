#!/usr/bin/env python3
from __future__ import annotations

import json
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "dist" / "research-packet"
ZIP = ROOT / "dist" / "digital-breakdown-runtime-research.zip"

EXPLICIT_FILES = [
    "package.json",
    "capacitor.config.json",
    "www/android-entry.mjs",
    "www/runtimes/stylo-v2.mjs",
    "native/tests/sim_smoke_test.cpp",
    "native-android/app/build.gradle",
    "native-android/build.gradle",
    "native-android/settings.gradle",
    "native-android/app/src/main/AndroidManifest.xml",
    "native-android/app/src/main/cpp/CMakeLists.txt",
    "native-android/app/src/main/cpp/native_bridge.cpp",
    "scripts/native-android.sh",
    "scripts/dev-menu.mjs",
    "research/RESEARCH_PROMPT.md",
]

TREE_RULES = [
    ("native/core", {".hpp", ".h", ".cpp", ".cc", ".cxx"}),
    ("native-android/app/src/main/cpp/game", {".hpp", ".h", ".cpp", ".cc", ".cxx"}),
    ("native-android/app/src/main/cpp/render", {".hpp", ".h", ".cpp", ".cc", ".cxx", ".glsl", ".vert", ".frag"}),
    ("native-android/app/src/main/java/com/indrolend/digitalbreakdown", {".java"}),
    ("native-android/app/src/main/res/values", {".xml"}),
]

IMPORT_RE = re.compile(r'(?:import\s+(?:[^\"\'()]+?\s+from\s+)?|export\s+[^\"\'()]+?\s+from\s+|import\s*\()\s*[\"\']([^\"\']+)[\"\']')


def git(*args: str) -> str:
    try:
        return subprocess.check_output(["git", "-C", str(ROOT), *args], text=True).strip()
    except Exception:
        return ""


def copy_file(rel: str) -> None:
    src = ROOT / rel
    if not src.is_file():
        return
    dst = OUT / rel
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def copy_tree(rel: str, allowed_suffixes: set[str]) -> None:
    base = ROOT / rel
    if not base.exists():
        return
    for src in base.rglob("*"):
        if src.is_file() and src.suffix.lower() in allowed_suffixes:
            copy_file(src.relative_to(ROOT).as_posix())


def resolve_local_import(from_rel: str, spec: str) -> str | None:
    if not spec.startswith("."):
        return None
    base = (Path(from_rel).parent / spec)
    candidates = [
        base,
        Path(f"{base}.mjs"),
        Path(f"{base}.js"),
        Path(f"{base}.json"),
        base / "index.mjs",
        base / "index.js",
    ]
    for candidate in candidates:
        if (ROOT / candidate).is_file():
            return candidate.as_posix()
    return None


def collect_js(rel: str, visited: set[str]) -> None:
    if rel in visited:
        return
    src = ROOT / rel
    if not src.is_file():
        return
    visited.add(rel)
    copy_file(rel)
    text = src.read_text(encoding="utf-8-sig", errors="replace")
    for match in IMPORT_RE.finditer(text):
        resolved = resolve_local_import(rel, match.group(1))
        if resolved:
            collect_js(resolved, visited)


def main() -> int:
    if not (ROOT / ".git").exists():
        print(f"ERROR: {ROOT} is not a Git checkout", file=sys.stderr)
        return 2

    shutil.rmtree(OUT, ignore_errors=True)
    ZIP.parent.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)

    for rel in EXPLICIT_FILES:
        copy_file(rel)

    for rel, suffixes in TREE_RULES:
        copy_tree(rel, suffixes)

    visited: set[str] = set()
    collect_js("www/runtimes/stylo-v2.mjs", visited)
    collect_js("www/android-entry.mjs", visited)

    metadata = {
        "repository": "indrolend/digital-breakdown-apk",
        "commit": git("rev-parse", "HEAD"),
        "branch": git("branch", "--show-current"),
        "status": git("status", "--short"),
        "recentCommits": git("log", "-8", "--oneline", "--decorate").splitlines(),
    }
    (OUT / "SNAPSHOT.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")

    manifest = sorted(p.relative_to(OUT).as_posix() for p in OUT.rglob("*") if p.is_file())
    (OUT / "MANIFEST.txt").write_text("\n".join(manifest) + "\n", encoding="utf-8")

    if ZIP.exists():
        ZIP.unlink()
    with zipfile.ZipFile(ZIP, "w", zipfile.ZIP_DEFLATED) as archive:
        for src in sorted(OUT.rglob("*")):
            if src.is_file():
                archive.write(src, src.relative_to(OUT))

    print(ZIP)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
