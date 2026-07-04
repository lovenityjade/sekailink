#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
DEFAULT_WHEELHOUSE = REPO / "runtime/tools/python/wheelhouse/win-amd64-cp312"

REQUIRED_WHEELS = {
    "setuptools",
    "wheel",
    "pyyaml",
    "pathspec",
    "platformdirs",
    "schema",
    "jinja2",
    "markupsafe",
    "colorama",
    "typing_extensions",
    "jellyfish",
    "bsdiff4",
    "websockets",
    "certifi",
    "orjson",
    "pillow",
    "python_dotenv",
    "pyaes",
    "docutils",
    "dolphin_memory_engine",
    "pyevermizer",
}

REQUIRED_IMPORTS = [
    "pkg_resources",
    "yaml",
    "pathspec",
    "platformdirs",
    "schema",
    "jinja2",
    "colorama",
    "typing_extensions",
    "jellyfish",
    "bsdiff4",
    "websockets",
    "certifi",
    "orjson",
    "PIL",
    "dotenv",
    "pyaes",
    "docutils",
    "dolphin_memory_engine",
    "pyevermizer",
]


def wheel_name(path: Path) -> str:
    name = path.name
    if not name.endswith(".whl"):
        return ""
    return re.split(r"-\d", name, maxsplit=1)[0].lower().replace("-", "_")


def verify_wheelhouse(path: Path) -> dict[str, object]:
    wheels = {wheel_name(p): p.name for p in path.glob("*.whl")}
    missing = sorted(REQUIRED_WHEELS - set(wheels))
    return {
        "ok": not missing,
        "path": str(path),
        "wheels": wheels,
        "missing": missing,
    }


def verify_python(python: Path) -> dict[str, object]:
    code = "\n".join([
        "import importlib, json, sys",
        "missing=[]",
        f"mods={REQUIRED_IMPORTS!r}",
        "ok_version=(3,12) <= sys.version_info[:2] < (3,14)",
        "for m in mods:",
        "    try: importlib.import_module(m)",
        "    except Exception as e: missing.append({'module': m, 'error': str(e)})",
        "print(json.dumps({'ok': ok_version and not missing, 'version': list(sys.version_info[:3]), 'missing': missing}))",
    ])
    proc = subprocess.run([str(python), "-c", code], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        payload = json.loads(proc.stdout.strip() or "{}")
    except json.JSONDecodeError:
        payload = {"ok": False, "missing": [], "stdout": proc.stdout, "stderr": proc.stderr}
    payload["python"] = str(python)
    payload["returncode"] = proc.returncode
    if proc.stderr:
        payload["stderr"] = proc.stderr.strip()
    return payload


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify SekaiLink bundled Python runtime inputs without network access.")
    parser.add_argument("--wheelhouse", type=Path, default=DEFAULT_WHEELHOUSE)
    parser.add_argument("--python", type=Path, default=None, help="Optional ready Python runtime to import-check.")
    args = parser.parse_args()

    report = {
        "wheelhouse": verify_wheelhouse(args.wheelhouse),
        "python": verify_python(args.python) if args.python else None,
    }
    report["ok"] = bool(report["wheelhouse"]["ok"] and (report["python"] is None or report["python"]["ok"]))
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
