#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import re
import shutil
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

import requests


REPO = Path(__file__).resolve().parents[2]
CATALOG = REPO / "apps/client-core/src/data/sekailinkGameCatalog.ts"
INDEX = REPO / "runtime/game-registry/archipelago-resource-index.json"
OUT_DIR = REPO / "runtime/downloaded-resources/sekailink-supported"
REPORT = OUT_DIR / "supported-resource-download-report.json"


CATALOG_ALIASES: dict[str, list[str]] = {
    "a_link_to_the_past": ["A Link to the Past"],
    "a_link_between_worlds": ["A Link Between Worlds", "The Legend of Zelda: A Link Between Worlds"],
    "chrono_trigger": ["Chrono Trigger"],
    "donkey_kong_64": ["Donkey Kong 64"],
    "donkey_kong_country": ["Donkey Kong Country"],
    "donkey_kong_country_2": ["Donkey Kong Country 2", "Donkey Kong Country 2: Diddy's Kong Quest"],
    "donkey_kong_country_3": ["Donkey Kong Country 3"],
    "earthbound": ["EarthBound"],
    "final_fantasy": ["Final Fantasy", "Final Fantasy 1"],
    "final_fantasy_iv": ["Final Fantasy IV", "Final Fantasy IV: Free Enterprise"],
    "final_fantasy_mystic_quest": ["Final Fantasy Mystic Quest"],
    "final_fantasy_tactics_advance": ["Final Fantasy Tactics Advance"],
    "final_fantasy_v": ["Final Fantasy V"],
    "final_fantasy_vi": ["Final Fantasy VI"],
    "kirbys_dream_land_3": ["Kirby's Dream Land 3"],
    "links_awakening_dx": ["Link's Awakening DX", "Links Awakening DX", "LADX Beta"],
    "lufia_ii": ["Lufia II", "Lufia II Ancient Cave"],
    "luigis_mansion": ["Luigi's Mansion"],
    "majoras_mask": ["Majora's Mask", "The Legend of Zelda: Majora's Mask"],
    "mario_luigi_superstar_saga": ["Mario & Luigi: Superstar Saga", "Mario and Luigi Superstar Saga"],
    "super_mario_sunshine": ["Super Mario Sunshine"],
    "metroid_fusion": ["Metroid Fusion"],
    "metroid_prime": ["Metroid Prime"],
    "metroid_zero_mission": ["Metroid: Zero Mission", "Metroid Zero Mission"],
    "the_minish_cap": ["The Minish Cap", "The Legend of Zelda: The Minish Cap"],
    "mario_kart_double_dash": ["Mario Kart: Double Dash", "Mario Kart: Double Dash!!"],
    "mega_man_2": ["Mega Man 2"],
    "mega_man_3": ["Mega Man 3"],
    "mega_man_battle_network_3": ["Mega Man Battle Network 3", "MegaMan Battle Network 3"],
    "mega_man_x3": ["Mega Man X3"],
    "ocarina_of_time": ["Ocarina of Time", "The Legend of Zelda: Ocarina of Time", "The Legend Zelda: Ocarina of Time (updated, not Ship)", "The Legend of Zelda: Ocarina of Time - Ship of Harkinian"],
    "oracle_of_ages": ["Oracle of Ages", "The Legend of Zelda: Oracle of Ages"],
    "oracle_of_seasons": ["Oracle of Seasons", "The Legend of Zelda: Oracle of Seasons"],
    "paper_mario": ["Paper Mario", "Paper Mario 64"],
    "pokemon_crystal": ["Pokemon Crystal", "Pokémon Crystal"],
    "pokemon_emerald": ["Pokemon Emerald", "Pokémon Emerald"],
    "pokemon_firered": ["Pokemon FireRed", "Pokémon FireRed and LeafGreen", "Pokemon FireRed and LeafGreen"],
    "pokemon_red_blue": ["Pokemon Red and Blue", "Pokémon Red and Blue"],
    "secret_of_evermore": ["Secret of Evermore"],
    "secret_of_mana": ["Secret of Mana"],
    "skyward_sword": ["Skyward Sword", "The Legend of Zelda: Skyward Sword"],
    "super_mario_64": ["Super Mario 64"],
    "super_mario_land_2": ["Super Mario Land 2", "Super Mario Land 2: The Golden Coins"],
    "super_mario_world": ["Super Mario World"],
    "smz3": ["SMZ3"],
    "star_fox_64": ["Star Fox 64"],
    "super_metroid": ["Super Metroid"],
    "tetris_attack": ["Tetris Attack"],
    "the_legend_of_zelda": ["The Legend of Zelda"],
    "twilight_princess": ["Twilight Princess", "The Legend of Zelda: Twilight Princess"],
    "the_wind_waker": ["The Wind Waker", "The Legend of Zelda: The Wind Waker"],
    "thousand_year_door": ["The Thousand-Year Door", "Paper Mario: The Thousand Year Door", "Paper Mario: The Thousand-Year Door"],
    "wario_land": ["Wario Land", "Wario Land 1", "Wario Land: Super Mario Land 3"],
    "wario_land_4": ["Wario Land 4"],
    "zelda_ii": ["Zelda II", "Zelda 2: The Adventure of Link", "Zelda II: The Adventure of Link"],
}

