#!/usr/bin/env python3
from __future__ import annotations

import base64
import hashlib
import html
import html.parser
import hmac
import json
import os
import re
import secrets
import time
import urllib.error
import urllib.parse
import urllib.request
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any


ROOT = Path(os.environ.get("PULSE_CONSOLE_ROOT", "/opt/sekailink/pulse-console"))
STATIC_ROOT = Path(os.environ.get("PULSE_CONSOLE_STATIC_ROOT", str(ROOT / "static")))
LOG_ROOT = Path(os.environ.get("PULSE_CONSOLE_LOG_ROOT", str(ROOT / "logs")))
AUTH_FILE = Path(os.environ.get("PULSE_CONSOLE_AUTH_FILE", str(ROOT / "etc/auth.json")))
LLAMA_KEY_FILE = Path(os.environ.get("PULSE_LLAMA_API_KEY_FILE", "/opt/sekailink/pulse/etc/llama_api_keys"))

HOST = os.environ.get("PULSE_CONSOLE_HOST", "127.0.0.1")
PORT = int(os.environ.get("PULSE_CONSOLE_PORT", "18183"))
RAG_URL = os.environ.get("PULSE_RAG_URL", "http://127.0.0.1:18182")
LLAMA_URL = os.environ.get("PULSE_LLAMA_URL", "http://127.0.0.1:18181/v1/chat/completions")
MODEL = os.environ.get("PULSE_MODEL", "pulse-qwen2.5-coder-3b")
SERPER_API_KEY_FILE = Path(os.environ.get("PULSE_SERPER_API_KEY_FILE", str(ROOT / "etc/serper_api_key")))
SERPER_URL = os.environ.get("PULSE_SERPER_URL", "https://google.serper.dev/search")
AUTO_WEB_SEARCH = os.environ.get("PULSE_AUTO_WEB_SEARCH", "1").strip().lower() not in {"0", "false", "no"}

MAX_MESSAGE_CHARS = 6000
MAX_CONTEXT_CHARS = 6000
MAX_FILE_CHARS = 160_000
MAX_TOTAL_FILE_CHARS = 220_000
MAX_FILE_COUNT = 6
MAX_WEB_RESULTS = 4
MAX_WEB_FETCHES = 3
MAX_WEB_PAGE_BYTES = 800_000
MAX_WEB_EXCERPT_CHARS = 1400
MAX_WEB_CONTEXT_CHARS = 5200
ALLOWED_FILE_EXTENSIONS = {".yaml", ".yml", ".json", ".txt", ".log", ".spoiler"}
USERNAME_RE = re.compile(r"^[A-Za-z0-9_.-]{2,64}$")
UNSAFE_RE = re.compile(
    r"(/etc/|~/.ssh|authorized_keys|private key|api[_ -]?key|token|secret|password|mot de passe|"
    r"\b(drop table|delete from|insert into|update\s+\w+\s+set)\b|"
    r"\b(ssh|scp|rsync|sudo|su|dnf|yum|apt|systemctl|journalctl|passwd|deploy|deploie|"
    r"reboot|shutdown|firewall|iptables|nftables|mysql|mariadb|rm -rf|chmod|chown)\b)",
    re.IGNORECASE,
)
WEB_SEARCH_HINT_RE = re.compile(
    r"("
    r"\b(cherche|recherche|search|web|internet|source|sources|verifie|v[ée]rifier|confirme|confirm|"
    r"actuel|actuelle|latest|dernier|derniere|r[ée]cent|aujourd'hui|release|version|changelog|"
    r"documentation|docs|guide|setup|installer|installation|wiki|github|site|lien|url)\b|"
    r"https?://|"
    r"\b(c'est quoi|qu'est-ce que|definition|d[ée]finition|explique.*terme|signifie quoi|"
    r"si n[ée]cessaire|au besoin|si besoin)\b|"
    r"\b(keysanity|shopsanity|entrance randomizer|entrance shuffle|boss shuffle|triforce hunt|"
    r"deathlink|death link|apworld|poptracker|universal tracker|bizhawk|sni|lua connector)\b"
    r")",
    re.IGNORECASE,
)
NO_WEB_HINT_RE = re.compile(r"\b(sans web|pas de recherche|ne cherche pas|offline only)\b", re.IGNORECASE)


