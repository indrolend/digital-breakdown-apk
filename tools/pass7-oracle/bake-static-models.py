#!/usr/bin/env python3
"""Compatibility entry point for the authoritative native model generator.

Static model generation used to exist independently in Python and JavaScript.
Keeping two implementations allowed the retired embedded iPhone to overwrite
the original DATA device. Delegate to one generator so either historical
command now produces the same authoritative assets.
"""

from pathlib import Path
import shutil
import subprocess


SCRIPT = Path(__file__).with_name("generate-native-models.mjs")
NODE = shutil.which("node")

if NODE is None:
    raise SystemExit("node is required to generate native models")

raise SystemExit(subprocess.call([NODE, str(SCRIPT)]))
