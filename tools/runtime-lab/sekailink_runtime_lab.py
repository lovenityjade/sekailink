#!/usr/bin/env python3
"""Linux-side TUI/controller for the SekaiLink Runtime Lab."""

from __future__ import annotations

import argparse
import json
import os
import readline
import re
import shlex
import shutil
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from rich import box
from rich.console import Console
from rich.panel import Panel
from rich.table import Table


SEKAI_TEAL = "#35f2d0"
SEKAI_CORAL = "#ff6b4a"
SEKAI_PURPLE = "#b28cff"
SEKAI_MUTED = "#87a0a3"
CONFIG_PATH = Path.home() / ".config" / "sekailink-runtime-lab" / "config.json"
HISTORY_PATH = Path.home() / ".local" / "state" / "sekailink-runtime-lab" / "history"


DEFAULT_CONFIG = {
    "ssh_host": "sekailink-windows",
    "windows_host_ip": "",
    "ssh_config": str(Path.home() / ".ssh" / "config"),
    "ssh_password": "",
    "remote_repo": "/d/SekaiLink/repos/sekailink-canonical",
    "lab_root": "/d/RuntimeLab",
    "worker": "./tools/windows-worker/sekai-worker-msys.sh",
    "local_rom_roots": [
        str(Path.home() / ".config" / "sekailink-client" / "roms"),
    ],
    "local_tracker_roots": [
        str(Path.home() / "Downloads"),
    ],
}

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


def slug(value: str) -> str:
    raw = re.sub(r"[^a-z0-9]+", "_", str(value).lower()).strip("_")
    return raw or "game"


def slot_name(client: dict[str, Any]) -> str:
    key = slug(str(client.get("game_key") or client.get("game") or "game"))
    code = SLOT_CODE_BY_GAME_KEY.get(key, key.replace("_", "")[:8].upper() or "GAME")
    return f"Jade-{code}"[:16]

ROM_EXTENSIONS = {".nes", ".sfc", ".smc", ".gb", ".gbc", ".gba", ".z64", ".n64", ".v64", ".iso", ".rvz", ".gcm", ".cci"}
ROM_HINTS_BY_GAME_KEY = {
    "a_link_to_the_past": ["kamigami no triforce", "alttp", "link to the past"],
    "chrono_trigger": ["chrono_trigger", "chrono trigger"],
    "donkey_kong_country": ["donkey_kong_country_usa", "donkey kong country"],
    "donkey_kong_country_2": ["donkey_kong_country_2", "diddy's kong quest"],
    "donkey_kong_country_3": ["donkey_kong_country_3", "dixie kong"],
    "earthbound": ["earthbound"],
    "final_fantasy": ["final_fantasy_usa", "final fantasy (usa"],
    "final_fantasy_iv": ["final_fantasy_ii", "final fantasy ii"],
    "final_fantasy_mystic_quest": ["final_fantasy_mystic_quest", "mystic quest"],
    "final_fantasy_v": ["final_fantasy_v", "final fantasy v", "ffv"],
    "kirbys_dream_land_3": ["kirbys_dream_land_3", "kirby's dream land 3"],
    "lufia_ii": ["lufia_ii", "lufia ii"],
    "mega_man_2": ["mega_man_2", "mega man 2"],
    "mega_man_3": ["mega_man_3", "mega man 3"],
    "mega_man_x3": ["mega_man_x3", "mega man x3", "mmx3"],
    "metroid_fusion": ["metroid_fusion", "metroid fusion"],
    "metroid_prime": ["metroid_prime", "metroid prime"],
    "metroid_zero_mission": ["metroid_zero_mission", "zero mission"],
    "mario_luigi_superstar_saga": ["mario_luigi_superstar_saga", "superstar saga"],
    "mega_man_battle_network_3": ["megaman_battle_network_3", "battle network 3"],
    "links_awakening_dx": ["links_awakening_dx", "link's awakening dx", "awakening dx"],
    "oracle_of_ages": ["oracle_of_ages", "oracle of ages"],
    "oracle_of_seasons": ["oracle_of_seasons", "oracle of seasons"],
    "super_mario_land_2": ["super_mario_land_2", "6 golden coins"],
    "the_legend_of_zelda": ["the_legend_of_zelda_prg0", "legend of zelda, the (u)"],
    "zelda_ii": ["zelda_ii", "zelda 2", "adventure of link"],
    "the_minish_cap": ["minish_cap", "the minish cap"],
    "wario_land": ["wario_land_super_mario_land_3", "wario land - super mario land 3"],
    "wario_land_4": ["wario_land_4", "wario land 4"],
    "ocarina_of_time": ["ocarina_of_time", "ocarina of time"],
    "majoras_mask": ["majoras_mask", "majora's mask"],
    "paper_mario": ["paper_mario_usa.z64", "paper mario (usa).z64"],
    "super_mario_64": ["super_mario_64", "super mario 64"],
    "thousand_year_door": ["paper_mario_ttyd", "thousand-year door", "thousand_year_door"],
    "super_metroid": ["super_metroid", "super metroid"],
    "super_mario_world": ["super_mario_world", "super mario world"],
    "secret_of_evermore": ["secret_of_evermore", "secret of evermore"],
}