def utc_stamp() -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


def read_json(path: Path, fallback: Any) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return fallback


def ensure_auth_file() -> None:
    if AUTH_FILE.exists():
        return
    AUTH_FILE.parent.mkdir(parents=True, exist_ok=True)
    password = secrets.token_urlsafe(18)
    salt = secrets.token_hex(16)
    digest = hashlib.pbkdf2_hmac("sha256", password.encode(), salt.encode(), 250_000).hex()
    payload = {
        "users": [
            {
                "username": "jade",
                "salt": salt,
                "password_hash": digest,
                "created_at": utc_stamp(),
            }
        ]
    }
    AUTH_FILE.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    os.chmod(AUTH_FILE, 0o600)
    print(f"Pulse Console generated login: jade / {password}", flush=True)


def verify_password(username: str, password: str) -> bool:
    if not USERNAME_RE.match(username):
        return False
    payload = read_json(AUTH_FILE, {"users": []})
    for user in payload.get("users", []):
        if user.get("username") != username:
            continue
        salt = str(user.get("salt") or "")
        expected = str(user.get("password_hash") or "")
        if not salt or not expected:
            return False
        actual = hashlib.pbkdf2_hmac("sha256", password.encode(), salt.encode(), 250_000).hex()
        return hmac.compare_digest(actual, expected)
    return False


def parse_basic_auth(value: str) -> tuple[str, str] | None:
    if not value.startswith("Basic "):
        return None
    try:
        decoded = base64.b64decode(value[6:], validate=True).decode("utf-8")
        username, password = decoded.split(":", 1)
        return username, password
    except Exception:
        return None


def llama_key() -> str:
    return LLAMA_KEY_FILE.read_text(encoding="utf-8").splitlines()[0].strip()


def serper_key() -> str:
    value = os.environ.get("PULSE_SERPER_API_KEY", "").strip()
    if value:
        return value
    try:
        return SERPER_API_KEY_FILE.read_text(encoding="utf-8").splitlines()[0].strip()
    except Exception:
        return ""


def json_request(
    url: str,
    payload: dict[str, Any],
    timeout: int = 180,
    headers: dict[str, str] | None = None,
) -> tuple[int, dict[str, Any]]:
    data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    request = urllib.request.Request(url, data=data, method="POST")
    request.add_header("Content-Type", "application/json")
    for key, value in (headers or {}).items():
        request.add_header(key, value)
    if "/v1/chat/completions" in url:
        request.add_header("Authorization", f"Bearer {llama_key()}")
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            body = json.loads(response.read().decode("utf-8"))
            return response.status, body
    except urllib.error.HTTPError as exc:
        raw = exc.read().decode("utf-8", errors="replace")
        try:
            return exc.code, json.loads(raw)
        except json.JSONDecodeError:
            return exc.code, {"ok": False, "error": "http_error", "message": raw}


def rag_task(mode: str) -> str:
    if mode == "draft":
        return "/internal/apworld/draft-config"
    if mode == "summary":
        return "/internal/apworld/summarize-game"
    return "/internal/apworld/explain-option"


def system_prompt(mode: str) -> str:
    base = (
        "Tu es Pulse, l'assistante de SekaiLink. Tu reponds en francais clair, "
        "avec prudence, sans inventer de faits. Tu n'executes aucune commande, tu ne demandes aucun secret, "
        "et tu refuses les demandes serveur/SSH/deploiement. Tu peux aider a challenger des idees, "
        "expliquer des concepts randomizer/APWorld/SekaiLink, et formuler des questions utiles. "
        "Quand un contexte web est fourni, utilise-le pour verifier ta reponse, cite les sources utiles "
        "par leur numero, et dis clairement quand une source ne confirme pas assez."
    )
    if mode == "challenge":
        return base + (
            " Structure la reponse en sections courtes: Verdict, Hypotheses, Contre-arguments, Risques, "
            "Tests concrets, Question a poser ensuite."
        )
    if mode == "debug":
        return base + (
            " Structure la reponse en: Symptome, Causes probables, Donnees a collecter, Tests sans risque, "
            "Escalade. Ne propose pas de commande destructive."
        )
    return base + " Reste concise, pratique et utile."


