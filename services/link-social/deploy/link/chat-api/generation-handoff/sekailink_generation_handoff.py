#!/usr/bin/env python3
"""Bridge Link Chat API lobby generation requests to the live Worlds queue.

The database remains the source of truth. This script only materializes the
selected config snapshots into transient generator input files and submits that
directory to the configured backend generation service.
"""

from __future__ import annotations

import argparse
import ast
import hashlib
import json
import os
import pickle
import re
import secrets
import signal
import shlex
import socket
import subprocess
import sys
import time
import warnings
import zlib
from urllib.parse import quote
import zipfile
from pathlib import Path
from typing import Any


os.environ.setdefault("SKIP_REQUIREMENTS_UPDATE", "1")
os.environ.setdefault("PYTHONNOUSERSITE", "1")


def safe_component(value: str, fallback: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9_.-]+", "-", value).strip("-._")
    return (cleaned or fallback)[:96]


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise ValueError("request_must_be_object")
    return value


def atomic_write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    with tmp.open("w", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write("\n")
    tmp.replace(path)


def atomic_write_text(path: Path, value: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    with tmp.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(value)
        if value and not value.endswith("\n"):
            stream.write("\n")
    tmp.replace(path)


def sha256_text(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def yaml_scalar(content: str, key: str) -> str:
    pattern = re.compile(rf"^{re.escape(key)}\s*:\s*(.*?)\s*(?:#.*)?$")
    for raw_line in content.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        match = pattern.match(line)
        if not match:
            continue
        value = match.group(1).strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in {"'", '"'}:
            value = value[1:-1]
        return value.strip()
    return ""


def yaml_quote(value: str) -> str:
    text = str(value).strip()
    if re.fullmatch(r"[A-Za-z0-9_. -]+", text) and text.lower() not in {
        "true",
        "false",
        "null",
        "none",
        "yes",
        "no",
        "on",
        "off",
    }:
        return text
    return json.dumps(text, ensure_ascii=False)


def set_top_level_yaml_scalar(content: str, key: str, value: str) -> str:
    rendered = f"{key}: {yaml_quote(value)}"
    lines = content.splitlines()
    pattern = re.compile(rf"^{re.escape(key)}\s*:")
    for index, line in enumerate(lines):
        if pattern.match(line):
            lines[index] = rendered
            return "\n".join(lines)
    return rendered + ("\n" + content if content else "")


def top_level_yaml_key(line: str) -> str:
    if not line or line[0].isspace() or line.lstrip().startswith("#"):
        return ""
    stripped = line.strip()
    if stripped[:1] in {"'", '"'}:
        quote_char = stripped[0]
        escaped = False
        value: list[str] = []
        for char in stripped[1:]:
            if quote_char == '"' and escaped:
                value.append(char)
                escaped = False
                continue
            if quote_char == '"' and char == "\\":
                escaped = True
                continue
            if char == quote_char:
                rest = stripped[len(value) + 2:].lstrip()
                return "".join(value).strip() if rest.startswith(":") else ""
            value.append(char)
        return ""
    match = re.match(r"^([^:#][^:]*?)\s*:", line)
    if not match:
        return ""
    return match.group(1).strip().strip("'\"")


def ensure_game_options_section(content: str, ap_game_name: str) -> str:
    game_name = str(ap_game_name).strip()
    if not game_name:
        return content
    lines = content.splitlines()
    if any(top_level_yaml_key(line) == game_name for line in lines):
        return content

    root_keys = {
        "name",
        "game",
        "description",
        "requires",
        "linked_options",
        "triggers",
    }
    root_lines: list[str] = []
    game_lines: list[str] = []
    in_game_block = False

    for line in lines:
        key = top_level_yaml_key(line)
        if key:
            in_game_block = key not in root_keys
        if in_game_block:
            game_lines.append(("  " + line) if line else line)
        else:
            root_lines.append(line)

    while root_lines and not root_lines[-1].strip():
        root_lines.pop()
    while game_lines and not game_lines[0].strip():
        game_lines.pop(0)

    if not game_lines:
        return "\n".join(root_lines)
    return "\n".join(root_lines + ["", f"{yaml_quote(game_name)}:"] + game_lines)


def normalized_token(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "", value.lower())


_AP_GAME_NAME_CACHE: dict[str, str] | None = None

AP_GAME_ALIASES: dict[str, str] = {
    # Live Worlds currently registers this APWorld without the title colon.
    "metroid_zero_mission": "Metroid Zero Mission",
    "metroidzeromission": "Metroid Zero Mission",
    "Metroid: Zero Mission": "Metroid Zero Mission",
    # The installed LADX APWorld is the beta package and registers this exact game name.
    "ladx": "Links Awakening DX Beta",
    "links_awakening_dx": "Links Awakening DX Beta",
    "linksawakeningdx": "Links Awakening DX Beta",
    "links_awakening_dx_beta": "Links Awakening DX Beta",
    "linksawakeningdxbeta": "Links Awakening DX Beta",
    "Links Awakening DX": "Links Awakening DX Beta",
}


def ap_runtime_root() -> Path | None:
    for name in (
        "SEKAILINK_AP_RUNTIME_ROOT",
        "SEKAILINK_ROOM_RUNTIME_CWD",
    ):
        value = os.environ.get(name, "").strip()
        if value:
            path = Path(value)
            if (path / "worlds").is_dir():
                return path
    multiserver = os.environ.get("SEKAILINK_ROOM_RUNTIME_MULTISERVER", "").strip()
    if multiserver:
        path = Path(multiserver).resolve().parent
        if (path / "worlds").is_dir():
            return path
    return None


def generation_ap_root() -> Path | None:
    for name in (
        "SEKAILINK_GENERATION_AP_ROOT",
        "SEKAILINK_GENERATOR_AP_ROOT",
    ):
        value = os.environ.get(name, "").strip()
        if value:
            path = Path(value)
            if (path / "worlds").is_dir() and (path / "NetUtils.py").is_file():
                return path
    default = Path("/opt/sekailink-generate")
    if (default / "worlds").is_dir() and (default / "NetUtils.py").is_file():
        return default
    return None


def game_names_from_python_source(path: Path) -> list[str]:
    try:
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", SyntaxWarning)
            module = ast.parse(path.read_text(encoding="utf-8", errors="ignore"))
    except Exception:
        return []
    names: list[str] = []
    for node in ast.walk(module):
        if not isinstance(node, ast.ClassDef):
            continue
        for statement in node.body:
            if not isinstance(statement, ast.Assign):
                continue
            if not any(isinstance(target, ast.Name) and target.id == "game" for target in statement.targets):
                continue
            if isinstance(statement.value, ast.Constant) and isinstance(statement.value.value, str):
                names.append(statement.value.value.strip())
    return [name for name in names if name]


def ap_game_name_cache() -> dict[str, str]:
    global _AP_GAME_NAME_CACHE
    if _AP_GAME_NAME_CACHE is not None:
        return _AP_GAME_NAME_CACHE
    cache: dict[str, str] = {}
    root = ap_runtime_root()
    worlds_root = root / "worlds" if root else None
    if worlds_root and worlds_root.is_dir():
        for world_dir in worlds_root.iterdir():
            if not world_dir.is_dir() or world_dir.name.startswith("__"):
                continue
            names: list[str] = []
            manifest = world_dir / "archipelago.json"
            if manifest.is_file():
                try:
                    game = json.loads(manifest.read_text(encoding="utf-8")).get("game", "")
                    if isinstance(game, str) and game.strip():
                        names.append(game.strip())
                except Exception:
                    pass
            init_py = world_dir / "__init__.py"
            if init_py.is_file():
                names.extend(game_names_from_python_source(init_py))
            for name in names:
                cache.setdefault(normalized_token(name), name)
                cache.setdefault(normalized_token(world_dir.name), name)
    _AP_GAME_NAME_CACHE = cache
    return cache


def ap_game_name_for(game_key: str, game_display_name: str, raw_game: str) -> str:
    cache = ap_game_name_cache()
    for candidate in (raw_game, game_key, game_display_name):
        if candidate in AP_GAME_ALIASES:
            return AP_GAME_ALIASES[candidate]
        token = normalized_token(candidate)
        if token and token in AP_GAME_ALIASES:
            return AP_GAME_ALIASES[token]
        if token and token in cache:
            return cache[token]
    return raw_game or game_display_name or game_key or "unknown"


def compact_token(value: str, fallback: str, limit: int) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9]+", "", value)
    if not cleaned:
        cleaned = fallback
    return cleaned[:limit]


def short_digest(*parts: object, size: int = 6) -> str:
    digest = hashlib.sha1()
    for part in parts:
        digest.update(str(part).encode("utf-8", errors="ignore"))
        digest.update(b"\0")
    return digest.hexdigest()[:size]


def compat_player_name(
    username: str,
    game_key: str,
    game_display_name: str,
    instance: int,
    config_id: str,
    version_id: str,
    used: set[str],
    limit: int = 16,
) -> str:
    user_part = compact_token(username, "P", 7)
    game_part = compact_token(game_key or game_display_name, "G", 4)
    digest = short_digest(username, game_key, game_display_name, instance, config_id, version_id, size=4)
    pieces = [user_part, game_part]
    if instance > 1:
        pieces.append(str(instance))
    pieces.append(digest)
    candidate = "-".join(pieces)
    if len(candidate) > limit:
        overflow = len(candidate) - limit
        user_part = user_part[:max(1, len(user_part) - overflow)]
        pieces[0] = user_part
        candidate = "-".join(pieces)[:limit]
    base = candidate
    counter = 2
    while candidate in used:
        suffix = f"-{counter}"
        candidate = (base[: max(1, limit - len(suffix))] + suffix)[:limit]
        counter += 1
    used.add(candidate)
    return candidate


def zip_member_slot(member: str) -> int | None:
    basename = Path(member).name
    for pattern in (r"(?:^|[_-])P(\d+)(?:[_-]|$)", r"(?:^|[_-])Player(\d+)(?:[_-]|$)"):
        match = re.search(pattern, basename, re.IGNORECASE)
        if match:
            return int(match.group(1))
    return None


def zip_member_slot_name(member: str) -> str:
    stem = Path(member).stem
    match = re.search(r"(?:^|[_-])P\d+[_-](.+)$", stem, re.IGNORECASE)
    return match.group(1).strip("-_ ") if match else ""


def is_launch_artifact_member(member: str) -> bool:
    if member.endswith("/"):
        return False
    ext = Path(member).suffix.lower()
    if not ext:
        return False
    if ext in {".archipelago", ".txt", ".html", ".htm", ".json", ".yaml", ".yml", ".log", ".zip"}:
        return False
    return True


def safe_member_filename(member: str, entry_id: str) -> str:
    name = Path(member).name
    ext = Path(name).suffix
    stem = Path(name).stem
    safe_stem = safe_component(stem, f"entry-{entry_id}")
    safe_ext = re.sub(r"[^A-Za-z0-9.]+", "", ext)[:24]
    return f"{safe_stem}{safe_ext}"


def now_unix() -> int:
    return int(time.time())


def match_sync_entry(member: str, slot: int | None, sync_entries: list[dict[str, Any]]) -> dict[str, Any]:
    member_key = normalized_token(Path(member).stem)
    for entry in sync_entries:
        candidates = [
            str(entry.get("slot_name", "")),
            str(entry.get("player_file_stem", "")),
            str(entry.get("username", "")),
            str(entry.get("title", "")),
        ]
        for candidate in candidates:
            token = normalized_token(candidate)
            if token and (token in member_key or member_key in token):
                return entry
    if slot is not None:
        for entry in sync_entries:
            if int(entry.get("slot_index") or 0) == slot:
                return entry
    return {}


def index_sync_package(
    sync_package_path: str,
    sync_entries: list[dict[str, Any]],
    generation_id: str = "",
    extract_root: Path | None = None,
) -> dict[str, Any]:
    if not sync_package_path:
        return {"launch_entries": [], "multidata_members": [], "members": []}
    path = Path(sync_package_path)
    if not path.exists() or not zipfile.is_zipfile(path):
        return {"launch_entries": [], "multidata_members": [], "members": []}
    launch_entries: list[dict[str, Any]] = []
    with zipfile.ZipFile(path, "r") as archive:
        members = [info.filename for info in archive.infolist() if not info.is_dir()]
        multidata_members = [member for member in members if Path(member).suffix.lower() == ".archipelago"]
        for index, member in enumerate(members, start=1):
            if not is_launch_artifact_member(member):
                continue
            slot = zip_member_slot(member)
            slot_name = zip_member_slot_name(member)
            sync_entry = match_sync_entry(member, slot, sync_entries)
            entry_id = str(sync_entry.get("entry_id", "")) or f"zip-{index}"
            effective_slot = slot if slot is not None else int(sync_entry.get("slot_index") or 0) or None
            effective_slot_name = slot_name or str(sync_entry.get("slot_name", ""))
            filename = safe_member_filename(member, entry_id)
            artifact_path = ""
            artifact_sha256 = ""
            if extract_root is not None:
                extract_root.mkdir(parents=True, exist_ok=True)
                artifact_file = extract_root / filename
                if not artifact_file.exists():
                    with archive.open(member, "r") as src, artifact_file.open("wb") as dst:
                        while True:
                            chunk = src.read(1024 * 1024)
                            if not chunk:
                                break
                            dst.write(chunk)
                artifact_path = str(artifact_file)
                artifact_sha256 = sha256_file(artifact_file)
            download = ""
            if generation_id:
                download = f"/generation_artifacts/{quote(str(generation_id), safe='')}/{quote(entry_id, safe='')}/{quote(filename, safe='')}"
            launch_entries.append(
                {
                    "entry_id": entry_id,
                    "user_id": sync_entry.get("user_id", ""),
                    "username": sync_entry.get("username", ""),
                    "config_id": sync_entry.get("config_id", ""),
                    "version_id": sync_entry.get("version_id", ""),
                    "title": sync_entry.get("title", ""),
                    "game": sync_entry.get("game", ""),
                    "game_key": sync_entry.get("game_key", ""),
                    "game_display_name": sync_entry.get("game_display_name", ""),
                    "display_game": sync_entry.get("display_game", ""),
                    "display_label": sync_entry.get("display_label", ""),
                    "sync_game_instance": sync_entry.get("sync_game_instance", 1),
                    "compat_player_name": sync_entry.get("compat_player_name", ""),
                    "slot": effective_slot,
                    "slot_name": effective_slot_name,
                    "artifact_member": member,
                    "artifact_filename": filename,
                    "artifact_path": artifact_path,
                    "artifact_sha256": artifact_sha256,
                    "artifact_extension": Path(member).suffix.lower(),
                    "artifact_kind": "patch",
                    "sync_package_path": sync_package_path,
                    "download": download,
                }
            )
    return {"launch_entries": launch_entries, "multidata_members": multidata_members, "members": members}


def room_runtime_enabled() -> bool:
    return os.environ.get("SEKAILINK_ROOM_RUNTIME_ENABLED", "").strip() == "1"


def pid_alive(pid: int, expected_fragment: str = "") -> bool:
    if pid <= 0:
        return False
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        pass
    if expected_fragment:
        try:
            cmdline = Path(f"/proc/{pid}/cmdline").read_bytes().replace(b"\x00", b" ").decode("utf-8", "replace")
            return expected_fragment in cmdline
        except Exception:
            return False
    return True


def tcp_connectable(host: str, port: int, timeout: float = 0.25) -> bool:
    if port <= 0:
        return False
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


def local_probe_host(host: str) -> str:
    return "127.0.0.1" if host in {"", "0.0.0.0", "::"} else host


def wait_for_tcp(host: str, port: int, timeout: float) -> bool:
    host = local_probe_host(host)
    deadline = time.time() + max(0.0, timeout)
    while time.time() < deadline:
        if tcp_connectable(host, port):
            return True
        time.sleep(0.2)
    return tcp_connectable(host, port)


def allocate_room_runtime_port(host: str) -> int:
    explicit = os.environ.get("SEKAILINK_ROOM_RUNTIME_PORT", "").strip()
    if explicit:
        port = int(explicit)
        if port <= 0:
            raise ValueError("room_runtime_port_invalid")
        return port
    range_text = os.environ.get("SEKAILINK_ROOM_RUNTIME_PORT_RANGE", "38290-38390").strip()
    match = re.match(r"^(\d+)\s*-\s*(\d+)$", range_text)
    if match:
        start = int(match.group(1))
        end = int(match.group(2))
        if start <= 0 or end < start:
            raise ValueError("room_runtime_port_range_invalid")
        bind_host = local_probe_host(host)
        for port in range(start, end + 1):
            if tcp_connectable(bind_host, port):
                continue
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                try:
                    sock.bind((bind_host, port))
                    return port
                except OSError:
                    continue
        raise RuntimeError("room_runtime_port_range_exhausted")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind((local_probe_host(host), 0))
        return int(sock.getsockname()[1])


def extract_multidata(sync_package_path: str, member: str, runtime_root: Path) -> str:
    if not sync_package_path or not member:
        return ""
    package = Path(sync_package_path)
    if not package.exists() or not zipfile.is_zipfile(package):
        return ""
    multidata_dir = runtime_root / "multidata"
    multidata_dir.mkdir(parents=True, exist_ok=True)
    output = multidata_dir / safe_member_filename(member, "multidata")
    with zipfile.ZipFile(package, "r") as archive:
        if member not in archive.namelist():
            return ""
        if not output.exists():
            with archive.open(member, "r") as src, output.open("wb") as dst:
                while True:
                    chunk = src.read(1024 * 1024)
                    if not chunk:
                        break
                    dst.write(chunk)
    return str(output)


def room_runtime_savefile(state: dict[str, Any], runtime_root: Path) -> Path:
    save_root = runtime_root / "saves"
    save_root.mkdir(parents=True, exist_ok=True)
    generation_id = safe_component(str(state.get("generation_id", "")), "generation")
    return save_root / f"{generation_id}.apsave"


def ensure_room_runtime_server_password(state: dict[str, Any]) -> str:
    configured = os.environ.get("SEKAILINK_ROOM_RUNTIME_SERVER_PASSWORD", "").strip()
    if configured:
        state["room_ap_admin_password"] = configured
        state["room_ap_admin_password_source"] = "env"
        return configured
    existing = str(state.get("room_ap_admin_password") or "").strip()
    if existing:
        state.setdefault("room_ap_admin_password_source", "state")
        return existing
    password = "skl-" + secrets.token_urlsafe(32)
    state["room_ap_admin_password"] = password
    state["room_ap_admin_password_source"] = "generated"
    return password


def patch_multidata_server_password(multidata_path: str, server_password: str) -> str:
    if not server_password:
        return multidata_path
    path = Path(multidata_path)
    if not path.is_file() or path.suffix.lower() != ".archipelago":
        return multidata_path

    multiserver = os.environ.get("SEKAILINK_ROOM_RUNTIME_MULTISERVER", "").strip()
    if not multiserver:
        return multidata_path
    data = path.read_bytes()
    if not data:
        return multidata_path
    format_version = data[:1]
    decode_root = generation_ap_root() or Path(multiserver).resolve().parent
    decode_dir = str(decode_root)
    if decode_dir not in sys.path:
        sys.path.insert(0, decode_dir)
    for venv_name in (".venv", "venv"):
        for site_packages in (decode_root / venv_name).glob("lib/python*/site-packages"):
            site_text = str(site_packages)
            if site_text not in sys.path:
                sys.path.insert(0, site_text)
    # NetUtils.Hint is pickled inside multidata and has changed shape across
    # AP/MWGG versions. Force the generator's NetUtils to be imported for this
    # decode/encode pass so we do not reconstruct hints with the room runtime's
    # potentially older class signature.
    for module_name in ("NetUtils",):
        module_path = getattr(sys.modules.get(module_name), "__file__", "")
        if module_path and not str(module_path).startswith(decode_dir):
            sys.modules.pop(module_name, None)
    decoded = pickle.loads(zlib.decompress(data[1:]))
    if not isinstance(decoded, dict):
        return multidata_path
    server_options = decoded.get("server_options")
    if not isinstance(server_options, dict):
        server_options = {}
        decoded["server_options"] = server_options
    server_options["server_password"] = server_password
    patched = format_version + zlib.compress(pickle.dumps(decoded))
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_bytes(patched)
    tmp.replace(path)
    return str(path)


def room_runtime_command(
    state: dict[str, Any],
    multidata_path: str,
    host: str,
    port: int,
    runtime_root: Path,
) -> tuple[list[str], Path]:
    savefile = room_runtime_savefile(state, runtime_root)
    state["room_runtime_savefile"] = str(savefile)
    server_password = ensure_room_runtime_server_password(state)
    replacements = {
        "{multidata_path}": multidata_path,
        "{host}": host,
        "{port}": str(port),
        "{room_id}": str(state.get("lobby_id", "")),
        "{generation_id}": str(state.get("generation_id", "")),
        "{output_dir}": str(state.get("output_dir", "")),
        "{runtime_root}": str(runtime_root),
        "{savefile}": str(savefile),
        "{server_password}": server_password,
    }
    command = env_command("SEKAILINK_ROOM_RUNTIME_COMMAND", [])
    if command:
        resolved: list[str] = []
        for arg in command:
            for needle, replacement in replacements.items():
                arg = arg.replace(needle, replacement)
            resolved.append(arg)
        cwd = Path(os.environ.get("SEKAILINK_ROOM_RUNTIME_CWD", str(runtime_root)))
        return resolved, cwd
    multiserver = os.environ.get("SEKAILINK_ROOM_RUNTIME_MULTISERVER", "").strip()
    if not multiserver:
        raise ValueError("room_runtime_multiserver_missing")
    python = os.environ.get("SEKAILINK_ROOM_RUNTIME_PYTHON", sys.executable).strip() or sys.executable
    command = [
        python,
        multiserver,
        multidata_path,
        "--host",
        host,
        "--port",
        str(port),
        "--savefile",
        str(savefile),
        "--loglevel",
        os.environ.get("SEKAILINK_ROOM_RUNTIME_LOGLEVEL", "info"),
    ]
    if server_password:
        command.extend(["--server_password", server_password])
    if os.environ.get("SEKAILINK_ROOM_RUNTIME_DISABLE_SAVE", "").strip() == "1":
        command.append("--disable_save")
    if os.environ.get("SEKAILINK_ROOM_RUNTIME_USE_EMBEDDED_OPTIONS", "").strip() == "1":
        command.append("--use_embedded_options")
    auto_shutdown = os.environ.get("SEKAILINK_ROOM_RUNTIME_AUTO_SHUTDOWN_SECONDS", "").strip()
    if auto_shutdown:
        command.extend(["--auto_shutdown", auto_shutdown])
    cwd = Path(os.environ.get("SEKAILINK_ROOM_RUNTIME_CWD", str(Path(multiserver).parent)))
    return command, cwd


def refresh_room_runtime_state(state: dict[str, Any]) -> None:
    host = local_probe_host(str(
        state.get("room_bind_host")
        or state.get("room_host")
        or os.environ.get("SEKAILINK_ROOM_RUNTIME_HOST", "127.0.0.1")
    ).strip())
    port = int(state.get("room_port") or 0)
    pid = int(state.get("room_runtime_pid") or 0)
    expected_fragment = str(state.get("room_multidata_path") or state.get("generation_id") or "").strip()
    if pid > 0 and pid_alive(pid, expected_fragment):
        if tcp_connectable(host, port):
            state["room_status"] = "ready"
            state["last_port"] = port
        else:
            state["room_status"] = "starting"
            state["last_port"] = 0
        state["room_runtime_alive"] = True
        state.pop("room_runtime_error", None)
        state.pop("room_runtime_stopped_at", None)
        return
    if pid > 0:
        state["room_status"] = "stopped"
        state["room_runtime_alive"] = False
        state["room_runtime_pid"] = 0
        state["last_port"] = 0
        state.pop("room_url", None)
        state.setdefault("room_runtime_stopped_at", now_unix())


def stop_room_runtime(state: dict[str, Any]) -> None:
    refresh_room_runtime_state(state)
    pid = int(state.get("room_runtime_pid") or 0)
    expected_fragment = str(state.get("room_multidata_path") or state.get("generation_id") or "").strip()
    if pid <= 0 or not pid_alive(pid, expected_fragment):
        state["room_status"] = "stopped"
        state["room_runtime_alive"] = False
        state["room_runtime_pid"] = 0
        state["last_port"] = 0
        state.pop("room_url", None)
        return

    def send(sig: int) -> None:
        try:
            os.killpg(pid, sig)
        except ProcessLookupError:
            return
        except PermissionError:
            os.kill(pid, sig)
        except OSError:
            os.kill(pid, sig)

    try:
        send(signal.SIGTERM)
        deadline = time.time() + float(os.environ.get("SEKAILINK_ROOM_RUNTIME_STOP_TIMEOUT_SECONDS", "5"))
        while time.time() < deadline:
            if not pid_alive(pid, expected_fragment):
                break
            time.sleep(0.2)
        if pid_alive(pid, expected_fragment):
            send(signal.SIGKILL)
    except Exception as exc:
        state["room_runtime_error"] = f"room_runtime_stop_failed:{exc}"
        refresh_room_runtime_state(state)
        return

    state["room_status"] = "stopped"
    state["room_runtime_alive"] = False
    state["room_runtime_pid"] = 0
    state["last_port"] = 0
    state.pop("room_url", None)
    state["room_runtime_stopped_at"] = now_unix()


def ensure_room_runtime(state: dict[str, Any], sync_package_path: str, package_index: dict[str, Any]) -> None:
    if not room_runtime_enabled():
        return
    refresh_room_runtime_state(state)
    if state.get("room_runtime_alive"):
        return
    if int(state.get("room_runtime_pid") or 0) > 0:
        return
    multidata_members = package_index.get("multidata_members", [])
    if not isinstance(multidata_members, list) or not multidata_members:
        state["room_status"] = "package_ready"
        state["room_runtime_error"] = "room_multidata_missing"
        state["last_port"] = 0
        return
    host = os.environ.get("SEKAILINK_ROOM_RUNTIME_HOST", "127.0.0.1").strip() or "127.0.0.1"
    public_host = os.environ.get("SEKAILINK_ROOM_RUNTIME_PUBLIC_HOST", host).strip() or host
    runtime_root = Path(str(state.get("output_dir", ""))) / "room-runtime"
    runtime_root.mkdir(parents=True, exist_ok=True)
    multidata_path = extract_multidata(sync_package_path, str(multidata_members[0]), runtime_root)
    if not multidata_path:
        state["room_status"] = "package_ready"
        state["room_runtime_error"] = "room_multidata_extract_failed"
        state["last_port"] = 0
        return
    try:
        server_password = ensure_room_runtime_server_password(state)
        multidata_path = patch_multidata_server_password(multidata_path, server_password)
        state["room_multidata_admin_password_embedded"] = True
    except Exception as exc:
        state["room_status"] = "package_ready"
        state["room_runtime_error"] = f"room_multidata_admin_password_patch_failed:{exc}"
        state["last_port"] = 0
        return
    try:
        port = allocate_room_runtime_port(host)
    except Exception as exc:
        state["room_status"] = "package_ready"
        state["room_runtime_error"] = str(exc)
        state["last_port"] = 0
        return
    try:
        command, cwd = room_runtime_command(state, multidata_path, host, port, runtime_root)
    except Exception as exc:
        state["room_status"] = "package_ready"
        state["room_runtime_error"] = str(exc)
        state["last_port"] = 0
        return
    runtime_log = runtime_root / "multiserver.log"
    env = dict(os.environ)
    env.setdefault("SKIP_REQUIREMENTS_UPDATE", "1")
    runtime_log.parent.mkdir(parents=True, exist_ok=True)
    try:
        with runtime_log.open("ab") as log:
            try:
                process = subprocess.Popen(
                    command,
                    cwd=str(cwd),
                    stdin=subprocess.DEVNULL,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                    start_new_session=True,
                    env=env,
                )
            except PermissionError:
                process = subprocess.Popen(
                    command,
                    cwd=str(cwd),
                    stdin=subprocess.DEVNULL,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                    env=env,
                )
    except Exception as exc:
        state["room_status"] = "package_ready"
        state["room_runtime_error"] = str(exc)
        state["last_port"] = 0
        return
    state["room_runtime_pid"] = process.pid
    state["room_runtime_alive"] = True
    state["room_host"] = public_host
    state["room_bind_host"] = host
    state["room_port"] = port
    state["room_multidata_path"] = multidata_path
    state["room_runtime_log"] = str(runtime_log)
    state["room_runtime_started_at"] = now_unix()
    state.pop("room_runtime_error", None)
    state.pop("room_runtime_stopped_at", None)
    state["room_status"] = "starting"
    state["last_port"] = 0
    timeout = float(os.environ.get("SEKAILINK_ROOM_RUNTIME_START_TIMEOUT_SECONDS", "10"))
    if wait_for_tcp(host, port, timeout):
        state["room_status"] = "ready"
        state["last_port"] = port
    elif process.poll() is not None:
        state["room_status"] = "failed"
        state["room_runtime_alive"] = False
        state["room_runtime_pid"] = 0
        state["room_runtime_error"] = f"room_runtime_exit:{process.returncode}"


def generation_server_configured() -> bool:
    return bool(os.environ.get("SEKAILINK_GENERATION_HOST") and os.environ.get("SEKAILINK_GENERATION_PORT"))


def remote_spool_configured() -> bool:
    return bool(
        os.environ.get("SEKAILINK_GENERATION_REMOTE_HOST")
        and os.environ.get("SEKAILINK_GENERATION_REMOTE_SPOOL_ROOT")
    )


def env_command(name: str, default: list[str]) -> list[str]:
    raw = os.environ.get(name, "").strip()
    return shlex.split(raw) if raw else list(default)


def remote_timeout() -> float:
    return float(os.environ.get("SEKAILINK_GENERATION_REMOTE_TIMEOUT_SECONDS", "20"))


def remote_spool_root() -> str:
    root = os.environ.get("SEKAILINK_GENERATION_REMOTE_SPOOL_ROOT", "").rstrip("/")
    if not root.startswith("/"):
        raise ValueError("generation_remote_spool_root_must_be_absolute")
    if re.search(r"\s", root):
        raise ValueError("generation_remote_spool_root_must_not_contain_spaces")
    return root


def remote_sftp_enabled() -> bool:
    return bool(os.environ.get("SEKAILINK_GENERATION_REMOTE_SFTP_ROOT") or os.environ.get("SEKAILINK_GENERATION_SFTP_COMMAND"))


def remote_sftp_root() -> str:
    root = os.environ.get("SEKAILINK_GENERATION_REMOTE_SFTP_ROOT", "").rstrip("/") or remote_spool_root()
    if not root.startswith("/"):
        raise ValueError("generation_remote_sftp_root_must_be_absolute")
    if re.search(r"\s", root):
        raise ValueError("generation_remote_sftp_root_must_not_contain_spaces")
    return root or "/"


def remote_paths(safe_id: str) -> tuple[str, str, str]:
    root = f"{remote_spool_root()}/{safe_id}"
    return root, f"{root}/Players", f"{root}/output"


def host_to_sftp_path(host_path: str) -> str:
    host_root = remote_spool_root()
    sftp_root = remote_sftp_root()
    path = host_path.rstrip("/")
    if path == host_root:
        return sftp_root
    prefix = host_root + "/"
    if not path.startswith(prefix):
        raise ValueError("generation_remote_path_outside_spool")
    suffix = path[len(host_root):]
    return sftp_root.rstrip("/") + suffix


def sftp_to_host_path(sftp_path: str) -> str:
    host_root = remote_spool_root()
    sftp_root = remote_sftp_root()
    path = sftp_path.rstrip("/")
    if path == sftp_root:
        return host_root
    prefix = sftp_root.rstrip("/") + "/"
    if not path.startswith(prefix):
        raise ValueError("generation_sftp_path_outside_spool")
    suffix = path[len(sftp_root.rstrip("/")):]
    return host_root + suffix


def run_checked(args: list[str], label: str) -> subprocess.CompletedProcess[str]:
    env = dict(os.environ)
    env.setdefault("SKIP_REQUIREMENTS_UPDATE", "1")
    env.setdefault("PYTHONNOUSERSITE", "1")
    completed = subprocess.run(
        args,
        check=False,
        capture_output=True,
        text=True,
        timeout=remote_timeout(),
        env=env,
    )
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout or "").strip().splitlines()
        suffix = f":{detail[-1][:160]}" if detail else ""
        raise RuntimeError(f"{label}_failed:{completed.returncode}{suffix}")
    return completed


def run_sftp(commands: list[str], label: str, allow_failure: bool = False) -> subprocess.CompletedProcess[str]:
    remote_host = os.environ.get("SEKAILINK_GENERATION_REMOTE_HOST", "").strip()
    if not remote_host:
        raise ValueError("generation_remote_host_missing")
    sftp_cmd = env_command(
        "SEKAILINK_GENERATION_SFTP_COMMAND",
        ["sftp", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=accept-new"],
    )
    script = "\n".join(commands) + "\n"
    completed = subprocess.run(
        sftp_cmd + [remote_host],
        input=script,
        check=False,
        capture_output=True,
        text=True,
        timeout=remote_timeout(),
    )
    if completed.returncode != 0 and not allow_failure:
        detail = (completed.stderr or completed.stdout or "").strip().splitlines()
        suffix = f":{detail[-1][:160]}" if detail else ""
        raise RuntimeError(f"{label}_failed:{completed.returncode}{suffix}")
    return completed


def sync_job_to_remote(job_root: Path, safe_id: str) -> tuple[str, str, str]:
    remote_host = os.environ.get("SEKAILINK_GENERATION_REMOTE_HOST", "").strip()
    if not remote_host:
        raise ValueError("generation_remote_host_missing")
    remote_job_root, remote_players_dir, remote_output_dir = remote_paths(safe_id)
    remote_parent = remote_spool_root()
    if remote_sftp_enabled():
        remote_sftp_job_root = host_to_sftp_path(remote_job_root)
        remote_sftp_parent = host_to_sftp_path(remote_parent)
        remote_sftp_output_dir = host_to_sftp_path(remote_output_dir)
        run_sftp(
            [
                f"mkdir {remote_sftp_job_root}",
                f"mkdir {remote_sftp_output_dir}",
                f"chmod 0770 {remote_sftp_output_dir}",
                f"put {job_root / 'manifest.json'} {remote_sftp_job_root}/manifest.json",
                f"put -r {job_root / 'Players'} {remote_sftp_job_root}/",
            ],
            "generation_remote_sftp_copy",
        )
        return remote_job_root, remote_players_dir, remote_output_dir
    ssh_cmd = env_command(
        "SEKAILINK_GENERATION_SSH_COMMAND",
        ["ssh", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=accept-new"],
    )
    scp_cmd = env_command(
        "SEKAILINK_GENERATION_SCP_COMMAND",
        ["scp", "-r", "-p", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=accept-new"],
    )
    run_checked(ssh_cmd + [remote_host, f"mkdir -p {shlex.quote(remote_parent)}"], "generation_remote_mkdir")
    run_checked(scp_cmd + [str(job_root), f"{remote_host}:{remote_parent}/"], "generation_remote_copy")
    run_checked(ssh_cmd + [remote_host, f"mkdir -p {shlex.quote(remote_output_dir)}"], "generation_remote_output_mkdir")
    return remote_job_root, remote_players_dir, remote_output_dir


def remote_latest_zip(remote_output_dir: str) -> str:
    if not remote_spool_configured() or not remote_output_dir:
        return ""
    if remote_sftp_enabled():
        remote_sftp_output_dir = host_to_sftp_path(remote_output_dir)
        completed = run_sftp([f"ls -1 {remote_sftp_output_dir}/*.zip"], "generation_remote_sftp_artifact_probe", True)
        if completed.returncode != 0:
            return ""
        candidates: list[str] = []
        for line in completed.stdout.splitlines():
            value = line.strip()
            if not value or value.lower().startswith("sftp>"):
                continue
            if value.endswith(".zip"):
                candidates.append(value)
        if not candidates:
            return ""
        return sftp_to_host_path(sorted(candidates)[-1])
    remote_host = os.environ.get("SEKAILINK_GENERATION_REMOTE_HOST", "").strip()
    ssh_cmd = env_command(
        "SEKAILINK_GENERATION_SSH_COMMAND",
        ["ssh", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=accept-new"],
    )
    code = (
        "import pathlib,sys;"
        "p=pathlib.Path(sys.argv[1]);"
        "files=[x for x in p.rglob('*.zip') if x.is_file()];"
        "files.sort(key=lambda x:x.stat().st_mtime, reverse=True);"
        "print(str(files[0]) if files else '')"
    )
    completed = run_checked(
        ssh_cmd + [remote_host, f"python3 -c {shlex.quote(code)} {shlex.quote(remote_output_dir)}"],
        "generation_remote_artifact_probe",
    )
    return completed.stdout.strip().splitlines()[0] if completed.stdout.strip() else ""


def pull_remote_artifact(remote_artifact_path: str, output_dir: Path) -> str:
    if not remote_artifact_path:
        return ""
    if re.search(r"\s", remote_artifact_path):
        raise ValueError("generation_remote_artifact_path_must_not_contain_spaces")
    remote_host = os.environ.get("SEKAILINK_GENERATION_REMOTE_HOST", "").strip()
    output_dir.mkdir(parents=True, exist_ok=True)
    local_path = output_dir / Path(remote_artifact_path).name
    if remote_sftp_enabled():
        run_sftp(
            [f"get {host_to_sftp_path(remote_artifact_path)} {local_path}"],
            "generation_remote_sftp_artifact_pull",
        )
        return str(local_path)
    scp_cmd = env_command(
        "SEKAILINK_GENERATION_SCP_COMMAND",
        ["scp", "-p", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=accept-new"],
    )
    run_checked(scp_cmd + [f"{remote_host}:{remote_artifact_path}", str(local_path)], "generation_remote_artifact_pull")
    return str(local_path)


def generation_tcp_request(payload: dict[str, Any]) -> dict[str, Any]:
    host = os.environ["SEKAILINK_GENERATION_HOST"]
    port = int(os.environ["SEKAILINK_GENERATION_PORT"])
    token = os.environ.get("SEKAILINK_GENERATION_TOKEN", "")
    timeout = float(os.environ.get("SEKAILINK_GENERATION_TIMEOUT_SECONDS", "8"))
    if token:
        payload = dict(payload)
        payload["auth_token"] = token
    message = json.dumps(payload, separators=(",", ":")).encode("utf-8") + b"\n"
    with socket.create_connection((host, port), timeout=timeout) as sock:
        sock.settimeout(timeout)
        sock.sendall(message)
        chunks: list[bytes] = []
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            chunks.append(chunk)
            if b"\n" in chunk:
                break
    raw = b"".join(chunks).split(b"\n", 1)[0].decode("utf-8")
    value = json.loads(raw)
    if not isinstance(value, dict):
        raise ValueError("generation_response_must_be_object")
    return value


def latest_zip(output_dir: Path) -> str:
    if not output_dir.exists():
        return ""
    zips = [path for path in output_dir.rglob("*.zip") if path.is_file()]
    if not zips:
        return ""
    zips.sort(key=lambda path: path.stat().st_mtime, reverse=True)
    return str(zips[0])


def response_from_state(state: dict[str, Any]) -> dict[str, Any]:
    output_dir_text = str(state.get("output_dir", ""))
    output_dir = Path(output_dir_text)
    artifact_path = latest_zip(output_dir) or str(state.get("artifact_path", ""))
    sync_entries = state.get("sync_entries", [])
    if not isinstance(sync_entries, list):
        sync_entries = []
    extract_root = output_dir / "launch-artifacts" if artifact_path and output_dir_text else None
    package_index = index_sync_package(artifact_path, sync_entries, str(state.get("generation_id", "")), extract_root)
    refresh_room_runtime_state(state)
    response = {
        "ok": True,
        "status": state.get("status", "queued"),
        "generation_id": state.get("generation_id", ""),
        "job_id": state.get("job_id", state.get("generation_id", "")),
        "sync_id": state.get("sync_id", state.get("lobby_id", "")),
        "artifact_path": artifact_path,
        "sync_package_path": artifact_path,
        "sync_package_ready": bool(artifact_path),
        "sync_entries": sync_entries,
        "launch_entries": package_index["launch_entries"],
        "multidata_members": package_index["multidata_members"],
        "players_dir": state.get("players_dir", ""),
        "output_dir": output_dir_text,
        "room_status": state.get("room_status", ""),
        "last_port": int(state.get("last_port") or 0),
        "room_port": int(state.get("room_port") or 0),
        "room_host": state.get("room_host", ""),
        "room_bind_host": state.get("room_bind_host", ""),
        "room_multidata_path": state.get("room_multidata_path", ""),
        "room_runtime_savefile": state.get("room_runtime_savefile", ""),
        "room_runtime_log": state.get("room_runtime_log", ""),
        "room_runtime_pid": int(state.get("room_runtime_pid") or 0),
        "room_runtime_alive": bool(state.get("room_runtime_alive", False)),
    }
    if state.get("remote_players_dir"):
        response["remote_players_dir"] = state["remote_players_dir"]
    if state.get("remote_output_dir"):
        response["remote_output_dir"] = state["remote_output_dir"]
    if state.get("remote_artifact_path"):
        response["remote_artifact_path"] = state["remote_artifact_path"]
    if state.get("room_url"):
        response["room_url"] = state["room_url"]
    if state.get("room_runtime_error"):
        response["room_runtime_error"] = state["room_runtime_error"]
    if state.get("error"):
        response["error"] = state["error"]
    if state.get("generation_exit_code") is not None:
        response["generation_exit_code"] = state["generation_exit_code"]
    if state.get("generation_log_path"):
        response["generation_log_path"] = state["generation_log_path"]
    return response


def submit_generation(request: dict[str, Any], state_path: Path, spool_root: Path) -> dict[str, Any]:
    generation_id = request.get("generation_id") or f"generation-{int(time.time() * 1000)}"
    safe_id = safe_component(str(generation_id), "generation")
    job_root = spool_root / safe_id
    players_dir = job_root / "Players"
    output_dir = job_root / "output"
    players_dir.mkdir(parents=True, exist_ok=True)
    output_dir.mkdir(parents=True, exist_ok=True)

    work_items: list[dict[str, Any]] = []
    for player_index, player in enumerate(request.get("players", []), start=1):
        username = str(player.get("username", f"player-{player_index}")).strip() or f"player-{player_index}"
        user_id = str(player.get("user_id", "")).strip()
        for config_index, config in enumerate(player.get("configs", []), start=1):
            config_id = str(config.get("config_id", f"config-{config_index}")).strip() or f"config-{config_index}"
            version_id = str(config.get("version_id", "")).strip()
            content = config.get("content", "")
            if not isinstance(content, str) or not content.strip():
                raise ValueError(f"empty_config_content:{safe_component(username, 'player')}:{safe_component(config_id, 'config')}")
            raw_game = yaml_scalar(content, "game")
            game_key = str(config.get("game_key", "")).strip()
            game_display_name = str(config.get("game_display_name") or config.get("game") or raw_game or game_key or "unknown").strip()
            ap_game_name = ap_game_name_for(game_key, game_display_name, raw_game)
            if not raw_game:
                raw_game = ap_game_name
            if not game_key:
                game_key = normalized_token(ap_game_name or game_display_name or raw_game) or "unknown"
            work_items.append(
                {
                    "player_index": player_index,
                    "config_index": config_index,
                    "user_id": user_id,
                    "username": username,
                    "config_id": config_id,
                    "version_id": version_id,
                    "title": str(config.get("title", "")).strip(),
                    "game_key": game_key,
                    "game_display_name": game_display_name,
                    "ap_game_name": ap_game_name,
                    "raw_game": raw_game,
                    "source_slot_name": yaml_scalar(content, "name"),
                    "source": str(config.get("source", "")).strip(),
                    "values_hash": str(config.get("values_hash", "")).strip(),
                    "content": content,
                }
            )

    if not work_items:
        raise ValueError("generation_has_no_configs")

    duplicate_counts: dict[tuple[str, str], int] = {}
    for item in work_items:
        identity_key = str(item["user_id"] or item["username"])
        game_key = str(item["game_key"] or normalized_token(str(item["game_display_name"])))
        key = (identity_key, game_key)
        duplicate_counts[key] = duplicate_counts.get(key, 0) + 1

    files: list[dict[str, Any]] = []
    sync_entries: list[dict[str, Any]] = []
    seen_instances: dict[tuple[str, str], int] = {}
    used_compat_names: set[str] = set()
    for entry_index, item in enumerate(work_items, start=1):
        identity_key = str(item["user_id"] or item["username"])
        game_key = str(item["game_key"] or normalized_token(str(item["game_display_name"])))
        key = (identity_key, game_key)
        seen_instances[key] = seen_instances.get(key, 0) + 1
        instance = seen_instances[key]
        needs_instance_suffix = duplicate_counts.get(key, 0) > 1
        game_display_name = str(item["game_display_name"] or item["raw_game"] or item["game_key"] or "unknown")
        ap_game_name = str(item["ap_game_name"] or item["raw_game"] or game_display_name or item["game_key"] or "unknown")
        display_game = f"{game_display_name} {{{instance}}}" if needs_instance_suffix else game_display_name
        compat_name = compat_player_name(
            str(item["username"]),
            str(item["game_key"]),
            game_display_name,
            instance if needs_instance_suffix else 1,
            str(item["config_id"]),
            str(item["version_id"]),
            used_compat_names,
        )
        content = str(item["content"])
        content = set_top_level_yaml_scalar(content, "name", compat_name)
        content = set_top_level_yaml_scalar(content, "game", ap_game_name)
        content = ensure_game_options_section(content, ap_game_name)
        config_file_id = safe_component(str(item["config_id"]), f"config-{item['config_index']}")
        path = players_dir / f"{entry_index:03d}-{safe_component(compat_name, 'player')}-{config_file_id}.yaml"
        atomic_write_text(path, content)
        entry = {
            "entry_id": str(entry_index),
            "slot_index": entry_index,
            "slot_name": compat_name,
            "compat_player_name": compat_name,
            "display_player": item["username"],
            "display_game": display_game,
            "display_label": f"{item['username']} ({display_game})",
            "user_id": item["user_id"],
            "username": item["username"],
            "config_id": item["config_id"],
            "version_id": item["version_id"],
            "title": item["title"],
            "game": ap_game_name,
            "game_key": item["game_key"],
            "game_display_name": game_display_name,
            "ap_game_name": ap_game_name,
            "sync_game_instance": instance,
            "sync_game_instance_required": needs_instance_suffix,
            "source_slot_name": item["source_slot_name"],
            "source": item["source"],
            "values_hash": item["values_hash"],
            "player_file": str(path),
            "player_file_stem": path.stem,
            "sha256": sha256_text(content),
        }
        files.append(dict(entry))
        sync_entries.append(entry)

    manifest = {
        "schema_version": "sekailink-live-generation-spool-v1",
        "generation_id": generation_id,
        "lobby_id": request.get("lobby_id", ""),
        "sync_id": request.get("sync_id", request.get("lobby_id", "")),
        "created_at": request.get("created_at", ""),
        "source": request.get("source", "link-chat-api"),
        "players_dir": str(players_dir),
        "output_dir": str(output_dir),
        "files": files,
        "sync_entries": sync_entries,
    }
    atomic_write_json(job_root / "manifest.json", manifest)

    state = {
        "schema_version": "sekailink-live-generation-state-v1",
        "generation_id": generation_id,
        "job_id": generation_id,
        "lobby_id": request.get("lobby_id", ""),
        "sync_id": request.get("sync_id", request.get("lobby_id", "")),
        "status": "queued",
        "players_dir": str(players_dir),
        "output_dir": str(output_dir),
        "manifest_path": str(job_root / "manifest.json"),
        "sync_entries": sync_entries,
        "submitted": False,
        "updated_at": int(time.time()),
    }

    if generation_server_configured():
        submit_players_dir = str(players_dir)
        submit_output_dir = str(output_dir)
        if remote_spool_configured():
            remote_job_root, remote_players_dir, remote_output_dir = sync_job_to_remote(job_root, safe_id)
            state["remote_job_root"] = remote_job_root
            state["remote_players_dir"] = remote_players_dir
            state["remote_output_dir"] = remote_output_dir
            submit_players_dir = remote_players_dir
            submit_output_dir = remote_output_dir
        elif os.environ.get("SEKAILINK_GENERATION_ALLOW_LOCAL_PATHS") != "1":
            state["status"] = "error"
            state["error"] = "generation_remote_spool_not_configured"
            atomic_write_json(state_path, state)
            return {"ok": False, "status": "error", "error": state["error"]}
        submit_response = generation_tcp_request(
            {
                "cmd": "submit_job",
                "job_id": generation_id,
                "yaml_path": submit_players_dir,
                "output_dir": submit_output_dir,
            }
        )
        if not submit_response.get("ok", False):
            state["status"] = "error"
            state["error"] = submit_response.get("error", "generation_submit_rejected")
            atomic_write_json(state_path, state)
            return {"ok": False, "status": "error", "error": state["error"]}
        state["submitted"] = True
    else:
        state["warning"] = "generation_server_not_configured"

    atomic_write_json(state_path, state)
    response = response_from_state(state)
    if state.get("warning"):
        response["warning"] = state["warning"]
    return response


def generation_status(request: dict[str, Any], state_path: Path) -> dict[str, Any]:
    if not state_path.exists():
        return {"ok": False, "status": "error", "error": "generation_state_missing"}
    state = read_json(state_path)
    if generation_server_configured() and state.get("submitted"):
        status_response = generation_tcp_request({"cmd": "job_status", "job_id": state.get("job_id", request.get("generation_id", ""))})
        if status_response.get("ok", False) and isinstance(status_response.get("job"), dict):
            job = status_response["job"]
            state["status"] = job.get("status", state.get("status", "queued"))
            if state["status"] == "failed":
                detail = str(job.get("error_detail") or "").strip()
                if detail:
                    state["error"] = detail[-1800:]
                    state["generation_exit_code"] = job.get("exit_code")
                    state["generation_log_path"] = job.get("log_path", "")
                else:
                    state["error"] = f"generation_exit_code:{job.get('exit_code')}"
            else:
                state["error"] = ""
            if state["status"] in {"succeeded", "success", "completed"} and remote_spool_configured():
                remote_artifact = remote_latest_zip(str(state.get("remote_output_dir", "")))
                if remote_artifact:
                    state["remote_artifact_path"] = remote_artifact
                    state["artifact_path"] = pull_remote_artifact(remote_artifact, Path(state.get("output_dir", "")))
            state["updated_at"] = int(time.time())
            atomic_write_json(state_path, state)
        elif not status_response.get("ok", False):
            status_error = str(status_response.get("error") or "generation_status_failed")
            local_artifact = latest_zip(Path(str(state.get("output_dir", "")))) or str(state.get("artifact_path", ""))
            if status_error != "job_not_found" or not local_artifact:
                return {"ok": False, "status": "error", "error": status_error}
            state["status"] = "succeeded"
            state["artifact_path"] = local_artifact
            state["error"] = ""
            state["updated_at"] = int(time.time())
            atomic_write_json(state_path, state)
    artifact_path = latest_zip(Path(str(state.get("output_dir", "")))) or str(state.get("artifact_path", ""))
    status_text = str(state.get("status", "")).lower()
    if artifact_path and status_text not in {"failed", "error", "cancelled", "canceled"}:
        sync_entries = state.get("sync_entries", [])
        if not isinstance(sync_entries, list):
            sync_entries = []
        output_dir = Path(str(state.get("output_dir", "")))
        package_index = index_sync_package(
            artifact_path,
            sync_entries,
            str(state.get("generation_id", "")),
            output_dir / "launch-artifacts",
        )
        ensure_room_runtime(state, artifact_path, package_index)
        state["updated_at"] = int(time.time())
        atomic_write_json(state_path, state)
    return response_from_state(state)


def generation_admin_secrets(request: dict[str, Any], state_path: Path) -> dict[str, Any]:
    if not state_path.exists():
        return {"ok": False, "status": "error", "error": "generation_state_missing"}
    state = read_json(state_path)
    password = str(state.get("room_ap_admin_password") or "").strip()
    if not password:
        return {"ok": False, "status": "error", "error": "room_ap_admin_password_missing"}
    return {
        "ok": True,
        "generation_id": state.get("generation_id", request.get("generation_id", "")),
        "sync_id": state.get("sync_id", state.get("lobby_id", "")),
        "room_status": state.get("room_status", ""),
        "room_host": state.get("room_host", ""),
        "room_port": int(state.get("room_port") or 0),
        "room_runtime_alive": bool(state.get("room_runtime_alive", False)),
        "ap_admin_password": password,
        "ap_admin_password_source": state.get("room_ap_admin_password_source", ""),
    }


def validate_seed_config(request: dict[str, Any], state_path: Path, spool_root: Path) -> dict[str, Any]:
    content = str(request.get("content", "")).strip()
    if not content:
        return {"ok": False, "status": "error", "error": "validation_empty_config_content"}
    timeout = float(os.environ.get("SEKAILINK_CONFIG_VALIDATION_TIMEOUT_SECONDS", "60"))
    poll_interval = float(os.environ.get("SEKAILINK_CONFIG_VALIDATION_POLL_SECONDS", "1"))
    player_name = str(request.get("player_name") or "validation").strip() or "validation"
    game_key = str(request.get("game_key") or "").strip()
    game_display_name = str(request.get("game_display_name") or request.get("game") or game_key or yaml_scalar(content, "game") or "unknown").strip()
    generation_id = str(request.get("generation_id") or f"config-validation-{int(time.time() * 1000)}")
    submit_request = {
        "schema_version": "sekailink-config-validation-v1",
        "generation_id": generation_id,
        "lobby_id": "config-validation",
        "sync_id": generation_id,
        "created_at": request.get("created_at", ""),
        "source": "link-chat-api-config-validation",
        "players": [
            {
                "user_id": str(request.get("user_id", "")),
                "username": player_name,
                "configs": [
                    {
                        "config_id": str(request.get("config_id", "validation")),
                        "version_id": str(request.get("version_id", "")),
                        "title": str(request.get("title", "Validation")),
                        "game": game_display_name,
                        "game_key": game_key,
                        "game_display_name": game_display_name,
                        "content": content,
                        "source": "nexus.seed_config_api.validation",
                    }
                ],
            }
        ],
    }
    submitted = submit_generation(submit_request, state_path, spool_root)
    if not submitted.get("ok", False):
        return {
            "ok": False,
            "status": "error",
            "error": submitted.get("error", "validation_submit_failed"),
            "detail": submitted.get("detail", ""),
        }
    started = time.time()
    last: dict[str, Any] = submitted
    while time.time() - started <= timeout:
        last = generation_status({"generation_id": generation_id}, state_path)
        status = str(last.get("status", "")).lower()
        if status in {"succeeded", "generated", "ready"}:
            return {
                "ok": True,
                "status": "validated",
                "generation_id": generation_id,
                "message": "config_validated_by_archipelago",
            }
        if status in {"failed", "error"}:
            return {
                "ok": False,
                "status": "error",
                "generation_id": generation_id,
                "error": last.get("error", "config_validation_failed"),
                "detail": last.get("detail") or last.get("error_detail") or last.get("message", ""),
                "generation_exit_code": last.get("generation_exit_code"),
                "generation_log_path": last.get("generation_log_path", ""),
            }
        time.sleep(max(0.1, poll_interval))
    return {
        "ok": False,
        "status": "error",
        "generation_id": generation_id,
        "error": "config_validation_timeout",
        "detail": last.get("error") or last.get("message") or "",
    }


def stop_generation(request: dict[str, Any], state_path: Path) -> dict[str, Any]:
    if not state_path.exists():
        return {"ok": False, "status": "error", "error": "generation_state_missing"}
    state = read_json(state_path)
    stop_room_runtime(state)
    state["updated_at"] = int(time.time())
    atomic_write_json(state_path, state)
    return response_from_state(state)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--request", required=True)
    parser.add_argument("--response", required=True)
    parser.add_argument("--spool-root", default=os.environ.get("SEKAILINK_GENERATION_SPOOL_ROOT", ""))
    args = parser.parse_args()

    request_path = Path(args.request)
    response_path = Path(args.response)
    spool_root = Path(args.spool_root) if args.spool_root else request_path.parent.parent
    request = read_json(request_path)
    generation_id = str(request.get("generation_id") or "generation")
    state_path = spool_root / safe_component(generation_id, "generation") / "generation_state.json"

    try:
      action = request.get("action", "submit_generation")
      if action == "generation_status":
          response = generation_status(request, state_path)
      elif action == "generation_admin_secrets":
          response = generation_admin_secrets(request, state_path)
      elif action == "validate_seed_config":
          response = validate_seed_config(request, state_path, spool_root)
      elif action == "stop_generation":
          response = stop_generation(request, state_path)
      elif action == "submit_generation":
          response = submit_generation(request, state_path, spool_root)
      else:
          response = {"ok": False, "status": "error", "error": f"unknown_action:{action}"}
    except Exception as exc:  # Keep the C++ caller on the JSON path.
      response = {"ok": False, "status": "error", "error": str(exc)}

    atomic_write_json(response_path, response)
    return 0


if __name__ == "__main__":
    sys.exit(main())