TRACKER_HINTS_BY_GAME_KEY = {
    "a_link_to_the_past": ["alttp-linkedworld", "alttp", "link to the past"],
    "donkey_kong_country": ["dkc", "donkey_kong_country"],
    "donkey_kong_country_2": ["dkc2", "donkey_kong_country_2"],
    "donkey_kong_country_3": ["dkc3", "donkey_kong_country_3"],
    "earthbound": ["earthbound"],
    "kirbys_dream_land_3": ["kdl3", "dream land 3"],
    "mario_luigi_superstar_saga": ["mlss", "superstar"],
    "mega_man_2": ["megaman2", "mega_man_2", "megaman2-ap"],
    "mega_man_x3": ["megamanx3", "mega_man_x3", "mmx3"],
    "mega_man_battle_network_3": ["mmbn3", "battle_network_3"],
    "metroid_fusion": ["metroid.fusion", "metroid_fusion", "fusion"],
    "metroid_zero_mission": ["metroidzeromission", "zero_mission", "mzm"],
    "pokemon_crystal": ["crystal-ap", "pokemon_crystal"],
    "pokemon_firered": ["frlg", "firered", "leafgreen"],
    "pokemon_red_blue": ["rb_tracker", "pokemon_red_blue"],
    "pokemon_emerald": ["emerald"],
    "secret_of_mana": ["som-open", "secret_of_mana"],
    "smz3": ["smz3-ap", "smz3"],
    "super_mario_land_2": ["supermarioland2", "sml2", "mario_land_2"],
    "super_mario_sunshine": ["sms-poptracker", "sms_poptracker", "sunshine"],
    "super_metroid": ["supermetroid", "super_metroid"],
    "the_minish_cap": ["tmcr", "minish", "tmc"],
    "twilight_princess": ["tprap", "twilight"],
    "zelda_ii": ["zelda-2", "zelda_2"],
}


@dataclass
class CommandResult:
    returncode: int
    stdout: str
    stderr: str