class TextExtractor(html.parser.HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.parts: list[str] = []
        self.skip_depth = 0

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if tag.lower() in {"script", "style", "noscript", "svg"}:
            self.skip_depth += 1

    def handle_endtag(self, tag: str) -> None:
        if tag.lower() in {"script", "style", "noscript", "svg"} and self.skip_depth:
            self.skip_depth -= 1

    def handle_data(self, data: str) -> None:
        if not self.skip_depth:
            text = re.sub(r"\s+", " ", data).strip()
            if text:
                self.parts.append(text)

    def text(self) -> str:
        return html.unescape(" ".join(self.parts))


def should_search_web(message: str, mode: str, scope: str) -> bool:
    if not AUTO_WEB_SEARCH or NO_WEB_HINT_RE.search(message):
        return False
    if mode in {"draft"}:
        return False
    if WEB_SEARCH_HINT_RE.search(message):
        return True
    # APWorld RAG already has local retrieval. Only add web when the prompt asks for freshness/source validation.
    if scope == "apworld":
        return False
    return False


def extract_urls(message: str) -> list[str]:
    urls = re.findall(r"https?://[^\s<>)\"']+", message)
    cleaned: list[str] = []
    for url in urls:
        parsed = urllib.parse.urlparse(url.rstrip(".,;:"))
        if parsed.scheme in {"http", "https"} and parsed.netloc:
            cleaned.append(urllib.parse.urlunparse(parsed))
    return cleaned[:MAX_WEB_RESULTS]


def is_low_value_search_result(url: str) -> bool:
    host = urllib.parse.urlparse(url).netloc.lower()
    return any(
        blocked in host
        for blocked in (
            "youtube.com",
            "youtu.be",
            "tiktok.com",
            "instagram.com",
            "facebook.com",
            "x.com",
            "twitter.com",
        )
    )


def build_search_query(message: str) -> str:
    lowered = message.lower()
    if "keysanity" in lowered:
        return "ALTTPR Archipelago keysanity randomizer definition documentation wiki"
    if "shopsanity" in lowered:
        return "ALTTPR Archipelago shopsanity randomizer definition documentation wiki"
    if "entrance randomizer" in lowered or "entrance shuffle" in lowered:
        return "Archipelago entrance randomizer entrance shuffle definition documentation"
    if "deathlink" in lowered or "death link" in lowered:
        return "Archipelago DeathLink definition documentation"
    return message


def fetch_page_text(url: str) -> tuple[str, str | None]:
    parsed = urllib.parse.urlparse(url)
    if parsed.scheme not in {"http", "https"} or not parsed.netloc:
        return "", "unsupported_url"
    request = urllib.request.Request(url, method="GET")
    request.add_header("User-Agent", "SekaiLinkPulse/0.1 (+https://sekailink.com)")
    try:
        with urllib.request.urlopen(request, timeout=12) as response:
            content_type = response.headers.get("Content-Type", "")
            raw = response.read(MAX_WEB_PAGE_BYTES)
        if "pdf" in content_type.lower():
            return "", "pdf_not_supported"
        decoded = raw.decode("utf-8", errors="replace")
        extractor = TextExtractor()
        extractor.feed(decoded)
        text = re.sub(r"\s+", " ", extractor.text()).strip()
        return text[:MAX_WEB_EXCERPT_CHARS], None
    except Exception as exc:
        return "", str(exc)[:180]


def web_research(message: str) -> dict[str, Any]:
    key = serper_key()
    if not key:
        return {"ok": False, "error": "serper_key_missing", "sources": [], "context": ""}

    direct_urls = extract_urls(message)
    payload = {"q": build_search_query(message), "num": MAX_WEB_RESULTS, "gl": "ca", "hl": "fr"}
    status, data = json_request(SERPER_URL, payload, timeout=20, headers={"X-API-KEY": key})
    if status >= 400:
        return {"ok": False, "error": "serper_error", "details": data, "sources": [], "context": ""}

    candidates: list[dict[str, Any]] = []
    for url in direct_urls:
        candidates.append({"title": url, "link": url, "snippet": "URL fournie par l'utilisateur."})
    organic_results = [item for item in data.get("organic", []) if isinstance(item, dict)]
    for item in organic_results[:10]:
        if not isinstance(item, dict):
            continue
        link = str(item.get("link") or "").strip()
        if not link:
            continue
        if link not in direct_urls and is_low_value_search_result(link):
            continue
        if any(existing.get("link") == link for existing in candidates):
            continue
        candidates.append(
            {
                "title": str(item.get("title") or link).strip(),
                "link": link,
                "snippet": str(item.get("snippet") or "").strip(),
            }
        )
        if len(candidates) >= MAX_WEB_RESULTS:
            break
    if not candidates:
        for item in organic_results[:MAX_WEB_RESULTS]:
            link = str(item.get("link") or "").strip()
            if not link:
                continue
            candidates.append(
                {
                    "title": str(item.get("title") or link).strip(),
                    "link": link,
                    "snippet": str(item.get("snippet") or "").strip(),
                }
            )
    sources: list[dict[str, Any]] = []
    context_parts: list[str] = []
    for idx, item in enumerate(candidates[:MAX_WEB_RESULTS], 1):
        page_text = ""
        error = None
        if len(sources) < MAX_WEB_FETCHES:
            page_text, error = fetch_page_text(str(item["link"]))
        source = {
            "id": idx,
            "title": item["title"],
            "url": item["link"],
            "snippet": item["snippet"],
            "fetched": bool(page_text),
        }
        if error and not page_text:
            source["fetch_error"] = error
        sources.append(source)
        excerpt = page_text or item["snippet"]
        if excerpt:
            context_parts.append(
                f"[Source {idx}] {item['title']}\nURL: {item['link']}\nExtrait: {excerpt[:MAX_WEB_EXCERPT_CHARS]}"
            )
    context = "\n\n".join(context_parts)[:MAX_WEB_CONTEXT_CHARS]
    return {"ok": True, "sources": sources, "context": context}


def append_source_footer(answer: str, sources: list[dict[str, Any]]) -> str:
    if not sources:
        return answer
    lines = ["", "Sources consultees:"]
    for source in sources[:MAX_WEB_RESULTS]:
        fetched = "lu" if source.get("fetched") else "resultat"
        lines.append(f"[{source.get('id')}] {source.get('title')} - {source.get('url')} ({fetched})")
    return answer.rstrip() + "\n" + "\n".join(lines)


def ask_llama(message: str, mode: str, context: str, web_context: str = "") -> dict[str, Any]:
    if web_context:
        context = (context + "\n\n" if context else "") + "WEB RESEARCH CONTEXT:\n" + web_context
    body = {
        "model": MODEL,
        "messages": [
            {"role": "system", "content": system_prompt(mode)},
            {
                "role": "user",
                "content": json.dumps(
                    {
                        "mode": mode,
                        "question": message,
                        "context": context,
                        "expected_style": "console Pulse de travail, directe, challenge-friendly.",
                    },
                    ensure_ascii=False,
                ),
            },
        ],
        "temperature": 0.25 if mode == "challenge" else 0.15,
        "max_tokens": 850,
    }
    status, payload = json_request(LLAMA_URL, body, timeout=240)
    if status >= 400:
        return {"ok": False, "error": payload.get("error") or "llm_error", "details": payload}
    answer = (((payload.get("choices") or [{}])[0].get("message") or {}).get("content") or "").strip()
    try:
        parsed = json.loads(answer)
        if isinstance(parsed, dict) and parsed.get("answer"):
            answer = str(parsed.get("answer") or "").strip()
            return {
                "ok": parsed.get("ok") is not False,
                "answer": answer,
                "source": "pulse-llm",
                "model": MODEL,
                "confidence": parsed.get("confidence"),
                "warnings": parsed.get("warnings") or [],
                "draft_values": parsed.get("draft_values") or {},
            }
    except json.JSONDecodeError:
        pass
    return {"ok": True, "answer": answer, "source": "pulse-llm", "model": MODEL}


def normalize_uploaded_files(raw_files: Any) -> tuple[str, list[dict[str, Any]], list[str]]:
    if not isinstance(raw_files, list):
        return "", [], []
    chunks: list[str] = []
    metadata: list[dict[str, Any]] = []
    warnings: list[str] = []
    total_chars = 0
    for index, entry in enumerate(raw_files[:MAX_FILE_COUNT]):
        if not isinstance(entry, dict):
            continue
        name = Path(str(entry.get("name") or f"file-{index + 1}.txt")).name
        suffix = Path(name).suffix.lower()
        if suffix not in ALLOWED_FILE_EXTENSIONS:
            warnings.append(f"File ignored because extension is not allowed: {name}")
            continue
        content = str(entry.get("content") or "")
        original_chars = len(content)
        if not content.strip():
            warnings.append(f"File ignored because it is empty: {name}")
            continue
        remaining = MAX_TOTAL_FILE_CHARS - total_chars
        if remaining <= 0:
            warnings.append("Additional files ignored because the file context limit was reached.")
            break
        clipped = content[: min(MAX_FILE_CHARS, remaining)]
        if len(clipped) < original_chars:
            warnings.append(f"File truncated for context limit: {name}")
        total_chars += len(clipped)
        metadata.append({"name": name, "chars": original_chars, "used_chars": len(clipped)})
        chunks.append(f"--- BEGIN FILE: {name} ({len(clipped)}/{original_chars} chars) ---\n{clipped}\n--- END FILE: {name} ---")
    return "\n\n".join(chunks), metadata, warnings


def audit(event: dict[str, Any]) -> None:
    LOG_ROOT.mkdir(parents=True, exist_ok=True)
    path = LOG_ROOT / f"pulse-console-{time.strftime('%Y-%m-%d', time.gmtime())}.jsonl"
    event.setdefault("time", utc_stamp())
    with path.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(event, ensure_ascii=False) + "\n")


