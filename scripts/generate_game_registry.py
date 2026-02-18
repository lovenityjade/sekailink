import json
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]
DOCS_ROOT = ROOT / "WebHostLib" / "static" / "generated" / "docs"
OUT_DIR = ROOT / "client" / "registry"
OUT_DIR.mkdir(parents=True, exist_ok=True)

SETUP_FILES = set(DOCS_ROOT.rglob("setup_en.md")) | set(DOCS_ROOT.rglob("*_setup_en.md"))

EMULATOR_RE = re.compile(
    r"\\b(BizHawk|RetroArch|Dolphin|PCSX2|DuckStation|PPSSPP|Cemu|Ryujinx|yuzu|Eden|"
    r"melonDS|DeSmuME|Snes9x|Mesen|mGBA|VBA|VisualBoyAdvance|bsnes|OpenEmu|MAME|"
    r"DOSBox|ScummVM|OpenGOAL|OpenRCT2|gzDoom|GZDoom)\\b",
    re.IGNORECASE,
)
MOD_RE = re.compile(r"\\b(BepInEx|SMAPI|Fabric|Forge|UnityExplorer|Thunderstore|r2modman)\\b", re.IGNORECASE)
PATCH_RE = re.compile(r"\\b(patch|patched|bps|ips|xdelta|rom|iso|cue|apworld)\\b", re.IGNORECASE)
CONNECT_RE = re.compile(r"\\b(connect|server|host:port|slot|password|/connect)\\b", re.IGNORECASE)


def game_id_from_path(path: pathlib.Path) -> str:
    parts = path.parts
    if "docs" in parts:
        idx = parts.index("docs")
        if idx + 1 < len(parts):
            return parts[idx + 1]
    return path.parent.name


def display_name(game_id: str) -> str:
    return game_id.replace("_", " ")


registry = []

for path in sorted(SETUP_FILES):
    text = path.read_text(encoding="utf-8", errors="ignore")
    game_id = game_id_from_path(path)

    emulators = sorted({m.group(0) for m in EMULATOR_RE.finditer(text)})
    mods = sorted({m.group(0) for m in MOD_RE.finditer(text)})
    patches = bool(PATCH_RE.search(text))
    connects = [ln.strip() for ln in text.splitlines() if CONNECT_RE.search(ln)]

    entry = {
        "game_id": game_id,
        "display_name": display_name(game_id),
        "platform": "unknown",
        "driver": {"family": "unknown", "core_id": ""},
        "patch_mode": "ap_patch" if patches else "none",
        "required_assets": [],
        "launch_steps": [],
        "connect_steps": [],
        "setup_files": [str(path.relative_to(DOCS_ROOT))],
        "notes": "",
        "hints": {
            "emulators": emulators,
            "mod_loaders": mods,
            "connect_examples": connects[:5],
        },
    }

    registry.append(entry)

out_path = OUT_DIR / "games.generated.json"
out_path.write_text(json.dumps(registry, indent=2), encoding="utf-8")
print(f"Wrote {out_path} ({len(registry)} entries)")