EXPECTED_STEMS: dict[str, set[str]] = {
    "a_link_between_worlds": {"albw"},
    "chrono_trigger": {"ctjot", "chrono_trigger", "chrono"},
    "donkey_kong_64": {"dk64"},
    "donkey_kong_country": {"dkc"},
    "donkey_kong_country_2": {"dkc2"},
    "donkey_kong_country_3": {"dkc3"},
    "final_fantasy_iv": {"ff4fe"},
    "final_fantasy_tactics_advance": {"ffta"},
    "final_fantasy_v": {"ffvcd", "ffv"},
    "final_fantasy_vi": {"ff6", "ffvi"},
    "links_awakening_dx": {"ladx", "ladx_beta"},
    "luigis_mansion": {"luigismansion", "luigi_mansion", "luigis_mansion"},
    "majoras_mask": {"mm_recomp", "mm", "majoras_mask"},
    "super_mario_sunshine": {"sms", "sms-poptracker", "sms_poptracker", "sm_poptracker"},
    "metroid_fusion": {"metroidfusion"},
    "metroid_prime": {"metroidprime"},
    "metroid_zero_mission": {"mzm"},
    "the_minish_cap": {"tmc"},
    "mario_kart_double_dash": {"mario_kart_double_dash"},
    "mega_man_2": {"mm2"},
    "mega_man_3": {"mm3"},
    "mega_man_battle_network_3": {"mmbn3"},
    "mega_man_x3": {"mmx3"},
    "ocarina_of_time": {"oot", "oot_soh"},
    "oracle_of_ages": {"tloz_ooa", "ooa"},
    "oracle_of_seasons": {"tloz_oos", "oos"},
    "paper_mario": {"papermario", "pmr", "paper_mario"},
    "pokemon_crystal": {"pokemon_crystal"},
    "pokemon_emerald": {"pokemon_emerald"},
    "pokemon_firered": {"pokemon_frlg", "pokemon_firered", "firered"},
    "secret_of_mana": {"som"},
    "skyward_sword": {"ss", "skyward_sword"},
    "super_mario_64": {"sm64ex", "sm64"},
    "star_fox_64": {"star_fox_64", "sf64"},
    "tetris_attack": {"tetrisattack", "tetris_attack"},
    "twilight_princess": {"tp", "twilight_princess"},
    "the_wind_waker": {"tww", "wind_waker", "the_wind_waker"},
    "thousand_year_door": {"ttyd"},
    "wario_land": {"wl"},
    "wario_land_4": {"wl4"},
    "zelda_ii": {"zelda2"},
}

