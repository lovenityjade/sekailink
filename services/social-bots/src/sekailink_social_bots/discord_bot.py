from __future__ import annotations

import asyncio
import base64
import io
import json
import logging
import re
import tempfile
import zipfile
import time
import secrets
from dataclasses import dataclass, field
from pathlib import Path
from datetime import datetime, timezone

import discord

from .ai_writer import AIWriter
from .config import ServiceConfig
from .models import FaqEntry


logger = logging.getLogger("sekailink.discord")
BUG_REPORT_ALLOWED_ROLE_IDS = {
    1456948157630906410,
    1457050764663853170,
    1456948161745522835,
}
AUTO_JOIN_ROLE_NAMES = ("Player", "Vault-Validated")


@dataclass(slots=True)
class DiscordRuntimeState:
    ready: bool = False
    managed_temp_channels: set[int] = field(default_factory=set)
    lobby_voice_cleanup_tasks: dict[int, asyncio.Task] = field(default_factory=dict)


class SekaiDiscordClient(discord.Client):
    def __init__(self, manager: "DiscordBotManager") -> None:
        intents = discord.Intents.none()
        intents.guilds = True
        intents.members = True
        intents.voice_states = True
        intents.guild_messages = True
        intents.message_content = True
        super().__init__(intents=intents)
        self.manager = manager

    async def on_ready(self) -> None:
        self.manager.state.ready = True
        await self.manager.reconcile_lobby_voice_channels()
        await self.manager.set_server_status("Online")
        logger.info("Discord bot connected as %s", self.user)

    async def on_voice_state_update(
        self,
        member: discord.Member,
        before: discord.VoiceState,
        after: discord.VoiceState,
    ) -> None:
        await self.manager.handle_voice_state_update(member, before, after)

    async def on_member_join(self, member: discord.Member) -> None:
        await self.manager.handle_member_join(member)

    async def on_message(self, message: discord.Message) -> None:
        if message.author.bot:
            return
        content = (message.content or "").strip()
        if content in {"!ongoing", "!fixed", "!close"}:
            await self.manager.handle_bug_thread_status_command(message, content)
            return
        cmd_prefixes = ("!sekai-ai", "!sekaiai")
        matched_prefix = next((p for p in cmd_prefixes if content.startswith(p)), None)
        if not matched_prefix:
            return
        if not self.manager.ai or not self.manager.ai.enabled:
            await message.channel.send("Sekai AI is disabled.")
            return
        request = content[len(matched_prefix):].strip()
        if not request:
            await message.channel.send("Usage: !sekai-ai <request>")
            return
        try:
            draft = await self.manager.ai.draft_announcement(request_text=request, language="fr")
            await message.channel.send(f"**Sekai AI Draft**\n{draft[:1900]}")
        except Exception as exc:
            logger.exception("discord ai command failed")
            await message.channel.send(f"AI error: {exc}")


