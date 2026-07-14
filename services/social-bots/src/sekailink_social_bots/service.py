from __future__ import annotations

import asyncio
from dataclasses import dataclass

from .ai_writer import AIWriter
from .config import ServiceConfig
from .discord_bot import DiscordBotManager
from .twitch_bot import TwitchBotManager


@dataclass(slots=True)
class BotService:
    config: ServiceConfig
    discord: DiscordBotManager
    twitch: TwitchBotManager
    ai: AIWriter

    @classmethod
    def create(cls, config: ServiceConfig) -> "BotService":
        ai = AIWriter(config)
        return cls(
            config=config,
            discord=DiscordBotManager(config, ai=ai),
            twitch=TwitchBotManager(config),
            ai=ai,
        )

    async def start(self) -> None:
        await asyncio.gather(self.discord.start(), self.twitch.start())

    async def stop(self) -> None:
        await asyncio.gather(self.discord.stop(), self.twitch.stop())