LOCAL_RESOURCE_OVERRIDES: dict[str, list[dict[str, str]]] = {
    "twilight_princess": [
        {
            "kind": "apworld",
            "status": "local",
            "game": "The Legend of Zelda: Twilight Princess",
            "file_name": "tp.apworld",
            "path": "/home/thelovenityjade/.local/share/Archipelago/worlds/tp.apworld",
        },
        {
            "kind": "poptracker",
            "status": "local",
            "game": "The Legend of Zelda: Twilight Princess",
            "file_name": "TPRAP_poptracker_v0.10.2.zip",
            "path": "/home/thelovenityjade/PopTracker/packs/TPRAP_poptracker_v0.10.2.zip",
        },
    ],
}


@dataclass
class Downloaded:
    key: str
    display_name: str
    resource_game: str
    kind: str
    status: str
    file_name: str
    url: str
    path: str
    size: int = 0
    sha256: str = ""
    skipped: str = ""
    error: str = ""


def norm(value: str) -> str:
    value = value.lower().replace("pokémon", "pokemon")
    value = value.replace("&", "and")
    return re.sub(r"[^a-z0-9]+", "", value)


def parse_catalog() -> list[dict[str, str]]:
    text = CATALOG.read_text(encoding="utf-8")
    entries: list[dict[str, str]] = []
    for block in re.findall(r"\{[^{}]*key:\s*['\"][^{}]+?\}", text, re.S):
        key_match = re.search(r"key:\s*['\"]([^'\"]+)['\"]", block)
        name_match = re.search(r"displayName:\s*(['\"])(.*?)\1", block, re.S)
        if key_match and name_match:
            entries.append({"key": key_match.group(1), "display_name": name_match.group(2)})
    if not entries:
        raise RuntimeError(f"catalog parse returned no entries: {CATALOG}")
    return entries


def expected_match(key: str, resource: dict[str, Any]) -> bool:
    expected = EXPECTED_STEMS.get(key)
    if not expected:
        return True
    stem = Path(str(resource.get("file_name") or "")).name
    stem = re.sub(r"\.(apworld|zip|ptpack|yaml|json|tar\.gz|7z)$", "", stem, flags=re.I).lower()
    return stem in expected


def token() -> str:
    try:
        return subprocess.check_output(["gh", "auth", "token"], text=True, stderr=subprocess.DEVNULL).strip()
    except Exception:
        return ""


def session() -> requests.Session:
    s = requests.Session()
    s.headers.update({"User-Agent": "SekaiLink-supported-resource-downloader"})
    gh_token = token()
    if gh_token:
        s.headers["Authorization"] = f"Bearer {gh_token}"
    return s


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def download_file(s: requests.Session, url: str, path: Path) -> int:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".part")
    with s.get(url, stream=True, timeout=60) as res:
        res.raise_for_status()
        total = 0
        with tmp.open("wb") as fh:
            for chunk in res.iter_content(chunk_size=1024 * 256):
                if not chunk:
                    continue
                total += len(chunk)
                fh.write(chunk)
    tmp.replace(path)
    return total


