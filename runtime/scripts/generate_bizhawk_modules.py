#!/usr/bin/env python3
"""
Generate runtime modules for BizHawk-based ROM patch worlds (GBA/SNES/NES/GB/GBC).

This is intentionally static (no imports from Archipelago code) so it can run in
minimal environments without python deps.

It scans `worlds/<world>` for:
- display name: `archipelago.json` -> `game`
- patch extension: `patch_file_ending = ".ap..."`
- base ROM requirements: `copy_to = "<name>.<ext>"` and `md5s = [...]`
  - md5s can reference constants (`MD5_US`) or `<PatchClass>.hash`.
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import re
import shutil
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple


# Accept both:
# - patch_file_ending = ".apfoo"
# - patch_file_ending: str = ".apfoo"
RE_PATCH_ENDING = re.compile(r'patch_file_ending\b[^=]*=\s*"(?P<ext>\.ap[a-zA-Z0-9_]+)"')
RE_COPY_TO = re.compile(r'copy_to\s*=\s*"(?P<name>[^"]+)"')
RE_MD5S_LIST = re.compile(r'md5s\s*=\s*\[(?P<body>[^\]]*)\]', re.MULTILINE | re.DOTALL)
RE_MD5S_TOKEN = re.compile(r'md5s\s*=\s*(?P<tok>[A-Za-z_][A-Za-z0-9_\.]*)')
RE_HASH_ASSIGN_LINE = re.compile(r'^\s*hash\s*=\s*(?P<rhs>[^#\r\n]+)', re.MULTILINE)
RE_CONST_HASH = re.compile(
    r'^(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*[\'"](?P<hash>[0-9A-Fa-f]{32})[\'"]\s*(?:#.*)?$',
    re.MULTILINE,
)
RE_STR_MD5 = re.compile(r'"(?P<hash>[0-9A-Fa-f]{32})"')
RE_STR_MD5_ANY = re.compile(r'[\'"](?P<hash>[0-9A-Fa-f]{32})[\'"]')
RE_GAME_VALUE = re.compile(r'\bgame\s*[:=]\s*"(?P<game>[^"]+)"')


@dataclass
class WorldInfo:
    world: str
    display_name: str
    patch_exts: List[str]
    rom_exts: List[str]
    md5s: List[str]


def read_text(p: Path) -> str:
    return p.read_text(encoding="utf-8", errors="ignore")


def find_patch_endings(world_dir: Path) -> List[str]:
    exts: List[str] = []
    for py in world_dir.rglob("*.py"):
        for m in RE_PATCH_ENDING.finditer(read_text(py)):
            exts.append(m.group("ext").lower())
    # de-dup preserving order
    out: List[str] = []
    for e in exts:
        if e not in out:
            out.append(e)
    return out


def find_display_name(world_dir: Path) -> str:
    ap_json = world_dir / "archipelago.json"
    if ap_json.exists():
        try:
            data = json.loads(ap_json.read_text(encoding="utf-8"))
            game = data.get("game")
            if isinstance(game, str) and game.strip():
                return game.strip()
        except Exception:
            pass
    return ""


def find_display_name_fallback_from_init(world_dir: Path) -> str:
    init_py = world_dir / "__init__.py"
    if not init_py.exists():
        return ""
    text = read_text(init_py)
    # Prefer the AutoWorld game string if present.
    # This catches both `game: str = "..."` and `game = "..."`.
    m = RE_GAME_VALUE.search(text)
    if m:
        g = m.group("game").strip()
        if g:
            return g
    return ""

def find_display_name_fallback_any(world_dir: Path) -> str:
    # Some worlds don't ship archipelago.json. Fall back to scanning for `game = "..."`
    # and pick the most frequent value.
    counts: Dict[str, int] = {}
    for py in world_dir.rglob("*.py"):
        text = read_text(py)
        for m in RE_GAME_VALUE.finditer(text):
            g = m.group("game").strip()
            if not g:
                continue
            counts[g] = counts.get(g, 0) + 1
    if not counts:
        return ""
    # Prefer longer names; break ties by frequency.
    best = sorted(counts.items(), key=lambda kv: (-len(kv[0]), -kv[1], kv[0]))[0][0]
    return best


def find_rom_exts(world_dir: Path) -> List[str]:
    init_py = world_dir / "__init__.py"
    if not init_py.exists():
        return []
    text = read_text(init_py)
    m = RE_COPY_TO.search(text)
    if not m:
        return []
    name = m.group("name").strip()
    ext = Path(name).suffix.lower()
    return [ext] if ext else []


def parse_md5_tokens(body: str) -> List[str]:
    # Tokens are usually: "deadbeef...", MD5_US, SomePatch.hash
    # We keep them as raw strings for later resolution.
    tokens: List[str] = []
    for part in body.split(","):
        tok = part.strip()
        if not tok:
            continue
        tok = tok.split("#", 1)[0].strip()
        if not tok:
            continue
        tokens.append(tok)
    return tokens


def resolve_md5s(world_dir: Path) -> List[str]:
    init_py = world_dir / "__init__.py"
    if not init_py.exists():
        return []
    text = read_text(init_py)

    tokens: List[str] = []
    m_list = RE_MD5S_LIST.search(text)
    if m_list:
        tokens = parse_md5_tokens(m_list.group("body"))
    else:
        m_tok = RE_MD5S_TOKEN.search(text)
        if m_tok:
            tokens = [m_tok.group("tok").strip()]
    if not tokens:
        # Some worlds don't define md5s in settings; fall back to a single `md5 = "<hex>"` constant if present.
        for py in world_dir.rglob("*.py"):
            t = read_text(py)
            mm = re.search(r'\bmd5\s*=\s*[\'"]([0-9A-Fa-f]{32})[\'"]', t)
            if mm:
                return [mm.group(1).lower()]
        return []

    # Build a map of constant name -> md5 value from all .py files in the world.
    consts: Dict[str, str] = {}
    for py in world_dir.rglob("*.py"):
        t = read_text(py)
        for cm in RE_CONST_HASH.finditer(t):
            consts[cm.group("name")] = cm.group("hash").lower()

    # Also capture any direct string md5s in the md5 list itself (when list literal is used).
    direct_src = m_list.group("body") if m_list else ""
    direct = [sm.group("hash").lower() for sm in RE_STR_MD5.finditer(direct_src)]

    resolved: List[str] = []
    for d in direct:
        if d not in resolved:
            resolved.append(d)

    for tok in tokens:
        if tok.startswith('"') and tok.endswith('"'):
            v = tok.strip('"').lower()
            if re.fullmatch(r"[0-9a-f]{32}", v) and v not in resolved:
                resolved.append(v)
            continue
        if tok.endswith(".hash"):
            class_name = tok.split(".", 1)[0]
            # Find the class definition file and resolve `hash = ...` in its header.
            found = False
            for py in world_dir.rglob("*.py"):
                text_py = read_text(py)
                if f"class {class_name}" not in text_py:
                    continue
                # Look for hash assignment within the first ~50 lines after class.
                parts = text_py.splitlines()
                for i, line in enumerate(parts):
                    if line.strip().startswith(f"class {class_name}"):
                        window = "\n".join(parts[i : i + 60])
                        hm = RE_HASH_ASSIGN_LINE.search(window)
                        if not hm:
                            continue
                        rhs = hm.group("rhs").strip()
                        values: List[str] = []
                        if rhs.startswith("[") and "]" in rhs:
                            inner = rhs.split("]", 1)[0].lstrip("[").strip()
                            values = [v.strip() for v in inner.split(",") if v.strip()]
                        else:
                            values = [rhs]

                        for vtok in values:
                            vtok = vtok.strip().strip("[]")
                            if (vtok.startswith('"') and vtok.endswith('"')) or (vtok.startswith("'") and vtok.endswith("'")):
                                v = vtok.strip("\"'").lower()
                                if re.fullmatch(r"[0-9a-f]{32}", v) and v not in resolved:
                                    resolved.append(v)
                                found = True
                                continue
                            if vtok in consts:
                                v = consts[vtok]
                                if v not in resolved:
                                    resolved.append(v)
                                found = True
                                continue
                            if re.fullmatch(r"[0-9A-Fa-f]{32}", vtok):
                                v = vtok.lower()
                                if v not in resolved:
                                    resolved.append(v)
                                found = True
                                continue
                        if found:
                            break
                        # As a last resort, grab the first 32-hex string in this window.
                        am = RE_STR_MD5_ANY.search(window)
                        if am:
                            v = am.group("hash").lower()
                            if v not in resolved:
                                resolved.append(v)
                            found = True
                            break
                if found:
                    break
            continue
        if tok in consts:
            v = consts[tok]
            if v not in resolved:
                resolved.append(v)

    return resolved


def world_info(world: str) -> Optional[WorldInfo]:
    world_dir = Path("worlds") / world
    if not world_dir.exists():
        return None
    patch_exts = find_patch_endings(world_dir)
    if not patch_exts:
        return None
    display_name = (
        find_display_name(world_dir)
        or find_display_name_fallback_from_init(world_dir)
        or find_display_name_fallback_any(world_dir)
        or world_dir.name
    )
    rom_exts = find_rom_exts(world_dir)
    md5s = resolve_md5s(world_dir)
    return WorldInfo(world=world, display_name=display_name, patch_exts=patch_exts, rom_exts=rom_exts, md5s=md5s)


def write_module(info: WorldInfo, modules_dir: Path, connector_src: Path, force: bool) -> None:
    module_id = f"{info.world}_bizhawk"
    dest = modules_dir / module_id
    lua_dir = dest / "lua"
    lua_dir.mkdir(parents=True, exist_ok=True)

    connector_dst = lua_dir / "connector.lua"
    if force or not connector_dst.exists():
        shutil.copy2(connector_src, connector_dst)

    manifest = {
        "game_id": info.world,
        "display_name": info.display_name,
        "emu": "bizhawk",
        "lua_connector": "lua/connector.lua",
        "patcher": "archipelago",
        "patch_extensions": info.patch_exts,
        "rom_requirements": {
            "extensions": info.rom_exts,
            "md5": info.md5s,
            "sha1": [],
        },
        "features": {
            "autotracking": True,
            "save_required": True,
        },
    }

    dest.mkdir(parents=True, exist_ok=True)
    (dest / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--force", action="store_true", help="Overwrite connector.lua if it exists")
    ap.add_argument("--world", action="append", default=[], help="World folder name (repeatable)")
    args = ap.parse_args()

    worlds = args.world or []
    if not worlds:
        ap.error("Provide at least one --world <folder> to generate.")

    modules_dir = Path("client/runtime/modules")
    connector_src = modules_dir / "bizhawk_base" / "lua" / "connector.lua"
    if not connector_src.exists():
        raise SystemExit(f"Missing BizHawk base connector at {connector_src}")

    for w in worlds:
        info = world_info(w)
        if not info:
            print(f"[skip] {w}: missing patch_file_ending or world not found")
            continue
        write_module(info, modules_dir, connector_src, force=args.force)
        print(f"[ok] {w} -> {w}_bizhawk")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
