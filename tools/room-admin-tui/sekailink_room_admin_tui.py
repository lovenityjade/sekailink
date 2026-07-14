#!/usr/bin/env python3
from __future__ import annotations

import argparse
import asyncio
import difflib
import getpass
import json
import os
import queue
import readline
import shlex
import socket
import sys
import threading
import time
import uuid
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any
from urllib.parse import urlparse

import requests
import websockets
from rich import box
from rich.console import Console
from rich.panel import Panel
from rich.prompt import Prompt
from rich.table import Table
from rich.text import Text


SEKAI_TEAL = "#35f2d0"
SEKAI_CORAL = "#ff6b4a"
SEKAI_PURPLE = "#b28cff"
SEKAI_MUTED = "#87a0a3"
CONFIG_PATH = Path.home() / ".config" / "sekailink-room-admin" / "config.json"
REPORT_DIR = Path.home() / "SekaiLinkReports" / "room-admin"
HISTORY_PATH = Path.home() / ".local" / "state" / "sekailink-room-admin" / "history"


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def redact(value: Any) -> Any:
    if isinstance(value, dict):
        clean: dict[str, Any] = {}
        for key, child in value.items():
            if "token" in str(key).lower() or "password" in str(key).lower() or str(key).lower() in {"auth", "authorization"}:
                clean[key] = "***redacted***"
            else:
                clean[key] = redact(child)
        return clean
    if isinstance(value, list):
        return [redact(child) for child in value]
    return value


def load_config() -> dict[str, Any]:
    if not CONFIG_PATH.exists():
        return {}
    try:
        return json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
    except Exception:
        return {}