class Handler(BaseHTTPRequestHandler):
    server_version = "SekaiLinkPulseConsole/0.1"

    def authenticated_user(self) -> str | None:
        parsed = parse_basic_auth(self.headers.get("Authorization", ""))
        if parsed is None:
            return None
        username, password = parsed
        return username if verify_password(username, password) else None

    def require_auth(self) -> str | None:
        username = self.authenticated_user()
        if username:
            return username
        self.send_response(HTTPStatus.UNAUTHORIZED)
        self.send_header("WWW-Authenticate", 'Basic realm="SekaiLink Pulse Console"')
        self.send_header("Content-Length", "0")
        self.end_headers()
        return None

    def send_json(self, code: int, payload: dict[str, Any]) -> None:
        data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self) -> None:
        if self.path == "/health":
            self.send_json(200, {"ok": True, "service": "pulse-console", "time": utc_stamp()})
            return
        if self.path.startswith("/api/"):
            username = self.require_auth()
            if not username:
                return
            if self.path == "/api/status":
                rag_status, rag_payload = 0, {}
                try:
                    with urllib.request.urlopen(f"{RAG_URL}/health", timeout=5) as response:
                        rag_status = response.status
                        rag_payload = json.loads(response.read().decode("utf-8"))
                except Exception as exc:
                    rag_payload = {"ok": False, "error": str(exc)}
                self.send_json(
                    200,
                    {
                        "ok": True,
                        "rag_status": rag_status,
                        "rag": rag_payload,
                        "model": MODEL,
                        "web_search": bool(AUTO_WEB_SEARCH and serper_key()),
                    },
                )
                return
            self.send_json(404, {"ok": False, "error": "not_found"})
            return
        if self.require_auth() is None:
            return
        target = self.path.split("?", 1)[0]
        if target == "/":
            target = "/index.html"
        safe = Path(target.lstrip("/"))
        if ".." in safe.parts:
            self.send_json(400, {"ok": False, "error": "invalid_path"})
            return
        file_path = STATIC_ROOT / safe
        if not file_path.exists() or not file_path.is_file():
            self.send_json(404, {"ok": False, "error": "not_found"})
            return
        content_type = "text/plain; charset=utf-8"
        if file_path.suffix == ".html":
            content_type = "text/html; charset=utf-8"
        elif file_path.suffix == ".css":
            content_type = "text/css; charset=utf-8"
        elif file_path.suffix == ".js":
            content_type = "application/javascript; charset=utf-8"
        elif file_path.suffix == ".png":
            content_type = "image/png"
        data = file_path.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store" if file_path.suffix in {".html", ".js", ".css"} else "public, max-age=3600")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self) -> None:
        username = self.require_auth()
        if not username:
            return
        if self.path != "/api/ask":
            self.send_json(404, {"ok": False, "error": "not_found"})
            return
        try:
            length = min(int(self.headers.get("Content-Length", "0")), 32_000)
            payload = json.loads(self.rfile.read(length).decode("utf-8") or "{}")
        except Exception:
            self.send_json(400, {"ok": False, "error": "invalid_json"})
            return
        message = str(payload.get("message") or "")[:MAX_MESSAGE_CHARS].strip()
        mode = str(payload.get("mode") or "normal").strip().lower()
        scope = str(payload.get("scope") or "pulse").strip().lower()
        game_key = str(payload.get("game_key") or "alttp").strip().lower()
        context = str(payload.get("context") or "")[:MAX_CONTEXT_CHARS].strip()
        file_context, file_metadata, file_warnings = normalize_uploaded_files(payload.get("files"))
        if file_context:
            context = (context + "\n\n" if context else "") + "UPLOADED FILE CONTEXT:\n" + file_context
        if not message:
            self.send_json(400, {"ok": False, "error": "empty_message"})
            return
        if UNSAFE_RE.search(message):
            result = {
                "ok": False,
                "error": "out_of_scope",
                "answer": "Pulse Console refuse les demandes de secrets, SSH, deploiement ou administration systeme.",
                "source": "guardrail",
            }
        elif scope == "apworld":
            web_result = web_research(message) if should_search_web(message, mode, scope) else {"sources": [], "context": ""}
            status, result = json_request(
                f"{RAG_URL}{rag_task(mode)}",
                {
                    "game_key": game_key,
                    "question": message,
                    "user_intent": message,
                    "external_context": web_result.get("context") or "",
                },
                timeout=210,
            )
            result.setdefault("source", "pulse-rag")
            if web_result.get("sources"):
                result["web_sources"] = web_result["sources"]
                if result.get("answer"):
                    result["answer"] = append_source_footer(str(result["answer"]), web_result["sources"])
            if status >= 400:
                result.setdefault("ok", False)
        else:
            web_result = web_research(message) if should_search_web(message, mode, scope) else {"sources": [], "context": ""}
            result = ask_llama(message, mode, context, str(web_result.get("context") or ""))
            if web_result.get("sources"):
                result["web_sources"] = web_result["sources"]
                result["source"] = f"{result.get('source') or 'pulse-llm'}+web"
                if result.get("answer"):
                    result["answer"] = append_source_footer(str(result["answer"]), web_result["sources"])
            elif web_result.get("error"):
                result.setdefault("warnings", [])
                if isinstance(result["warnings"], list):
                    result["warnings"].append(f"Web research unavailable: {web_result.get('error')}")
        if file_metadata:
            result["files"] = file_metadata
        if file_warnings:
            result.setdefault("warnings", [])
            if isinstance(result["warnings"], list):
                result["warnings"].extend(file_warnings)
        audit(
            {
                "actor": username,
                "mode": mode,
                "scope": scope,
                "game_key": game_key,
                "question_chars": len(message),
                "question_preview": message[:240],
                "file_count": len(file_metadata),
                "file_names": [item["name"] for item in file_metadata],
                "ok": result.get("ok"),
                "source": result.get("source"),
                "web_source_count": len(result.get("web_sources") or []),
                "error": result.get("error"),
            }
        )
        self.send_json(200 if result.get("ok") is not False else 400, result)

    def log_message(self, fmt: str, *args: Any) -> None:
        return


def main() -> None:
    ensure_auth_file()
    LOG_ROOT.mkdir(parents=True, exist_ok=True)
    httpd = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"Pulse Console listening on {HOST}:{PORT}", flush=True)
    httpd.serve_forever()


if __name__ == "__main__":
    main()
