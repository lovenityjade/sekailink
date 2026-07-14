from __future__ import annotations

import asyncio
import logging

from twitchio.ext import commands

from .config import ServiceConfig


logger = logging.getLogger("sekailink.twitch")


class SekaiTwitchBot(commands.Bot):
    def __init__(self, manager: "TwitchBotManager") -> None:
        initial_channels = [manager.config.twitch_default_channel] if manager.config.twitch_default_channel else []
        super().__init__(
            token=manager.config.twitch_token,
            prefix="!",
            initial_channels=initial_channels,
            nick=manager.config.twitch_bot_nick,
            client_id=manager.config.twitch_client_id or None,
            client_secret=manager.config.twitch_client_secret or None,
        )
        self.manager = manager

    async def event_ready(self) -> None:
        self.manager.ready = True
        logger.info("Twitch bot connected as %s", self.nick)

    @commands.command(name="sekai")
    async def sekai_status(self, ctx: commands.Context) -> None:
        await ctx.send("SekaiLink is online.")


class TwitchBotManager:
    def __init__(self, config: ServiceConfig) -> None:
        self.config = config
        self.ready = False
        self.bot = SekaiTwitchBot(self)
        self._task: asyncio.Task | None = None

    @property
    def enabled(self) -> bool:
        return (
            self.config.twitch_enabled
            and bool(self.config.twitch_token)
            and bool(self.config.twitch_bot_nick)
        )

    async def start(self) -> None:
        if not self.enabled:
            logger.info("Twitch bot disabled or missing credentials.")
            return
        if self._task and not self._task.done():
            return
        self._task = asyncio.create_task(self.bot.start(), name="twitch-bot")

    async def stop(self) -> None:
        if self._task and not self._task.done():
            self._task.cancel()

    async def send_message(self, message: str, channel: str | None = None) -> None:
        target = channel or self.config.twitch_default_channel
        if not target:
            raise RuntimeError("twitch_target_channel_missing")
        normalized = target.lstrip("#")
        if not self.ready:
            raise RuntimeError("twitch_bot_not_ready")

        chan = self.bot.get_channel(normalized)
        if chan is None:
            raise RuntimeError("twitch_channel_not_joined")
        await chan.send(message)