class DiscordBotManager:
    def __init__(self, config: ServiceConfig, ai: AIWriter | None = None) -> None:
        self.config = config
        self.ai = ai
        self.state = DiscordRuntimeState()
        self.client = SekaiDiscordClient(self)
        self._task: asyncio.Task | None = None
        self._lobby_voice_file = Path("/opt/sekailink-social-bots/data/discord_lobby_voice_map.json")
        self._lobby_voice_file.parent.mkdir(parents=True, exist_ok=True)
        self._voice_lock = asyncio.Lock()
        self._lobby_voice_map: dict[str, int] = self._load_lobby_voice_map()

    @property
    def enabled(self) -> bool:
        return self.config.discord_enabled and bool(self.config.discord_token)

    async def start(self) -> None:
        self._purge_expired_bug_bundles()
        if not self.enabled:
            logger.info("Discord bot disabled or missing token.")
            return
        if self._task and not self._task.done():
            return
        self._task = asyncio.create_task(self.client.start(self.config.discord_token), name="discord-bot")

    async def stop(self) -> None:
        if self.client.is_ready() or not self.client.is_closed():
            await self.client.close()
        if self._task and not self._task.done():
            self._task.cancel()

    async def announce(self, message: str, title: str | None = None, channel_id: int | None = None, mention_everyone: bool = False) -> None:
        channel = await self._resolve_text_channel(channel_id or self.config.discord_announce_channel_id)
        if channel is None:
            raise RuntimeError("discord_announce_channel_not_found")
        content = "@everyone" if mention_everyone else None
        embed = discord.Embed(description=message, color=discord.Color.teal())
        if title:
            embed.title = title
        await channel.send(content=content, embed=embed)

    async def create_bug_report_thread(
        self,
        title: str,
        description: str,
        reporter_name: str,
        screenshot_base64: str | None = None,
        logs_text: str | None = None,
        system_info: dict | None = None,
        app_info: dict | None = None,
        bundle_base64: str | None = None,
        bundle_manifest: dict | None = None,
        channel_id: int | None = None,
    ) -> dict:
        resolved_channel_id = channel_id or self.config.discord_bug_report_channel_id or self.config.discord_announce_channel_id
        channel = await self._resolve_bug_report_channel(resolved_channel_id)
        if channel is None:
            raise RuntimeError("discord_bug_report_channel_not_found")

        clean_title = self._sanitize_bug_title(title)
        clean_description = (description or "").strip()[:200]
        clean_reporter = (reporter_name or "").strip()[:80] or "Unknown"
        report_id = f"br-{int(time.time())}-{secrets.token_hex(5)}"
        bundle_path = self._store_bug_report_bundle(report_id, bundle_base64)

        embed = discord.Embed(title=clean_title, description=clean_description, color=discord.Color.orange())
        embed.add_field(name="Reporter", value=clean_reporter, inline=True)
        embed.add_field(name="Status", value="OPEN", inline=True)
        embed.add_field(name="Report ID", value=report_id, inline=False)
        if bundle_path:
            embed.add_field(name="Private diagnostics", value=f"Stored for 30 days as `{report_id}`", inline=False)

        files: list[discord.File] = []
        screenshot_bytes = self._decode_base64_blob(screenshot_base64, max_bytes=6 * 1024 * 1024)
        if screenshot_bytes:
            files.append(discord.File(io.BytesIO(screenshot_bytes), filename="screenshot.png"))

        safe_logs = (logs_text or "").strip()
        if safe_logs:
            safe_logs = safe_logs[:700_000]
            files.append(discord.File(io.BytesIO(safe_logs.encode("utf-8", errors="ignore")), filename="session.log"))

        details_payload = {
            "system_info": system_info or {},
            "app_info": app_info or {},
            "bundle_manifest": bundle_manifest or {},
            "report_id": report_id,
        }
        details_bytes = json.dumps(details_payload, ensure_ascii=True, indent=2).encode("utf-8", errors="ignore")
        files.append(discord.File(io.BytesIO(details_bytes[:350_000]), filename="system-info.json"))

        thread_name = self._set_thread_prefix(clean_title, None)
        message = None
        if isinstance(channel, discord.TextChannel):
            message = await channel.send(embed=embed, files=files)
            thread = await message.create_thread(name=thread_name[:100], auto_archive_duration=10080)
        elif isinstance(channel, discord.ForumChannel):
            created = await channel.create_thread(
                name=thread_name[:100],
                content=f"Reporter: {clean_reporter}\nStatus: OPEN\n\n{clean_description}",
                embed=embed,
                files=files,
                auto_archive_duration=10080,
            )
            thread = getattr(created, "thread", None) or created
            message = getattr(created, "message", None)
        else:
            raise RuntimeError("discord_bug_report_channel_not_supported")
        await thread.send("Bug report received. Use `!ongoing`, `!fixed`, or `!close` to update status.")
        return {
            "channel_id": channel.id,
            "message_id": int(message.id) if message is not None else 0,
            "thread_id": thread.id,
            "thread_name": thread.name,
            "report_id": report_id,
            "bundle_stored": bool(bundle_path),
        }

    def _bug_bundle_root(self) -> Path:
        root = Path("/opt/sekailink-social-bots/data/bug-report-bundles")
        root.mkdir(parents=True, exist_ok=True)
        return root

    def _purge_expired_bug_bundles(self) -> None:
        cutoff = time.time() - 30 * 24 * 60 * 60
        for path in self._bug_bundle_root().glob("br-*.zip"):
            try:
                if path.stat().st_mtime < cutoff:
                    path.unlink(missing_ok=True)
            except OSError:
                logger.exception("failed purging bug report bundle %s", path)

    def _store_bug_report_bundle(self, report_id: str, raw: str | None) -> Path | None:
        self._purge_expired_bug_bundles()
        value = str(raw or "").strip()
        if not value:
            return None
        try:
            data = base64.b64decode(value, validate=False)
        except Exception as exc:
            raise RuntimeError("bug_report_bundle_invalid_base64") from exc
        if len(data) > 48 * 1024 * 1024:
            raise RuntimeError("bug_report_bundle_too_large")
        if not data.startswith(b"PK"):
            raise RuntimeError("bug_report_bundle_invalid_zip")
        path = self._bug_bundle_root() / f"{report_id}.zip"
        path.write_bytes(data)
        path.chmod(0o600)
        return path

    def get_bug_report_bundle(self, report_id: str) -> Path:
        clean = re.sub(r"[^A-Za-z0-9-]+", "", str(report_id or ""))
        path = self._bug_bundle_root() / f"{clean}.zip"
        if not clean.startswith("br-") or not path.is_file():
            raise RuntimeError("bug_report_bundle_not_found")
        return path

    async def export_bug_reports(
        self,
        statuses: list[str] | None = None,
        include_attachments: bool = True,
        max_threads: int = 500,
    ) -> Path:
        channel = await self._resolve_bug_report_channel(self.config.discord_bug_report_channel_id)
        if channel is None:
            raise RuntimeError("discord_bug_report_channel_not_found")
        wanted = {str(status).strip().upper() for status in (statuses or []) if str(status).strip()}
        threads: dict[int, discord.Thread] = {}
        for thread in getattr(channel, "threads", []):
            threads[int(thread.id)] = thread
        if hasattr(channel, "archived_threads"):
            async for thread in channel.archived_threads(limit=None):
                threads[int(thread.id)] = thread

        selected: list[discord.Thread] = []
        for thread in sorted(threads.values(), key=lambda item: item.created_at or datetime.min.replace(tzinfo=timezone.utc), reverse=True):
            status = self._bug_thread_status(thread.name)
            if wanted and status not in wanted:
                continue
            selected.append(thread)
            if len(selected) >= max(1, min(int(max_threads), 2000)):
                break

        export_root = Path(tempfile.mkdtemp(prefix="sekailink-bug-export-"))
        export_path = export_root / f"sekailink-bug-reports-{datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')}.zip"
        index: list[dict] = []
        with zipfile.ZipFile(export_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for thread in selected:
                status = self._bug_thread_status(thread.name)
                thread_key = f"{status.lower()}-{thread.id}"
                messages: list[dict] = []
                markdown = [f"# {thread.name}", "", f"Status: {status}", f"Thread ID: {thread.id}", ""]
                async for message in thread.history(limit=None, oldest_first=True):
                    entry = {
                        "id": int(message.id),
                        "author_id": int(message.author.id),
                        "author": str(getattr(message.author, "display_name", message.author.name)),
                        "created_at": message.created_at.isoformat(),
                        "content": message.content or "",
                        "embeds": [embed.to_dict() for embed in message.embeds],
                        "attachments": [],
                    }
                    markdown.extend([f"## {entry['author']} - {entry['created_at']}", "", entry["content"] or "_(no text)_", ""])
                    for attachment in message.attachments:
                        safe_name = re.sub(r"[^A-Za-z0-9_.-]+", "_", attachment.filename or f"attachment-{attachment.id}")
                        member = f"threads/{thread_key}/attachments/{message.id}-{attachment.id}-{safe_name}"
                        attachment_meta = {"id": int(attachment.id), "filename": safe_name, "size": int(attachment.size), "content_type": attachment.content_type, "archive_path": member}
                        if include_attachments:
                            try:
                                archive.writestr(member, await attachment.read())
                                attachment_meta["included"] = True
                            except Exception as exc:
                                attachment_meta["included"] = False
                                attachment_meta["error"] = str(exc)
                        entry["attachments"].append(attachment_meta)
                        markdown.append(f"Attachment: `{safe_name}`")
                    messages.append(entry)
                report_ids = {
                    str(field.get("value") or "").strip()
                    for entry in messages
                    for embed in entry.get("embeds", [])
                    for field in embed.get("fields", [])
                    if str(field.get("name") or "").strip().lower() == "report id"
                }
                for report_id in sorted(report_ids):
                    try:
                        bundle_path = self.get_bug_report_bundle(report_id)
                        archive.write(bundle_path, f"threads/{thread_key}/private-diagnostics/{bundle_path.name}")
                    except Exception:
                        logger.warning("private diagnostics unavailable for export report_id=%s", report_id)
                transcript = {
                    "thread_id": int(thread.id),
                    "name": thread.name,
                    "status": status,
                    "archived": bool(thread.archived),
                    "locked": bool(thread.locked),
                    "created_at": thread.created_at.isoformat() if thread.created_at else "",
                    "messages": messages,
                }
                archive.writestr(f"threads/{thread_key}/transcript.json", json.dumps(transcript, ensure_ascii=False, indent=2))
                archive.writestr(f"threads/{thread_key}/transcript.md", "\n".join(markdown))
                index.append({key: transcript[key] for key in ("thread_id", "name", "status", "archived", "locked", "created_at")})
            archive.writestr("index.json", json.dumps({"exported_at": datetime.now(timezone.utc).isoformat(), "count": len(index), "threads": index}, ensure_ascii=False, indent=2))
        return export_path

    async def sync_faq(self, entries: list[FaqEntry], channel_id: int | None = None) -> None:
        channel = await self._resolve_text_channel(channel_id or self.config.discord_faq_channel_id)
        if channel is None:
            raise RuntimeError("discord_faq_channel_not_found")

        async for message in channel.history(limit=200):
            if message.author == self.client.user and message.embeds:
                footer = message.embeds[0].footer.text if message.embeds[0].footer else ""
                if footer and footer.startswith("SekaiLink FAQ"):
                    await message.delete()

        for index, entry in enumerate(entries, start=1):
            embed = discord.Embed(title=entry.question, description=entry.answer, color=discord.Color.blurple())
            embed.set_footer(text=f"SekaiLink FAQ #{index}")
            await channel.send(embed=embed)

    async def create_temp_voice(self, name: str, user_id: int | None = None, guild_id: int | None = None, category_id: int | None = None) -> int:
        guild = self._resolve_guild(guild_id or self.config.discord_guild_id)
        if guild is None:
            raise RuntimeError("discord_guild_not_found")

        category = guild.get_channel(category_id or self.config.discord_temp_voice_category_id)
        if category_id and not isinstance(category, discord.CategoryChannel):
            raise RuntimeError("discord_voice_category_not_found")

        channel_name = f"{self.config.discord_temp_voice_prefix} • {name}"
        created = await guild.create_voice_channel(
            name=channel_name,
            category=category if isinstance(category, discord.CategoryChannel) else None,
            reason="SekaiLink temporary room",
        )
        self.state.managed_temp_channels.add(created.id)

        if user_id:
            member = guild.get_member(user_id)
            if member and member.voice:
                await member.move_to(created, reason="SekaiLink temporary room")

        return created.id

    async def create_lobby_voice(self, lobby_id: str, lobby_name: str, guild_id: int | None = None, category_id: int | None = None, member_ids: list[int] | None = None) -> dict:
        async with self._voice_lock:
            existing_id = self._lobby_voice_map.get(lobby_id)
            if existing_id:
                existing = self.client.get_channel(existing_id)
                if isinstance(existing, discord.VoiceChannel):
                    for member_id in member_ids or []:
                        await self._grant_voice_member(existing, member_id)
                    return {"channel_id": existing.id, "channel_url": self._voice_channel_url(existing.id)}

            guild = self._resolve_guild(guild_id or self.config.discord_guild_id)
            if guild is None:
                raise RuntimeError("discord_guild_not_found")
            category = guild.get_channel(category_id or self.config.discord_temp_voice_category_id)
            if category_id and not isinstance(category, discord.CategoryChannel):
                raise RuntimeError("discord_voice_category_not_found")

            safe_name = (lobby_name or "").strip() or f"Lobby {lobby_id[:8]}"
            safe_name = safe_name[:95]
            overwrites = {guild.default_role: discord.PermissionOverwrite(view_channel=False, connect=False)}
            if guild.me:
                overwrites[guild.me] = discord.PermissionOverwrite(view_channel=True, connect=True, manage_channels=True)
            for member_id in member_ids or []:
                member = guild.get_member(int(member_id))
                if member:
                    overwrites[member] = discord.PermissionOverwrite(view_channel=True, connect=True, speak=True)
            created = await guild.create_voice_channel(
                name=safe_name,
                category=category if isinstance(category, discord.CategoryChannel) else None,
                reason=f"SekaiLink lobby voice {lobby_id}",
                overwrites=overwrites,
            )
            self._lobby_voice_map[lobby_id] = created.id
            self._save_lobby_voice_map()
            return {"channel_id": created.id, "channel_url": self._voice_channel_url(created.id)}

    async def _grant_voice_member(self, channel: discord.VoiceChannel, member_id: int) -> None:
        member = channel.guild.get_member(int(member_id))
        if member is None:
            raise RuntimeError("discord_member_not_found")
        await channel.set_permissions(member, view_channel=True, connect=True, speak=True, reason="SekaiLink lobby participant")

    async def grant_lobby_voice(self, lobby_id: str, member_id: int) -> dict:
        channel_id = self._lobby_voice_map.get(lobby_id)
        channel = self.client.get_channel(channel_id) if channel_id else None
        if not isinstance(channel, discord.VoiceChannel):
            raise RuntimeError("discord_lobby_voice_not_found")
        await self._grant_voice_member(channel, member_id)
        return {"channel_id": channel.id, "channel_url": self._voice_channel_url(channel.id)}

    async def delete_lobby_voice(self, lobby_id: str) -> bool:
        async with self._voice_lock:
            channel_id = self._lobby_voice_map.get(lobby_id)
            if not channel_id:
                return False
            chan = self.client.get_channel(channel_id)
            task = self.state.lobby_voice_cleanup_tasks.pop(channel_id, None)
            if task and not task.done():
                task.cancel()
            if isinstance(chan, discord.VoiceChannel):
                await chan.delete(reason=f"SekaiLink lobby closed {lobby_id}")
            self._lobby_voice_map.pop(lobby_id, None)
            self._save_lobby_voice_map()
            return True

    def get_lobby_voice(self, lobby_id: str) -> dict | None:
        channel_id = self._lobby_voice_map.get(lobby_id)
        if not channel_id:
            return None
        channel = self.client.get_channel(channel_id)
        members = []
        if isinstance(channel, discord.VoiceChannel):
            members = [{"discord_id": member.id, "display_name": member.display_name, "avatar_url": str(member.display_avatar.url), "muted": bool(member.voice.mute if member.voice else False), "deafened": bool(member.voice.deaf if member.voice else False)} for member in channel.members]
        return {"channel_id": channel_id, "channel_url": self._voice_channel_url(channel_id), "members": members}

    async def set_server_status(self, status: str) -> None:
        text = f"SekaiLink: {status.strip()[:24] or 'Online'}"
        await self.client.change_presence(activity=discord.Game(name=text))

    async def handle_bug_thread_status_command(self, message: discord.Message, command: str) -> None:
        if not isinstance(message.channel, discord.Thread):
            return
        thread = message.channel
        parent_id = getattr(thread, "parent_id", None)
        if int(parent_id or 0) != int(self.config.discord_bug_report_channel_id or 0):
            return
        if not isinstance(message.author, discord.Member):
            return
        member_role_ids = {int(role.id) for role in getattr(message.author, "roles", [])}
        if not member_role_ids.intersection(BUG_REPORT_ALLOWED_ROLE_IDS):
            await thread.send("You are not allowed to change bug status.")
            return

        status_map = {
            "!ongoing": ("ONGOING", False),
            "!fixed": ("FIXED", True),
            "!close": ("CLOSED", True),
        }
        status, lock_thread = status_map.get(command, ("", False))
        if not status:
            return

        new_name = self._set_thread_prefix(thread.name, status)
        try:
            await thread.edit(name=new_name[:100], archived=lock_thread, locked=lock_thread)
        except Exception:
            logger.exception("failed to edit bug report thread status")
            await thread.send(f"Failed to set status to [{status}].")
            return
        await thread.send(f"Status updated to [{status}] by {message.author.display_name}.")

    async def handle_member_join(self, member: discord.Member) -> None:
        roles_by_name = {role.name.lower(): role for role in member.guild.roles}
        wanted_roles: list[discord.Role] = []
        missing_role_names: list[str] = []
        member_role_ids = {role.id for role in member.roles}

        for role_name in AUTO_JOIN_ROLE_NAMES:
            role = roles_by_name.get(role_name.lower())
            if role is None:
                missing_role_names.append(role_name)
                continue
            if role.id not in member_role_ids:
                wanted_roles.append(role)

        if missing_role_names:
            logger.warning(
                "Discord join role(s) missing in guild %s: %s",
                member.guild.id,
                ", ".join(missing_role_names),
            )
        if not wanted_roles:
            return

        try:
            await member.add_roles(*wanted_roles, reason="SekaiLink auto role on member join")
            logger.info(
                "Assigned Discord join roles to %s (%s): %s",
                member.display_name,
                member.id,
                ", ".join(role.name for role in wanted_roles),
            )
        except discord.Forbidden:
            logger.exception(
                "Discord bot cannot assign join roles to %s (%s); check role hierarchy/permissions",
                member.display_name,
                member.id,
            )
        except discord.HTTPException:
            logger.exception("Discord API failed while assigning join roles to %s (%s)", member.display_name, member.id)

    async def handle_voice_state_update(
        self,
        member: discord.Member,
        before: discord.VoiceState,
        after: discord.VoiceState,
    ) -> None:
        lobby_id = self.config.discord_temp_voice_lobby_channel_id
        if lobby_id and after.channel and after.channel.id == lobby_id:
            name = member.display_name[:40]
            await self.create_temp_voice(name=name, user_id=member.id, guild_id=member.guild.id)
            return

        if before.channel and before.channel.id in self.state.managed_temp_channels:
            channel = before.channel
            if isinstance(channel, discord.VoiceChannel) and len(channel.members) == 0:
                await channel.delete(reason="SekaiLink temporary room cleanup")
                self.state.managed_temp_channels.discard(channel.id)

        lobby_channel_ids = set(self._lobby_voice_map.values())
        if after.channel and after.channel.id in lobby_channel_ids:
            task = self.state.lobby_voice_cleanup_tasks.pop(after.channel.id, None)
            if task and not task.done():
                task.cancel()
        if before.channel and before.channel.id in lobby_channel_ids and isinstance(before.channel, discord.VoiceChannel) and not before.channel.members:
            existing = self.state.lobby_voice_cleanup_tasks.get(before.channel.id)
            if not existing or existing.done():
                self.state.lobby_voice_cleanup_tasks[before.channel.id] = asyncio.create_task(
                    self._delete_empty_lobby_voice_after(before.channel.id, 15 * 60),
                    name=f"discord-lobby-voice-cleanup-{before.channel.id}",
                )

    async def _delete_empty_lobby_voice_after(self, channel_id: int, delay_seconds: int) -> None:
        try:
            await asyncio.sleep(delay_seconds)
            channel = self.client.get_channel(channel_id)
            if not isinstance(channel, discord.VoiceChannel) or channel.members:
                return
            lobby_id = next((key for key, value in self._lobby_voice_map.items() if value == channel_id), "")
            if lobby_id:
                await channel.delete(reason="SekaiLink lobby voice empty for 15 minutes")
                self._lobby_voice_map.pop(lobby_id, None)
                self._save_lobby_voice_map()
        except asyncio.CancelledError:
            return
        except Exception:
            logger.exception("failed delayed lobby voice cleanup channel=%s", channel_id)
        finally:
            self.state.lobby_voice_cleanup_tasks.pop(channel_id, None)

    async def reconcile_lobby_voice_channels(self) -> None:
        changed = False
        for lobby_id, channel_id in list(self._lobby_voice_map.items()):
            channel = self.client.get_channel(channel_id)
            if not isinstance(channel, discord.VoiceChannel):
                self._lobby_voice_map.pop(lobby_id, None)
                changed = True
                continue
            if not channel.members:
                self.state.lobby_voice_cleanup_tasks[channel.id] = asyncio.create_task(
                    self._delete_empty_lobby_voice_after(channel.id, 15 * 60),
                    name=f"discord-lobby-voice-reconcile-{channel.id}",
                )
        if changed:
            self._save_lobby_voice_map()

    def _resolve_guild(self, guild_id: int | None) -> discord.Guild | None:
        if guild_id is None:
            return None
        return self.client.get_guild(guild_id)

    async def _resolve_text_channel(self, channel_id: int | None) -> discord.TextChannel | None:
        if channel_id is None:
            return None
        channel = self.client.get_channel(channel_id)
        if isinstance(channel, discord.TextChannel):
            return channel
        fetched = await self.client.fetch_channel(channel_id)
        return fetched if isinstance(fetched, discord.TextChannel) else None

    async def _resolve_bug_report_channel(self, channel_id: int | None) -> discord.TextChannel | discord.ForumChannel | None:
        if channel_id is None:
            return None
        channel = self.client.get_channel(channel_id)
        if isinstance(channel, (discord.TextChannel, discord.ForumChannel)):
            return channel
        fetched = await self.client.fetch_channel(channel_id)
        return fetched if isinstance(fetched, (discord.TextChannel, discord.ForumChannel)) else None

    def _voice_channel_url(self, channel_id: int) -> str:
        guild_id = self.config.discord_guild_id
        if not guild_id:
            return ""
        return f"https://discord.com/channels/{guild_id}/{channel_id}"

    def _sanitize_bug_title(self, title: str) -> str:
        value = re.sub(r"\s+", " ", str(title or "").strip())
        value = self._remove_thread_prefix(value)
        return value[:100] or "Bug report"

    def _bug_thread_status(self, title: str) -> str:
        match = re.match(r"^\[(ONGOING|FIXED|CLOSED)\]\s*", str(title or ""), flags=re.IGNORECASE)
        return match.group(1).upper() if match else "OPEN"

    def _remove_thread_prefix(self, title: str) -> str:
        return re.sub(r"^\[(ONGOING|FIXED|CLOSED)\]\s*", "", str(title or "").strip(), flags=re.IGNORECASE)

    def _set_thread_prefix(self, title: str, status: str | None) -> str:
        base = self._remove_thread_prefix(title)[:100]
        if not status:
            return base or "Bug report"
        prefix = f"[{status.upper()}] "
        allowed = max(1, 100 - len(prefix))
        return f"{prefix}{(base or 'Bug report')[:allowed]}"

    def _decode_base64_blob(self, raw: str | None, max_bytes: int) -> bytes:
        value = str(raw or "").strip()
        if not value:
            return b""
        if "," in value and value.startswith("data:"):
            value = value.split(",", 1)[1]
        try:
            out = base64.b64decode(value, validate=False)
        except Exception:
            return b""
        if not out:
            return b""
        return out[:max_bytes]

    def _load_lobby_voice_map(self) -> dict[str, int]:
        try:
            if not self._lobby_voice_file.exists():
                return {}
            raw = json.loads(self._lobby_voice_file.read_text(encoding="utf-8"))
            if not isinstance(raw, dict):
                return {}
            out: dict[str, int] = {}
            for k, v in raw.items():
                try:
                    out[str(k)] = int(v)
                except Exception:
                    continue
            return out
        except Exception:
            logger.exception("failed loading lobby voice map")
            return {}

    def _save_lobby_voice_map(self) -> None:
        try:
            self._lobby_voice_file.write_text(json.dumps(self._lobby_voice_map, indent=2, sort_keys=True), encoding="utf-8")
        except Exception:
            logger.exception("failed saving lobby voice map")
