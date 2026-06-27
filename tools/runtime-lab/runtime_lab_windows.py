#!/usr/bin/env python3
"""Windows-side SekaiLink runtime lab helper.

This script is intentionally runnable from MSYS2 on the Windows workbench.  It
keeps the runtime compatibility lab separate from the Windows client build tree:
D:/SekaiLink remains the build/client area, while D:/RuntimeLab holds generated
YAMLs, multidata, logs, status, ROM references, and temporary runtime output.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import zipfile
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


FAMILY_PLATFORMS: dict[str, set[str]] = {
    "nes": {"NES"},
    "snes": {"SNES"},
    "gb_gbc": {"GB", "GBC"},
    "gba": {"GBA"},
    "n64": {"N64", "N64/PC"},
    "gamecube": {"GameCube", "Wii", "GameCube/Wii"},
}

DEFAULT_PORT_BY_FAMILY = {
    "nes": 38310,
    "snes": 38320,
    "gb_gbc": 38330,
    "gba": 38340,
    "n64": 38350,
    "gamecube": 38360,
    "other": 38390,
}

MAX_SLOT_NAME_LEN = 16
SLOT_CODE_BY_GAME_KEY = {
    "a_link_to_the_past": "ALTTP",
    "chrono_trigger": "CT",
    "donkey_kong_country": "DKC",
    "donkey_kong_country_2": "DKC2",
    "donkey_kong_country_3": "DKC3",
    "earthbound": "EB",
    "final_fantasy": "FF1",
    "final_fantasy_iv": "FF4FE",
    "final_fantasy_mystic_quest": "FFMQ",
    "final_fantasy_tactics_advance": "FFTA",
    "final_fantasy_v": "FFV",
    "kirbys_dream_land_3": "KDL3",
    "links_awakening_dx": "LADX",
    "lufia_ii": "L2AC",
    "mario_luigi_superstar_saga": "MLSS",
    "mega_man_2": "MM2",
    "mega_man_3": "MM3",
    "mega_man_battle_network_3": "MMBN3",
    "mega_man_x3": "MMX3",
    "metroid_fusion": "MF",
    "metroid_zero_mission": "MZM",
    "oracle_of_ages": "OOA",
    "oracle_of_seasons": "OOS",
    "pokemon_crystal": "PKC",
    "pokemon_emerald": "PKE",
    "pokemon_firered": "PKFRLG",
    "pokemon_red_blue": "PKRB",
    "secret_of_mana": "SOM",
    "smz3": "SMZ3",
    "super_mario_land_2": "SML2",
    "super_mario_world": "SMW",
    "super_metroid": "SM",
    "tetris_attack": "TA",
    "the_legend_of_zelda": "TLOZ",
    "the_minish_cap": "TMC",
    "wario_land": "WL",
    "wario_land_4": "WL4",
    "zelda_ii": "Z2",
    "paper_mario": "PM64",
    "ocarina_of_time": "OOT",
    "donkey_kong_64": "DK64",
    "star_fox_64": "SF64",
    "super_mario_64": "SM64",
    "mario_kart_double_dash": "MKDD",
    "metroid_prime": "MP",
    "super_mario_sunshine": "SMS",
    "the_wind_waker": "TWW",
    "thousand_year_door": "TTYD",
    "twilight_princess": "TP",
    "luigis_mansion": "LM",
    "skyward_sword": "SS",
    "a_link_between_worlds": "ALBW",
    "majoras_mask": "MMR",
    "secret_of_evermore": "SOE",
}

LAB_SUBDIRS = (
    "apworlds",
    "yamls",
    "seeds",
    "roms",
    "patches",
    "trackers",
    "logs",
    "status",
    "reports",
    "tmp",
)

HOST_ROM_SETTINGS: dict[str, list[tuple[str, str]]] = {
    "a_link_to_the_past": [("lttp_options", "rom_file")],
    "donkey_kong_country": [("dkc_options", "rom_file")],
    "donkey_kong_country_2": [("dkc2_options", "rom_file")],
    "donkey_kong_country_3": [("dkc3_options", "rom_file")],
    "earthbound": [("earthbound_options", "rom_file")],
    "final_fantasy_iv": [("ff4fe_options", "rom_file")],
    "final_fantasy_tactics_advance": [("ffta_options", "rom_file")],
    "final_fantasy_v": [("ffvcd_options", "rom_file")],
    "kirbys_dream_land_3": [("kdl3_options", "rom_file")],
    "links_awakening_dx": [("ladx_beta_options", "rom_file"), ("ladx_options", "rom_file")],
    "lufia_ii": [("lufia2ac_options", "rom_file")],
    "mario_luigi_superstar_saga": [("mlss_options", "rom_file")],
    "mega_man_2": [("mm2_options", "rom_file")],
    "mega_man_3": [("mm3_options", "rom_file")],
    "mega_man_battle_network_3": [("mmbn3_options", "rom_file")],
    "mega_man_x3": [("mmx3_options", "rom_file")],
    "metroid_fusion": [("metroidfusion_options", "rom_file")],
    "metroid_prime": [("metroidprime_options", "rom_file")],
    "metroid_zero_mission": [("mzm_options", "rom_file")],
    "ocarina_of_time": [("oot_options", "rom_file")],
    "oracle_of_ages": [("tloz_ooa_options", "rom_file")],
    "oracle_of_seasons": [("tloz_oos_options", "rom_file")],
    "paper_mario": [("paper_mario_settings", "rom_file")],
    "pokemon_crystal": [("pokemon_crystal_settings", "rom_file")],
    "pokemon_emerald": [("pokemon_emerald_settings", "rom_file")],
    "pokemon_firered": [("pokemon_frlg_settings", "firered_rom_file")],
    "pokemon_red_blue": [("pokemon_rb_options", "red_rom_file")],
    "secret_of_evermore": [("soe_options", "rom_file")],
    "secret_of_mana": [("som_options", "rom_file")],
    "super_mario_land_2": [("sml2_options", "rom_file")],
    "super_mario_world": [("smw_options", "rom_file")],
    "super_metroid": [("sm_options", "rom_file")],
    "super_mario_sunshine": [("sms_options", "iso_file")],
    "tetris_attack": [("tetrisattack_options", "rom_file")],
    "the_legend_of_zelda": [("tloz_options", "rom_file")],
    "the_minish_cap": [("tmc_options", "rom_file")],
    "thousand_year_door": [("ttyd_options", "rom_file")],
    "wario_land": [("wl_options", "rom_file")],
    "wario_land_4": [("wl4_options", "rom_file")],
    "zelda_ii": [("zelda2_options", "rom_file")],
}


@dataclass(frozen=True)
class LabPaths:
    repo: Path
    runtime: Path
    ap: Path
    registry: Path
    lab: Path
    status_file: Path


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def slug(value: str) -> str:
    raw = re.sub(r"[^a-z0-9]+", "_", value.lower()).strip("_")
    return raw or "game"


def short_slot_code(client: dict[str, Any]) -> str:
    game_key = slug(str(client.get("game_key") or ""))
    if game_key in SLOT_CODE_BY_GAME_KEY:
        return SLOT_CODE_BY_GAME_KEY[game_key]
    source = game_key or slug(str(client.get("game") or "game"))
    digest = hashlib.sha1(source.encode("utf-8")).hexdigest()[:3].upper()
    compact = "".join(part[:1].upper() for part in source.split("_") if part)[:7] or "GAME"
    return f"{compact}{digest}"


def default_lab_root() -> Path:
    raw = os.environ.get("SEKAILINK_RUNTIME_LAB_ROOT", "")
    if raw:
        return Path(raw)
    if os.name == "nt":
        return Path("D:/RuntimeLab")
    if Path("/d").exists():
        return Path("/d/RuntimeLab")
    # Linux-only dry runs should not try to create a fake MSYS2 /d mount.
    # The production path is still /d/RuntimeLab when invoked on Windows/MSYS2.
    if sys.platform.startswith("linux"):
        return Path.home() / ".cache" / "sekailink-runtime-lab"
    return Path("/d/RuntimeLab")


def default_repo_root() -> Path:
    return Path(os.environ.get("SEKAILINK_REPO", Path(__file__).resolve().parents[2]))


def lab_paths() -> LabPaths:
    repo = default_repo_root()
    runtime = repo / "runtime"
    return LabPaths(
        repo=repo,
        runtime=runtime,
        ap=runtime / "ap",
        registry=runtime / "game-registry" / "archipelago-clients.json",
        lab=default_lab_root(),
        status_file=default_lab_root() / "status" / "compatibility.json",
    )


def emit(payload: dict[str, Any]) -> None:
    print(json.dumps(payload, ensure_ascii=False, indent=2, default=str))


def load_json(path: Path, fallback: Any) -> Any:
    if not path.exists():
        return fallback
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return fallback


def write_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2, default=str) + "\n", encoding="utf-8")


def sha1_file(path: Path) -> str:
    h = hashlib.sha1()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def read_tail(path: Path, max_chars: int = 6000) -> str:
    if not path.exists():
        return ""
    data = path.read_text(encoding="utf-8", errors="replace")
    return data[-max_chars:]


def load_registry(paths: LabPaths) -> list[dict[str, Any]]:
    data = load_json(paths.registry, {})
    return list(data.get("clients") or [])


def archipelago_python(paths: LabPaths) -> str:
    is_windowsish = os.name == "nt" or bool(os.environ.get("MSYSTEM")) or Path("/c/Windows").exists()
    if is_windowsish:
        candidates = [
            paths.runtime / "tools" / "python" / "portable-win" / "tools" / "python.exe",
            paths.runtime / "tools" / "python" / "portable-win" / "python.exe",
        ]
        for candidate in candidates:
            if candidate.exists():
                return str(candidate)
    found = shutil.which("python3.13") or shutil.which("python3.12") or shutil.which("python") or shutil.which("python3")
    return found or sys.executable


def python_version(python: str) -> str:
    try:
        proc = subprocess.run(
            [python, "-c", "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}')"],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            timeout=5,
        )
        return proc.stdout.strip() if proc.returncode == 0 else "unknown"
    except Exception:
        return "unknown"


def python_module_available(python: str, module: str) -> bool:
    try:
        proc = subprocess.run(
            [python, "-c", f"import {module}"],
            text=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=8,
        )
        return proc.returncode == 0
    except Exception:
        return False


def python_site_packages(python: str) -> Path:
    proc = subprocess.run(
        [python, "-c", "import sysconfig; print(sysconfig.get_paths().get('platlib') or sysconfig.get_paths().get('purelib'))"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        timeout=8,
    )
    if proc.returncode != 0 or not proc.stdout.strip():
        raise RuntimeError("python_site_packages_unavailable")
    return Path(proc.stdout.strip())


def wheel_name_key(path: Path) -> str:
    return path.name.split("-")[0].lower().replace("_", "-")


def family_for(client: dict[str, Any]) -> str:
    platform = str(client.get("platform") or "")
    for family, platforms in FAMILY_PLATFORMS.items():
        if platform in platforms:
            return family
    return "other"


def clients_for_family(paths: LabPaths, family: str) -> list[dict[str, Any]]:
    if family == "all":
        return load_registry(paths)
    if family not in FAMILY_PLATFORMS:
        raise SystemExit(f"unknown_family:{family}")
    return [client for client in load_registry(paths) if family_for(client) == family]


def world_available(paths: LabPaths, client: dict[str, Any]) -> bool:
    world = str(client.get("world") or "").strip()
    if not world:
        return False
    worlds_dir = paths.ap / "worlds"
    return (worlds_dir / world).exists() or (worlds_dir / f"{world}.apworld").exists()


def slot_name(client: dict[str, Any]) -> str:
    name = f"Jade-{short_slot_code(client)}"
    return name[:MAX_SLOT_NAME_LEN]


def rom_index_path(paths: LabPaths) -> Path:
    return paths.lab / "status" / "roms.json"


def tracker_index_path(paths: LabPaths) -> Path:
    return paths.lab / "status" / "trackers.json"


def load_rom_index(paths: LabPaths) -> dict[str, Any]:
    return load_json(rom_index_path(paths), {"schema": 1, "updated_at": utc_now(), "roms": {}})


def load_tracker_index(paths: LabPaths) -> dict[str, Any]:
    return load_json(tracker_index_path(paths), {"schema": 1, "updated_at": utc_now(), "trackers": {}})


def save_rom_index(paths: LabPaths, index: dict[str, Any]) -> None:
    index["updated_at"] = utc_now()
    write_json(rom_index_path(paths), index)


def save_tracker_index(paths: LabPaths, index: dict[str, Any]) -> None:
    index["updated_at"] = utc_now()
    write_json(tracker_index_path(paths), index)


def imported_rom_for(paths: LabPaths, game_key: str) -> str:
    index = load_rom_index(paths)
    entry = index.get("roms", {}).get(slug(game_key), {})
    path = Path(str(entry.get("path") or ""))
    return str(path) if path.exists() else ""


def imported_tracker_for(paths: LabPaths, game_key: str) -> str:
    index = load_tracker_index(paths)
    entry = index.get("trackers", {}).get(slug(game_key), {})
    path = Path(str(entry.get("path") or ""))
    return str(path) if path.exists() else ""


def update_host_yaml_rom(paths: LabPaths, game_key: str, rom_path: str) -> list[str]:
    updates = HOST_ROM_SETTINGS.get(slug(game_key), [])
    host_yaml = paths.ap / "host.yaml"
    if not updates or not host_yaml.exists():
        return []
    lines = host_yaml.read_text(encoding="utf-8").splitlines()
    changed: list[str] = []
    active_section = ""
    for i, line in enumerate(lines):
        section_match = re.match(r"^([A-Za-z0-9_]+):\s*$", line)
        if section_match:
            active_section = section_match.group(1)
            continue
        for section, key in updates:
            if active_section == section and re.match(rf"^\s*{re.escape(key)}\s*:", line):
                indent = line[: len(line) - len(line.lstrip())]
                lines[i] = f"{indent}{key}: {json.dumps(rom_path)}"
                changed.append(f"{section}.{key}")
    if changed:
        host_yaml.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return changed


def yaml_text(client: dict[str, Any]) -> str:
    game = str(client.get("game") or "")
    name = slot_name(client)
    game_key = json.dumps(game)
    # Archipelago fills missing options from defaults.  Keeping YAML minimal
    # makes the lab resilient when APWorld option names change between versions.
    return (
        f"name: {json.dumps(name)}\n"
        f"game: {json.dumps(game)}\n"
        "description: \"SekaiLink Runtime Lab default config\"\n"
        f"{game_key}:\n"
        "  progression_balancing: 50\n"
        "  accessibility: items\n"
    )


def ensure_lab(paths: LabPaths) -> None:
    paths.lab.mkdir(parents=True, exist_ok=True)
    for subdir in LAB_SUBDIRS:
        (paths.lab / subdir).mkdir(parents=True, exist_ok=True)
    status = load_json(paths.status_file, {"schema": 1, "games": {}})
    changed = not paths.status_file.exists()
    status.setdefault("schema", 1)
    status.setdefault("games", {})
    for client in load_registry(paths):
        key = str(client.get("game_key") or slug(str(client.get("game") or "")))
        entry = status["games"].setdefault(key, {})
        before = dict(entry)
        entry.setdefault("status", "untested")
        entry.setdefault("notes", [])
        entry.setdefault("last_result", "")
        entry.update({
            "game": client.get("game"),
            "platform": client.get("platform"),
            "family": family_for(client),
            "wrapper": client.get("wrapper"),
        })
        entry.setdefault("updated_at", utc_now())
        changed = changed or before != entry
    if changed:
        status["updated_at"] = utc_now()
        write_json(paths.status_file, status)


def cmd_init(_args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    emit({
        "ok": True,
        "lab": str(paths.lab),
        "repo": str(paths.repo),
        "status_file": str(paths.status_file),
        "subdirs": list(LAB_SUBDIRS),
    })
    return 0


def cmd_doctor(_args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    ap_python = archipelago_python(paths)
    ap_python_version = python_version(ap_python)
    checks = {
        "repo": paths.repo.exists(),
        "runtime": paths.runtime.exists(),
        "archipelago_generate": (paths.ap / "Generate.py").exists(),
        "archipelago_multiserver": (paths.ap / "MultiServer.py").exists(),
        "registry": paths.registry.exists(),
        "wrapper_supervisor": (paths.runtime / "wrapper_supervisor.py").exists(),
        "bootchain_supervisor": (paths.runtime / "bootchain_supervisor.py").exists(),
        "python": bool(ap_python),
        "python_supported_by_ap": ap_python_version.startswith("3.12.") or ap_python_version.startswith("3.13."),
        "python_yaml": python_module_available(ap_python, "yaml"),
        "python_websockets": python_module_available(ap_python, "websockets"),
        "python_pkg_resources": python_module_available(ap_python, "pkg_resources"),
        "python_requests": python_module_available(ap_python, "requests"),
        "lab": paths.lab.exists(),
    }
    clients = load_registry(paths)
    emit({
        "ok": all(checks.values()),
        "checks": checks,
        "lab": str(paths.lab),
        "archipelago_python": ap_python,
        "archipelago_python_version": ap_python_version,
        "clients": len(clients),
        "families": {family: len(clients_for_family(paths, family)) for family in FAMILY_PLATFORMS},
    })
    return 0 if all(checks.values()) else 1


def cmd_bootstrap_python(_args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    python = archipelago_python(paths)
    wheelhouse = paths.runtime / "tools" / "python" / "wheelhouse" / "win-amd64-cp312"
    if not wheelhouse.exists():
        emit({"ok": False, "error": "wheelhouse_missing", "wheelhouse": str(wheelhouse)})
        return 2
    packages = [
        "setuptools",
        "wheel",
        "pyyaml",
        "websockets",
        "orjson",
        "jinja2",
        "markupsafe",
        "schema",
        "bsdiff4",
        "pillow",
        "docutils",
        "requests",
        "urllib3",
        "charset_normalizer",
        "idna",
        "certifi",
        "dolphin_memory_engine",
        "pyevermizer",
    ]
    site_packages = python_site_packages(python)
    site_packages.mkdir(parents=True, exist_ok=True)
    wheels = sorted(path for path in wheelhouse.glob("*.whl") if path.is_file())
    # The wheelhouse is curated for the portable runtime.  Extract all wheels so
    # Archipelago world imports do not fail one dependency at a time.
    selected = wheels
    if not selected:
        emit({"ok": False, "error": "no_matching_wheels", "wheelhouse": str(wheelhouse), "packages": packages})
        return 2
    extracted: list[str] = []
    for wheel in selected:
        with zipfile.ZipFile(wheel) as archive:
            archive.extractall(site_packages)
        extracted.append(wheel.name)
    checks = {
        "yaml": python_module_available(python, "yaml"),
        "websockets": python_module_available(python, "websockets"),
        "pkg_resources": python_module_available(python, "pkg_resources"),
        "docutils": python_module_available(python, "docutils"),
        "requests": python_module_available(python, "requests"),
    }
    ok = all(checks.values())
    emit({
        "ok": ok,
        "method": "manual_wheel_extract",
        "python": python,
        "site_packages": str(site_packages),
        "wheelhouse": str(wheelhouse),
        "extracted": extracted,
        "checks": checks,
    })
    return 0 if ok else 1


def cmd_list(args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    status = load_json(paths.status_file, {"games": {}})
    rows = []
    for client in clients_for_family(paths, args.family):
        key = str(client.get("game_key") or slug(str(client.get("game") or "")))
        state = status.get("games", {}).get(key, {})
        rows.append({
            "game_key": key,
            "game": client.get("game"),
            "platform": client.get("platform"),
            "family": family_for(client),
            "world": client.get("world"),
            "world_available": world_available(paths, client),
            "wrapper": client.get("wrapper"),
            "registry_status": client.get("status"),
            "lab_status": state.get("status", "untested"),
            "last_result": state.get("last_result", ""),
        })
    emit({"ok": True, "family": args.family, "games": rows})
    return 0


def cmd_generate_yamls(args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    out_dir = paths.lab / "yamls" / args.family
    out_dir.mkdir(parents=True, exist_ok=True)
    for stale in list(out_dir.glob("*.yaml")) + list(out_dir.glob("*.yml")) + [out_dir / "index.json"]:
        try:
            stale.unlink()
        except FileNotFoundError:
            pass
    written = []
    skipped = []
    for client in clients_for_family(paths, args.family):
        if client.get("status") == "generator_only" and args.runtime_only:
            skipped.append({"game_key": client.get("game_key"), "game": client.get("game"), "reason": "generator_only"})
            continue
        if not world_available(paths, client):
            skipped.append({"game_key": client.get("game_key"), "game": client.get("game"), "reason": f"world_missing:{client.get('world')}"})
            continue
        if not client.get("game"):
            skipped.append({"game_key": client.get("game_key"), "game": client.get("game"), "reason": "game_missing"})
            continue
        out = out_dir / f"{slug(str(client.get('game_key') or client.get('game')))}.yaml"
        out.write_text(yaml_text(client), encoding="utf-8")
        written.append({"game_key": client.get("game_key"), "game": client.get("game"), "path": str(out), "slot": slot_name(client)})
    write_json(paths.lab / "status" / f"yamls-{args.family}.json", {"updated_at": utc_now(), "family": args.family, "yamls": written, "skipped": skipped})
    emit({"ok": True, "family": args.family, "directory": str(out_dir), "written": written, "skipped": skipped})
    return 0


def run_logged(paths: LabPaths, name: str, argv: list[str], cwd: Path | None = None) -> int:
    log_dir = paths.lab / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    log = log_dir / f"{datetime.now().strftime('%Y%m%d-%H%M%S')}-{name}.log"
    with log.open("w", encoding="utf-8") as fh:
        fh.write(f"started={utc_now()}\n")
        fh.write(f"cwd={cwd or paths.repo}\n")
        fh.write(f"argv={json.dumps(argv)}\n\n")
        env = os.environ.copy()
        env.setdefault("SKIP_REQUIREMENTS_UPDATE", "1")
        env.setdefault("KIVY_NO_ARGS", "1")
        env.setdefault("KIVY_NO_FILELOG", "1")
        proc = subprocess.run(argv, cwd=str(cwd or paths.repo), text=True, stdout=fh, stderr=subprocess.STDOUT, env=env)
        fh.write(f"\nended={utc_now()} code={proc.returncode}\n")
    write_json(paths.lab / "status" / "last-command.json", {
        "name": name,
        "argv": argv,
        "returncode": proc.returncode,
        "log": str(log),
        "updated_at": utc_now(),
    })
    return proc.returncode


def cmd_generate(args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    gen_args = argparse.Namespace(family=args.family, runtime_only=args.runtime_only)
    cmd_generate_yamls(gen_args)
    yaml_dir = paths.lab / "yamls" / args.family
    out_dir = paths.lab / "seeds" / args.family
    out_dir.mkdir(parents=True, exist_ok=True)
    python = archipelago_python(paths)
    argv = [
        python,
        str(paths.ap / "Generate.py"),
        "--player_files_path",
        str(yaml_dir),
        "--outputpath",
        str(out_dir),
    ]
    code = run_logged(paths, f"generate-{args.family}", argv, cwd=paths.ap)
    emit({"ok": code == 0, "family": args.family, "returncode": code, "yaml_dir": str(yaml_dir), "output": str(out_dir)})
    return code


def cmd_generate_game(args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    client = find_game(paths, args.game_key)
    key = slug(str(client.get("game_key") or client.get("game") or args.game_key))
    if client.get("status") == "generator_only" and args.runtime_only:
        emit({"ok": False, "error": "generator_only", "game_key": key, "game": client.get("game")})
        return 2
    if not world_available(paths, client):
        emit({"ok": False, "error": f"world_missing:{client.get('world')}", "game_key": key, "game": client.get("game")})
        return 2
    yaml_dir = paths.lab / "yamls" / "games" / key
    out_dir = paths.lab / "seeds" / "games" / key
    yaml_dir.mkdir(parents=True, exist_ok=True)
    out_dir.mkdir(parents=True, exist_ok=True)
    for stale in list(yaml_dir.glob("*.yaml")) + list(yaml_dir.glob("*.yml")):
        stale.unlink(missing_ok=True)
    yaml_path = yaml_dir / f"{key}.yaml"
    yaml_path.write_text(yaml_text(client), encoding="utf-8")
    python = archipelago_python(paths)
    argv = [
        python,
        str(paths.ap / "Generate.py"),
        "--player_files_path",
        str(yaml_dir),
        "--outputpath",
        str(out_dir),
    ]
    code = run_logged(paths, f"generate-game-{key}", argv, cwd=paths.ap)
    emit({
        "ok": code == 0,
        "game_key": key,
        "game": client.get("game"),
        "family": family_for(client),
        "slot": slot_name(client),
        "returncode": code,
        "yaml": str(yaml_path),
        "output": str(out_dir),
        "latest_seed": str(latest_seed(paths, f"game:{key}") or ""),
    })
    return code


def seed_files(paths: LabPaths, family: str = "all") -> list[Path]:
    if family == "all":
        roots = [paths.lab / "seeds"] if (paths.lab / "seeds").exists() else []
    elif family.startswith("game:"):
        roots = [paths.lab / "seeds" / "games" / slug(family.split(":", 1)[1])]
    else:
        roots = [paths.lab / "seeds" / family]
    files: list[Path] = []
    for root in roots:
        files.extend(root.rglob("*.zip"))
        files.extend(root.rglob("*.archipelago"))
    return sorted(files, key=lambda path: path.stat().st_mtime, reverse=True)


def latest_seed(paths: LabPaths, family: str) -> Path | None:
    files = seed_files(paths, family)
    return files[0] if files else None


def cmd_seeds(args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    rows = []
    for path in seed_files(paths, args.family)[: args.limit]:
        rows.append({
            "family": path.parent.name,
            "name": path.name,
            "path": str(path),
            "size": path.stat().st_size,
            "mtime": path.stat().st_mtime,
        })
    emit({"ok": True, "family": args.family, "seeds": rows})
    return 0


def cmd_roms(args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    index = load_rom_index(paths)
    rows = []
    for key, entry in sorted(index.get("roms", {}).items()):
        path = Path(str(entry.get("path") or ""))
        rows.append({
            **entry,
            "game_key": key,
            "exists": path.exists(),
            "size": path.stat().st_size if path.exists() else 0,
        })
    emit({"ok": True, "roms": rows, "index": str(rom_index_path(paths))})
    return 0


def cmd_rom_register(args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    client = find_game(paths, args.game_key)
    key = slug(str(client.get("game_key") or args.game_key))
    rom_path = Path(args.path)
    if not rom_path.exists():
        emit({"ok": False, "error": "rom_missing", "game_key": key, "path": str(rom_path)})
        return 2
    index = load_rom_index(paths)
    entry = {
        "game_key": key,
        "game": client.get("game"),
        "platform": client.get("platform"),
        "family": family_for(client),
        "path": str(rom_path),
        "filename": rom_path.name,
        "size": rom_path.stat().st_size,
        "sha1": sha1_file(rom_path) if args.sha1 else "",
        "registered_at": utc_now(),
    }
    index.setdefault("roms", {})[key] = entry
    save_rom_index(paths, index)
    host_updates = update_host_yaml_rom(paths, key, str(rom_path)) if args.update_host else []
    emit({"ok": True, "rom": entry, "host_updates": host_updates, "index": str(rom_index_path(paths))})
    return 0


def cmd_trackers(args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    index = load_tracker_index(paths)
    rows = []
    for key, entry in sorted(index.get("trackers", {}).items()):
        path = Path(str(entry.get("path") or ""))
        rows.append({
            **entry,
            "game_key": key,
            "exists": path.exists(),
            "size": path.stat().st_size if path.exists() else 0,
        })
    emit({"ok": True, "trackers": rows, "index": str(tracker_index_path(paths))})
    return 0


def cmd_tracker_register(args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    client = find_game(paths, args.game_key)
    key = slug(str(client.get("game_key") or args.game_key))
    tracker_path = Path(args.path)
    if not tracker_path.exists():
        emit({"ok": False, "error": "tracker_missing", "game_key": key, "path": str(tracker_path)})
        return 2
    index = load_tracker_index(paths)
    entry = {
        "game_key": key,
        "game": client.get("game"),
        "platform": client.get("platform"),
        "family": family_for(client),
        "path": str(tracker_path),
        "filename": tracker_path.name,
        "size": tracker_path.stat().st_size,
        "sha1": sha1_file(tracker_path) if args.sha1 else "",
        "variant": args.variant or str(client.get("default_tracker_variant") or ""),
        "registered_at": utc_now(),
    }
    index.setdefault("trackers", {})[key] = entry
    save_tracker_index(paths, index)
    emit({"ok": True, "tracker": entry, "index": str(tracker_index_path(paths))})
    return 0


def server_state_path(paths: LabPaths, family: str) -> Path:
    return paths.lab / "status" / f"server-{family}.json"


def is_pid_running(pid: int) -> bool:
    if pid <= 0:
        return False
    if os.name == "nt" or Path("/c/Windows").exists():
        proc = subprocess.run(["tasklist", "/FI", f"PID eq {pid}", "/FO", "CSV"], text=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        return str(pid) in proc.stdout
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


def terminate_pid(pid: int) -> bool:
    if pid <= 0:
        return False
    if os.name == "nt" or Path("/c/Windows").exists():
        proc = subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        return proc.returncode == 0
    try:
        os.kill(pid, 15)
        return True
    except OSError:
        return False


def start_multiserver(paths: LabPaths, family: str, seed: Path, port: int, password: str = "") -> dict[str, Any]:
    python = archipelago_python(paths)
    log_dir = paths.lab / "logs" / "servers"
    log_dir.mkdir(parents=True, exist_ok=True)
    log_path = log_dir / f"{datetime.now().strftime('%Y%m%d-%H%M%S')}-{family}-multiserver.log"
    argv = [
        python,
        str(paths.ap / "MultiServer.py"),
        "--host",
        "0.0.0.0",
        "--port",
        str(port),
        "--loglevel",
        "info",
        "--logtime",
        "--compatibility",
        "2",
    ]
    if password:
        argv.extend(["--password", password])
    argv.append(str(seed))
    env = os.environ.copy()
    env.setdefault("SKIP_REQUIREMENTS_UPDATE", "1")
    env.setdefault("KIVY_NO_ARGS", "1")
    env.setdefault("KIVY_NO_FILELOG", "1")
    out = log_path.open("w", encoding="utf-8")
    creationflags = 0
    if os.name == "nt":
        creationflags = getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
    proc = subprocess.Popen(argv, cwd=str(paths.ap), stdout=out, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, env=env, creationflags=creationflags)
    state = {
        "ok": True,
        "family": family,
        "pid": proc.pid,
        "host": "127.0.0.1",
        "bind_host": "0.0.0.0",
        "port": port,
        "endpoint": f"127.0.0.1:{port}",
        "seed": str(seed),
        "log": str(log_path),
        "started_at": utc_now(),
        "password": password,
    }
    write_json(server_state_path(paths, family), state)
    return state


def cmd_server(args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    family = args.family
    if args.action == "game":
        client = find_game(paths, family)
        key = slug(str(client.get("game_key") or client.get("game") or family))
        family = family_for(client)
        state_path = server_state_path(paths, family)
        seed = latest_seed(paths, f"game:{key}")
        if not seed:
            emit({"ok": False, "error": "seed_missing", "game_key": key, "family": family, "hint": f"run generate-game {key} first"})
            return 2
        current = load_json(state_path, {})
        if current and is_pid_running(int(current.get("pid") or 0)) and not args.force:
            current["running"] = True
            emit({"ok": True, "already_running": True, "server": current})
            return 0
        port = args.port or DEFAULT_PORT_BY_FAMILY.get(family, DEFAULT_PORT_BY_FAMILY["other"])
        state = start_multiserver(paths, family, seed, port, args.password)
        state["game_key"] = key
        write_json(state_path, state)
        emit({"ok": True, "server": state})
        return 0
    state_path = server_state_path(paths, family)
    if args.action == "status":
        states = []
        targets = list(FAMILY_PLATFORMS) if family == "all" else [family]
        for fam in targets:
            state = load_json(server_state_path(paths, fam), {})
            if state:
                state["running"] = is_pid_running(int(state.get("pid") or 0))
                if state.get("log"):
                    state["log_tail"] = read_tail(Path(state["log"]), 1600)
            states.append({"family": fam, "state": state})
        emit({"ok": True, "servers": states})
        return 0
    if args.action == "stop":
        state = load_json(state_path, {})
        pid = int(state.get("pid") or 0)
        stopped = terminate_pid(pid)
        if state:
            state["running"] = False
            state["stopped_at"] = utc_now()
            write_json(state_path, state)
        emit({"ok": True, "family": family, "pid": pid, "stopped": stopped})
        return 0
    if args.action == "start":
        current = load_json(state_path, {})
        if current and is_pid_running(int(current.get("pid") or 0)) and not args.force:
            current["running"] = True
            emit({"ok": True, "already_running": True, "server": current})
            return 0
        seed = Path(args.seed) if args.seed else latest_seed(paths, family)
        if not seed or not seed.exists():
            emit({"ok": False, "error": "seed_missing", "family": family, "hint": f"run generate {family} first"})
            return 2
        port = args.port or DEFAULT_PORT_BY_FAMILY.get(family, DEFAULT_PORT_BY_FAMILY["other"])
        state = start_multiserver(paths, family, seed, port, args.password)
        emit({"ok": True, "server": state})
        return 0
    emit({"ok": False, "error": f"unknown_server_action:{args.action}"})
    return 2


def find_game(paths: LabPaths, game_key: str) -> dict[str, Any]:
    needle = slug(game_key)
    for client in load_registry(paths):
        if slug(str(client.get("game_key") or "")) == needle or slug(str(client.get("game") or "")) == needle:
            return client
    raise SystemExit(f"game_not_found:{game_key}")


def cmd_plan_launch(args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    client = find_game(paths, args.game_key)
    family = family_for(client)
    log_dir = paths.lab / "logs" / "sessions" / f"{slug(str(client.get('game_key')))}-{datetime.now().strftime('%Y%m%d-%H%M%S')}"
    log_dir.mkdir(parents=True, exist_ok=True)
    python = archipelago_python(paths)
    game_key = str(client.get("game_key") or args.game_key)
    rom = args.rom or imported_rom_for(paths, game_key)
    tracker_pack = args.tracker_pack or imported_tracker_for(paths, game_key)
    argv = [
        python,
        str(paths.runtime / "bootchain_supervisor.py"),
        "plan",
        "--game-key",
        str(client.get("game_key")),
        "--slot",
        args.slot or slot_name(client),
        "--server",
        args.server,
        "--password",
        args.password,
        "--log-dir",
        str(log_dir),
    ]
    if rom:
        argv.extend(["--rom", rom])
    if tracker_pack:
        argv.extend(["--tracker-pack", tracker_pack])
    proc = subprocess.run(argv, cwd=str(paths.repo), text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        payload = json.loads(proc.stdout)
    except json.JSONDecodeError:
        payload = {"ok": False, "raw": proc.stdout}
    payload.update({"returncode": proc.returncode, "family": family, "log_dir": str(log_dir)})
    emit(payload)
    return proc.returncode


def cmd_launch(args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    client = find_game(paths, args.game_key)
    family = args.family or family_for(client)
    server_state = load_json(server_state_path(paths, family), {})
    if not server_state or not is_pid_running(int(server_state.get("pid") or 0)):
        if args.auto_server:
            key = slug(str(client.get("game_key") or args.game_key))
            seed = latest_seed(paths, f"game:{key}") or latest_seed(paths, family)
            if not seed:
                emit({"ok": False, "error": "seed_missing", "family": family, "hint": f"run generate-game {key} first or generate {family}"})
                return 2
            server_state = start_multiserver(paths, family, seed, args.port or DEFAULT_PORT_BY_FAMILY.get(family, DEFAULT_PORT_BY_FAMILY["other"]), args.password)
        else:
            emit({"ok": False, "error": "server_not_running", "family": family, "hint": f"run server start {family} first"})
            return 2
    session_id = f"{slug(str(client.get('game_key')))}-{datetime.now().strftime('%Y%m%d-%H%M%S')}"
    log_dir = paths.lab / "logs" / "sessions" / session_id
    log_dir.mkdir(parents=True, exist_ok=True)
    python = archipelago_python(paths)
    game_key = str(client.get("game_key") or args.game_key)
    rom = args.rom or imported_rom_for(paths, game_key)
    tracker_pack = args.tracker_pack or imported_tracker_for(paths, game_key)
    command = [
        python,
        str(paths.runtime / "bootchain_supervisor.py"),
        "launch" if args.run else "plan",
        "--game-key",
        str(client.get("game_key")),
        "--slot",
        args.slot or slot_name(client),
        "--server",
        args.server or str(server_state.get("endpoint") or ""),
        "--password",
        args.password or str(server_state.get("password") or ""),
        "--log-dir",
        str(log_dir),
        "--session-id",
        session_id,
    ]
    if rom:
        command.extend(["--rom", rom])
    if tracker_pack:
        command.extend(["--tracker-pack", tracker_pack])
    if args.max_runtime:
        command.extend(["--max-runtime", str(args.max_runtime)])
    if args.with_emu:
        command.append("--launch-sekaiemu")
    if args.with_tracker:
        command.append("--launch-tracker")
    if args.dry_run:
        command.append("--dry-run")

    if args.run:
        code = run_logged(paths, f"launch-{slug(str(client.get('game_key')))}", command, cwd=paths.repo)
        payload = {
            "ok": code == 0,
            "returncode": code,
            "game_key": client.get("game_key"),
            "game": client.get("game"),
            "family": family,
            "server": server_state,
            "log_dir": str(log_dir),
        }
    else:
        proc = subprocess.run(command, cwd=str(paths.repo), text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        try:
            payload = json.loads(proc.stdout)
        except json.JSONDecodeError:
            payload = {"ok": False, "raw": proc.stdout}
        payload.update({
            "returncode": proc.returncode,
            "game_key": client.get("game_key"),
            "game": client.get("game"),
            "family": family,
            "server": server_state,
            "log_dir": str(log_dir),
        })
        code = proc.returncode
    write_json(paths.lab / "status" / "last-launch.json", payload)
    emit(payload)
    return code


def cmd_mark(args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    client = find_game(paths, args.game_key)
    key = str(client.get("game_key"))
    status = load_json(paths.status_file, {"schema": 1, "games": {}})
    entry = status.setdefault("games", {}).setdefault(key, {})
    entry.update({
        "game": client.get("game"),
        "platform": client.get("platform"),
        "family": family_for(client),
        "wrapper": client.get("wrapper"),
        "status": args.status,
        "last_result": args.note or args.status,
        "updated_at": utc_now(),
    })
    write_json(paths.status_file, status)
    emit({"ok": True, "game_key": key, "status": args.status, "status_file": str(paths.status_file)})
    return 0


def cmd_logs(args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    logs = sorted((paths.lab / "logs").glob("**/*.log"), key=lambda p: p.stat().st_mtime, reverse=True)
    rows = []
    for path in logs[: args.limit]:
        entry = {"path": str(path), "size": path.stat().st_size, "mtime": path.stat().st_mtime}
        if args.tail:
            entry["tail"] = read_tail(path, args.tail)
        rows.append(entry)
    emit({"ok": True, "logs": rows})
    return 0


def cmd_status(args: argparse.Namespace) -> int:
    paths = lab_paths()
    ensure_lab(paths)
    status = load_json(paths.status_file, {"games": {}})
    counts: dict[str, int] = {}
    for entry in status.get("games", {}).values():
        counts[str(entry.get("status") or "untested")] = counts.get(str(entry.get("status") or "untested"), 0) + 1
    last_command = load_json(paths.lab / "status" / "last-command.json", {})
    last_launch = load_json(paths.lab / "status" / "last-launch.json", {})
    isolated_root = paths.lab / "seeds" / "games"
    isolated_count = len(list(isolated_root.rglob("*.zip"))) + len(list(isolated_root.rglob("*.archipelago"))) if isolated_root.exists() else 0
    seeds = {family: len(seed_files(paths, family)) for family in FAMILY_PLATFORMS}
    seeds["isolated_games"] = isolated_count
    servers = []
    for family in FAMILY_PLATFORMS:
        server = load_json(server_state_path(paths, family), {})
        if server:
            server["running"] = is_pid_running(int(server.get("pid") or 0))
        servers.append({"family": family, "server": server})
    emit({
        "ok": True,
        "lab": str(paths.lab),
        "counts": counts,
        "seeds": seeds,
        "servers": servers,
        "last_command": last_command,
        "last_launch": last_launch,
    })
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="SekaiLink Runtime Lab Windows helper")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("init")
    sub.add_parser("doctor")
    sub.add_parser("bootstrap-python")
    list_parser = sub.add_parser("list")
    list_parser.add_argument("family", nargs="?", default="all")
    yaml_parser = sub.add_parser("generate-yamls")
    yaml_parser.add_argument("family")
    yaml_parser.add_argument("--runtime-only", action="store_true")
    gen_parser = sub.add_parser("generate")
    gen_parser.add_argument("family")
    gen_parser.add_argument("--runtime-only", action="store_true")
    gen_game_parser = sub.add_parser("generate-game")
    gen_game_parser.add_argument("game_key")
    gen_game_parser.add_argument("--runtime-only", action="store_true")
    seeds_parser = sub.add_parser("seeds")
    seeds_parser.add_argument("family", nargs="?", default="all")
    seeds_parser.add_argument("--limit", type=int, default=20)
    sub.add_parser("roms")
    rom_register_parser = sub.add_parser("rom-register")
    rom_register_parser.add_argument("game_key")
    rom_register_parser.add_argument("path")
    rom_register_parser.add_argument("--sha1", action="store_true")
    rom_register_parser.add_argument("--update-host", action="store_true")
    sub.add_parser("trackers")
    tracker_register_parser = sub.add_parser("tracker-register")
    tracker_register_parser.add_argument("game_key")
    tracker_register_parser.add_argument("path")
    tracker_register_parser.add_argument("--variant", default="")
    tracker_register_parser.add_argument("--sha1", action="store_true")
    server_parser = sub.add_parser("server")
    server_parser.add_argument("action", choices=("start", "stop", "status", "game"))
    server_parser.add_argument("family", nargs="?", default="all")
    server_parser.add_argument("--seed", default="")
    server_parser.add_argument("--port", type=int, default=0)
    server_parser.add_argument("--password", default="")
    server_parser.add_argument("--force", action="store_true")
    plan_parser = sub.add_parser("plan-launch")
    plan_parser.add_argument("game_key")
    plan_parser.add_argument("--server", default="")
    plan_parser.add_argument("--slot", default="")
    plan_parser.add_argument("--password", default="")
    plan_parser.add_argument("--rom", default="")
    plan_parser.add_argument("--tracker-pack", default="")
    launch_parser = sub.add_parser("launch")
    launch_parser.add_argument("game_key")
    launch_parser.add_argument("--family", default="")
    launch_parser.add_argument("--server", default="")
    launch_parser.add_argument("--slot", default="")
    launch_parser.add_argument("--password", default="")
    launch_parser.add_argument("--rom", default="")
    launch_parser.add_argument("--tracker-pack", default="")
    launch_parser.add_argument("--port", type=int, default=0)
    launch_parser.add_argument("--auto-server", action="store_true")
    launch_parser.add_argument("--run", action="store_true")
    launch_parser.add_argument("--dry-run", action="store_true")
    launch_parser.add_argument("--with-emu", action="store_true")
    launch_parser.add_argument("--with-tracker", action="store_true")
    launch_parser.add_argument("--max-runtime", type=float, default=0.0)
    mark_parser = sub.add_parser("mark")
    mark_parser.add_argument("game_key")
    mark_parser.add_argument("status", choices=("stable", "unstable", "untested", "not_ideal", "trackerless"))
    mark_parser.add_argument("note", nargs="?", default="")
    logs_parser = sub.add_parser("logs")
    logs_parser.add_argument("--limit", type=int, default=20)
    logs_parser.add_argument("--tail", type=int, default=0)
    status_parser = sub.add_parser("status")
    status_parser.add_argument("family", nargs="?", default="all")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    commands = {
        "init": cmd_init,
        "doctor": cmd_doctor,
        "bootstrap-python": cmd_bootstrap_python,
        "list": cmd_list,
        "generate-yamls": cmd_generate_yamls,
        "generate": cmd_generate,
        "generate-game": cmd_generate_game,
        "seeds": cmd_seeds,
        "roms": cmd_roms,
        "rom-register": cmd_rom_register,
        "trackers": cmd_trackers,
        "tracker-register": cmd_tracker_register,
        "server": cmd_server,
        "plan-launch": cmd_plan_launch,
        "launch": cmd_launch,
        "mark": cmd_mark,
        "logs": cmd_logs,
        "status": cmd_status,
    }
    return commands[args.command](args)


if __name__ == "__main__":
    raise SystemExit(main())
