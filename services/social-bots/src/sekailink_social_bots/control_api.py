from __future__ import annotations

from fastapi import Depends, FastAPI, HTTPException

from .models import (
    AIDraftAnnouncementRequest,
    AIDraftChangelogRequest,
    AIDraftFaqRequest,
    AIMemoryIngestRequest,
    DiscordAnnouncementRequest,
    DiscordFaqSyncRequest,
    DiscordLobbyVoiceCreateRequest,
    DiscordLobbyVoiceDeleteRequest,
    DiscordStatusRequest,
    DiscordTempVoiceRequest,
    HealthResponse,
    TwitchMessageRequest,
)
from .security import build_auth_dependency
from .service import BotService



def create_app(service: BotService) -> FastAPI:
    app = FastAPI(title="SekaiLink Social Bots", version="0.1.0")
    auth = build_auth_dependency(service.config.control_api_key)

    @app.on_event("startup")
    async def _startup() -> None:
        await service.start()

    @app.on_event("shutdown")
    async def _shutdown() -> None:
        await service.stop()

    @app.get("/health", response_model=HealthResponse)
    async def health() -> HealthResponse:
        return HealthResponse(
            ok=True,
            discord_enabled=service.discord.enabled,
            discord_ready=service.discord.state.ready,
            twitch_enabled=service.twitch.enabled,
            twitch_ready=service.twitch.ready,
        )

    @app.post("/discord/announce", dependencies=[Depends(auth)])
    async def discord_announce(payload: DiscordAnnouncementRequest) -> dict:
        try:
            await service.discord.announce(
                message=payload.message,
                title=payload.title,
                channel_id=payload.channel_id,
                mention_everyone=payload.mention_everyone,
            )
        except Exception as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        return {"ok": True}

    @app.post("/discord/faq/sync", dependencies=[Depends(auth)])
    async def discord_faq_sync(payload: DiscordFaqSyncRequest) -> dict:
        try:
            await service.discord.sync_faq(entries=payload.entries, channel_id=payload.channel_id)
        except Exception as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        return {"ok": True, "count": len(payload.entries)}

    @app.post("/discord/temp-voice/create", dependencies=[Depends(auth)])
    async def discord_temp_voice_create(payload: DiscordTempVoiceRequest) -> dict:
        try:
            channel_id = await service.discord.create_temp_voice(
                name=payload.name,
                user_id=payload.user_id,
                guild_id=payload.guild_id,
                category_id=payload.category_id,
            )
        except Exception as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        return {"ok": True, "channel_id": channel_id}

    @app.post("/discord/lobby-voice/create", dependencies=[Depends(auth)])
    async def discord_lobby_voice_create(payload: DiscordLobbyVoiceCreateRequest) -> dict:
        try:
            result = await service.discord.create_lobby_voice(
                lobby_id=payload.lobby_id,
                lobby_name=payload.lobby_name,
                guild_id=payload.guild_id,
                category_id=payload.category_id,
            )
        except Exception as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        return {"ok": True, **result}

    @app.post("/discord/lobby-voice/delete", dependencies=[Depends(auth)])
    async def discord_lobby_voice_delete(payload: DiscordLobbyVoiceDeleteRequest) -> dict:
        try:
            deleted = await service.discord.delete_lobby_voice(payload.lobby_id)
        except Exception as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        return {"ok": True, "deleted": bool(deleted)}

    @app.get("/discord/lobby-voice/{lobby_id}", dependencies=[Depends(auth)])
    async def discord_lobby_voice_get(lobby_id: str) -> dict:
        try:
            result = service.discord.get_lobby_voice(lobby_id) or {}
        except Exception as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        return {"ok": True, **result}

    @app.post("/discord/status", dependencies=[Depends(auth)])
    async def discord_status(payload: DiscordStatusRequest) -> dict:
        try:
            await service.discord.set_server_status(payload.status)
        except Exception as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        return {"ok": True}

    @app.post("/twitch/message", dependencies=[Depends(auth)])
    async def twitch_message(payload: TwitchMessageRequest) -> dict:
        try:
            await service.twitch.send_message(message=payload.message, channel=payload.channel)
        except Exception as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        return {"ok": True}

    @app.get("/ai/status", dependencies=[Depends(auth)])
    async def ai_status() -> dict:
        return {"ok": True, **service.ai.stats()}

    @app.post("/ai/memory/ingest", dependencies=[Depends(auth)])
    async def ai_memory_ingest(payload: AIMemoryIngestRequest) -> dict:
        count = 0
        try:
            if payload.path:
                count += service.ai.ingest_file(payload.path, source_prefix=payload.source_prefix)
            if payload.directory:
                count += service.ai.ingest_directory(payload.directory)
        except Exception as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        return {"ok": True, "ingested_chunks": count, **service.ai.stats()}

    @app.post("/ai/draft/announcement", dependencies=[Depends(auth)])
    async def ai_draft_announcement(payload: AIDraftAnnouncementRequest) -> dict:
        try:
            text = await service.ai.draft_announcement(payload.request, language=payload.language)
            if payload.post_to_discord:
                await service.discord.announce(
                    message=text,
                    title=payload.title or "SekaiLink Announcement Draft",
                    channel_id=payload.channel_id,
                    mention_everyone=False,
                )
        except Exception as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        return {"ok": True, "draft": text}

    @app.post("/ai/draft/changelog", dependencies=[Depends(auth)])
    async def ai_draft_changelog(payload: AIDraftChangelogRequest) -> dict:
        try:
            text = await service.ai.draft_changelog(payload.version, payload.changes, language=payload.language)
            if payload.post_to_discord:
                await service.discord.announce(
                    message=text,
                    title=payload.title or f"SekaiLink Changelog {payload.version}",
                    channel_id=payload.channel_id,
                    mention_everyone=False,
                )
        except Exception as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        return {"ok": True, "draft": text}

    @app.post("/ai/draft/faq", dependencies=[Depends(auth)])
    async def ai_draft_faq(payload: AIDraftFaqRequest) -> dict:
        try:
            text = await service.ai.draft_faq_update(payload.request, language=payload.language)
        except Exception as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        return {"ok": True, "draft": text}

    return app
