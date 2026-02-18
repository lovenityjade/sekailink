from __future__ import annotations

import datetime as dt
import json
import logging
import os
import sqlite3
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import httpx
import numpy as np
from fastembed import TextEmbedding

from .config import ServiceConfig


logger = logging.getLogger("sekailink.ai")


def _utc_now() -> str:
    return dt.datetime.utcnow().replace(microsecond=0).isoformat() + "Z"


@dataclass(slots=True)
class MemoryHit:
    score: float
    source: str
    title: str
    text: str


class SemanticMemory:
    def __init__(self, db_path: str, embed_model: str) -> None:
        self.db_path = db_path
        self.embedder = TextEmbedding(model_name=embed_model)
        Path(db_path).parent.mkdir(parents=True, exist_ok=True)
        self._init_db()

    def _connect(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        return conn

    def _init_db(self) -> None:
        with self._connect() as conn:
            conn.executescript(
                """
                CREATE TABLE IF NOT EXISTS memory_chunks (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    source TEXT NOT NULL,
                    title TEXT NOT NULL,
                    text TEXT NOT NULL,
                    vector BLOB NOT NULL,
                    norm REAL NOT NULL,
                    created_at TEXT NOT NULL
                );
                CREATE INDEX IF NOT EXISTS idx_memory_chunks_source ON memory_chunks(source);
                """
            )
            conn.commit()

    @staticmethod
    def _chunk_text(text: str, max_chars: int = 900, overlap: int = 140) -> list[str]:
        clean = (text or "").strip()
        if not clean:
            return []
        chunks: list[str] = []
        start = 0
        while start < len(clean):
            end = min(len(clean), start + max_chars)
            chunk = clean[start:end].strip()
            if chunk:
                chunks.append(chunk)
            if end >= len(clean):
                break
            start = max(0, end - overlap)
        return chunks

    def _embed_many(self, texts: Iterable[str]) -> list[np.ndarray]:
        vectors = list(self.embedder.embed(list(texts)))
        return [np.array(v, dtype=np.float32) for v in vectors]

    def upsert_document(self, source: str, title: str, text: str) -> int:
        chunks = self._chunk_text(text)
        if not chunks:
            return 0
        vectors = self._embed_many(chunks)
        now = _utc_now()
        with self._connect() as conn:
            conn.execute("DELETE FROM memory_chunks WHERE source = ?", (source,))
            for chunk, vec in zip(chunks, vectors):
                norm = float(np.linalg.norm(vec)) or 1.0
                conn.execute(
                    "INSERT INTO memory_chunks(source,title,text,vector,norm,created_at) VALUES(?,?,?,?,?,?)",
                    (source, title, chunk, vec.tobytes(), norm, now),
                )
            conn.commit()
        return len(chunks)

    def query(self, text: str, top_k: int = 8) -> list[MemoryHit]:
        query_vec = self._embed_many([text])[0]
        qnorm = float(np.linalg.norm(query_vec)) or 1.0
        with self._connect() as conn:
            rows = conn.execute("SELECT source,title,text,vector,norm FROM memory_chunks").fetchall()
        hits: list[MemoryHit] = []
        for row in rows:
            vec = np.frombuffer(row["vector"], dtype=np.float32)
            denom = (row["norm"] or 1.0) * qnorm
            score = float(np.dot(vec, query_vec) / denom) if denom else 0.0
            hits.append(MemoryHit(score=score, source=row["source"], title=row["title"], text=row["text"]))
        hits.sort(key=lambda h: h.score, reverse=True)
        return hits[:top_k]

    def count(self) -> int:
        with self._connect() as conn:
            row = conn.execute("SELECT COUNT(*) AS c FROM memory_chunks").fetchone()
            return int(row["c"]) if row else 0


class AIWriter:
    def __init__(self, config: ServiceConfig) -> None:
        self.config = config
        self.memory = SemanticMemory(config.ai_memory_db_path, config.ai_memory_embedding_model)
        self._seed_default_memory()

    @property
    def enabled(self) -> bool:
        return bool(self.config.ai_enabled)

    def _seed_default_memory(self) -> None:
        if self.memory.count() > 0:
            return
        base_brief = f"""
Project: {self.config.ai_project_name}
Role: community assistant writer for announcements, changelogs and FAQ drafts.
Infrastructure: hosted on VPS {self.config.ai_vps_hostname} ({self.config.ai_vps_ip}).
Audience: SekaiLink players/community.
Tone: hype, friendly, clear, concise, non-toxic, never overpromise.
Rules:
- Keep production-ready wording.
- Highlight practical impact for players.
- Prefer short sections and bullets.
- Never leak secrets/tokens/internal credentials.
- When uncertain, say assumption explicitly.
"""
        self.memory.upsert_document(
            source="internal:sekailink_project_brief",
            title="SekaiLink Project Brief",
            text=base_brief.strip(),
        )

    def ingest_file(self, path: str, source_prefix: str = "file") -> int:
        p = Path(path)
        text = p.read_text(encoding="utf-8", errors="ignore")
        source = f"{source_prefix}:{p.resolve()}"
        return self.memory.upsert_document(source=source, title=p.name, text=text)

    def ingest_directory(self, directory: str, globs: tuple[str, ...] = ("*.md", "*.txt", "*.json")) -> int:
        total = 0
        root = Path(directory)
        if not root.exists():
            return total
        for pattern in globs:
            for path in root.rglob(pattern):
                if path.is_file():
                    try:
                        total += self.ingest_file(str(path))
                    except Exception:
                        logger.exception("ai memory ingestion failed for %s", path)
        return total

    async def _chat(self, system_prompt: str, user_prompt: str, max_tokens: int | None = None) -> str:
        headers = {"Content-Type": "application/json"}
        if self.config.ai_api_key:
            headers["Authorization"] = f"Bearer {self.config.ai_api_key}"
        payload = {
            "model": self.config.ai_model,
            "temperature": self.config.ai_temperature,
            "max_tokens": max_tokens or self.config.ai_max_tokens,
            "messages": [
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt},
            ],
        }
        async with httpx.AsyncClient(timeout=90) as client:
            resp = await client.post(f"{self.config.ai_base_url.rstrip('/')}/v1/chat/completions", headers=headers, json=payload)
            resp.raise_for_status()
            data = resp.json()
        return str(data["choices"][0]["message"]["content"]).strip()

    def _build_context(self, query: str, limit: int = 6) -> str:
        hits = self.memory.query(query, top_k=limit)
        if not hits:
            return "No memory context available."
        lines: list[str] = []
        for idx, hit in enumerate(hits, start=1):
            lines.append(f"[{idx}] ({hit.source}) {hit.title}\n{hit.text}")
        return "\n\n".join(lines)

    async def draft_announcement(self, request_text: str, language: str = "fr") -> str:
        context = self._build_context(request_text)
        system = (
            "You are SekaiLink Writer. "
            "Write polished Discord announcements in a hype, friendly tone. "
            "Keep structure clear and concise, include practical player impact and next steps. "
            "Always write in the requested language only. Limit emojis to 0-2 max."
        )
        user = (
            f"Language: {language}\n"
            f"Task: Draft a Discord announcement.\n"
            f"User request: {request_text}\n\n"
            f"Project memory context:\n{context}\n\n"
            "Output format:\n"
            "1) Title line\n2) Short intro\n3) Bullet highlights\n4) Optional CTA"
        )
        return await self._chat(system, user, max_tokens=700)

    async def draft_changelog(self, version: str, changes: list[str], language: str = "fr") -> str:
        prompt = f"Version: {version}\nChanges:\n- " + "\n- ".join(changes)
        context = self._build_context(prompt)
        system = (
            "You are SekaiLink Writer. Generate release-grade changelogs for Discord and web posts. "
            "Tone: clear, friendly, practical, hype but not exaggerated."
        )
        user = (
            f"Language: {language}\n"
            f"Task: Draft changelog from raw notes.\n{prompt}\n\n"
            f"Project memory context:\n{context}\n\n"
            "Output sections:\n- TL;DR\n- Added\n- Improved\n- Fixed\n- Notes"
        )
        return await self._chat(system, user, max_tokens=900)

    async def draft_faq_update(self, request_text: str, language: str = "fr") -> str:
        context = self._build_context(request_text)
        system = "You are SekaiLink Writer. Draft clean FAQ entries with short direct answers."
        user = (
            f"Language: {language}\nTask: Draft FAQ update.\nRequest: {request_text}\n\n"
            f"Project memory context:\n{context}\n\n"
            "Output:\n- Question\n- Answer\n- Optional extra tip"
        )
        return await self._chat(system, user, max_tokens=600)

    def stats(self) -> dict:
        return {
            "enabled": self.enabled,
            "model": self.config.ai_model,
            "base_url": self.config.ai_base_url,
            "memory_chunks": self.memory.count(),
            "memory_db_path": os.path.abspath(self.config.ai_memory_db_path),
        }