def main() -> int:
    catalog = parse_catalog()
    index = json.loads(INDEX.read_text(encoding="utf-8"))
    all_resources = index["resources"]
    resources = [r for r in all_resources if r.get("kind") in {"apworld", "poptracker", "tracker"} and r.get("url")]
    core_resources = [r for r in all_resources if r.get("kind") == "core-world"]
    by_norm_game: dict[str, list[dict[str, Any]]] = {}
    for res in resources:
        by_norm_game.setdefault(norm(res["game"]), []).append(res)
    by_norm_core_game: dict[str, list[dict[str, Any]]] = {}
    for res in core_resources:
        by_norm_core_game.setdefault(norm(res["game"]), []).append(res)

    s = session()
    downloads: list[Downloaded] = []
    covered: dict[str, list[dict[str, Any]]] = {}
    core_references: dict[str, list[dict[str, Any]]] = {}
    missing: list[dict[str, Any]] = []

    for entry in catalog:
        key = entry["key"]
        aliases = CATALOG_ALIASES.get(key, [entry["display_name"]])
        matches: list[dict[str, Any]] = []
        core_matches: list[dict[str, Any]] = []
        seen_urls: set[str] = set()
        seen_core_games: set[str] = set()
        for alias in aliases:
            for res in by_norm_game.get(norm(alias), []):
                if res["url"] in seen_urls:
                    continue
                if not expected_match(key, res):
                    continue
                seen_urls.add(res["url"])
                matches.append(res)
            for res in by_norm_core_game.get(norm(alias), []):
                core_key = str(res.get("game") or alias)
                if core_key in seen_core_games:
                    continue
                seen_core_games.add(core_key)
                core_matches.append(res)
        if not matches and key in EXPECTED_STEMS:
            for res in resources:
                if res["url"] in seen_urls:
                    continue
                if not expected_match(key, res):
                    continue
                seen_urls.add(res["url"])
                matches.append(res)
        for override in LOCAL_RESOURCE_OVERRIDES.get(key, []):
            local_path = Path(override["path"])
            if not local_path.exists():
                continue
            matches.append({
                "game": override["game"],
                "kind": override["kind"],
                "status": override["status"],
                "file_name": override["file_name"],
                "url": "",
                "local_path": str(local_path),
            })
        deduped_matches: list[dict[str, Any]] = []
        seen_assets: set[tuple[str, str]] = set()
        for res in matches:
            asset_key = (str(res.get("kind") or ""), Path(str(res.get("file_name") or res.get("url") or "")).name)
            if asset_key in seen_assets:
                continue
            seen_assets.add(asset_key)
            deduped_matches.append(res)
        matches = deduped_matches
        if not matches:
            reason = "core_archipelago_no_separate_download" if core_matches else "no_matching_downloadable_resource"
            if core_matches:
                core_references[key] = core_matches
            missing.append({
                "key": key,
                "display_name": entry["display_name"],
                "aliases": aliases,
                "reason": reason,
                "core_references": core_matches,
            })
            continue
        if core_matches:
            core_references[key] = core_matches
        covered[key] = matches
        for res in matches:
            safe_name = Path(res["file_name"]).name or Path(res["url"]).name
            out = OUT_DIR / res["kind"] / key / safe_name
            row = Downloaded(
                key=key,
                display_name=entry["display_name"],
                resource_game=res["game"],
                kind=res["kind"],
                status=res["status"],
                file_name=safe_name,
                url=res.get("url") or "",
                path=str(out),
            )
            try:
                if out.exists() and out.stat().st_size > 0:
                    row.size = out.stat().st_size
                    row.skipped = "already_exists"
                elif res.get("local_path"):
                    print(f"copy {key}: {safe_name}", flush=True)
                    out.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(res["local_path"], out)
                    row.size = out.stat().st_size
                    row.skipped = "local_copy"
                else:
                    print(f"download {key}: {safe_name}", flush=True)
                    row.size = download_file(s, res["url"], out)
                    time.sleep(0.1)
                row.sha256 = sha256_file(out)
            except Exception as err:
                row.error = f"{type(err).__name__}: {err}"
            downloads.append(row)

    report = {
        "source_index": str(INDEX),
        "scope": "SekaiLink client catalog, NES/SNES/GB/GBC/GBA/N64/GameCube support scope",
        "counts": {
            "catalog_games": len(catalog),
            "covered_games": len(covered),
            "core_reference_games": len(core_references),
            "missing_games": len(missing),
            "downloads": len(downloads),
            "errors": sum(1 for d in downloads if d.error),
        },
        "downloads": [asdict(d) for d in downloads],
        "core_references": core_references,
        "missing": missing,
    }
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(report["counts"], indent=2, ensure_ascii=False))
    if report["counts"]["errors"]:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