class RuntimeLabTui:
    def __init__(self, args: argparse.Namespace) -> None:
        self.console = Console()
        self.config = self.load_config()
        self.args = args

    def load_config(self) -> dict[str, Any]:
        if CONFIG_PATH.exists():
            try:
                data = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
                return {**DEFAULT_CONFIG, **data}
            except Exception:
                pass
        return dict(DEFAULT_CONFIG)

    def save_config(self) -> None:
        CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
        CONFIG_PATH.write_text(json.dumps(self.config, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    def remote_worker_command(self, lab_args: list[str]) -> list[str]:
        repo = self.config["remote_repo"]
        lab_root = self.config["lab_root"]
        worker = self.config["worker"]
        inner = " ".join([
            f"export SEKAILINK_RUNTIME_LAB_ROOT={shlex.quote(lab_root)}",
            "&&",
            f"cd {shlex.quote(repo)}",
            "&&",
            shlex.quote(worker),
            "lab",
            *[shlex.quote(arg) for arg in lab_args],
        ])
        inner_for_ps = inner.replace("'", "''")
        remote = (
            "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
            f"\"& 'C:\\msys64\\usr\\bin\\bash.exe' -lc '{inner_for_ps}'\""
        )
        cmd = []
        password = str(self.config.get("ssh_password") or "")
        if password and shutil.which("sshpass"):
            cmd.extend(["sshpass", "-p", password])
        cmd.append("ssh")
        ssh_config = str(self.config.get("ssh_config") or "")
        if ssh_config:
            cmd.extend(["-F", ssh_config])
        cmd.extend([self.config["ssh_host"], remote])
        return cmd

    def ssh_command(self, remote: str) -> list[str]:
        cmd = []
        password = str(self.config.get("ssh_password") or "")
        if password and shutil.which("sshpass"):
            cmd.extend(["sshpass", "-p", password])
        cmd.append("ssh")
        ssh_config = str(self.config.get("ssh_config") or "")
        if ssh_config:
            cmd.extend(["-F", ssh_config])
        cmd.extend([self.config["ssh_host"], remote])
        return cmd

    def scp_command(self, source: Path, destination: str) -> list[str]:
        cmd = []
        password = str(self.config.get("ssh_password") or "")
        if password and shutil.which("sshpass"):
            cmd.extend(["sshpass", "-p", password])
        cmd.append("scp")
        ssh_config = str(self.config.get("ssh_config") or "")
        if ssh_config:
            cmd.extend(["-F", ssh_config])
        cmd.extend([str(source), f"{self.config['ssh_host']}:{destination}"])
        return cmd

    def run_remote(self, lab_args: list[str]) -> CommandResult:
        cmd = self.remote_worker_command(lab_args)
        proc = subprocess.run(cmd, text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return CommandResult(proc.returncode, proc.stdout, proc.stderr)

    def local_repo_root(self) -> Path:
        return Path(__file__).resolve().parents[2]

    def load_local_clients(self) -> list[dict[str, Any]]:
        path = self.local_repo_root() / "runtime" / "game-registry" / "archipelago-clients.json"
        try:
            return list(json.loads(path.read_text(encoding="utf-8")).get("clients") or [])
        except Exception:
            return []

    def local_rom_roots(self, extra: list[str] | None = None) -> list[Path]:
        return self.local_roots("local_rom_roots", extra)

    def local_tracker_roots(self, extra: list[str] | None = None) -> list[Path]:
        return self.local_roots("local_tracker_roots", extra)

    def local_roots(self, config_key: str, extra: list[str] | None = None) -> list[Path]:
        roots = [Path(os.path.expanduser(str(root))) for root in self.config.get(config_key, [])]
        for raw in extra or []:
            roots.append(Path(os.path.expanduser(raw)))
        seen = set()
        result = []
        for root in roots:
            try:
                resolved = root.resolve()
            except Exception:
                resolved = root
            if resolved in seen or not root.exists():
                continue
            seen.add(resolved)
            result.append(root)
        return result

    def discover_rom_files(self, extra_roots: list[str] | None = None) -> list[Path]:
        files: list[Path] = []
        for root in self.local_rom_roots(extra_roots):
            for path in root.rglob("*"):
                if path.is_file() and path.suffix.lower() in ROM_EXTENSIONS:
                    # RVZ is useful as an archive format, but the current lab
                    # launchers expect directly usable ROM/ISO files.
                    if path.suffix.lower() == ".rvz":
                        continue
                    files.append(path)
        return sorted(files)

    def match_roms(self, extra_roots: list[str] | None = None) -> list[dict[str, Any]]:
        files = self.discover_rom_files(extra_roots)
        rows = []
        used: set[Path] = set()
        for client in self.load_local_clients():
            key = str(client.get("game_key") or "")
            hints = [hint.lower() for hint in ROM_HINTS_BY_GAME_KEY.get(key, [])]
            best: tuple[int, Path] | None = None
            if not hints:
                continue
            for path in files:
                name = path.name.lower()
                score = 0
                for hint in hints:
                    if hint and hint in name:
                        score = max(score, 100 + len(hint))
                if score and (best is None or score > best[0]):
                    best = (score, path)
            if best:
                used.add(best[1])
                rows.append({
                    "game_key": key,
                    "game": client.get("game"),
                    "platform": client.get("platform"),
                    "path": str(best[1]),
                    "filename": best[1].name,
                    "score": best[0],
                })
        return rows

    def discover_tracker_files(self, extra_roots: list[str] | None = None) -> list[Path]:
        files: list[Path] = []
        for root in self.local_tracker_roots(extra_roots):
            for path in root.rglob("*.zip"):
                if path.is_file():
                    files.append(path)
        return sorted(files)

    def match_trackers(self, extra_roots: list[str] | None = None) -> list[dict[str, Any]]:
        files = self.discover_tracker_files(extra_roots)
        rows = []
        for client in self.load_local_clients():
            key = str(client.get("game_key") or "")
            hints = [hint.lower() for hint in TRACKER_HINTS_BY_GAME_KEY.get(key, [])]
            if not hints:
                continue
            best: tuple[int, Path] | None = None
            for path in files:
                name = path.name.lower()
                score = 0
                for hint in hints:
                    if hint and hint in name:
                        score = max(score, 100 + len(hint))
                if score and (best is None or score > best[0]):
                    best = (score, path)
            if best:
                rows.append({
                    "game_key": key,
                    "game": client.get("game"),
                    "platform": client.get("platform"),
                    "path": str(best[1]),
                    "filename": best[1].name,
                    "score": best[0],
                })
        return rows

    def find_local_client(self, game_key: str) -> dict[str, Any] | None:
        wanted = slug(game_key)
        for client in self.load_local_clients():
            if slug(str(client.get("game_key") or "")) == wanted:
                return client
        return None

    def local_rom_for(self, game_key: str, explicit: str = "") -> str:
        if explicit:
            path = Path(os.path.expanduser(explicit))
            return str(path) if path.exists() else ""
        wanted = slug(game_key)
        for row in self.match_roms():
            if slug(str(row.get("game_key") or "")) == wanted:
                return str(row.get("path") or "")
        return ""

    def local_tracker_for(self, game_key: str, explicit: str = "") -> str:
        if explicit:
            path = Path(os.path.expanduser(explicit))
            return str(path) if path.exists() else ""
        wanted = slug(game_key)
        for row in self.match_trackers():
            if slug(str(row.get("game_key") or "")) == wanted:
                return str(row.get("path") or "")
        return ""

    def windows_reachable_endpoint(self, endpoint: str) -> str:
        raw = str(endpoint or "").strip()
        if not raw:
            return ""
        if ":" not in raw:
            return raw
        host, port = raw.rsplit(":", 1)
        if host in {"127.0.0.1", "localhost", "0.0.0.0"}:
            host = str(self.config.get("windows_host_ip") or self.config.get("ssh_host") or "sekailink-windows")
        return f"{host}:{port}"

    def ensure_windows_server(self, game_key: str, family: str, auto_server: bool, password: str, port: int, force: bool) -> dict[str, Any]:
        if auto_server:
            command = ["runtime-server", "game", game_key]
            if port:
                command.extend(["--port", str(port)])
            if password:
                command.extend(["--password", password])
            if force:
                command.append("--force")
        else:
            command = ["runtime-server", "status", family or "all"]
        result = self.run_remote(command)
        payload = self.parse_jsonish(result.stdout)
        if result.returncode != 0 or not isinstance(payload, dict) or not payload.get("ok"):
            return {
                "ok": False,
                "error": "windows_server_failed",
                "command": command,
                "returncode": result.returncode,
                "stdout": result.stdout,
                "stderr": result.stderr,
                "payload": payload,
            }
        server = payload.get("server") if isinstance(payload.get("server"), dict) else {}
        if not server and isinstance(payload.get("servers"), list):
            for row in payload["servers"]:
                candidate = row.get("state") if isinstance(row, dict) else {}
                if isinstance(candidate, dict) and candidate.get("running"):
                    server = candidate
                    break
        if not server:
            return {"ok": False, "error": "windows_server_state_missing", "payload": payload}
        endpoint = self.windows_reachable_endpoint(str(server.get("endpoint") or ""))
        if not endpoint:
            return {"ok": False, "error": "windows_server_endpoint_missing", "payload": payload}
        server = dict(server)
        server["linux_endpoint"] = endpoint
        return {"ok": True, "server": server, "payload": payload}

    def local_bootchain(self, argv: list[str], plan_only: bool) -> dict[str, Any]:
        parser = argparse.ArgumentParser(prog="launch-local", add_help=False)
        parser.add_argument("game_key")
        parser.add_argument("--family", default="")
        parser.add_argument("--server", default="")
        parser.add_argument("--slot", default="")
        parser.add_argument("--password", default="")
        parser.add_argument("--rom", default="")
        parser.add_argument("--tracker-pack", default="")
        parser.add_argument("--port", type=int, default=0)
        parser.add_argument("--auto-server", dest="auto_server", action="store_true", default=True)
        parser.add_argument("--no-auto-server", dest="auto_server", action="store_false")
        parser.add_argument("--force-server", dest="force_server", action="store_true", default=True)
        parser.add_argument("--no-force-server", dest="force_server", action="store_false")
        parser.add_argument("--run", action="store_true")
        parser.add_argument("--dry-run", action="store_true")
        parser.add_argument("--with-emu", action="store_true")
        parser.add_argument("--with-tracker", action="store_true")
        parser.add_argument("--max-runtime", type=float, default=0.0)
        try:
            args = parser.parse_args(argv)
        except SystemExit:
            return {"ok": False, "error": "invalid_local_launch_args", "argv": argv}

        client = self.find_local_client(args.game_key)
        if not client:
            return {"ok": False, "error": "game_not_found", "game_key": args.game_key}
        family = args.family or {
            "NES": "nes",
            "SNES": "snes",
            "GB": "gb_gbc",
            "GBC": "gb_gbc",
            "GBA": "gba",
            "N64": "n64",
            "GameCube": "gamecube",
            "Wii": "gamecube",
        }.get(str(client.get("platform") or ""), "other")

        server_state: dict[str, Any] = {}
        server_endpoint = args.server
        if not server_endpoint:
            server_result = self.ensure_windows_server(args.game_key, family, args.auto_server, args.password, args.port, args.force_server)
            if not server_result.get("ok"):
                return server_result
            server_state = dict(server_result.get("server") or {})
            server_endpoint = str(server_state.get("linux_endpoint") or "")
        else:
            server_endpoint = self.windows_reachable_endpoint(server_endpoint)

        game_key = str(client.get("game_key") or args.game_key)
        session_id = f"linux-{slug(game_key)}-{int(time.time())}"
        log_dir = Path.home() / ".cache" / "sekailink-runtime-lab" / "sessions" / session_id
        log_dir.mkdir(parents=True, exist_ok=True)
        command = [
            os.environ.get("PYTHON", "python3"),
            str(self.local_repo_root() / "runtime" / "bootchain_supervisor.py"),
            "plan" if plan_only or not args.run else "launch",
            "--game-key", game_key,
            "--slot", args.slot or slot_name(client),
            "--server", server_endpoint,
            "--password", args.password or str(server_state.get("password") or ""),
            "--log-dir", str(log_dir),
            "--session-id", session_id,
        ]
        rom = self.local_rom_for(game_key, args.rom)
        tracker_pack = self.local_tracker_for(game_key, args.tracker_pack)
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

        proc = subprocess.run(command, cwd=str(self.local_repo_root()), text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        payload = self.parse_jsonish(proc.stdout)
        if not isinstance(payload, dict):
            payload = {"ok": proc.returncode == 0, "raw": proc.stdout}
        payload.update({
            "returncode": proc.returncode,
            "mode": "linux-local-runtime",
            "windows_server": server_state,
            "linux_endpoint": server_endpoint,
            "local_rom": rom,
            "local_tracker_pack": tracker_pack,
            "command": command,
            "log_dir": str(log_dir),
        })
        return payload

    def remote_mkdir(self, remote_dir: str) -> CommandResult:
        escaped = shlex.quote(remote_dir)
        remote = (
            "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
            f"\"& 'C:\\msys64\\usr\\bin\\bash.exe' -lc 'mkdir -p {escaped}'\""
        )
        proc = subprocess.run(self.ssh_command(remote), text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return CommandResult(proc.returncode, proc.stdout, proc.stderr)

    def rom_scan(self, extra_roots: list[str] | None = None) -> dict[str, Any]:
        rows = self.match_roms(extra_roots)
        return {"ok": True, "roots": [str(root) for root in self.local_rom_roots(extra_roots)], "roms": rows}

    def rom_import(self, extra_roots: list[str] | None = None) -> dict[str, Any]:
        rows = self.match_roms(extra_roots)
        imported = []
        failed = []
        for row in rows:
            key = row["game_key"]
            source = Path(row["path"])
            remote_dir_msys = f"/d/RuntimeLab/roms/by-game/{key}"
            remote_dir_win = f"D:/RuntimeLab/roms/by-game/{key}"
            mkdir_res = self.remote_mkdir(remote_dir_msys)
            if mkdir_res.returncode != 0:
                failed.append({"game_key": key, "path": str(source), "error": mkdir_res.stderr or mkdir_res.stdout})
                continue
            safe_name = "".join(ch if ch.isalnum() or ch in " ._()-[]" else "_" for ch in source.name)
            remote_path = f"{remote_dir_win}/{safe_name}"
            scp = subprocess.run(self.scp_command(source, remote_path), text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if scp.returncode != 0:
                failed.append({"game_key": key, "path": str(source), "error": scp.stderr or scp.stdout})
                continue
            register = self.run_remote(["runtime-rom-register", key, remote_path])
            payload = self.parse_jsonish(register.stdout)
            if register.returncode != 0 or not isinstance(payload, dict) or not payload.get("ok"):
                failed.append({"game_key": key, "path": remote_path, "error": register.stderr or register.stdout})
                continue
            imported.append({"game_key": key, "game": row.get("game"), "platform": row.get("platform"), "path": remote_path})
        return {"ok": not failed, "kind": "rom", "imported": imported, "failed": failed, "count": len(imported)}

    def tracker_scan(self, extra_roots: list[str] | None = None) -> dict[str, Any]:
        rows = self.match_trackers(extra_roots)
        return {"ok": True, "roots": [str(root) for root in self.local_tracker_roots(extra_roots)], "trackers": rows}

    def tracker_import(self, extra_roots: list[str] | None = None) -> dict[str, Any]:
        rows = self.match_trackers(extra_roots)
        imported = []
        failed = []
        for row in rows:
            key = row["game_key"]
            source = Path(row["path"])
            remote_dir_msys = f"/d/RuntimeLab/trackers/by-game/{key}"
            remote_dir_win = f"D:/RuntimeLab/trackers/by-game/{key}"
            mkdir_res = self.remote_mkdir(remote_dir_msys)
            if mkdir_res.returncode != 0:
                failed.append({"game_key": key, "path": str(source), "error": mkdir_res.stderr or mkdir_res.stdout})
                continue
            safe_name = "".join(ch if ch.isalnum() or ch in " ._()-[]" else "_" for ch in source.name)
            remote_path = f"{remote_dir_win}/{safe_name}"
            scp = subprocess.run(self.scp_command(source, remote_path), text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if scp.returncode != 0:
                failed.append({"game_key": key, "path": str(source), "error": scp.stderr or scp.stdout})
                continue
            register = self.run_remote(["runtime-tracker-register", key, remote_path])
            payload = self.parse_jsonish(register.stdout)
            if register.returncode != 0 or not isinstance(payload, dict) or not payload.get("ok"):
                failed.append({"game_key": key, "path": remote_path, "error": register.stderr or register.stdout})
                continue
            imported.append({"game_key": key, "game": row.get("game"), "platform": row.get("platform"), "path": remote_path})
        return {"ok": not failed, "kind": "tracker", "imported": imported, "failed": failed, "count": len(imported)}

    def parse_jsonish(self, text: str) -> Any:
        stripped = text.strip()
        if not stripped:
            return None
        # The worker wraps commands with log lines before and after JSON.
        decoder = json.JSONDecoder()
        starts = [idx for idx, ch in enumerate(stripped) if ch == "{"]
        top_level_ok = []
        ok_candidates = []
        fallback = None
        best_span = -1
        for idx in starts:
            try:
                payload, end = decoder.raw_decode(stripped[idx:])
                line_start = stripped.rfind("\n", 0, idx) + 1
                is_top_level = stripped[line_start:idx] == ""
                if end > best_span:
                    fallback = payload
                    best_span = end
                # Prefer the last command-level result. Nested rows inside a
                # larger payload usually do not carry an explicit ok field.
                if isinstance(payload, dict) and isinstance(payload.get("ok"), bool):
                    ok_candidates.append((end, payload))
                    if is_top_level:
                        top_level_ok.append(payload)
            except json.JSONDecodeError:
                continue
        if top_level_ok:
            return top_level_ok[-1]
        if ok_candidates:
            return max(ok_candidates, key=lambda pair: pair[0])[1]
        return fallback

    def print_result(self, result: CommandResult) -> None:
        payload = self.parse_jsonish(result.stdout)
        if payload is not None:
            self.print_payload(payload)
        else:
            if result.stdout.strip():
                self.console.print(result.stdout.rstrip())
        if result.stderr.strip():
            self.console.print(Panel(result.stderr.rstrip(), title="stderr", border_style=SEKAI_CORAL))
        if result.returncode != 0:
            self.console.print(f"[{SEKAI_CORAL}]exit code {result.returncode}[/]")

    def print_payload(self, payload: Any) -> None:
        if isinstance(payload, dict) and "games" in payload and isinstance(payload["games"], list):
            table = Table(title=f"Runtime Lab - {payload.get('family', 'all')}", box=box.ROUNDED, border_style=SEKAI_TEAL)
            for col in ("game_key", "game", "platform", "wrapper", "world", "lab_status", "last_result"):
                table.add_column(col)
            for row in payload["games"]:
                table.add_row(
                    str(row.get("game_key", "")),
                    str(row.get("game", "")),
                    str(row.get("platform", "")),
                    str(row.get("wrapper", "")),
                    "yes" if row.get("world_available") else f"missing:{row.get('world', '')}",
                    str(row.get("lab_status", "")),
                    str(row.get("last_result", "")),
                )
            self.console.print(table)
            return
        if isinstance(payload, dict) and "seeds" in payload and isinstance(payload["seeds"], list):
            table = Table(title=f"Seeds - {payload.get('family', 'all')}", box=box.ROUNDED, border_style=SEKAI_TEAL)
            table.add_column("Family")
            table.add_column("Seed")
            table.add_column("Size")
            table.add_column("Path")
            for row in payload["seeds"]:
                table.add_row(
                    str(row.get("family", "")),
                    str(row.get("name", "")),
                    str(row.get("size", "")),
                    str(row.get("path", "")),
                )
            self.console.print(table)
            return
        if isinstance(payload, dict) and "roms" in payload and isinstance(payload["roms"], list):
            table = Table(title=f"Runtime Lab ROMs ({len(payload['roms'])})", box=box.ROUNDED, border_style=SEKAI_TEAL)
            table.add_column("Game Key")
            table.add_column("Platform")
            table.add_column("ROM")
            table.add_column("State")
            for row in payload["roms"]:
                exists = row.get("exists")
                state = "ok" if exists is True else ("local" if exists is None else "missing")
                table.add_row(
                    str(row.get("game_key", "")),
                    str(row.get("platform", "")),
                    str(row.get("filename") or Path(str(row.get("path", ""))).name),
                    state,
                    style=None if state != "missing" else SEKAI_CORAL,
                )
            self.console.print(table)
            return
        if isinstance(payload, dict) and "trackers" in payload and isinstance(payload["trackers"], list):
            table = Table(title=f"Runtime Lab Trackers ({len(payload['trackers'])})", box=box.ROUNDED, border_style=SEKAI_TEAL)
            table.add_column("Game Key")
            table.add_column("Platform")
            table.add_column("Pack")
            table.add_column("State")
            for row in payload["trackers"]:
                exists = row.get("exists")
                state = "ok" if exists is True else ("local" if exists is None else "missing")
                table.add_row(
                    str(row.get("game_key", "")),
                    str(row.get("platform", "")),
                    str(row.get("filename") or Path(str(row.get("path", ""))).name),
                    state,
                    style=None if state != "missing" else SEKAI_CORAL,
                )
            self.console.print(table)
            return
        if isinstance(payload, dict) and "imported" in payload and "failed" in payload:
            kind = str(payload.get("kind") or "rom").upper()
            table = Table(title=f"{kind} Import - {payload.get('count', 0)} imported", box=box.ROUNDED, border_style=SEKAI_TEAL)
            table.add_column("Game Key")
            table.add_column("Platform")
            table.add_column("Path")
            for row in payload.get("imported", []):
                table.add_row(str(row.get("game_key", "")), str(row.get("platform", "")), str(row.get("path", "")))
            self.console.print(table)
            if payload.get("failed"):
                self.console.print(Panel(json.dumps(payload["failed"], indent=2, ensure_ascii=False), title="Failed", border_style=SEKAI_CORAL))
            return
        if isinstance(payload, dict) and "plan" in payload and isinstance(payload["plan"], dict):
            plan = payload["plan"]
            table = Table(title=f"Bootchain Plan - {plan.get('game_key', payload.get('game_key', ''))}", box=box.ROUNDED, border_style=SEKAI_TEAL)
            table.add_column("Field")
            table.add_column("Value")
            table.add_row("Game", str(plan.get("game") or payload.get("game") or ""))
            table.add_row("Family", str(payload.get("family", "")))
            table.add_row("Server", str(plan.get("server") or payload.get("server", {}).get("endpoint", "")))
            table.add_row("Slot", str(plan.get("slot", "")))
            table.add_row("Wrapper", " ".join(plan.get("wrapper_command") or []) or "not planned")
            table.add_row("Sekaiemu", " ".join(plan.get("sekaiemu_command") or []) or "not planned")
            table.add_row("Tracker", " ".join(plan.get("tracker_command") or []) or "not planned")
            table.add_row("Log dir", str(plan.get("log_dir") or payload.get("log_dir", "")))
            self.console.print(table)
            readiness = plan.get("readiness") or {}
            if readiness:
                ready_table = Table(title="Readiness", box=box.SIMPLE, border_style=SEKAI_PURPLE)
                ready_table.add_column("Part")
                ready_table.add_column("OK")
                for key, value in readiness.items():
                    ready_table.add_row(str(key), "yes" if value else "no", style=None if value else SEKAI_CORAL)
                self.console.print(ready_table)
            notes = plan.get("notes") or []
            if notes:
                self.console.print(Panel("\n".join(str(note) for note in notes), title="Notes", border_style=SEKAI_CORAL))
            return
        if isinstance(payload, dict) and "counts" in payload:
            counts = Table(title="Runtime Lab Status", box=box.ROUNDED, border_style=SEKAI_TEAL)
            counts.add_column("Status")
            counts.add_column("Count")
            for key, value in sorted(payload.get("counts", {}).items()):
                counts.add_row(str(key), str(value))
            self.console.print(counts)
            if payload.get("seeds"):
                seeds = Table(title="Seeds by Family", box=box.SIMPLE, border_style=SEKAI_PURPLE)
                seeds.add_column("Family")
                seeds.add_column("Count")
                for key, value in sorted(payload["seeds"].items()):
                    seeds.add_row(str(key), str(value))
                self.console.print(seeds)
            if payload.get("servers"):
                self.print_payload({"ok": True, "servers": payload["servers"]})
            return
        if isinstance(payload, dict) and "servers" in payload and isinstance(payload["servers"], list):
            table = Table(title="Runtime Lab Servers", box=box.ROUNDED, border_style=SEKAI_TEAL)
            table.add_column("Family")
            table.add_column("Running")
            table.add_column("PID")
            table.add_column("Endpoint")
            table.add_column("Seed")
            table.add_column("Log")
            for row in payload["servers"]:
                state = row.get("state") or row.get("server") or {}
                running = bool(state.get("running"))
                table.add_row(
                    str(row.get("family", "")),
                    "yes" if running else "no",
                    str(state.get("pid", "")),
                    str(state.get("endpoint", "")),
                    str(Path(str(state.get("seed", ""))).name if state.get("seed") else ""),
                    str(state.get("log", "")),
                    style=None if running else SEKAI_MUTED,
                )
            self.console.print(table)
            return
        if isinstance(payload, dict) and "server" in payload and isinstance(payload.get("server"), dict):
            server = payload["server"]
            table = Table(title=f"Server {server.get('family', '')}", box=box.ROUNDED, border_style=SEKAI_TEAL)
            table.add_column("Field")
            table.add_column("Value")
            for key in ("pid", "endpoint", "seed", "log", "started_at"):
                table.add_row(key, str(server.get(key, "")))
            self.console.print(table)
            return
        if isinstance(payload, dict) and "logs" in payload and isinstance(payload["logs"], list):
            table = Table(title="Runtime Lab Logs", box=box.ROUNDED, border_style=SEKAI_TEAL)
            table.add_column("Size")
            table.add_column("Path")
            for row in payload["logs"]:
                table.add_row(str(row.get("size", "")), str(row.get("path", "")))
            self.console.print(table)
            for row in payload["logs"]:
                if row.get("tail"):
                    self.console.print(Panel(str(row["tail"]).rstrip(), title=str(row.get("path", "")), border_style=SEKAI_PURPLE))
            return
        if isinstance(payload, dict) and "checks" in payload:
            table = Table(title="Runtime Lab Doctor", box=box.ROUNDED, border_style=SEKAI_TEAL)
            table.add_column("Check")
            table.add_column("OK")
            for key, value in payload.get("checks", {}).items():
                table.add_row(key, "yes" if value else "no", style=None if value else SEKAI_CORAL)
            self.console.print(table)
            self.console.print_json(json.dumps({k: v for k, v in payload.items() if k != "checks"}, ensure_ascii=False, default=str))
            return
        self.console.print_json(json.dumps(payload, ensure_ascii=False, default=str))

    def help(self) -> None:
        table = Table(title="Commandes", box=box.ROUNDED, border_style=SEKAI_PURPLE)
        table.add_column("Commande")
        table.add_column("Effet")
        rows = [
            ("doctor", "Vérifie le labo Windows et les briques runtime."),
            ("init", "Crée D:/RuntimeLab et la matrice de statut."),
            ("bootstrap-python", "Installe les wheels runtime dans le Python portable Windows."),
            ("list [family]", "Liste les jeux et statuts: all, nes, snes, gb_gbc, gba, n64, gamecube."),
            ("yamls <family>", "Génère les YAML par défaut Jade-<jeu>."),
            ("generate <family>", "Génère la sync Archipelago de cette famille."),
            ("generate-game <game_key>", "Génère une seed isolée pour un seul jeu."),
            ("rom-scan [dir]", "Repère les ROMs disponibles localement pour les jeux supportés."),
            ("rom-import [dir]", "Copie les ROMs vers D:/RuntimeLab et les enregistre."),
            ("roms", "Liste les ROMs déjà enregistrées côté Windows."),
            ("tracker-scan [dir]", "Repère les packs PopTracker disponibles localement."),
            ("tracker-import [dir]", "Copie les packs PopTracker vers D:/RuntimeLab."),
            ("trackers", "Liste les packs tracker enregistrés côté Windows."),
            ("seeds [family]", "Liste les seeds générées."),
            ("server start|stop|status [family]", "Contrôle un MultiServer AP local par famille."),
            ("server game <game_key>", "Démarre le serveur sur la seed isolée du jeu."),
            ("launch <game_key> [--auto-server] [--run]", "Prépare ou lance le bootchain runtime."),
            ("plan <game_key>", "Construit un plan de lancement bootchain pour un jeu."),
            ("plan-local <game_key>", "Démarre/repère la room Windows et prépare le runtime Linux local."),
            ("launch-local <game_key> [--run]", "Lance le runtime Linux local contre le serveur Windows."),
            ("status", "Résumé labo: statuts, seeds, serveurs, dernière commande."),
            ("mark <game> <status> [note]", "Met à jour stable/unstable/untested/not_ideal/trackerless."),
            ("logs [--tail N]", "Affiche les derniers logs RuntimeLab."),
            ("config", "Affiche la config locale."),
            ("quit", "Quitte."),
        ]
        for command, effect in rows:
            table.add_row(command, effect)
        self.console.print(table)

    def execute(self, line: str) -> bool:
        try:
            parts = shlex.split(line)
        except ValueError as exc:
            self.console.print(f"[{SEKAI_CORAL}]commande invalide: {exc}[/]")
            return True
        if not parts:
            return True
        command, *args = parts
        if command in {"quit", "exit"}:
            return False
        if command == "help":
            self.help()
            return True
        if command == "config":
            safe_config = dict(self.config)
            if safe_config.get("ssh_password"):
                safe_config["ssh_password"] = "***stored locally***"
            self.console.print_json(json.dumps(safe_config, indent=2, ensure_ascii=False))
            return True
        if command == "rom-scan":
            self.print_payload(self.rom_scan(args))
            return True
        if command == "rom-import":
            self.print_payload(self.rom_import(args))
            return True
        if command == "tracker-scan":
            self.print_payload(self.tracker_scan(args))
            return True
        if command == "tracker-import":
            self.print_payload(self.tracker_import(args))
            return True
        if command == "plan-local":
            self.print_payload(self.local_bootchain(args, plan_only=True))
            return True
        if command == "launch-local":
            self.print_payload(self.local_bootchain(args, plan_only=False))
            return True
        mapping = {
            "init": ["runtime-init"],
            "bootstrap-python": ["runtime-bootstrap-python"],
            "doctor": ["runtime-doctor"],
            "list": ["runtime-list", *(args or ["all"])],
            "yamls": ["runtime-yamls", *args],
            "generate": ["runtime-generate", *args],
            "generate-game": ["runtime-generate-game", *args],
            "roms": ["runtime-roms", *args],
            "trackers": ["runtime-trackers", *args],
            "seeds": ["runtime-seeds", *args],
            "server": ["runtime-server", *args],
            "start": ["runtime-server", "start", *(args or ["nes"])],
            "stop": ["runtime-server", "stop", *(args or ["nes"])],
            "launch": ["runtime-launch", *args],
            "plan": ["runtime-plan", *args],
            "mark": ["runtime-mark", *args],
            "status": ["runtime-status", *(args or ["all"])],
            "logs": ["runtime-logs", *args],
        }
        if command not in mapping:
            self.console.print(f"[{SEKAI_CORAL}]Commande inconnue:[/] {command}")
            return True
        self.print_result(self.run_remote(mapping[command]))
        return True

    def run_once(self, command: list[str]) -> int:
        result = self.run_remote(command)
        self.print_result(result)
        return result.returncode

    def loop(self) -> int:
        HISTORY_PATH.parent.mkdir(parents=True, exist_ok=True)
        try:
            readline.read_history_file(HISTORY_PATH)
        except FileNotFoundError:
            pass
        self.console.print(Panel.fit(
            "[bold]SekaiLink Runtime Lab[/]\nLinux TUI -> Windows MSYS2 -> D:/RuntimeLab",
            border_style=SEKAI_TEAL,
        ))
        self.help()
        while True:
            try:
                line = input("skl-runtime-lab: ")
            except (EOFError, KeyboardInterrupt):
                self.console.print()
                break
            if line.strip():
                readline.add_history(line)
            if not self.execute(line):
                break
        try:
            readline.write_history_file(HISTORY_PATH)
        except Exception:
            pass
        return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="SekaiLink Runtime Lab TUI")
    parser.add_argument("--remote", nargs=argparse.REMAINDER, help="Run a raw RuntimeLab worker command and exit")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    tui = RuntimeLabTui(args)
    if args.remote:
        return tui.run_once(args.remote)
    return tui.loop()


if __name__ == "__main__":
    raise SystemExit(main())