def save_config(config: dict[str, Any]) -> None:
    CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
    CONFIG_PATH.write_text(json.dumps(config, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    try:
        CONFIG_PATH.chmod(0o600)
    except OSError:
        pass


def json_dumps(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"))


def normalize_api_base(value: str) -> str:
    raw = value.strip() or "https://sekailink.com"
    return raw.rstrip("/")


def extract_list(payload: Any, keys: tuple[str, ...]) -> list[Any]:
    if isinstance(payload, list):
        return payload
    if isinstance(payload, dict):
        for key in keys:
            value = payload.get(key)
            if isinstance(value, list):
                return value
    return []


def parse_json_arg(raw: str | None, fallback: Any) -> Any:
    if raw is None or raw == "":
        return fallback
    try:
        return json.loads(raw)
    except json.JSONDecodeError as exc:
        raise ValueError(f"json_invalide: {exc}") from exc


@dataclass
class LobbyRef:
    raw: dict[str, Any]
    index: int
    lobby_id: str
    title: str
    host: str
    status: str
    async_lobby: bool
    players: str


@dataclass
class RoomTarget:
    room_id: str = ""
    host: str = ""
    port: int = 0
    endpoint_kind: str = "ap"
    room_url: str = ""
    status: dict[str, Any] = field(default_factory=dict)

    def ready(self) -> bool:
        return bool(self.room_id and self.host and self.port)

    def admin_ready(self) -> bool:
        return self.ready() and self.endpoint_kind == "admin-tcp"


class PersistentApConnection:
    def __init__(self, owner: "SekaiLinkRoomAdmin") -> None:
        self.owner = owner
        self.uri = ""
        self.slot = ""
        self.game = ""
        self.password = ""
        self.admin_password = ""
        self.admin_authenticated = False
        self.outgoing: queue.Queue[dict[str, Any] | None] = queue.Queue()
        self.stop_event = threading.Event()
        self.connected_event = threading.Event()
        self.thread: threading.Thread | None = None
        self.lock = threading.Lock()
        self.log: list[dict[str, Any]] = []

    def start(self, uri: str, slot: str, game: str, password: str = "", admin_password: str = "") -> None:
        if self.connected_for(uri, slot, game):
            return
        self.stop()
        self.uri = uri
        self.slot = slot
        self.game = game
        self.password = password
        self.admin_password = admin_password
        self.admin_authenticated = False
        self.outgoing = queue.Queue()
        self.stop_event.clear()
        self.connected_event.clear()
        self.thread = threading.Thread(target=self._thread_main, name="sekailink-room-admin-ap", daemon=True)
        self.thread.start()

    def stop(self) -> None:
        self.stop_event.set()
        try:
            self.outgoing.put_nowait(None)
        except Exception:
            pass
        if self.thread and self.thread.is_alive():
            self.thread.join(timeout=1.5)
        self.thread = None
        self.connected_event.clear()

    def connected_for(self, uri: str, slot: str, game: str) -> bool:
        return (
            self.connected_event.is_set()
            and self.thread is not None
            and self.thread.is_alive()
            and self.uri == uri
            and self.slot.lower() == slot.lower()
            and self.game == game
        )

    def wait_connected(self, timeout: float = 6.0) -> bool:
        return self.connected_event.wait(timeout)

    def send(self, packet: dict[str, Any]) -> None:
        if not self.connected_event.is_set():
            raise RuntimeError("connexion AP persistante non connectée")
        self.outgoing.put(packet)

    def recent(self, limit: int = 40) -> list[dict[str, Any]]:
        with self.lock:
            return list(self.log[-limit:])

    def _thread_main(self) -> None:
        try:
            asyncio.run(self._run())
        except Exception as exc:
            self._remember("error", {"message": str(exc)})

    def _remember(self, direction: str, payload: Any) -> None:
        entry = {"ts": utc_now(), "direction": direction, "uri": self.uri, "slot": self.slot, "game": self.game, "payload": redact(payload)}
        with self.lock:
            self.log.append(entry)
            self.log = self.log[-400:]
        self.owner.record("ap_persistent", entry)
        if direction in {"recv", "send", "error"}:
            self.owner.print_ap_persistent_event(entry)

    async def _run(self) -> None:
        self._remember("state", {"message": "connecting"})
        async with websockets.connect(self.uri, open_timeout=8) as ws:
            room_info_raw = await asyncio.wait_for(ws.recv(), timeout=8)
            self._remember("recv", json.loads(room_info_raw))
            connect_packet = {
                "cmd": "Connect",
                "password": self.password,
                "name": self.slot,
                "game": self.game,
                "version": {"major": 0, "minor": 6, "build": 7, "class": "Version"},
                # Monitor/admin mode: can talk, but must not consume items or submit checks.
                "items_handling": 0,
                "tags": ["AP", "Tracker", "TextOnly"],
                "uuid": f"sekailink-admin-tui-persistent-{uuid.uuid4()}",
            }
            await ws.send(json.dumps([connect_packet]))
            self._remember("send", [connect_packet])

            sender = asyncio.create_task(self._sender(ws))
            try:
                while not self.stop_event.is_set():
                    try:
                        raw = await asyncio.wait_for(ws.recv(), timeout=0.5)
                    except asyncio.TimeoutError:
                        continue
                    batch = json.loads(raw)
                    self._remember("recv", batch)
                    if any(isinstance(packet, dict) and packet.get("cmd") == "Connected" for packet in batch):
                        if self.admin_password and not self.admin_authenticated:
                            login_packet = {"cmd": "Say", "text": f"!admin login {self.admin_password}"}
                            await ws.send(json.dumps([login_packet]))
                            self._remember("send", [{"cmd": "Say", "text": "!admin login ***redacted***"}])
                            self.admin_authenticated = True
                        self.connected_event.set()
                    if any(isinstance(packet, dict) and packet.get("cmd") == "ConnectionRefused" for packet in batch):
                        self.stop_event.set()
                        self.connected_event.clear()
                        break
            finally:
                sender.cancel()
                self.connected_event.clear()
        self._remember("state", {"message": "disconnected"})

    async def _sender(self, ws: Any) -> None:
        while not self.stop_event.is_set():
            packet = await asyncio.to_thread(self.outgoing.get)
            if packet is None:
                break
            await ws.send(json.dumps([packet]))
            self._remember("send", [packet])


class SekaiLinkRoomAdmin:
    def __init__(self, args: argparse.Namespace) -> None:
        config = load_config()
        self.console = Console()
        self.api_base = normalize_api_base(args.api_base or os.getenv("SEKAILINK_API_BASE") or config.get("api_base", "https://sekailink.com"))
        self.identity_token = args.token or os.getenv("SEKAILINK_TOKEN") or os.getenv("SEKAILINK_DESKTOP_TOKEN") or config.get("identity_token", "")
        self.room_admin_token = args.room_admin_token or os.getenv("SEKAILINK_ROOM_ADMIN_TOKEN") or config.get("room_admin_token", "")
        self.room_runtime_token = args.room_runtime_token or os.getenv("SEKAILINK_ROOM_RUNTIME_TOKEN") or config.get("room_runtime_token", "")
        self.room_admin_tool_token = args.room_admin_tool_token or os.getenv("SEKAILINK_ROOM_ADMIN_TOOL_TOKEN") or config.get("room_admin_tool_token", "")
        self.ap_admin_password = args.ap_admin_password or os.getenv("SEKAILINK_AP_ADMIN_PASSWORD") or config.get("ap_admin_password", "")
        self.lobbies: list[LobbyRef] = []
        self.selected_lobby: LobbyRef | None = None
        self.selected_generation: Any = None
        self.room = RoomTarget()
        self.ap_uri = ""
        self.history: list[dict[str, Any]] = []
        self.item_cache: dict[str, dict[str, int]] = {}
        self.ap_connection = PersistentApConnection(self)
        self.commands = [
            "help", "login", "whoami", "lobbies", "select", "generation", "status", "snapshot", "summary",
            "ap-info", "events", "watch", "reports", "check", "item", "give", "release", "raw", "ap-check", "ap-say",
            "ap-log", "ap-connect", "ap-disconnect", "secrets", "players", "items", "export", "config", "quit", "exit",
        ]

    def record(self, kind: str, payload: Any) -> None:
        self.history.append({"ts": utc_now(), "kind": kind, "payload": redact(payload)})

    def headers(self) -> dict[str, str]:
        headers = {"Accept": "application/json", "User-Agent": "SekaiLink-Room-Admin-TUI/0.1"}
        if self.identity_token:
            headers["Authorization"] = f"Bearer {self.identity_token}"
        return headers

    def api_get(self, path: str) -> Any:
        url = f"{self.api_base}{path if path.startswith('/') else '/' + path}"
        response = requests.get(url, headers=self.headers(), timeout=12)
        text = response.text
        if response.status_code >= 400:
            if response.status_code == 401:
                raise RuntimeError(
                    f"401 unauthorized: token Nexus/API manquant ou expiré. "
                    f"Définis SEKAILINK_TOKEN ou utilise la commande config. Réponse: {text[:220]}"
                )
            raise RuntimeError(f"{response.status_code}: {text[:400]}")
        try:
            payload = response.json()
        except ValueError as exc:
            raise RuntimeError(f"réponse non JSON depuis {url}: {text[:160]}") from exc
        self.record("api_get", {"path": path, "payload": payload})
        return payload

    def api_post(self, path: str, payload: dict[str, Any]) -> Any:
        url = f"{self.api_base}{path if path.startswith('/') else '/' + path}"
        headers = dict(self.headers())
        headers["Content-Type"] = "application/json"
        response = requests.post(url, headers=headers, data=json.dumps(payload), timeout=12)
        text = response.text
        if response.status_code >= 400:
            if response.status_code == 401:
                raise RuntimeError("401 unauthorized: token Nexus/API ou token admin outil refusé")
            raise RuntimeError(f"{response.status_code}: {text[:400]}")
        try:
            parsed = response.json()
        except ValueError as exc:
            raise RuntimeError(f"réponse non JSON depuis {url}: {text[:160]}") from exc
        self.record("api_post", {"path": path, "payload": parsed})
        return parsed

    def normalize_lobby(self, raw: Any, index: int) -> LobbyRef | None:
        if not isinstance(raw, dict):
            return None
        lobby_id = str(raw.get("id") or raw.get("lobby_id") or raw.get("_id") or "").strip()
        title = str(raw.get("title") or raw.get("name") or raw.get("display_name") or lobby_id or "Untitled").strip()
        host_raw = raw.get("host") or raw.get("host_username") or raw.get("owner") or raw.get("created_by") or ""
        if isinstance(host_raw, dict):
            host = str(host_raw.get("username") or host_raw.get("display_name") or host_raw.get("id") or "").strip()
        else:
            host = str(host_raw).strip()
        status = str(raw.get("status") or raw.get("state") or "unknown").strip()
        async_lobby = bool(raw.get("is_async") or raw.get("async") or raw.get("asynchronous"))
        current = raw.get("player_count") or raw.get("players_count") or raw.get("current_players")
        max_players = raw.get("max_players") or raw.get("capacity")
        if current is None and isinstance(raw.get("players"), list):
            current = len(raw["players"])
        players = f"{current if current is not None else '?'}"
        if max_players is not None:
            players += f"/{max_players}"
        if not lobby_id:
            lobby_id = title
        return LobbyRef(raw=raw, index=index, lobby_id=lobby_id, title=title, host=host or "-", status=status, async_lobby=async_lobby, players=players)

    def print_header(self) -> None:
        title = Text("SekaiLink Room Admin Console", style=f"bold {SEKAI_TEAL}")
        subtitle = Text("lobbies • room server • AP debug • reports", style=SEKAI_MUTED)
        body = Text.assemble(title, "\n", subtitle)
        self.console.print(Panel(body, border_style=SEKAI_PURPLE, box=box.ROUNDED))

    def print_status(self) -> None:
        table = Table(box=box.SIMPLE_HEAVY, show_header=False, expand=True)
        table.add_column("k", style=SEKAI_MUTED, width=14)
        table.add_column("v")
        table.add_row("API", self.api_base)
        table.add_row("Lobby", self.selected_lobby.title if self.selected_lobby else "[dim]aucun[/dim]")
        table.add_row("Room", f"{self.room.room_id} @ {self.room.host}:{self.room.port}" if self.room.ready() else "[dim]non résolue[/dim]")
        table.add_row(
            "Tokens",
            " ".join([
                f"identity={'oui' if self.identity_token else 'non'}",
                f"room-admin={'oui' if self.room_admin_token else 'non'}",
                f"runtime={'oui' if self.room_runtime_token else 'non'}",
                f"tool={'oui' if self.room_admin_tool_token else 'non'}",
                f"ap-admin={'oui' if self.ap_admin_password else 'non'}",
            ]),
        )
        self.console.print(Panel(table, title="État", border_style=SEKAI_TEAL, box=box.ROUNDED))

    def show_help(self) -> None:
        table = Table(title="Commandes", box=box.ROUNDED, border_style=SEKAI_PURPLE)
        table.add_column("Commande", style=f"bold {SEKAI_TEAL}")
        table.add_column("Effet")
        commands = [
            ("lobbies [limit]", "Liste les lobbies depuis Nexus/API."),
            ("login [identity]", "Ouvre une session Nexus/API et garde le token localement."),
            ("whoami", "Teste le token courant avec /api/identity/me."),
            ("select <index|lobby_id>", "Sélectionne un lobby, charge sa génération et tente de résoudre la room."),
            ("generation", "Affiche le payload de génération du lobby sélectionné."),
            ("status", "Recharge /api/room_status/<room_id>."),
            ("snapshot", "Demande snapshot_room au room server."),
            ("summary", "Demande room_summary au room server."),
            ("ap-info", "Affiche les infos Archipelago de la room."),
            ("ap-log [limit]", "Affiche les derniers paquets AP persistants."),
            ("ap-connect", "Reconnecte le moniteur AP persistant au slot par défaut."),
            ("ap-disconnect", "Ferme le moniteur AP persistant."),
            ("secrets", "Recharge les secrets admin de la room via le token outil."),
            ("events [limit] [source]", "Affiche les événements room server."),
            ("watch [seconds]", "Surveille les events room server en boucle."),
            ("reports [limit]", "Affiche les client reports."),
            ("check <location_id>", "Injecte record_check via canal admin."),
            ("item <slot> <item_id> <name> [location_id]", "Injecte enqueue_received_item dans la room."),
            ("give <slotname> <item name...>", "Valide l'item AP et envoie !admin /send à ce slot."),
            ("release <slotname>", "Envoie !admin /release pour ce slot via le moniteur AP admin."),
            ("compensator [status|release <mode>]", "Inspecte ou libere le Compensator (all, progression, accelerate)."),
            ("players", "Liste les noms de slots disponibles pour autocomplete."),
            ("items [game]", "Liste les items AP connus pour le jeu."),
            ("raw <json>", "Envoie une commande admin brute au room server."),
            ("ap-check [slot game] <loc...>", "Envoie LocationChecks; slot/jeu auto si sélectionné."),
            ("ap-say [slot game] <text...>", "Envoie Say; slot/jeu auto si sélectionné."),
            ("export", "Écrit un rapport JSONL local."),
            ("config", "Affiche où placer les tokens locaux."),
            ("quit", "Quitte."),
        ]
        for command, description in commands:
            table.add_row(command, description)
        self.console.print(table)

    def default_player(self) -> tuple[str, str]:
        players = self.room.status.get("players") if isinstance(self.room.status.get("players"), list) else []
        if players and isinstance(players[0], dict):
            name = str(players[0].get("name") or "").strip()
            game = str(players[0].get("game") or "").strip()
            if name and game:
                return name, game
        entries = self.room.status.get("launch_entries") if isinstance(self.room.status.get("launch_entries"), list) else []
        if entries and isinstance(entries[0], dict):
            name = str(entries[0].get("slot_name") or entries[0].get("compat_player_name") or "").strip()
            game = str(entries[0].get("game") or entries[0].get("display_game") or "").strip()
            if name and game:
                return name, game
        raise RuntimeError("slot/jeu par défaut introuvable; utilise la forme explicite avec slot et game")

    def room_players(self) -> list[tuple[str, str]]:
        out: list[tuple[str, str]] = []
        players = self.room.status.get("players") if isinstance(self.room.status.get("players"), list) else []
        for player in players:
            if isinstance(player, dict):
                name = str(player.get("name") or "").strip()
                game = str(player.get("game") or "").strip()
                if name:
                    out.append((name, game))
        return out

    def game_for_slot_name(self, slot_name: str) -> str:
        for name, game in self.room_players():
            if name.lower() == slot_name.lower() and game:
                return game
        entries = self.room.status.get("launch_entries") if isinstance(self.room.status.get("launch_entries"), list) else []
        for entry in entries:
            if not isinstance(entry, dict):
                continue
            names = [entry.get("slot_name"), entry.get("compat_player_name"), entry.get("username")]
            if any(str(name or "").lower() == slot_name.lower() for name in names):
                game = str(entry.get("game") or entry.get("display_game") or "").strip()
                if game:
                    return game
        known = ", ".join(name for name, _game in self.room_players())
        detail = f" Slots valides: {known}" if known else ""
        raise RuntimeError(f"slot introuvable dans la room: {slot_name}.{detail}")

    def validate_slot_and_game(self, slot_name: str, game: str) -> tuple[str, str]:
        expected_game = self.game_for_slot_name(slot_name)
        if expected_game and game and expected_game.lower() != game.lower():
            raise RuntimeError(
                f"jeu invalide pour le slot {slot_name}: reçu {game}, attendu {expected_game}"
            )
        return slot_name, expected_game or game

    def cmd_players(self) -> None:
        table = Table(title="Players / slots", box=box.ROUNDED, border_style=SEKAI_TEAL)
        table.add_column("Slot name", style=f"bold {SEKAI_TEAL}")
        table.add_column("Game")
        for name, game in self.room_players():
            table.add_row(name, game or "-")
        if not self.room_players():
            table.add_row("-", "Aucune room sélectionnée ou aucun joueur dans room_status.")
        self.console.print(table)

    def cmd_login(self, identity: str = "") -> None:
        identity = identity.strip() or Prompt.ask("Identifiant SekaiLink")
        password = getpass.getpass("Mot de passe SekaiLink: ")
        two_factor = Prompt.ask("Code 2FA si requis, sinon Enter", default="", show_default=False)
        body: dict[str, str] = {"identity": identity, "password": password}
        if two_factor.strip():
            body["two_factor_code"] = two_factor.strip()
        url = f"{self.api_base}/api/identity/login"
        response = requests.post(
            url,
            headers={"Content-Type": "application/json", "Accept": "application/json", "User-Agent": "SekaiLink-Room-Admin-TUI/0.1"},
            data=json.dumps(body),
            timeout=12,
        )
        try:
            payload = response.json()
        except ValueError as exc:
            raise RuntimeError(f"login: réponse non JSON: {response.text[:160]}") from exc
        if not response.ok or not payload.get("ok") or not payload.get("session", {}).get("session_token"):
            error = str(payload.get("error") or response.status_code)
            if error == "two_factor_required":
                raise RuntimeError("2FA requis: relance `login` et entre ton code.")
            raise RuntimeError(f"login refusé: {error}")
        token = str(payload["session"]["session_token"])
        self.identity_token = token
        config = load_config()
        config.update({"api_base": self.api_base, "identity_token": token})
        if self.room_admin_token:
            config["room_admin_token"] = self.room_admin_token
        if self.room_runtime_token:
            config["room_runtime_token"] = self.room_runtime_token
        if self.room_admin_tool_token:
            config["room_admin_tool_token"] = self.room_admin_tool_token
        save_config(config)
        self.record("login", {"identity": identity, "result": "ok"})
        self.console.print(f"[{SEKAI_TEAL}]Login OK.[/] Token Nexus/API sauvegardé localement dans {CONFIG_PATH}")

    def cmd_whoami(self) -> None:
        payload = self.api_get("/api/identity/me")
        self.print_response(payload)

    def cmd_lobbies(self, limit: int = 80) -> None:
        payload = self.api_get(f"/api/lobbies?limit={limit}")
        raw_lobbies = extract_list(payload, ("lobbies", "items", "data", "results"))
        self.lobbies = []
        for index, raw in enumerate(raw_lobbies, 1):
            lobby = self.normalize_lobby(raw, index)
            if lobby:
                self.lobbies.append(lobby)
        table = Table(title=f"Lobbies ({len(self.lobbies)})", box=box.ROUNDED, border_style=SEKAI_TEAL)
        table.add_column("#", justify="right", style=SEKAI_MUTED)
        table.add_column("Titre", style="bold white")
        table.add_column("Host")
        table.add_column("Statut")
        table.add_column("Type")
        table.add_column("Joueurs")
        table.add_column("ID", style="dim")
        for lobby in self.lobbies:
            table.add_row(str(lobby.index), lobby.title, lobby.host, lobby.status, "async" if lobby.async_lobby else "active", lobby.players, lobby.lobby_id)
        self.console.print(table)

    def find_lobby(self, selector: str) -> LobbyRef:
        selector = selector.strip()
        if selector.isdigit():
            index = int(selector)
            for lobby in self.lobbies:
                if lobby.index == index:
                    return lobby
        for lobby in self.lobbies:
            if lobby.lobby_id == selector or lobby.title.lower() == selector.lower():
                return lobby
        raise RuntimeError("lobby introuvable; lance `lobbies` puis `select <#>`")

    def cmd_select(self, selector: str) -> None:
        if not self.lobbies:
            self.cmd_lobbies()
        self.selected_lobby = self.find_lobby(selector)
        self.console.print(f"[{SEKAI_TEAL}]Lobby sélectionné:[/] {self.selected_lobby.title}")
        generation = self.fetch_generation()
        self.selected_generation = generation
        room_id, room_url = self.extract_room_identity(generation)
        if not room_id:
            room_id = self.selected_lobby.lobby_id
        if room_id:
            self.room.room_id = room_id
            self.room.room_url = room_url
            self.apply_generation_endpoint(generation)
            self.cmd_status()
            self.cmd_ap_connect()
        else:
            self.console.print("[yellow]Aucune room générée trouvée pour ce lobby.[/yellow]")

    def fetch_generation(self) -> Any:
        if not self.selected_lobby:
            raise RuntimeError("aucun lobby sélectionné")
        return self.api_get(f"/api/lobbies/{self.selected_lobby.lobby_id}/generation")

    def extract_room_identity(self, payload: Any) -> tuple[str, str]:
        candidates: list[str] = []
        if isinstance(payload, dict):
            generation = payload.get("generation") if isinstance(payload.get("generation"), dict) else payload
            for key in ("room_id", "roomId"):
                if generation.get(key):
                    return str(generation[key]), str(generation.get("room_url") or generation.get("room_server_url") or "")
            for key in ("room_url", "room_server_url", "url"):
                if generation.get(key):
                    candidates.append(str(generation[key]))
            response = generation.get("response")
            if isinstance(response, dict):
                for key in ("room_id", "roomId"):
                    if response.get(key):
                        return str(response[key]), str(generation.get("room_url") or generation.get("room_server_url") or response.get("room_url") or response.get("room_server_url") or "")
                for key in ("room_url", "room_server_url", "url"):
                    if response.get(key):
                        candidates.append(str(response[key]))
        for url in candidates:
            parsed = urlparse(url)
            if parsed.scheme and not parsed.netloc and ":" in url and "/" not in url:
                return "", url
            parts = [part for part in parsed.path.split("/") if part]
            if parts:
                return parts[-1], url
        return "", candidates[0] if candidates else ""

    def generation_root(self, payload: Any | None = None) -> dict[str, Any]:
        raw = self.selected_generation if payload is None else payload
        if not isinstance(raw, dict):
            return {}
        generation = raw.get("generation") if isinstance(raw.get("generation"), dict) else raw
        return generation if isinstance(generation, dict) else {}

    def generation_response(self, payload: Any | None = None) -> dict[str, Any]:
        root = self.generation_root(payload)
        response = root.get("response")
        return response if isinstance(response, dict) else {}

    def apply_generation_endpoint(self, payload: Any | None = None) -> None:
        root = self.generation_root(payload)
        response = self.generation_response(payload)
        host = str(root.get("room_host") or response.get("room_host") or "").strip()
        port_raw = root.get("room_port") or root.get("last_port") or response.get("room_port") or response.get("last_port")
        room_url = str(root.get("room_url") or response.get("room_url") or root.get("room_server_url") or response.get("room_server_url") or "").strip()
        if (not host or not port_raw) and room_url:
            parsed = urlparse(room_url if "://" in room_url else f"//{room_url}")
            if parsed.hostname:
                host = host or parsed.hostname
            if parsed.port:
                port_raw = port_raw or parsed.port
        try:
            port = int(port_raw or 0)
        except (TypeError, ValueError):
            port = 0
        if host:
            self.room.host = host
        if port:
            self.room.port = port
            self.ap_uri = f"ws://{self.room.host or host}:{port}"

    def merge_generation_status(self, status: dict[str, Any]) -> dict[str, Any]:
        root = self.generation_root()
        response = self.generation_response()
        merged = dict(status)
        for source in (response, root):
            for key in (
                "launch_entries",
                "players",
                "room_host",
                "room_port",
                "last_port",
                "room_runtime_log",
                "room_runtime_alive",
                "room_status",
                "sync_package_path",
            ):
                if key not in merged or merged.get(key) in (None, "", [], 0):
                    if isinstance(source, dict) and source.get(key) not in (None, "", [], 0):
                        merged[key] = source[key]
        return merged

    def cmd_generation(self) -> None:
        payload = self.fetch_generation()
        self.console.print_json(json.dumps(redact(payload), ensure_ascii=False))

    def cmd_status(self) -> None:
        if not self.room.room_id:
            raise RuntimeError("room_id manquant")
        payload = self.api_get(f"/api/room_status/{self.room.room_id}")
        status = self.merge_generation_status(payload if isinstance(payload, dict) else {})
        self.room.status = status
        parsed_base = urlparse(self.api_base)
        host = str(status.get("room_host") or parsed_base.hostname or "127.0.0.1")
        port = int(status.get("last_port") or status.get("room_port") or status.get("port") or 0)
        self.room.host = host
        self.room.port = port
        if port:
            self.ap_uri = f"ws://{host}:{port}"
        self.fetch_room_admin_secrets(silent=True)
        self.print_room_status(status)

    def fetch_room_admin_secrets(self, silent: bool = False) -> bool:
        if not self.room.room_id or not self.room_admin_tool_token:
            return False
        try:
            payload = self.api_post(
                f"/api/room_admin_secrets/{self.room.room_id}",
                {"admin_tool_token": self.room_admin_tool_token},
            )
        except Exception as exc:
            if not silent:
                self.console.print(f"[{SEKAI_CORAL}]Secrets admin indisponibles:[/] {exc}")
            self.record("room_admin_secrets_error", {"message": str(exc)})
            return False
        password = str(payload.get("ap_admin_password") or "").strip() if isinstance(payload, dict) else ""
        if not password:
            if not silent:
                self.console.print(f"[{SEKAI_CORAL}]Secrets admin:[/] ap_admin_password absent.")
            return False
        self.ap_admin_password = password
        if not silent:
            source = str(payload.get("ap_admin_password_source") or "-")
            self.console.print(f"[{SEKAI_TEAL}]Secrets admin chargés:[/] AP remote admin password ({source}).")
        return True

    def print_room_status(self, status: dict[str, Any]) -> None:
        players = status.get("players") if isinstance(status.get("players"), list) else []
        table = Table(title=f"Room {self.room.room_id}", box=box.ROUNDED, border_style=SEKAI_TEAL)
        table.add_column("Slot")
        table.add_column("Nom")
        table.add_column("Jeu")
        table.add_column("Raw", style="dim")
        for player in players:
            if isinstance(player, dict):
                table.add_row(str(player.get("slot", "?")), str(player.get("name", "-")), str(player.get("game", "-")), json_dumps(redact(player))[:80])
        if not players:
            table.add_row("-", "-", "-", "aucun joueur dans room_status")
        self.console.print(table)
        endpoint_label = "Archipelago WebSocket"
        self.room.endpoint_kind = "ap"
        self.console.print(f"[dim]Endpoint:[/] {endpoint_label} {self.room.host}:{self.room.port}")
        if status.get("room_runtime_log"):
            self.console.print(f"[dim]Runtime log serveur:[/] {status.get('room_runtime_log')}")
        self.console.print("[dim]Note:[/] cette API expose le port AP public, pas le port admin TCP interne.")

    def print_ap_persistent_event(self, entry: dict[str, Any]) -> None:
        payload = entry.get("payload")
        direction = str(entry.get("direction") or "")
        if direction == "error":
            self.console.print(f"[{SEKAI_CORAL}]AP monitor error:[/] {payload.get('message') if isinstance(payload, dict) else payload}")
            return
        if direction == "send":
            packets = payload if isinstance(payload, list) else [payload]
            for packet in packets:
                if isinstance(packet, dict):
                    self.console.print(f"[dim]{entry.get('ts')}[/dim] [{SEKAI_PURPLE}]AP send[/] {packet.get('cmd', '?')} {json_dumps(redact(packet))[:220]}")
            return
        if direction != "recv":
            return
        packets = payload if isinstance(payload, list) else [payload]
        for packet in packets:
            if not isinstance(packet, dict):
                continue
            cmd = str(packet.get("cmd") or "?")
            if cmd == "RoomInfo":
                games = ", ".join(str(game) for game in packet.get("games", []))
                self.console.print(f"[dim]{entry.get('ts')}[/dim] [{SEKAI_TEAL}]AP RoomInfo[/] seed={packet.get('seed_name', '-')} games={games}")
            elif cmd == "Connected":
                checked = packet.get("checked_locations", [])
                missing = packet.get("missing_locations", [])
                self.console.print(f"[dim]{entry.get('ts')}[/dim] [{SEKAI_TEAL}]AP Connected[/] slot={packet.get('slot')} checked={len(checked)} missing={len(missing)}")
            elif cmd == "ConnectionRefused":
                self.console.print(f"[dim]{entry.get('ts')}[/dim] [{SEKAI_CORAL}]AP Refused[/] {', '.join(str(err) for err in packet.get('errors', []))}")
            elif cmd == "ReceivedItems":
                items = packet.get("items", [])
                self.console.print(f"[dim]{entry.get('ts')}[/dim] [{SEKAI_CORAL}]AP ReceivedItems on monitor[/] index={packet.get('index')} count={len(items)}")
            elif cmd == "RoomUpdate":
                checked = packet.get("checked_locations")
                self.console.print(f"[dim]{entry.get('ts')}[/dim] [{SEKAI_TEAL}]AP RoomUpdate[/] checked={len(checked) if isinstance(checked, list) else '-'}")
            elif cmd == "PrintJSON":
                parts = packet.get("data", [])
                text = "".join(str(part.get("text", "")) if isinstance(part, dict) else str(part) for part in parts)
                self.console.print(f"[dim]{entry.get('ts')}[/dim] [{SEKAI_TEAL}]AP Chat[/] {text}")
            elif cmd in {"DataPackage", "Retrieved"}:
                self.console.print(f"[dim]{entry.get('ts')}[/dim] [{SEKAI_MUTED}]AP {cmd}[/] {json_dumps(redact(packet))[:180]}")
            else:
                self.console.print(f"[dim]{entry.get('ts')}[/dim] [{SEKAI_MUTED}]AP {cmd}[/] {json_dumps(redact(packet))[:220]}")

    def cmd_ap_connect(self, slot: str = "", game: str = "") -> None:
        if not self.ap_uri:
            if not self.room.ready():
                raise RuntimeError("AP endpoint inconnu; sélectionne une room")
            self.ap_uri = f"ws://{self.room.host}:{self.room.port}"
        if not slot or not game:
            slot, game = self.default_player()
        self.ap_connection.start(self.ap_uri, slot, game, admin_password=self.ap_admin_password)
        if self.ap_connection.wait_connected(6):
            self.console.print(f"[{SEKAI_TEAL}]AP monitor connecté:[/] {slot} ({game}) @ {self.ap_uri}")
            if self.ap_admin_password:
                self.console.print(f"[{SEKAI_MUTED}]AP admin login envoyé via !admin.[/]")
            else:
                self.console.print(f"[{SEKAI_CORAL}]AP admin password absent:[/] `give`/`release` ne peuvent pas exécuter les commandes admin. Définis SEKAILINK_AP_ADMIN_PASSWORD.")
        else:
            self.console.print(f"[{SEKAI_CORAL}]AP monitor non connecté après timeout.[/] Consulte `ap-log`.")

    def cmd_ap_disconnect(self) -> None:
        self.ap_connection.stop()
        self.console.print(f"[{SEKAI_MUTED}]AP monitor fermé.[/]")

    def cmd_ap_log(self, limit: int = 40) -> None:
        entries = self.ap_connection.recent(limit)
        table = Table(title=f"AP monitor log ({len(entries)})", box=box.ROUNDED, border_style=SEKAI_PURPLE)
        table.add_column("Time", style=SEKAI_MUTED)
        table.add_column("Dir", style=f"bold {SEKAI_TEAL}")
        table.add_column("Packet")
        table.add_column("Résumé")
        for entry in entries:
            payload = entry.get("payload")
            packets = payload if isinstance(payload, list) else [payload]
            for packet in packets:
                cmd = packet.get("cmd", entry.get("direction", "?")) if isinstance(packet, dict) else entry.get("direction", "?")
                summary = json_dumps(redact(packet))[:180] if isinstance(packet, (dict, list)) else str(packet)[:180]
                if isinstance(packet, dict) and cmd == "PrintJSON":
                    parts = packet.get("data", [])
                    summary = "".join(str(part.get("text", "")) if isinstance(part, dict) else str(part) for part in parts)[:180]
                table.add_row(str(entry.get("ts", "")), str(entry.get("direction", "")), str(cmd), summary)
        self.console.print(table)

    def send_room_command(self, command: dict[str, Any], channel: str = "admin") -> dict[str, Any]:
        if not self.room.ready():
            raise RuntimeError("room non résolue; utilise select/status d'abord")
        if not self.room.admin_ready():
            raise RuntimeError(
                "endpoint_admin_non_exposé: room_status expose seulement le serveur Archipelago public. "
                "Utilise ap-info/ap-check/ap-say, ou expose un endpoint admin room server séparé."
            )
        token = self.room_admin_token if channel == "admin" else self.room_runtime_token
        envelope = {"channel": channel, "command": command}
        if token:
            envelope["auth_token"] = token
        line = json_dumps(envelope) + "\n"
        with socket.create_connection((self.room.host, self.room.port), timeout=5) as sock:
            sock.sendall(line.encode("utf-8"))
            sock.settimeout(8)
            chunks: list[bytes] = []
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                chunks.append(chunk)
                if b"\n" in chunk:
                    break
        raw = b"".join(chunks).decode("utf-8", errors="replace").strip()
        try:
            payload = json.loads(raw)
        except ValueError:
            payload = {"ok": False, "error": "non_json_response", "raw": raw}
        self.record("room_command", {"request": envelope, "response": payload})
        return payload

    def print_response(self, payload: Any) -> None:
        if isinstance(payload, dict) and isinstance(payload.get("responses"), list):
            self.print_ap_responses(payload["responses"])
            return
        style = SEKAI_TEAL if isinstance(payload, dict) and payload.get("ok") else SEKAI_CORAL
        self.console.print(Panel(json.dumps(redact(payload), indent=2, ensure_ascii=False), border_style=style, box=box.ROUNDED))

    def print_ap_responses(self, responses: list[dict[str, Any]]) -> None:
        table = Table(title="Archipelago", box=box.ROUNDED, border_style=SEKAI_TEAL)
        table.add_column("Packet", style=f"bold {SEKAI_TEAL}")
        table.add_column("Résumé")
        for packet in responses:
            if not isinstance(packet, dict):
                continue
            cmd = str(packet.get("cmd", "?"))
            if cmd == "RoomInfo":
                games = ", ".join(str(game) for game in packet.get("games", []))
                version = packet.get("generator_version") or packet.get("version") or {}
                version_text = f"{version.get('major', '?')}.{version.get('minor', '?')}.{version.get('build', '?')}" if isinstance(version, dict) else "-"
                summary = f"Seed {packet.get('seed_name', '-')}; jeux: {games}; AP {version_text}"
            elif cmd == "Connected":
                slot_info = packet.get("slot_info", {})
                checked = packet.get("checked_locations", [])
                missing = packet.get("missing_locations", [])
                summary = f"Connecté. checked={len(checked)} missing={len(missing)} slots={len(slot_info) if isinstance(slot_info, dict) else '?'}"
            elif cmd == "ConnectionRefused":
                summary = f"Refusé: {', '.join(str(err) for err in packet.get('errors', []))}"
            elif cmd in {"ReceivedItems", "Retrieved"}:
                summary = json_dumps(redact(packet))[:180]
            elif cmd == "PrintJSON":
                parts = packet.get("data", [])
                text = "".join(str(part.get("text", "")) if isinstance(part, dict) else str(part) for part in parts)
                summary = text[:180]
            else:
                summary = json_dumps(redact(packet))[:180]
            table.add_row(cmd, summary)
        self.console.print(table)

    def cmd_room_simple(self, cmd: str, extra: dict[str, Any] | None = None) -> None:
        command = {"cmd": cmd, "room_id": self.room.room_id}
        if extra:
            command.update(extra)
        self.print_response(self.send_room_command(command))

    def cmd_events(self, limit: int = 20, source: str = "") -> dict[str, Any]:
        if not self.room.admin_ready():
            self.console.print(Panel(
                "Events room server non disponibles par ce port: la room expose le WebSocket Archipelago public.\n"
                "Utilise `ap-info` pour vérifier la connexion AP ou `status` pour relire l'état Nexus.",
                border_style=SEKAI_CORAL,
                box=box.ROUNDED,
            ))
            return {"ok": False, "error": "endpoint_admin_non_exposé"}
        extra: dict[str, Any] = {"limit": limit}
        if source:
            extra["source"] = source
        payload = self.send_room_command({"cmd": "room_events", "room_id": self.room.room_id, **extra})
        events = payload.get("events") if isinstance(payload, dict) else None
        if isinstance(events, list):
            table = Table(title=f"Events ({len(events)})", box=box.ROUNDED, border_style=SEKAI_PURPLE)
            table.add_column("Time", style=SEKAI_MUTED)
            table.add_column("Type", style=f"bold {SEKAI_TEAL}")
            table.add_column("Source")
            table.add_column("Severity")
            table.add_column("Payload")
            for event in events:
                if isinstance(event, dict):
                    table.add_row(str(event.get("timestamp", "")), str(event.get("event_type", "")), str(event.get("source", "")), str(event.get("severity", "")), json_dumps(redact(event.get("payload", {})))[:130])
            self.console.print(table)
        else:
            self.print_response(payload)
        return payload

    def cmd_reports(self, limit: int = 20) -> None:
        payload = self.send_room_command({"cmd": "client_reports", "room_id": self.room.room_id, "limit": limit})
        reports = payload.get("reports") if isinstance(payload, dict) else None
        if isinstance(reports, list):
            table = Table(title=f"Client reports ({len(reports)})", box=box.ROUNDED, border_style=SEKAI_CORAL)
            table.add_column("Severity")
            table.add_column("Type")
            table.add_column("Source")
            table.add_column("Message")
            for report in reports:
                if isinstance(report, dict):
                    table.add_row(str(report.get("severity", "")), str(report.get("report_type", "")), str(report.get("source", "")), str(report.get("message", ""))[:160])
            self.console.print(table)
        else:
            self.print_response(payload)

    def cmd_watch(self, seconds: int = 60) -> None:
        end = time.monotonic() + max(1, seconds)
        seen: set[str] = set()
        mode = "events room server" if self.room.admin_ready() else "room_status Nexus/API"
        self.console.print(f"[{SEKAI_TEAL}]Watch {mode} pendant {seconds}s. Ctrl+C pour arrêter.[/]")
        while time.monotonic() < end:
            try:
                if self.room.admin_ready():
                    payload = self.cmd_events(10)
                    for event in payload.get("events", []) if isinstance(payload, dict) else []:
                        key = json_dumps(event)
                        if key not in seen:
                            seen.add(key)
                else:
                    payload = self.api_get(f"/api/room_status/{self.room.room_id}")
                    checks = payload.get("checks_done", "?") if isinstance(payload, dict) else "?"
                    total = payload.get("total_locations", "?") if isinstance(payload, dict) else "?"
                    completed = payload.get("completed_players", "?") if isinstance(payload, dict) else "?"
                    status = payload.get("status", "?") if isinstance(payload, dict) else "?"
                    self.console.print(f"[dim]{utc_now()}[/dim] status={status} checks={checks}/{total} completed_players={completed}")
            except KeyboardInterrupt:
                break
            except Exception as exc:
                self.console.print(f"[{SEKAI_CORAL}]watch error:[/] {exc}")
            time.sleep(2)

    def cmd_check(self, location_id: str) -> None:
        self.cmd_room_simple("record_check", {"location_id": int(location_id)})

    def cmd_item(self, slot: str, item_id: str, name: str, location_id: str = "0") -> None:
        item = {
            "receiver_slot": int(slot),
            "item_id": int(item_id),
            "item_name": name,
            "location_id": int(location_id),
            "sender_slot": 0,
            "sender_name": "SekaiLink Admin TUI",
            "flags": 0,
        }
        self.cmd_room_simple("enqueue_received_item", {"item": item})

    async def ap_exchange(self, slot: str, game: str, packets: list[dict[str, Any]], password: str = "") -> list[dict[str, Any]]:
        if not self.ap_uri:
            if not self.room.ready():
                raise RuntimeError("AP endpoint inconnu; sélectionne une room")
            self.ap_uri = f"ws://{self.room.host}:{self.room.port}"
        responses: list[dict[str, Any]] = []
        async with websockets.connect(self.ap_uri, open_timeout=8) as ws:
            room_info_raw = await asyncio.wait_for(ws.recv(), timeout=8)
            responses.extend(json.loads(room_info_raw))
            connect_packet = {
                "cmd": "Connect",
                "password": password,
                "name": slot,
                "game": game,
                "version": {"major": 0, "minor": 6, "build": 7, "class": "Version"},
                "items_handling": 7,
                "tags": ["AP"],
                "uuid": f"sekailink-admin-tui-{uuid.uuid4()}",
            }
            await ws.send(json.dumps([connect_packet]))
            while True:
                raw = await asyncio.wait_for(ws.recv(), timeout=8)
                batch = json.loads(raw)
                responses.extend(batch)
                if any(packet.get("cmd") in {"Connected", "ConnectionRefused"} for packet in batch if isinstance(packet, dict)):
                    break
            if any(packet.get("cmd") == "ConnectionRefused" for packet in responses if isinstance(packet, dict)):
                return responses
            await ws.send(json.dumps(packets))
            try:
                raw = await asyncio.wait_for(ws.recv(), timeout=1.5)
                responses.extend(json.loads(raw))
            except asyncio.TimeoutError:
                pass
        self.record("ap_exchange", {"uri": self.ap_uri, "slot": slot, "game": game, "packets": packets, "responses": responses})
        return responses

    async def ap_data_package(self, game: str) -> dict[str, Any]:
        if not self.ap_uri:
            if not self.room.ready():
                raise RuntimeError("AP endpoint inconnu; sélectionne une room")
            self.ap_uri = f"ws://{self.room.host}:{self.room.port}"
        responses: list[dict[str, Any]] = []
        async with websockets.connect(self.ap_uri, open_timeout=8) as ws:
            raw = await asyncio.wait_for(ws.recv(), timeout=8)
            responses.extend(json.loads(raw))
            await ws.send(json.dumps([{"cmd": "GetDataPackage", "games": [game]}]))
            raw = await asyncio.wait_for(ws.recv(), timeout=8)
            responses.extend(json.loads(raw))
        for packet in responses:
            if isinstance(packet, dict) and packet.get("cmd") == "DataPackage":
                games = packet.get("data", {}).get("games", {})
                if isinstance(games, dict) and isinstance(games.get(game), dict):
                    self.record("ap_datapackage", {"game": game, "items": len(games[game].get("item_name_to_id", {}))})
                    return games[game]
        raise RuntimeError(f"datapackage introuvable pour {game}")

    def load_items_for_game(self, game: str) -> dict[str, int]:
        if game not in self.item_cache:
            package = asyncio.run(self.ap_data_package(game))
            items = package.get("item_name_to_id", {}) if isinstance(package, dict) else {}
            if not isinstance(items, dict) or not items:
                raise RuntimeError(f"aucune table item_name_to_id pour {game}")
            self.item_cache[game] = {str(name): int(item_id) for name, item_id in items.items()}
        return self.item_cache[game]

    def cmd_ap_check(self, slot: str, game: str, locations: list[str]) -> None:
        slot, game = self.validate_slot_and_game(slot, game)
        ids = [int(value) for value in locations]
        responses = asyncio.run(self.ap_exchange(slot, game, [{"cmd": "LocationChecks", "locations": ids}]))
        self.print_response({"ok": True, "responses": responses})

    def cmd_ap_say(self, slot: str, game: str, text: str) -> None:
        slot, game = self.validate_slot_and_game(slot, game)
        responses = asyncio.run(self.ap_exchange(slot, game, [{"cmd": "Say", "text": text}]))
        self.print_response({"ok": True, "responses": responses})

    def cmd_items(self, game: str = "") -> None:
        if not game:
            _, game = self.default_player()
        items = self.load_items_for_game(game)
        table = Table(title=f"Items AP - {game}", box=box.ROUNDED, border_style=SEKAI_PURPLE)
        table.add_column("Nom", style=f"bold {SEKAI_TEAL}")
        table.add_column("ID", justify="right", style=SEKAI_MUTED)
        for name, item_id in sorted(items.items()):
            table.add_row(name, str(item_id))
        self.console.print(table)

    def cmd_give(self, slot_name: str, item_name: str) -> None:
        game = self.game_for_slot_name(slot_name)
        items = self.load_items_for_game(game)
        exact = next((name for name in items if name.lower() == item_name.lower()), "")
        if not exact:
            choices = sorted(items)
            suggestions = difflib.get_close_matches(item_name, choices, n=8, cutoff=0.35)
            table = Table(title=f"Item introuvable: {item_name}", box=box.ROUNDED, border_style=SEKAI_CORAL)
            table.add_column("Suggestions proches", style=f"bold {SEKAI_TEAL}")
            for suggestion in suggestions or choices[:12]:
                table.add_row(suggestion)
            self.console.print(table)
            return
        self.console.print(f"[{SEKAI_TEAL}]Give:[/] {exact} -> {slot_name} ({game})")
        self.console.print(
            f"[{SEKAI_MUTED}]Note:[/] `give` envoie `!admin /send <slot> <item>` via la connexion AP monitor persistante "
            "en mode tracker/admin (items_handling=0)."
        )
        if not self.ap_connection.connected_for(self.ap_uri, slot_name, game):
            self.cmd_ap_connect(slot_name, game)
        if not self.ap_admin_password:
            raise RuntimeError("SEKAILINK_AP_ADMIN_PASSWORD manquant: /send doit passer par !admin login puis !admin /send")
        self.ap_connection.send({"cmd": "Say", "text": f"!admin /send {slot_name} {exact}"})
        time.sleep(0.4)
        self.cmd_ap_log(12)

    def cmd_release(self, slot_name: str) -> None:
        game = self.game_for_slot_name(slot_name)
        self.console.print(f"[{SEKAI_TEAL}]Release:[/] {slot_name} ({game})")
        self.console.print(
            f"[{SEKAI_MUTED}]Note:[/] `release` envoie `!admin /release <slot>` via la connexion AP monitor persistante "
            "en mode tracker/admin (items_handling=0)."
        )
        if not self.ap_connection.connected_for(self.ap_uri, slot_name, game):
            self.cmd_ap_connect(slot_name, game)
        if not self.ap_admin_password:
            raise RuntimeError("SEKAILINK_AP_ADMIN_PASSWORD manquant: /release doit passer par !admin login puis !admin /release")
        self.ap_connection.send({"cmd": "Say", "text": f"!admin /release {slot_name}"})
        time.sleep(0.4)
        self.cmd_ap_log(12)

    def cmd_compensator(self, args: list[str]) -> None:
        action = args[0].lower() if args else "status"
        if action == "status":
            command = "!admin /compensator status"
        elif action == "release" and len(args) == 2 and args[1].lower() in {"all", "progression", "accelerate"}:
            command = f"!admin /compensator release {args[1].lower()}"
        else:
            raise RuntimeError("usage: compensator [status|release <all|progression|accelerate>]")
        slot_name, game = self.default_player()
        if not self.ap_connection.connected_for(self.ap_uri, slot_name, game):
            self.cmd_ap_connect(slot_name, game)
        if not self.ap_admin_password:
            raise RuntimeError("SEKAILINK_AP_ADMIN_PASSWORD manquant: compensator requiert l'admin AP")
        self.ap_connection.send({"cmd": "Say", "text": command})
        time.sleep(0.4)
        self.cmd_ap_log(12)

    async def ap_room_info(self) -> list[dict[str, Any]]:
        if not self.ap_uri:
            if not self.room.ready():
                raise RuntimeError("AP endpoint inconnu; sélectionne une room")
            self.ap_uri = f"ws://{self.room.host}:{self.room.port}"
        async with websockets.connect(self.ap_uri, open_timeout=8) as ws:
            raw = await asyncio.wait_for(ws.recv(), timeout=8)
            responses = json.loads(raw)
        self.record("ap_info", {"uri": self.ap_uri, "responses": responses})
        return responses

    def cmd_ap_info(self) -> None:
        responses = asyncio.run(self.ap_room_info())
        self.print_ap_responses(responses)

    def cmd_export(self) -> None:
        REPORT_DIR.mkdir(parents=True, exist_ok=True)
        path = REPORT_DIR / f"room-admin-{datetime.now().strftime('%Y%m%d-%H%M%S')}.jsonl"
        with path.open("w", encoding="utf-8") as handle:
            for entry in self.history:
                handle.write(json.dumps(entry, ensure_ascii=False) + "\n")
        self.console.print(f"[{SEKAI_TEAL}]Rapport écrit:[/] {path}")

    def cmd_config(self) -> None:
        config = {
            "api_base": self.api_base,
            "identity_token": "***set via SEKAILINK_TOKEN or edit manually***",
            "room_admin_token": "***set via SEKAILINK_ROOM_ADMIN_TOKEN or edit manually***",
            "room_runtime_token": "***optional SEKAILINK_ROOM_RUNTIME_TOKEN***",
            "room_admin_tool_token": "***optional SEKAILINK_ROOM_ADMIN_TOOL_TOKEN for generated AP admin passwords***",
            "ap_admin_password": "***optional SEKAILINK_AP_ADMIN_PASSWORD for !admin /send***",
        }
        CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
        if not CONFIG_PATH.exists():
            CONFIG_PATH.write_text(json.dumps(config, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        self.console.print(Panel(f"Config locale: {CONFIG_PATH}\nVariables env supportées:\nSEKAILINK_API_BASE\nSEKAILINK_TOKEN\nSEKAILINK_ROOM_ADMIN_TOKEN\nSEKAILINK_ROOM_RUNTIME_TOKEN\nSEKAILINK_ROOM_ADMIN_TOOL_TOKEN\nSEKAILINK_AP_ADMIN_PASSWORD", border_style=SEKAI_PURPLE))

    def completion_words(self, line: str, begin: int) -> list[str]:
        try:
            parts = shlex.split(line[:begin])
        except ValueError:
            parts = line[:begin].split()
        if begin == 0 or not parts:
            return self.commands
        command = parts[0].lower()
        if len(parts) == 1 and not line[:begin].endswith(" "):
            return self.commands
        players = [name for name, _game in self.room_players()]
        if command in {"give", "release", "ap-say", "ap-check"} and len(parts) <= 2:
            return players
        if command == "give" and len(parts) >= 2:
            try:
                game = self.game_for_slot_name(parts[1])
                return sorted(self.load_items_for_game(game))
            except Exception:
                return []
        if command == "select":
            return [str(lobby.index) for lobby in self.lobbies] + [lobby.lobby_id for lobby in self.lobbies]
        return []

    def setup_readline(self) -> None:
        HISTORY_PATH.parent.mkdir(parents=True, exist_ok=True)
        try:
            readline.read_history_file(str(HISTORY_PATH))
        except FileNotFoundError:
            pass
        readline.set_history_length(1000)
        readline.parse_and_bind("tab: complete")
        readline.set_completer_delims(" \t\n")

        def complete(text: str, state: int) -> str | None:
            line = readline.get_line_buffer()
            begin = readline.get_begidx()
            words = self.completion_words(line, begin)
            matches = [word for word in words if word.lower().startswith(text.lower())]
            if state < len(matches):
                suffix = " " if " " not in matches[state] else ""
                return matches[state] + suffix
            return None

        readline.set_completer(complete)

    def save_readline_history(self) -> None:
        try:
            readline.write_history_file(str(HISTORY_PATH))
        except OSError:
            pass

    def dispatch(self, line: str) -> bool:
        parts = shlex.split(line)
        if not parts:
            return True
        command = parts[0].lower()
        args = parts[1:]
        if command in {"quit", "exit", "q"}:
            return False
        if command in {"help", "h", "?"}:
            self.show_help()
        elif command == "login":
            self.cmd_login(args[0] if args else "")
        elif command == "whoami":
            self.cmd_whoami()
        elif command == "statusbar":
            self.print_status()
        elif command == "lobbies":
            self.cmd_lobbies(int(args[0]) if args else 80)
        elif command == "select":
            if not args:
                raise RuntimeError("usage: select <index|lobby_id>")
            self.cmd_select(args[0])
        elif command == "generation":
            self.cmd_generation()
        elif command == "status":
            self.cmd_status()
        elif command == "snapshot":
            self.cmd_room_simple("snapshot_room")
        elif command == "summary":
            self.cmd_room_simple("room_summary")
        elif command == "ap-info":
            self.cmd_ap_info()
        elif command == "ap-connect":
            if len(args) >= 2:
                self.cmd_ap_connect(args[0], " ".join(args[1:]))
            else:
                self.cmd_ap_connect()
        elif command == "ap-disconnect":
            self.cmd_ap_disconnect()
        elif command == "secrets":
            if not self.fetch_room_admin_secrets(silent=False):
                raise RuntimeError("secrets admin non chargés")
        elif command == "ap-log":
            self.cmd_ap_log(int(args[0]) if args else 40)
        elif command == "events":
            self.cmd_events(int(args[0]) if args else 20, args[1] if len(args) > 1 else "")
        elif command == "watch":
            self.cmd_watch(int(args[0]) if args else 60)
        elif command == "reports":
            self.cmd_reports(int(args[0]) if args else 20)
        elif command == "check":
            if not args:
                raise RuntimeError("usage: check <location_id>")
            self.cmd_check(args[0])
        elif command == "item":
            if len(args) < 3:
                raise RuntimeError("usage: item <slot> <item_id> <name> [location_id]")
            self.cmd_item(args[0], args[1], args[2], args[3] if len(args) > 3 else "0")
        elif command == "players":
            self.cmd_players()
        elif command == "items":
            self.cmd_items(" ".join(args) if args else "")
        elif command == "give":
            if len(args) < 2:
                raise RuntimeError("usage: give <slotname> <item name...>")
            self.cmd_give(args[0], " ".join(args[1:]))
        elif command == "release":
            if not args:
                raise RuntimeError("usage: release <slotname>")
            self.cmd_release(args[0])
        elif command == "compensator":
            self.cmd_compensator(args)
        elif command == "raw":
            if not args:
                raise RuntimeError('usage: raw \'{"cmd":"room_summary","room_id":"..."}\'')
            payload = parse_json_arg(" ".join(args), {})
            if not isinstance(payload, dict):
                raise RuntimeError("raw attend un objet JSON")
            if "room_id" not in payload and self.room.room_id:
                payload["room_id"] = self.room.room_id
            self.print_response(self.send_room_command(payload))
        elif command == "ap-check":
            if len(args) >= 3 and not args[0].lstrip("-").isdigit():
                self.cmd_ap_check(args[0], args[1], args[2:])
            elif args:
                slot, game = self.default_player()
                self.cmd_ap_check(slot, game, args)
            else:
                raise RuntimeError("usage: ap-check [slot_name game] <location_id...>")
        elif command == "ap-say":
            if len(args) >= 3 and any(player.get("name") == args[0] for player in self.room.status.get("players", []) if isinstance(player, dict)):
                self.cmd_ap_say(args[0], args[1], " ".join(args[2:]))
            elif args:
                slot, game = self.default_player()
                self.cmd_ap_say(slot, game, " ".join(args))
            else:
                raise RuntimeError("usage: ap-say [slot_name game] <message...>")
        elif command == "export":
            self.cmd_export()
        elif command == "config":
            self.cmd_config()
        else:
            raise RuntimeError(f"commande inconnue: {command}")
        return True

    def run(self) -> int:
        self.setup_readline()
        self.print_header()
        self.print_status()
        self.show_help()
        while True:
            try:
                line = Prompt.ask(f"[bold {SEKAI_TEAL}]skl-room[/]")
                if not self.dispatch(line):
                    break
            except KeyboardInterrupt:
                self.console.print("\n[yellow]Interrompu.[/yellow]")
            except EOFError:
                break
            except Exception as exc:
                self.console.print(f"[{SEKAI_CORAL}]Erreur:[/] {exc}")
                self.record("error", {"message": str(exc)})
        self.ap_connection.stop()
        self.save_readline_history()
        return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="SekaiLink Room Admin TUI")
    parser.add_argument("--api-base", default="", help="Base URL API SekaiLink, défaut https://sekailink.com")
    parser.add_argument("--token", default="", help="Token identity/client pour lire les lobbies si requis.")
    parser.add_argument("--room-admin-token", default="", help="Token admin du room server.")
    parser.add_argument("--room-runtime-token", default="", help="Token runtime du room server.")
    parser.add_argument("--room-admin-tool-token", default="", help="Token outil pour /api/room_admin_secrets/<room_id>.")
    parser.add_argument("--ap-admin-password", default="", help="Mot de passe remote admin Archipelago pour !admin /send et !admin /release.")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    return SekaiLinkRoomAdmin(args).run()


if __name__ == "__main__":
    raise SystemExit(main())
