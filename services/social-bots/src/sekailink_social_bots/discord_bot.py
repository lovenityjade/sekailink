from __future__ import annotations

import asyncio
import json
import logging
from dataclasses import dataclass, field
from pathlib import Path

import discord

from .ai_writer import AIWriter
from .config import ServiceConfig
from .models import FaqEntry


logger = logging.getLogger("sekailink.discord")


@dataclass(slots=True)
class DiscordRuntimeState:
    ready: bool = False
    managed_temp_channels: set[int] = field(default_factory=set)


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
        await self.manager.set_server_status("Online")
        logger.info("Discord bot connected as %s", self.user)

    async def on_voice_state_update(
        self,
        member: discord.Member,
        before: discord.VoiceState,
        after: discord.VoiceState,
    ) -> None:
        await self.manager.handle_voice_state_update(member, before, after)

    async def on_message(self, message: discord.Message) -> None:
        if message.author.bot:
            return
        content = (message.content or "").strip()
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

    async def create_lobby_voice(self, lobby_id: str, lobby_name: str, guild_id: int | None = None, category_id: int | None = None) -> dict:
        async with self._voice_lock:
            existing_id = self._lobby_voice_map.get(lobby_id)
            if existing_id:
                existing = self.client.get_channel(existing_id)
                if isinstance(existing, discord.VoiceChannel):
                    return {"channel_id": existing.id, "channel_url": self._voice_channel_url(existing.id)}

            guild = self._resolve_guild(guild_id or self.config.discord_guild_id)
            if guild is None:
                raise RuntimeError("discord_guild_not_found")
            category = guild.get_channel(category_id or self.config.discord_temp_voice_category_id)
            if category_id and not isinstance(category, discord.CategoryChannel):
                raise RuntimeError("discord_voice_category_not_found")

            safe_name = (lobby_name or "").strip() or f"Lobby {lobby_id[:8]}"
            safe_name = safe_name[:95]
            created = await guild.create_voice_channel(
                name=safe_name,
                category=category if isinstance(category, discord.CategoryChannel) else None,
                reason=f"SekaiLink lobby voice {lobby_id}",
            )
            self._lobby_voice_map[lobby_id] = created.id
            self._save_lobby_voice_map()
            return {"channel_id": created.id, "channel_url": self._voice_channel_url(created.id)}

    async def delete_lobby_voice(self, lobby_id: str) -> bool:
        async with self._voice_lock:
            channel_id = self._lobby_voice_map.get(lobby_id)
            if not channel_id:
                return False
            chan = self.client.get_channel(channel_id)
            if isinstance(chan, discord.VoiceChannel):
                await chan.delete(reason=f"SekaiLink lobby closed {lobby_id}")
            self._lobby_voice_map.pop(lobby_id, None)
            self._save_lobby_voice_map()
            return True

    def get_lobby_voice(self, lobby_id: str) -> dict | None:
        channel_id = self._lobby_voice_map.get(lobby_id)
        if not channel_id:
            return None
        return {"channel_id": channel_id, "channel_url": self._voice_channel_url(channel_id)}

    async def set_server_status(self, status: str) -> None:
        text = f"SekaiLink: {status.strip()[:24] or 'Online'}"
        await self.client.change_presence(activity=discord.Game(name=text))

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

    def _voice_channel_url(self, channel_id: int) -> str:
        guild_id = self.config.discord_guild_id
        if not guild_id:
            return ""
        return f"https://discord.com/channels/{guild_id}/{channel_id}"

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
