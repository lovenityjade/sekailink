from __future__ import annotations

from pydantic import BaseModel, Field


class HealthResponse(BaseModel):
    ok: bool
    discord_enabled: bool
    discord_ready: bool
    twitch_enabled: bool
    twitch_ready: bool


class DiscordAnnouncementRequest(BaseModel):
    channel_id: int | None = None
    title: str | None = None
    message: str = Field(min_length=1, max_length=1900)
    mention_everyone: bool = False


class FaqEntry(BaseModel):
    question: str = Field(min_length=1, max_length=256)
    answer: str = Field(min_length=1, max_length=1800)


class DiscordFaqSyncRequest(BaseModel):
    channel_id: int | None = None
    entries: list[FaqEntry] = Field(default_factory=list)


class DiscordTempVoiceRequest(BaseModel):
    guild_id: int | None = None
    category_id: int | None = None
    name: str = Field(min_length=1, max_length=100)
    user_id: int | None = None


class DiscordLobbyVoiceCreateRequest(BaseModel):
    lobby_id: str = Field(min_length=3, max_length=80)
    lobby_name: str = Field(min_length=1, max_length=100)
    guild_id: int | None = None
    category_id: int | None = None
    member_ids: list[int] = Field(default_factory=list)


class DiscordLobbyVoiceDeleteRequest(BaseModel):
    lobby_id: str = Field(min_length=3, max_length=80)


class DiscordLobbyVoiceGrantRequest(BaseModel):
    lobby_id: str = Field(min_length=3, max_length=80)
    member_id: int


class DiscordStatusRequest(BaseModel):
    status: str = Field(min_length=2, max_length=32)


class DiscordBugReportRequest(BaseModel):
    channel_id: int | None = None
    title: str = Field(min_length=3, max_length=100)
    description: str = Field(min_length=1, max_length=200)
    reporter_name: str = Field(min_length=1, max_length=80)
    screenshot_base64: str | None = None
    logs_text: str | None = None
    system_info: dict = Field(default_factory=dict)
    app_info: dict = Field(default_factory=dict)
    bundle_base64: str | None = None
    bundle_manifest: dict = Field(default_factory=dict)


class DiscordBugReportExportRequest(BaseModel):
    statuses: list[str] = Field(default_factory=list)
    include_attachments: bool = True
    max_threads: int = Field(default=500, ge=1, le=2000)


class TwitchMessageRequest(BaseModel):
    channel: str | None = None
    message: str = Field(min_length=1, max_length=350)


class AIDraftAnnouncementRequest(BaseModel):
    request: str = Field(min_length=4, max_length=3000)
    language: str = Field(default="fr", min_length=2, max_length=8)
    post_to_discord: bool = False
    channel_id: int | None = None
    title: str | None = Field(default=None, max_length=120)


class AIDraftChangelogRequest(BaseModel):
    version: str = Field(min_length=1, max_length=80)
    changes: list[str] = Field(default_factory=list)
    language: str = Field(default="fr", min_length=2, max_length=8)
    post_to_discord: bool = False
    channel_id: int | None = None
    title: str | None = Field(default=None, max_length=120)


class AIDraftFaqRequest(BaseModel):
    request: str = Field(min_length=4, max_length=3000)
    language: str = Field(default="fr", min_length=2, max_length=8)


class AIMemoryIngestRequest(BaseModel):
    path: str | None = None
    directory: str | None = None
    source_prefix: str = "file"
