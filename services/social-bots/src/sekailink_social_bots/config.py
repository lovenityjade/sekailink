from __future__ import annotations

import os
from dataclasses import dataclass

from dotenv import load_dotenv


load_dotenv()


def _as_bool(value: str | None, default: bool = False) -> bool:
    if value is None:
        return default
    return value.strip().lower() in {"1", "true", "yes", "on"}


def _as_int(value: str | None) -> int | None:
    if not value:
        return None
    try:
        return int(value)
    except ValueError:
        return None


@dataclass(slots=True)
class ServiceConfig:
    control_api_key: str
    host: str
    port: int
    log_level: str

    discord_enabled: bool
    discord_token: str
    discord_guild_id: int | None
    discord_announce_channel_id: int | None
    discord_faq_channel_id: int | None
    discord_temp_voice_category_id: int | None
    discord_temp_voice_lobby_channel_id: int | None
    discord_temp_voice_prefix: str

    twitch_enabled: bool
    twitch_bot_nick: str
    twitch_token: str
    twitch_client_id: str
    twitch_client_secret: str
    twitch_default_channel: str

    ai_enabled: bool
    ai_base_url: str
    ai_model: str
    ai_api_key: str
    ai_temperature: float
    ai_max_tokens: int
    ai_memory_db_path: str
    ai_memory_embedding_model: str
    ai_project_name: str
    ai_vps_hostname: str
    ai_vps_ip: str



def load_config() -> ServiceConfig:
    return ServiceConfig(
        control_api_key=os.getenv("SEKAILINK_BOT_CONTROL_API_KEY", ""),
        host=os.getenv("SEKAILINK_BOT_HOST", "127.0.0.1"),
        port=int(os.getenv("SEKAILINK_BOT_PORT", "8095")),
        log_level=os.getenv("SEKAILINK_BOT_LOG_LEVEL", "INFO"),
        discord_enabled=_as_bool(os.getenv("SEKAILINK_DISCORD_ENABLED"), True),
        discord_token=os.getenv("SEKAILINK_DISCORD_TOKEN", ""),
        discord_guild_id=_as_int(os.getenv("SEKAILINK_DISCORD_GUILD_ID")),
        discord_announce_channel_id=_as_int(os.getenv("SEKAILINK_DISCORD_ANNOUNCE_CHANNEL_ID")),
        discord_faq_channel_id=_as_int(os.getenv("SEKAILINK_DISCORD_FAQ_CHANNEL_ID")),
        discord_temp_voice_category_id=_as_int(
            os.getenv("SEKAILINK_DISCORD_TEMP_VOICE_CATEGORY_ID", "1456949966789677118")
        ),
        discord_temp_voice_lobby_channel_id=_as_int(os.getenv("SEKAILINK_DISCORD_TEMP_VOICE_LOBBY_CHANNEL_ID")),
        discord_temp_voice_prefix=os.getenv("SEKAILINK_DISCORD_TEMP_VOICE_PREFIX", "SekaiLink"),
        twitch_enabled=_as_bool(os.getenv("SEKAILINK_TWITCH_ENABLED"), False),
        twitch_bot_nick=os.getenv("SEKAILINK_TWITCH_BOT_NICK", ""),
        twitch_token=os.getenv("SEKAILINK_TWITCH_TOKEN", ""),
        twitch_client_id=os.getenv("SEKAILINK_TWITCH_CLIENT_ID", ""),
        twitch_client_secret=os.getenv("SEKAILINK_TWITCH_CLIENT_SECRET", ""),
        twitch_default_channel=os.getenv("SEKAILINK_TWITCH_DEFAULT_CHANNEL", ""),
        ai_enabled=_as_bool(os.getenv("SEKAILINK_AI_ENABLED"), True),
        ai_base_url=os.getenv("SEKAILINK_AI_BASE_URL", "http://127.0.0.1:8096"),
        ai_model=os.getenv("SEKAILINK_AI_MODEL", "qwen2.5-3b-instruct-q4km"),
        ai_api_key=os.getenv("SEKAILINK_AI_API_KEY", ""),
        ai_temperature=float(os.getenv("SEKAILINK_AI_TEMPERATURE", "0.65")),
        ai_max_tokens=int(os.getenv("SEKAILINK_AI_MAX_TOKENS", "650")),
        ai_memory_db_path=os.getenv("SEKAILINK_AI_MEMORY_DB_PATH", "./data/sekailink-ai-memory.sqlite3"),
        ai_memory_embedding_model=os.getenv("SEKAILINK_AI_MEMORY_EMBED_MODEL", "intfloat/multilingual-e5-small"),
        ai_project_name=os.getenv("SEKAILINK_AI_PROJECT_NAME", "SekaiLink"),
        ai_vps_hostname=os.getenv("SEKAILINK_AI_VPS_HOSTNAME", "topaz-sekailink.com"),
        ai_vps_ip=os.getenv("SEKAILINK_AI_VPS_IP", "72.61.6.82"),
    )
