import datetime
import json
import os
import secrets
import time
import re
import warnings
import shutil
import subprocess
import urllib.parse
import urllib.request
import urllib.error
import uuid
import smtplib
import ssl
import yaml
import hmac
import hashlib
import hmac
import hashlib
from enum import StrEnum
from email.message import EmailMessage
from typing import Any, IO, Dict, Iterator, List, Tuple, Union

import jinja2.exceptions
from flask import request, redirect, url_for, render_template, Response, session, abort, send_from_directory, jsonify
from pony.orm import count, commit, db_session, desc, select
from werkzeug.utils import secure_filename
from werkzeug.security import check_password_hash, generate_password_hash
from Utils import __version__

from worlds.AutoWorld import AutoWorldRegister, World
from . import app, cache, to_url, socketio
from .markdown import render_markdown
from .models import Seed, Room, Command, UUID, uuid4, User, UserYaml, Lobby, LobbyMember, LobbyMessage, LobbyGeneration, SupportTicket, FriendRequest, Friendship, DirectMessage, DesktopToken, DesktopOAuthState, AuthIdentity
from Utils import title_sorted


def _get_discord_oauth_config() -> dict[str, str]:
    client_id = app.config.get("DISCORD_CLIENT_ID")
    client_secret = app.config.get("DISCORD_CLIENT_SECRET")
    redirect_uri = app.config.get("DISCORD_REDIRECT_URI")
    scopes = app.config.get("DISCORD_OAUTH_SCOPES", "identify").strip()
    permissions = app.config.get("DISCORD_OAUTH_PERMISSIONS", "0")
    integration_type = app.config.get("DISCORD_OAUTH_INTEGRATION_TYPE", "0")
    if not client_id or not client_secret or not redirect_uri:
        raise RuntimeError("Discord OAuth config is missing.")
    return {
        "client_id": client_id,
        "client_secret": client_secret,
        "redirect_uri": redirect_uri,
        "scopes": scopes,
        "permissions": permissions,
        "integration_type": integration_type,
    }


def _get_patreon_oauth_config() -> dict[str, str]:
    client_id = app.config.get("PATREON_CLIENT_ID")
    client_secret = app.config.get("PATREON_CLIENT_SECRET")
    redirect_uri = app.config.get("PATREON_REDIRECT_URI")
    scopes = app.config.get("PATREON_OAUTH_SCOPES", "identity identity[email] memberships").strip()
    if not client_id or not client_secret or not redirect_uri:
        raise RuntimeError("Patreon OAuth config is missing.")
    return {
        "client_id": client_id,
        "client_secret": client_secret,
        "redirect_uri": redirect_uri,
        "scopes": scopes,
    }


def _get_patreon_oauth_config() -> dict[str, str]:
    client_id = app.config.get("PATREON_CLIENT_ID")
    client_secret = app.config.get("PATREON_CLIENT_SECRET")
    redirect_uri = app.config.get("PATREON_REDIRECT_URI")
    scopes = app.config.get("PATREON_OAUTH_SCOPES", "identity identity[email] memberships").strip()
    if not client_id or not client_secret or not redirect_uri:
        raise RuntimeError("Patreon OAuth config is missing.")
    return {
        "client_id": client_id,
        "client_secret": client_secret,
        "redirect_uri": redirect_uri,
        "scopes": scopes,
    }


def _get_discord_desktop_oauth_config() -> dict[str, str]:
    client_id = app.config.get("DISCORD_CLIENT_ID")
    client_secret = app.config.get("DISCORD_CLIENT_SECRET")
    redirect_uri = app.config.get("DISCORD_DESKTOP_REDIRECT_URI", "https://sekailink.com/api/auth/desktop-redirect")
    scopes = app.config.get("DISCORD_OAUTH_SCOPES", "identify").strip()
    permissions = app.config.get("DISCORD_OAUTH_PERMISSIONS", "0")
    integration_type = app.config.get("DISCORD_OAUTH_INTEGRATION_TYPE", "0")
    if not client_id or not client_secret or not redirect_uri:
        raise RuntimeError("Discord OAuth config is missing.")
    return {
        "client_id": client_id,
        "client_secret": client_secret,
        "redirect_uri": redirect_uri,
        "scopes": scopes,
        "permissions": permissions,
        "integration_type": integration_type,
    }


def _desktop_oauth_state_ttl_seconds() -> int:
    return 10 * 60


def _desktop_oauth_store_state(state: str) -> None:
    expires_at = datetime.datetime.utcnow() + datetime.timedelta(seconds=_desktop_oauth_state_ttl_seconds())
    with db_session:
        DesktopOAuthState(state=state, expires_at=expires_at)
        commit()


def _desktop_oauth_validate_and_consume_state(state: str) -> bool:
    if not state:
        return False
    with db_session:
        entry = DesktopOAuthState.get(state=state)
        if not entry:
            return False
        now = datetime.datetime.utcnow()
        if entry.expires_at <= now:
            entry.delete()
            commit()
            return False
        entry.delete()
        commit()
        return True


def _extract_desktop_callback_scheme(state: str) -> str:
    raw = str(state or "").strip().lower()
    prefix = raw.split(":", 1)[0] if raw else ""
    if prefix in {"sekailink", "sekailink-admin"}:
        return prefix
    return "sekailink"


def _desktop_token_ttl_seconds() -> int:
    return int(app.config.get("DESKTOP_TOKEN_TTL_SECONDS", 30 * 24 * 60 * 60))


def _create_desktop_token(discord_id: str) -> tuple[str, datetime.datetime]:
    token = secrets.token_urlsafe(48)
    expires_at = datetime.datetime.utcnow() + datetime.timedelta(seconds=_desktop_token_ttl_seconds())
    with db_session:
        user = User.get(discord_id=discord_id)
        if not user:
            raise ValueError("User not found for desktop token")
        DesktopToken(token=token, user=user, expires_at=expires_at)
        commit()
    return token, expires_at


def _desktop_token_auth_context(token: str) -> dict[str, Any] | None:
    if not token:
        return None
    with db_session:
        entry = DesktopToken.get(token=token)
        if not entry:
            return None
        now = datetime.datetime.utcnow()
        if entry.expires_at <= now:
            entry.delete()
            commit()
            return None
        entry.last_used_at = now
        user = entry.user
        if not user:
            return None
        context = {
            "id": user.discord_id,
            "username": user.username,
            "global_name": user.global_name,
            "avatar": user.avatar,
            "email": user.email,
            "role": user.role,
            "banned": user.banned,
            "ban_reason": user.ban_reason,
            "terms_version": user.terms_version,
            "terms_accepted": user.terms_accepted,
        }
        commit()
    return context


def _get_bearer_token() -> str | None:
    auth = request.headers.get("Authorization", "")
    token = None
    if auth:
        parts = auth.split(None, 1)
        if len(parts) == 2 and parts[0].lower() == "bearer":
            token = parts[1].strip() or None
    if not token:
        token = (request.args.get("token") or request.args.get("desktop_token") or "").strip() or None
    return token


_CRITICAL_LOBBY_ACTIONS = {
    "join",
    "active-yaml",
    "ready",
    "generate",
    "settings",
    "close",
    "transfer-host",
    "vote-host",
    "release",
    "kick",
}


def _is_critical_client_endpoint() -> bool:
    method = (request.method or "").upper()
    if method not in {"POST", "PUT", "PATCH", "DELETE"}:
        return False
    path = request.path or ""

    if path in {"/uploads", "/generate", "/api/generate", "/api/yamls/new", "/api/yamls/import"}:
        return True

    if re.match(r"^/api/player-options/[^/]+/dashboard-save$", path):
        return True

    if path.startswith("/api/lobbies/"):
        action = path.rsplit("/", 1)[-1]
        if action in _CRITICAL_LOBBY_ACTIONS:
            return True

    if path.startswith("/api/yamls/"):
        return True

    return False


def _looks_like_desktop_runtime_request() -> bool:
    ua = (request.headers.get("User-Agent") or "").lower()
    origin = (request.headers.get("Origin") or "").lower()
    is_electron = "electron/" in ua
    origin_ok = (not origin) or origin == "null" or origin.startswith("file://") or origin.startswith("app://")
    return is_electron and origin_ok


def _has_valid_sekailink_client_key() -> bool:
    expected = str(app.config.get("SEKAILINK_CLIENT_API_KEY", "") or "").strip()
    if not expected:
        return False
    provided = (request.headers.get("X-SekaiLink-Client-Key") or "").strip()
    if not provided:
        return False
    return hmac.compare_digest(expected, provided)


def _is_authorized_sekailink_client_request() -> bool:
    client_hint = (request.headers.get("X-SekaiLink-Client") or "").strip().lower()
    if client_hint in {"desktop", "sekailink-client"}:
        return True
    if _has_valid_sekailink_client_key():
        return True
    return _looks_like_desktop_runtime_request()


@app.before_request
def _handle_cors_preflight():
    if request.method == "OPTIONS" and request.path.startswith("/api/"):
        resp = app.make_response("")
        origin = request.headers.get("Origin")
        if origin:
            resp.headers["Access-Control-Allow-Origin"] = origin
            resp.headers["Vary"] = "Origin"
            resp.headers["Access-Control-Allow-Credentials"] = "true"
        else:
            resp.headers["Access-Control-Allow-Origin"] = "*"
        resp.headers["Access-Control-Allow-Headers"] = "Authorization, Content-Type, X-SekaiLink-Client, X-SekaiLink-Client-Key"
        resp.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, PATCH, DELETE, OPTIONS"
        return resp
    return None


@app.after_request
def _add_cors_headers(response):
    if request.path.startswith("/api/"):
        origin = request.headers.get("Origin")
        if origin:
            response.headers["Access-Control-Allow-Origin"] = origin
            response.headers["Vary"] = "Origin"
            response.headers["Access-Control-Allow-Credentials"] = "true"
        else:
            response.headers["Access-Control-Allow-Origin"] = "*"
        response.headers["Access-Control-Allow-Headers"] = "Authorization, Content-Type, X-SekaiLink-Client, X-SekaiLink-Client-Key"
        response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, PATCH, DELETE, OPTIONS"
    return response


@app.before_request
def redirect_admin_subdomain():
    host = (request.host or "").split(":", 1)[0].lower()
    if host == "admin.sekailink.com" and request.path == "/":
        return redirect("/admin")
    return None


@app.before_request
def desktop_bearer_auth():
    if session.get("discord_user"):
        return None
    token = _get_bearer_token()
    if not token:
        return None
    context = _desktop_token_auth_context(token)
    if not context:
        if request.path.startswith("/api/"):
            return jsonify({"error": "Invalid or expired token."}), 401
        return abort(401)
    if context.get("banned"):
        reason = (context.get("ban_reason") or "No reason provided.").strip()
        if request.path.startswith("/api/"):
            return jsonify({"error": "You are globally banned.", "reason": reason}), 403
        session["ban_reason"] = reason
        return redirect("/banned")
    avatar_hash = context.get("avatar")
    discord_id = context.get("id")
    if discord_id and avatar_hash:
        avatar_url = f"https://cdn.discordapp.com/avatars/{discord_id}/{avatar_hash}.png?size=64"
    elif discord_id:
        avatar_url = f"https://cdn.discordapp.com/embed/avatars/{int(discord_id) % 5}.png"
    else:
        avatar_url = None
    session["discord_user"] = {
        "id": discord_id,
        "username": context.get("username"),
        "global_name": context.get("global_name"),
        "avatar": avatar_hash,
        "avatar_url": avatar_url,
        "email": context.get("email"),
    }
    session["role"] = context.get("role") or "player"
    current_version = app.config.get("TERMS_VERSION", "v1")
    session["terms_version"] = context.get("terms_version")
    session["terms_accepted"] = bool(context.get("terms_accepted") and context.get("terms_version") == current_version)
    return None


@app.before_request
def enforce_critical_client_only():
    if not _is_critical_client_endpoint():
        return None

    token = _get_bearer_token()
    if not token:
        if request.path.startswith("/api/"):
            return jsonify({"error": "Client bearer token required for this action."}), 403
        return abort(403)

    if not _desktop_token_auth_context(token):
        if request.path.startswith("/api/"):
            return jsonify({"error": "Invalid or expired desktop token."}), 403
        return abort(403)

    if not _is_authorized_sekailink_client_request():
        if request.path.startswith("/api/"):
            return jsonify({"error": "This action is restricted to the SekaiLink desktop client."}), 403
        return abort(403)
    return None


@app.before_request
def enforce_global_ban():
    if request.path.startswith(("/static", "/socket.io")):
        return None
    if request.path in ("/api/auth/login", "/api/auth/callback", "/api/auth/logout", "/api/auth/desktop-login",
                        "/api/auth/desktop-redirect", "/api/auth/desktop-callback", "/api/auth/twitch/login", "/api/auth/twitch/callback",
                        "/api/auth/email/register", "/api/auth/email/login", "/api/auth/email/verify", "/auth", "/banned",
                        "/api/support/ban-appeal", "/dev-login", "/dev-logout"):
        return None
    if not session.get("discord_user"):
        return None
    discord_id = session["discord_user"].get("id")
    if not discord_id:
        return None
    with db_session:
        user = User.get(discord_id=discord_id)
        if not user:
            session.pop("discord_user", None)
            session.pop("discord_oauth_state", None)
            session.pop("terms_accepted", None)
            session.pop("ban_reason", None)
            session.pop("role", None)
            if request.path.startswith("/api/"):
                return jsonify({"error": "Account not found. Please reconnect with Discord."}), 401
            return redirect("/api/auth/login")
        if user.banned:
            reason = (user.ban_reason or "No reason provided.").strip()
            session["ban_reason"] = reason
            if request.path.startswith("/api/"):
                return jsonify({"error": "You are globally banned.", "reason": reason}), 403
            return redirect("/banned")
        session["role"] = user.role
    return None


@app.context_processor
def inject_admin_flag():
    return {
        "is_admin": session.get("role") == "admin",
        "app_version": __version__,
    }


def _require_admin_user() -> str:
    user = _get_session_user()
    if not user:
        abort(401)
    if user.role != "admin":
        abort(403)
    return user.discord_id


def _require_user() -> str:
    user = _get_session_user()
    if not user:
        abort(401)
    return user.discord_id


def _get_session_user() -> User | None:
    discord = session.get("discord_user")
    if discord:
        discord_id = discord.get("id")
        if not discord_id:
            return None
        with db_session:
            user = User.get(discord_id=discord_id)
            if not user:
                return None
            if user.banned:
                abort(403)
            return user
    token = _get_bearer_token()
    if not token:
        return None
    context = _desktop_token_auth_context(token)
    if not context:
        return None
    discord_id = context.get("id")
    if not discord_id:
        return None
    with db_session:
        user = User.get(discord_id=discord_id)
        if not user:
            return None
        if user.banned:
            abort(403)
        return user


def _display_name(user: User) -> str:
    return user.global_name or user.username or user.discord_id


def _avatar_url(user: User) -> str | None:
    if user.custom_avatar_url:
        return user.custom_avatar_url
    if user.discord_id and user.avatar:
        return f"https://cdn.discordapp.com/avatars/{user.discord_id}/{user.avatar}.png?size=64"
    if user.discord_id:
        try:
            idx = int(user.discord_id) % 5
        except Exception:
            idx = 0
        return f"https://cdn.discordapp.com/embed/avatars/{idx}.png"
    return None


def _are_friends(user: User, other: User) -> bool:
    return Friendship.get(user=user, friend=other) is not None


def _user_room(discord_id: str) -> str:
    return f"user:{discord_id}"


def _effective_presence(user: User) -> str:
    if user.presence_status == "offline":
        return "offline"
    if user.presence_status == "dnd":
        return "dnd" if user.is_online else "offline"
    return "online" if user.is_online else "offline"


def _emit_presence_update(user: User) -> None:
    status = _effective_presence(user)
    payload = {
        "user_id": user.discord_id,
        "display_name": _display_name(user),
        "status": status,
        "is_online": status == "online",
    }
    friends = select(f.friend for f in Friendship if f.user == user)
    for friend in friends:
        socketio.emit("friend_presence", payload, to=_user_room(friend.discord_id))


def _get_support_user() -> User:
    support_id = "support"
    user = User.get(discord_id=support_id)
    if user:
        return user
    user = User(
        discord_id=support_id,
        username="SekaiLink Support",
        global_name="SekaiLink Support",
        role="admin",
        dm_policy="anyone",
        presence_status="online",
    )
    return user


def _ensure_user_uuid(user: User) -> str:
    if not user.user_uuid:
        user.user_uuid = str(uuid.uuid4())
    return user.user_uuid


def _get_user_yaml_dir(user: User) -> str:
    root = app.config.get("USER_YAML_ROOT", os.path.abspath("user_yamls"))
    user_uuid = _ensure_user_uuid(user)
    path = os.path.join(root, user_uuid)
    os.makedirs(path, exist_ok=True)
    return path


def _sync_discord_user_from_payload(user_payload: dict[str, Any]) -> tuple[User, str | None, str, str | None, bool, str | None]:
    discord_id = user_payload.get("id")
    if not discord_id:
        raise ValueError("Missing Discord user id")
    avatar_hash = user_payload.get("avatar")
    if discord_id and avatar_hash:
        avatar_url = f"https://cdn.discordapp.com/avatars/{discord_id}/{avatar_hash}.png?size=64"
    elif discord_id:
        avatar_url = f"https://cdn.discordapp.com/embed/avatars/{int(discord_id) % 5}.png"
    else:
        avatar_url = None

    banned_reason = None
    user_role = "player"
    terms_version = None
    terms_accepted = False
    with db_session:
        user = User.get(discord_id=discord_id)
        if not user:
            user = User(
                discord_id=discord_id,
                user_uuid=str(uuid.uuid4()),
                username=user_payload.get("username"),
                global_name=user_payload.get("global_name"),
                avatar=avatar_hash,
                email=user_payload.get("email"),
                raw_profile=json.dumps(user_payload),
                terms_accepted=False,
                last_login=datetime.datetime.utcnow(),
            )
        else:
            _ensure_user_uuid(user)
            user.username = user_payload.get("username")
            user.global_name = user_payload.get("global_name")
            user.avatar = avatar_hash
            user.email = user_payload.get("email")
            user.raw_profile = json.dumps(user_payload)
            user.last_login = datetime.datetime.utcnow()
        if user.banned:
            banned_reason = (user.ban_reason or "No reason provided.").strip()
        user_role = user.role
        _get_user_yaml_dir(user)
        current_version = app.config.get("TERMS_VERSION", "v1")
        terms_version = user.terms_version
        terms_accepted = bool(user.terms_accepted and user.terms_version == current_version)
        commit()
    return user, banned_reason, user_role, terms_version, terms_accepted, avatar_url




def _get_twitch_oauth_config() -> dict[str, str]:
    client_id = app.config.get("TWITCH_CLIENT_ID")
    client_secret = app.config.get("TWITCH_CLIENT_SECRET")
    redirect_uri = app.config.get("TWITCH_REDIRECT_URI")
    scopes = app.config.get("TWITCH_OAUTH_SCOPES", "user:read:email").strip()
    if not client_id or not client_secret or not redirect_uri:
        raise RuntimeError("Twitch OAuth config is missing.")
    return {
        "client_id": client_id,
        "client_secret": client_secret,
        "redirect_uri": redirect_uri,
        "scopes": scopes,
    }


def _generate_internal_user_id() -> str:
    while True:
        candidate = str(secrets.randbelow(10**18 - 10**17) + 10**17)
        if not User.get(discord_id=candidate):
            return candidate


def _normalize_locale(value: str | None) -> str:
    raw = (value or "en").strip().lower()
    mapping = {
        "fr": "fr",
        "en": "en",
        "es": "es",
        "ja": "ja",
        "zh": "zh-CN",
        "zh-cn": "zh-CN",
        "zh-tw": "zh-TW",
    }
    return mapping.get(raw, "en")


def _password_recommendations(password: str) -> list[str]:
    recommendations: list[str] = []
    if len(password) < 12:
        recommendations.append("Use at least 12 characters.")
    if not re.search(r"[A-Z]", password):
        recommendations.append("Add at least one uppercase letter.")
    if not re.search(r"[a-z]", password):
        recommendations.append("Add at least one lowercase letter.")
    if not re.search(r"\d", password):
        recommendations.append("Add at least one number.")
    if not re.search(r"[^A-Za-z0-9]", password):
        recommendations.append("Add at least one special character.")
    return recommendations


def _hash_verification_token(token: str) -> str:
    return hashlib.sha256(token.encode("utf-8")).hexdigest()


def _verify_signup_captcha(captcha_response: str | None) -> tuple[bool, str | None]:
    secret = (app.config.get("CAPTCHA_SECRET_KEY") or "").strip()
    if not secret:
        return False, "Captcha is not configured on the server."
    token = (captcha_response or "").strip()
    if not token:
        return False, "Captcha validation is required."
    data = urllib.parse.urlencode({
        "secret": secret,
        "response": token,
        "remoteip": request.remote_addr or "",
    }).encode("utf-8")
    req = urllib.request.Request(
        "https://challenges.cloudflare.com/turnstile/v0/siteverify",
        data=data,
        headers={"Content-Type": "application/x-www-form-urlencoded", "Accept": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            payload = json.loads(resp.read().decode("utf-8"))
        if payload.get("success"):
            return True, None
        return False, "Captcha verification failed."
    except Exception:
        return False, "Captcha verification failed."


def _send_confirmation_email(to_email: str, verify_url: str, locale: str) -> None:
    smtp_host = app.config.get("SMTP_HOST", "smtp.hostinger.com")
    smtp_port = int(app.config.get("SMTP_PORT", 465))
    smtp_user = app.config.get("SMTP_USERNAME")
    smtp_password = app.config.get("SMTP_PASSWORD")
    from_email = app.config.get("SMTP_FROM", "no-reply@sekailink.com")
    if not smtp_host or not smtp_user or not smtp_password:
        raise RuntimeError("SMTP is not configured.")

    localized = {
        "fr": ("Confirme ton compte SekaiLink", "Clique sur ce lien pour confirmer ton compte:", "Si tu n'as pas cree ce compte, ignore ce message."),
        "es": ("Confirma tu cuenta de SekaiLink", "Haz clic en este enlace para confirmar tu cuenta:", "Si no creaste esta cuenta, ignora este correo."),
        "ja": ("SekaiLinkアカウント確認", "以下のリンクをクリックしてアカウントを確認してください:", "この登録に心当たりがない場合は、このメールを無視してください。"),
        "zh-CN": ("确认你的 SekaiLink 账号", "请点击以下链接确认你的账号：", "如果这不是你发起的注册，请忽略此邮件。"),
        "zh-TW": ("確認你的 SekaiLink 帳號", "請點擊以下連結確認你的帳號：", "如果這不是你發起的註冊，請忽略此郵件。"),
        "en": ("Confirm your SekaiLink account", "Click this link to confirm your account:", "If you did not create this account, ignore this email."),
    }
    subject, lead, foot = localized.get(locale, localized["en"])
    msg = EmailMessage()
    msg["Subject"] = subject
    msg["From"] = from_email
    msg["To"] = to_email
    msg.set_content(f"{lead}\n\n{verify_url}\n\n{foot}")

    with smtplib.SMTP_SSL(smtp_host, smtp_port, context=ssl.create_default_context()) as smtp:
        smtp.login(smtp_user, smtp_password)
        smtp.send_message(msg)


def _set_authenticated_session(user: User, avatar_url: str | None = None) -> None:
    session["discord_user"] = {
        "id": user.discord_id,
        "username": user.username,
        "global_name": user.global_name,
        "avatar": user.avatar,
        "avatar_url": avatar_url,
        "email": user.email,
    }
    session["role"] = user.role
    current_version = app.config.get("TERMS_VERSION", "v1")
    session["terms_version"] = user.terms_version
    session["terms_accepted"] = bool(user.terms_accepted and user.terms_version == current_version)


def _default_avatar_url_for(user_id: str) -> str:
    try:
        idx = int(user_id) % 5
    except Exception:
        idx = 0
    return f"https://cdn.discordapp.com/embed/avatars/{idx}.png"


def _render_auth_page(error: str | None = None, success: str | None = None, default_locale: str = "en"):
    return render_template(
        "auth.html",
        auth_error=error,
        auth_success=success,
        default_locale=default_locale,
        captcha_site_key=(app.config.get("CAPTCHA_SITE_KEY") or "").strip(),
    )

@app.route("/api/auth/login")
def discord_login():
    try:
        cfg = _get_discord_oauth_config()
    except RuntimeError:
        return abort(503)
    if session.get("discord_user"):
        discord_id = session["discord_user"].get("id")
        if discord_id:
            with db_session:
                user = User.get(discord_id=discord_id)
                if user:
                    return redirect("/rooms")
        session.pop("discord_user", None)
        session.pop("discord_oauth_state", None)
        session.pop("discord_oauth_state_ts", None)
        session.pop("terms_accepted", None)
        session.pop("ban_reason", None)
        session.pop("role", None)
    state = session.get("discord_oauth_state")
    state_ts = session.get("discord_oauth_state_ts")
    now = int(time.time())
    if not state or not state_ts or now - int(state_ts) > 600:
        state = secrets.token_urlsafe(24)
        session["discord_oauth_state"] = state
        session["discord_oauth_state_ts"] = now
    query = {
        "client_id": cfg["client_id"],
        "redirect_uri": cfg["redirect_uri"],
        "response_type": "code",
        "scope": cfg["scopes"],
        "permissions": cfg["permissions"],
        "integration_type": cfg["integration_type"],
        "state": state,
    }
    return redirect("https://discord.com/oauth2/authorize?" + urllib.parse.urlencode(query))


@app.route("/api/auth/desktop-login")
def discord_desktop_login():
    try:
        cfg = _get_discord_desktop_oauth_config()
    except RuntimeError:
        return abort(503)
    requested_scheme = (request.args.get("scheme") or "sekailink").strip().lower()
    if requested_scheme not in {"sekailink", "sekailink-admin"}:
        requested_scheme = "sekailink"
    state = f"{requested_scheme}:{secrets.token_urlsafe(24)}"
    _desktop_oauth_store_state(state)
    query = {
        "client_id": cfg["client_id"],
        "redirect_uri": cfg["redirect_uri"],
        "response_type": "code",
        "scope": cfg["scopes"],
        "permissions": cfg["permissions"],
        "integration_type": cfg["integration_type"],
        "state": state,
    }
    return redirect("https://discord.com/oauth2/authorize?" + urllib.parse.urlencode(query))


@app.route("/api/auth/desktop-redirect")
def discord_desktop_redirect():
    state = request.args.get("state") or ""
    callback_scheme = _extract_desktop_callback_scheme(state)
    html = f"""<!doctype html>
<html>
<head><meta charset="utf-8"><title>Redirecting...</title></head>
<body>
  <script>
    const params = new URLSearchParams(window.location.search);
    const code = params.get("code") || "";
    const state = params.get("state") || "";
    const target = `{callback_scheme}://auth?code=${{encodeURIComponent(code)}}&state=${{encodeURIComponent(state)}}`;
    window.location.replace(target);
  </script>
  <p>Redirecting back to SekaiLink...</p>
</body>
</html>
"""
    return Response(html, mimetype="text/html")


@app.route("/dev-login")
def dev_login():
    if not app.config.get("DEV_LOGIN_ENABLED", False):
        return abort(404)
    with db_session:
        user = User.get(discord_id="local-test")
        if not user:
            user = User(
                discord_id="local-test",
                user_uuid=str(uuid.uuid4()),
                username="LocalTest",
                global_name="LocalTest",
                avatar="",
                email="",
                raw_profile="{}",
                role="player",
                terms_accepted=True,
                terms_version=app.config.get("TERMS_VERSION", "v1"),
            )
        _ensure_user_uuid(user)
        _get_user_yaml_dir(user)
        session["discord_user"] = {
            "id": user.discord_id,
            "username": user.username,
            "global_name": user.global_name,
            "avatar": None,
            "avatar_url": None,
            "email": None,
        }
        session["role"] = user.role
        session["terms_version"] = user.terms_version
        session["terms_accepted"] = True
    return redirect("/rooms")


@app.route("/dev-logout")
def dev_logout():
    if not app.config.get("DEV_LOGIN_ENABLED", False):
        return abort(404)
    session.pop("discord_user", None)
    session.pop("discord_oauth_state", None)
    session.pop("terms_accepted", None)
    session.pop("ban_reason", None)
    session.pop("role", None)
    return redirect("/")


@app.route("/api/auth/callback")
def discord_callback():
    try:
        cfg = _get_discord_oauth_config()
    except RuntimeError:
        return abort(503)
    code = request.args.get("code")
    state = request.args.get("state")
    if not code or not state or state != session.get("discord_oauth_state"):
        session.pop("discord_oauth_state", None)
        session.pop("discord_oauth_state_ts", None)
        return abort(400)
    token_data = urllib.parse.urlencode({
        "client_id": cfg["client_id"],
        "client_secret": cfg["client_secret"],
        "grant_type": "authorization_code",
        "code": code,
        "redirect_uri": cfg["redirect_uri"],
    }).encode("utf-8")
    token_req = urllib.request.Request(
        "https://discord.com/api/oauth2/token",
        data=token_data,
        headers={
            "Content-Type": "application/x-www-form-urlencoded",
            # Avoid Cloudflare 1010 by using a normal User-Agent.
            "User-Agent": "SekaiLink/1.0 (+https://sekailink.com)",
            "Accept": "application/json",
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(token_req, timeout=10) as resp:
            token_payload = json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        try:
            body = exc.read().decode("utf-8", "replace")[:500]
        except Exception:
            body = ""
        app.logger.error("Discord token HTTPError %s: %s", exc.code, body)
        return abort(502)
    except urllib.error.URLError as exc:
        app.logger.error("Discord token URLError: %s", exc)
        return abort(502)

    access_token = token_payload.get("access_token")
    if not access_token:
        return abort(502)

    user_req = urllib.request.Request(
        "https://discord.com/api/users/@me",
        headers={
            "Authorization": f"Bearer {access_token}",
            "User-Agent": "SekaiLink/1.0 (+https://sekailink.com)",
            "Accept": "application/json",
        },
    )
    try:
        with urllib.request.urlopen(user_req, timeout=10) as resp:
            user_payload = json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        try:
            body = exc.read().decode("utf-8", "replace")[:500]
        except Exception:
            body = ""
        app.logger.error("Discord user HTTPError %s: %s", exc.code, body)
        return abort(502)
    except urllib.error.URLError as exc:
        app.logger.error("Discord user URLError: %s", exc)
        return abort(502)

    try:
        user, banned_reason, user_role, terms_version, terms_accepted, avatar_url = _sync_discord_user_from_payload(user_payload)
    except ValueError:
        session.pop("discord_oauth_state", None)
        session.pop("discord_oauth_state_ts", None)
        return abort(502)
    discord_id = user.discord_id
    avatar_hash = user.avatar
    if banned_reason:
        session["ban_reason"] = banned_reason
        session.pop("discord_oauth_state", None)
        session.pop("discord_oauth_state_ts", None)
        session.pop("discord_user", None)
        session.pop("role", None)
        return redirect("/banned")
    session["discord_user"] = {
        "id": discord_id,
        "username": user_payload.get("username"),
        "global_name": user_payload.get("global_name"),
        "avatar": avatar_hash,
        "avatar_url": avatar_url,
        "email": user_payload.get("email"),
    }
    session["role"] = user_role
    session["terms_version"] = terms_version
    session["terms_accepted"] = terms_accepted
    session.pop("discord_oauth_state", None)
    session.pop("discord_oauth_state_ts", None)
    return redirect("/rooms")


@app.route("/api/auth/desktop-callback", methods=["POST"])
def discord_desktop_callback():
    try:
        cfg = _get_discord_desktop_oauth_config()
    except RuntimeError:
        return abort(503)
    payload = request.get_json(silent=True) or {}
    code = payload.get("code")
    state = payload.get("state")
    if not code or not state or not _desktop_oauth_validate_and_consume_state(state):
        return jsonify({"error": "Invalid OAuth state."}), 400
    token_data = urllib.parse.urlencode({
        "client_id": cfg["client_id"],
        "client_secret": cfg["client_secret"],
        "grant_type": "authorization_code",
        "code": code,
        "redirect_uri": cfg["redirect_uri"],
    }).encode("utf-8")
    token_req = urllib.request.Request(
        "https://discord.com/api/oauth2/token",
        data=token_data,
        headers={
            "Content-Type": "application/x-www-form-urlencoded",
            # Avoid Cloudflare 1010 by using a normal User-Agent.
            "User-Agent": "SekaiLink/1.0 (+https://sekailink.com)",
            "Accept": "application/json",
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(token_req, timeout=10) as resp:
            token_payload = json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        try:
            body = exc.read().decode("utf-8", "replace")[:500]
        except Exception:
            body = ""
        app.logger.error("Discord desktop token HTTPError %s: %s", exc.code, body)
        return jsonify({"error": "Failed to exchange code."}), 502
    except urllib.error.URLError as exc:
        app.logger.error("Discord desktop token URLError: %s", exc)
        return jsonify({"error": "Failed to exchange code."}), 502

    access_token = token_payload.get("access_token")
    if not access_token:
        return jsonify({"error": "Failed to exchange code."}), 502

    user_req = urllib.request.Request(
        "https://discord.com/api/users/@me",
        headers={
            "Authorization": f"Bearer {access_token}",
            "User-Agent": "SekaiLink/1.0 (+https://sekailink.com)",
            "Accept": "application/json",
        },
    )
    try:
        with urllib.request.urlopen(user_req, timeout=10) as resp:
            user_payload = json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        try:
            body = exc.read().decode("utf-8", "replace")[:500]
        except Exception:
            body = ""
        app.logger.error("Discord desktop user HTTPError %s: %s", exc.code, body)
        return jsonify({"error": "Failed to fetch user."}), 502
    except urllib.error.URLError as exc:
        app.logger.error("Discord desktop user URLError: %s", exc)
        return jsonify({"error": "Failed to fetch user."}), 502

    try:
        user, banned_reason, _, terms_version, terms_accepted, avatar_url = _sync_discord_user_from_payload(user_payload)
    except ValueError:
        return jsonify({"error": "Invalid user payload."}), 502
    if banned_reason:
        return jsonify({"error": "You are globally banned.", "reason": banned_reason}), 403
    try:
        token, expires_at = _create_desktop_token(user.discord_id)
    except ValueError:
        return jsonify({"error": "Account not found."}), 404
    response_user = {
        "id": user.discord_id,
        "username": user.username,
        "global_name": user.global_name,
        "avatar_url": avatar_url,
        "terms_version": terms_version,
        "terms_accepted": terms_accepted,
    }
    return jsonify({
        "token": token,
        "user": response_user,
        "expires_at": expires_at.replace(microsecond=0).isoformat() + "Z",
    })


@app.route("/api/auth/logout")
def discord_logout():
    session.clear()
    return redirect("/")



@app.route("/auth")
def auth_page():
    if session.get("discord_user"):
        return redirect("/rooms")
    return _render_auth_page()


@app.route("/api/auth/twitch/login")
def twitch_login():
    try:
        cfg = _get_twitch_oauth_config()
    except RuntimeError:
        return abort(503)
    state = secrets.token_urlsafe(24)
    session["twitch_oauth_state"] = state
    session["twitch_oauth_state_ts"] = int(time.time())
    query = {
        "client_id": cfg["client_id"],
        "redirect_uri": cfg["redirect_uri"],
        "response_type": "code",
        "scope": cfg["scopes"],
        "state": state,
    }
    return redirect("https://id.twitch.tv/oauth2/authorize?" + urllib.parse.urlencode(query))


@app.route("/api/auth/twitch/callback")
def twitch_callback():
    try:
        cfg = _get_twitch_oauth_config()
    except RuntimeError:
        return abort(503)
    code = request.args.get("code")
    state = request.args.get("state")
    if not code or not state or state != session.get("twitch_oauth_state"):
        return abort(400)

    data = urllib.parse.urlencode({
        "client_id": cfg["client_id"],
        "client_secret": cfg["client_secret"],
        "code": code,
        "grant_type": "authorization_code",
        "redirect_uri": cfg["redirect_uri"],
    }).encode("utf-8")
    token_req = urllib.request.Request(
        "https://id.twitch.tv/oauth2/token",
        data=data,
        headers={"Content-Type": "application/x-www-form-urlencoded", "Accept": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(token_req, timeout=10) as resp:
            token_payload = json.loads(resp.read().decode("utf-8"))
    except Exception:
        return abort(502)
    access_token = token_payload.get("access_token")
    if not access_token:
        return abort(502)

    user_req = urllib.request.Request(
        "https://api.twitch.tv/helix/users",
        headers={
            "Authorization": f"Bearer {access_token}",
            "Client-Id": cfg["client_id"],
            "Accept": "application/json",
        },
    )
    try:
        with urllib.request.urlopen(user_req, timeout=10) as resp:
            user_payload = json.loads(resp.read().decode("utf-8"))
    except Exception:
        return abort(502)

    rows = user_payload.get("data") or []
    if not rows:
        return abort(502)
    twitch_user = rows[0]
    twitch_id = str(twitch_user.get("id") or "").strip()
    if not twitch_id:
        return abort(502)
    email = (twitch_user.get("email") or "").strip().lower() or None
    display_name = twitch_user.get("display_name") or twitch_user.get("login") or "Twitch User"

    with db_session:
        identity = AuthIdentity.get(provider="twitch", provider_user_id=twitch_id)
        if not identity and email:
            identity = AuthIdentity.get(provider="email", email=email)

        if identity:
            user = identity.user
        else:
            user = User(
                discord_id=_generate_internal_user_id(),
                user_uuid=str(uuid.uuid4()),
                username=display_name,
                global_name=display_name,
                avatar=None,
                email=email,
                raw_profile=json.dumps({"provider": "twitch", "payload": twitch_user}),
                terms_accepted=False,
                last_login=datetime.datetime.utcnow(),
            )

        if not AuthIdentity.get(provider="twitch", provider_user_id=twitch_id):
            AuthIdentity(
                user=user,
                provider="twitch",
                provider_user_id=twitch_id,
                email=email,
                email_verified=bool(email),
                preferred_locale="en",
            )

        user.username = twitch_user.get("login") or user.username
        user.global_name = display_name
        if email:
            user.email = email
        user.raw_profile = json.dumps({"provider": "twitch", "payload": twitch_user})
        user.last_login = datetime.datetime.utcnow()
        _ensure_user_uuid(user)
        _get_user_yaml_dir(user)

        if user.banned:
            session["ban_reason"] = (user.ban_reason or "No reason provided.").strip()
            return redirect("/banned")

        avatar_url = (twitch_user.get("profile_image_url") or "").strip() or _default_avatar_url_for(user.discord_id)
        _set_authenticated_session(user, avatar_url=avatar_url)
        commit()

    session.pop("twitch_oauth_state", None)
    session.pop("twitch_oauth_state_ts", None)
    return redirect("/rooms")


@app.route("/api/auth/email/register", methods=["POST"])
def email_register():
    email = (request.form.get("email") or "").strip().lower()
    password = request.form.get("password") or ""
    display_name = (request.form.get("display_name") or "").strip()
    locale = _normalize_locale(request.form.get("locale"))
    captcha_response = request.form.get("cf-turnstile-response") or request.form.get("captcha_token")

    if not email or "@" not in email:
        return _render_auth_page(error="Valid email is required.", default_locale=locale), 400
    if not display_name:
        return _render_auth_page(error="Display name is required.", default_locale=locale), 400

    captcha_ok, captcha_error = _verify_signup_captcha(captcha_response)
    if not captcha_ok:
        return _render_auth_page(error=captcha_error, default_locale=locale), 400

    recs = _password_recommendations(password)
    if recs:
        return _render_auth_page(error=" ".join(recs), default_locale=locale), 400

    verification_token = secrets.token_urlsafe(48)
    verify_hash = _hash_verification_token(verification_token)

    with db_session:
        if AuthIdentity.get(provider="email", email=email):
            return _render_auth_page(error="An account with this email already exists.", default_locale=locale), 409

        user = User(
            discord_id=_generate_internal_user_id(),
            user_uuid=str(uuid.uuid4()),
            username=display_name,
            global_name=display_name,
            avatar=None,
            email=email,
            raw_profile=json.dumps({"provider": "email"}),
            terms_accepted=False,
            last_login=datetime.datetime.utcnow(),
        )
        AuthIdentity(
            user=user,
            provider="email",
            email=email,
            password_hash=generate_password_hash(password),
            email_verified=False,
            verification_token_hash=verify_hash,
            verification_sent_at=datetime.datetime.utcnow(),
            preferred_locale=locale,
        )
        _ensure_user_uuid(user)
        _get_user_yaml_dir(user)
        commit()

    verify_url = urllib.parse.urljoin(request.url_root, f"api/auth/email/verify?token={urllib.parse.quote(verification_token)}")
    try:
        _send_confirmation_email(email, verify_url, locale)
    except Exception:
        return _render_auth_page(error="Account created but email confirmation failed. Contact support.", default_locale=locale), 500

    return _render_auth_page(success="Account created. Check your email to confirm your account.", default_locale=locale)


@app.route("/api/auth/email/login", methods=["POST"])
def email_login():
    email = (request.form.get("email") or "").strip().lower()
    password = request.form.get("password") or ""
    locale = _normalize_locale(request.form.get("locale"))
    if not email or not password:
        return _render_auth_page(error="Email and password are required.", default_locale=locale), 400

    with db_session:
        identity = AuthIdentity.get(provider="email", email=email)
        if not identity or not identity.password_hash:
            return _render_auth_page(error="Invalid credentials.", default_locale=locale), 401
        if not check_password_hash(identity.password_hash, password):
            return _render_auth_page(error="Invalid credentials.", default_locale=locale), 401
        if not identity.email_verified:
            return _render_auth_page(error="Please confirm your email before signing in.", default_locale=locale), 403

        user = identity.user
        if not user:
            return _render_auth_page(error="Account error. Contact support.", default_locale=locale), 500
        if user.banned:
            session["ban_reason"] = (user.ban_reason or "No reason provided.").strip()
            return redirect("/banned")

        identity.last_login = datetime.datetime.utcnow()
        user.last_login = datetime.datetime.utcnow()
        _set_authenticated_session(user, avatar_url=_default_avatar_url_for(user.discord_id))
        commit()

    return redirect("/rooms")


@app.route("/api/auth/email/verify")
def email_verify():
    token = (request.args.get("token") or "").strip()
    if not token:
        return _render_auth_page(error="Invalid verification token."), 400
    token_hash = _hash_verification_token(token)

    with db_session:
        identity = AuthIdentity.get(provider="email", verification_token_hash=token_hash)
        if not identity:
            return _render_auth_page(error="Invalid or expired verification token."), 400

        identity.email_verified = True
        identity.verification_token_hash = None
        user = identity.user
        if user:
            user.last_login = datetime.datetime.utcnow()
            _set_authenticated_session(user, avatar_url=_default_avatar_url_for(user.discord_id))
        commit()

    return redirect("/rooms")


@app.route("/api/client/version")
def client_version():
    platform_raw = (request.args.get("platform") or "").strip().lower()
    platform_map = {
        "win": "windows",
        "windows": "windows",
        "linux": "linux",
        "mac": "macos",
        "macos": "macos",
        "darwin": "macos",
        "osx": "macos",
    }
    platform_key = platform_map.get(platform_raw, "")

    latest = str(app.config.get("CLIENT_LATEST_VERSION", "0.0.2-beta.0.2-topaz")).strip()
    min_supported = str(app.config.get("CLIENT_MIN_VERSION", "0.0.1")).strip()
    notes = str(app.config.get("CLIENT_RELEASE_NOTES", "")).strip()
    download_url = str(
        app.config.get(
            "CLIENT_DOWNLOAD_URL",
            "https://sekailink.com/static/downloads/SekaiLink-client-0.0.2-beta.0.2-topaz.AppImage",
        )
    ).strip()
    sha256 = str(app.config.get("CLIENT_RELEASE_SHA256", "")).strip().lower()

    manifest_path = str(app.config.get("CLIENT_VERSION_MANIFEST_PATH", "")).strip()
    if manifest_path:
        try:
            with open(manifest_path, "r", encoding="utf-8") as f:
                manifest = json.load(f)
            if isinstance(manifest, dict):
                latest = str(manifest.get("latest", latest)).strip() or latest
                min_supported = str(manifest.get("min_supported", min_supported)).strip() or min_supported
                notes = str(manifest.get("notes", notes)).strip()
                default_url = str(manifest.get("download_url", download_url)).strip()
                if default_url:
                    download_url = default_url
                sha256 = str(manifest.get("sha256", sha256)).strip().lower() or sha256
                platforms = manifest.get("platforms")
                if isinstance(platforms, dict):
                    selected = platforms.get(platform_key or "") or platforms.get("default")
                    if isinstance(selected, dict):
                        selected_url = str(selected.get("download_url", download_url)).strip()
                        if selected_url:
                            download_url = selected_url
                        sha256 = str(selected.get("sha256", sha256)).strip().lower() or sha256
        except Exception as exc:
            app.logger.warning("Failed to parse client version manifest: %s", exc)

    if platform_key:
        cfg_url = str(app.config.get(f"CLIENT_DOWNLOAD_URL_{platform_key.upper()}", "")).strip()
        if cfg_url:
            download_url = cfg_url
        cfg_sha = str(app.config.get(f"CLIENT_RELEASE_SHA256_{platform_key.upper()}", "")).strip().lower()
        if cfg_sha:
            sha256 = cfg_sha

    incremental_manifest_url = str(
        app.config.get(
            f"CLIENT_INCREMENTAL_MANIFEST_URL_{platform_key.upper()}" if platform_key else "CLIENT_INCREMENTAL_MANIFEST_URL",
            app.config.get("CLIENT_INCREMENTAL_MANIFEST_URL", "/api/client/incremental-manifest"),
        )
    ).strip()
    if (
        incremental_manifest_url.startswith("/api/client/incremental-manifest")
        and platform_key
        and "platform=" not in incremental_manifest_url
    ):
        sep = "&" if "?" in incremental_manifest_url else "?"
        incremental_manifest_url = f"{incremental_manifest_url}{sep}platform={platform_key}"

    update_notes_url = str(
        app.config.get("CLIENT_UPDATE_NOTES_URL", "/static/UPDATE.md")
    ).strip()
    update_notes_version = str(
        app.config.get("CLIENT_UPDATE_NOTES_VERSION", latest)
    ).strip()
    update_notes_title = str(
        app.config.get("CLIENT_UPDATE_NOTES_TITLE", "SekaiLink Update")
    ).strip()

    payload = {
        "latest": latest,
        "min_supported": min_supported,
        "download_url": download_url,
        "notes": notes,
        "incremental_manifest_url": incremental_manifest_url,
        "update_notes_url": update_notes_url,
        "update_notes_version": update_notes_version,
        "update_notes_title": update_notes_title,
    }
    if platform_key:
        payload["platform"] = platform_key
    if sha256 and len(sha256) == 64 and all(ch in "0123456789abcdef" for ch in sha256):
        payload["sha256"] = sha256
    return jsonify(payload)



SEKAILINK_FEATURED_BOXART_BY_NAME: dict[str, str] = {
    "A Link to the Past": "a_link_to_the_past.png",
    "A Link Between Worlds": "a_link_between_worlds.png",
    "Donkey Kong Country": "donkey_kong_country.png",
    "Donkey Kong Country 2": "donkey_kong_country_2.png",
    "Donkey Kong Country 3": "donkey_kong_country_3.png",
    "EarthBound": "earthbound.png",
    "Final Fantasy IV Free Enterprise": "final_fantasy_iv_free_enterprise.png",
    "Final Fantasy Tactics Advance": "final_fantasy_tactics_advance.png",
    "Kirbys Dream Land 3": "kirbys_dream_land_3.png",
    "Lufia II Ancient Cave": "lufia_ii_ancient_cave.png",
    "Mega Man 2": "mega_man_2.png",
    "Mega Man 3": "mega_man_3.png",
    "Metroid Fusion": "metroid_fusion.png",
    "Metroid Zero Mission": "metroid_zero_mission.png",
    "Ocarina of Time": "ocarina_of_time.webp",
    "Pokemon Crystal": "pokemon_crystal.png",
    "Pokemon Emerald": "pokemon_emerald.png",
    "Pokemon FireRed and LeafGreen": "pokemon_firered_and_leafgreen.png",
    "Pokemon Red and Blue": "pokemon_red_and_blue.png",
    "Ship of Harkinian": "ship_of_harkinian.png",
    "SMZ3": "smz3.png",
    "Super Mario 64": "super_mario_64.png",
    "Super Mario Land 2": "super_mario_land_2.png",
    "Super Mario World": "super_mario_world.png",
    "Super Metroid": "super_metroid.jpg",
    "The Legend of Zelda": "the_legend_of_zelda.png",
    "The Legend of Zelda - Oracle of Seasons": "the_legend_of_zelda_oracle_of_seasons.png",
    "The Minish Cap": "the_minish_cap.png",
    "Wario Land": "wario_land.png",
    "Wario Land 4": "wario_land_4.png",
    "Yoshis Island": "yoshis_island.png",
    "Zelda II: The Adventure of Link": "zelda_ii_the_adventure_of_link.jpg",
}


def _normalize_boxart_key(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", str(value or "").strip().lower()).strip("_")


def _resolve_featured_boxart_filename(game_id: str) -> str | None:
    key = str(game_id or "").strip()
    if not key:
        return None
    direct = SEKAILINK_FEATURED_BOXART_BY_NAME.get(key)
    if direct:
        return direct

    normalized = _normalize_boxart_key(key)
    for display_name, filename in SEKAILINK_FEATURED_BOXART_BY_NAME.items():
        if _normalize_boxart_key(display_name) == normalized:
            return filename
    return None


def _build_featured_boxart_url(filename: str | None) -> str | None:
    if not filename:
        return None
    safe_name = os.path.basename(filename)
    if not safe_name:
        return None
    return f"/static/boxart/{safe_name}"





@app.route("/api/client/games")
def client_games():
    """List games the desktop client should show in Game Manager.

    - Server controls the universe of games (from AutoWorldRegister).
    - Optional allowlist via CLIENT_SUPPORTED_GAMES.
    """
    show_all_raw = (request.args.get("all") or "").strip().lower()
    show_all = show_all_raw in {"1", "true", "yes", "y"}

    hidden = set(app.config.get("HIDDEN_WEBWORLDS") or [])

    allow_cfg = app.config.get("CLIENT_SUPPORTED_GAMES")
    allowlist = None
    if allow_cfg and not show_all:
        if isinstance(allow_cfg, (list, tuple, set)):
            allowlist = {str(x).strip() for x in allow_cfg if str(x).strip()}
        else:
            raw = str(allow_cfg).strip()
            try:
                import json as _json
                parsed = _json.loads(raw)
                if isinstance(parsed, list):
                    allowlist = {str(x).strip() for x in parsed if str(x).strip()}
            except Exception:
                allowlist = {x.strip() for x in raw.split(",") if x.strip()}

    games = []
    for world_name, world in AutoWorldRegister.world_types.items():
        try:
            if getattr(world, "hidden", False):
                continue
        except Exception:
            pass
        if world_name in hidden:
            continue
        if allowlist is not None and world_name not in allowlist:
            continue
        games.append({"game_id": world_name, "display_name": world_name})

    games.sort(key=lambda x: str(x.get("display_name") or "").lower())
    return jsonify({"games": games})


@app.route("/api/client/boxart")
def client_boxart():
    raw_ids = str(request.args.get("game_ids", "") or "").strip()
    requested_ids = [part.strip() for part in raw_ids.split(",") if part.strip()]

    if not requested_ids:
        all_map = {name: _build_featured_boxart_url(filename) for name, filename in SEKAILINK_FEATURED_BOXART_BY_NAME.items()}
        return jsonify({"boxarts": {k: v for k, v in all_map.items() if v}})

    out: dict[str, str] = {}
    for game_id in requested_ids:
        resolved = _build_featured_boxart_url(_resolve_featured_boxart_filename(game_id))
        if resolved:
            out[game_id] = resolved

    return jsonify({"boxarts": out})


@app.route("/api/client/gamebox")
def client_gamebox():
    return client_boxart()



@app.route("/api/client/incremental-manifest")
def client_incremental_manifest():
    platform_raw = (request.args.get("platform") or "").strip().lower()
    platform_map = {
        "win": "windows",
        "windows": "windows",
        "linux": "linux",
        "mac": "macos",
        "macos": "macos",
        "darwin": "macos",
        "osx": "macos",
    }
    platform_key = platform_map.get(platform_raw, "")
    default_manifest_path = os.path.join(app.root_path, "static", "downloads", "client-incremental-manifest.json")
    manifest_path = str(
        app.config.get(
            f"CLIENT_INCREMENTAL_MANIFEST_PATH_{platform_key.upper()}" if platform_key else "CLIENT_INCREMENTAL_MANIFEST_PATH",
            app.config.get("CLIENT_INCREMENTAL_MANIFEST_PATH", default_manifest_path),
        )
    ).strip()

    if not manifest_path:
        return jsonify({"enabled": False, "platform": platform_key, "files": []})

    if not os.path.isabs(manifest_path):
        manifest_path = os.path.join(app.root_path, manifest_path)
    if not os.path.exists(manifest_path):
        return jsonify({"enabled": False, "platform": platform_key, "files": [], "error": "manifest_missing"}), 404

    try:
        with open(manifest_path, "r", encoding="utf-8") as f:
            manifest = json.load(f)
    except Exception as exc:
        app.logger.warning("Failed to read incremental manifest %s: %s", manifest_path, exc)
        return jsonify({"enabled": False, "platform": platform_key, "files": [], "error": "manifest_invalid"}), 500

    if not isinstance(manifest, dict):
        return jsonify({"enabled": False, "platform": platform_key, "files": [], "error": "manifest_invalid"}), 500

    platforms = manifest.get("platforms")
    if isinstance(platforms, dict):
        selected = platforms.get(platform_key or "") or platforms.get("default")
        if isinstance(selected, dict):
            merged = dict(manifest)
            merged.update(selected)
            manifest = merged

    files = []
    for raw in (manifest.get("files") or []):
        if not isinstance(raw, dict):
            continue
        rel_path = str(raw.get("path") or raw.get("relative_path") or "").replace("\\", "/").lstrip("/")
        sha256 = str(raw.get("sha256") or "").strip().lower()
        if not rel_path or ".." in rel_path:
            continue
        if not sha256 or len(sha256) != 64 or any(ch not in "0123456789abcdef" for ch in sha256):
            continue
        row = {"path": rel_path, "sha256": sha256}
        file_url = str(raw.get("url") or "").strip()
        if file_url:
            row["url"] = file_url
        size_value = raw.get("size")
        if isinstance(size_value, int) and size_value >= 0:
            row["size"] = size_value
        files.append(row)

    response_payload = {
        "enabled": bool(manifest.get("enabled", True)),
        "platform": platform_key,
        "version": str(manifest.get("version") or "").strip(),
        "base_url": str(manifest.get("base_url") or "").strip(),
        "files": files,
    }
    return jsonify(response_payload)


@app.route("/api/auth/patreon/login")
def patreon_login():
    if not session.get("discord_user"):
        return redirect(url_for("discord_login"))
    cfg = _get_patreon_oauth_config()
    state = secrets.token_urlsafe(24)
    session["patreon_oauth_state"] = state
    query = {
        "response_type": "code",
        "client_id": cfg["client_id"],
        "redirect_uri": cfg["redirect_uri"],
        "scope": cfg["scopes"],
        "state": state,
    }
    return redirect("https://www.patreon.com/oauth2/authorize?" + urllib.parse.urlencode(query))


def _patreon_api_request(url: str, access_token: str) -> dict[str, Any]:
    req = urllib.request.Request(url)
    req.add_header("Authorization", f"Bearer {access_token}")
    req.add_header("User-Agent", "SekaiLink/1.0")
    req.add_header("Accept", "application/json")
    with urllib.request.urlopen(req, timeout=10) as resp:
        return json.loads(resp.read().decode("utf-8"))


def _patreon_extract_supporter(payload: dict[str, Any]) -> dict[str, Any]:
    data = payload.get("data") or {}
    included = payload.get("included") or []
    patreon_id = data.get("id")
    email = (data.get("attributes") or {}).get("email")

    memberships = (data.get("relationships") or {}).get("memberships", {}).get("data", []) or []
    member_ids = {m.get("id") for m in memberships if isinstance(m, dict)}
    member = next((inc for inc in included if inc.get("type") in {"member", "members"} and inc.get("id") in member_ids), None)

    tier_title = None
    patron_status = None
    last_charge_status = None
    member_id = None

    if member:
        member_id = member.get("id")
        attrs = member.get("attributes") or {}
        patron_status = attrs.get("patron_status")
        last_charge_status = attrs.get("last_charge_status")
        tier_ids = {
            t.get("id")
            for t in (member.get("relationships") or {}).get("currently_entitled_tiers", {}).get("data", []) or []
            if isinstance(t, dict)
        }
        if tier_ids:
            tier = next((inc for inc in included if inc.get("type") == "tier" and inc.get("id") in tier_ids), None)
            if tier:
                tier_title = (tier.get("attributes") or {}).get("title")

    is_supporter = patron_status == "active_patron" and (last_charge_status in (None, "Paid", "paid", "successful"))
    return {
        "patreon_id": patreon_id,
        "patreon_email": email,
        "patreon_member_id": member_id,
        "patreon_status": patron_status,
        "patreon_tier": tier_title,
        "patreon_is_supporter": bool(is_supporter),
        "raw": payload,
    }


@app.route("/api/auth/patreon/callback")
def patreon_callback():
    if not session.get("discord_user"):
        return redirect(url_for("discord_login"))
    cfg = _get_patreon_oauth_config()
    code = request.args.get("code")
    state = request.args.get("state")
    if not code or not state or state != session.get("patreon_oauth_state"):
        session.pop("patreon_oauth_state", None)
        return redirect("/")

    token_data = urllib.parse.urlencode({
        "grant_type": "authorization_code",
        "code": code,
        "client_id": cfg["client_id"],
        "client_secret": cfg["client_secret"],
        "redirect_uri": cfg["redirect_uri"],
    }).encode("utf-8")
    token_req = urllib.request.Request(
        "https://www.patreon.com/api/oauth2/token",
        data=token_data,
        method="POST",
    )
    token_req.add_header("Content-Type", "application/x-www-form-urlencoded")
    token_req.add_header("User-Agent", "SekaiLink/1.0")
    token_req.add_header("Accept", "application/json")
    try:
        with urllib.request.urlopen(token_req, timeout=10) as resp:
            token_payload = json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError:
        session.pop("patreon_oauth_state", None)
        return redirect("/")
    except urllib.error.URLError:
        session.pop("patreon_oauth_state", None)
        return redirect("/")

    access_token = token_payload.get("access_token")
    if not access_token:
        session.pop("patreon_oauth_state", None)
        return redirect("/")

    identity_url = (
        "https://www.patreon.com/api/oauth2/v2/identity"
        "?include=memberships,memberships.currently_entitled_tiers"
        "&fields%5Buser%5D=email,full_name"
        "&fields%5Bmember%5D=patron_status,last_charge_status,currently_entitled_amount_cents,pledge_relationship_start"
        "&fields%5Btier%5D=title"
    )
    try:
        identity_payload = _patreon_api_request(identity_url, access_token)
    except urllib.error.HTTPError:
        session.pop("patreon_oauth_state", None)
        return redirect("/")
    except urllib.error.URLError:
        session.pop("patreon_oauth_state", None)
        return redirect("/")

    support_info = _patreon_extract_supporter(identity_payload)
    session.pop("patreon_oauth_state", None)
    discord_id = session["discord_user"].get("id")
    if not discord_id:
        return redirect("/")
    with db_session:
        user = User.get(discord_id=discord_id)
        if user:
            user.patreon_id = support_info["patreon_id"]
            user.patreon_email = support_info["patreon_email"]
            user.patreon_member_id = support_info["patreon_member_id"]
            user.patreon_status = support_info["patreon_status"]
            user.patreon_tier = support_info["patreon_tier"]
            user.patreon_is_supporter = support_info["patreon_is_supporter"]
            user.patreon_last_sync = datetime.datetime.utcnow()
            user.patreon_raw = json.dumps(support_info["raw"])[:20000]
    return redirect("/account")


@app.route("/api/patreon/webhook", methods=["POST"])
def patreon_webhook():
    secret = app.config.get("PATREON_WEBHOOK_SECRET")
    if not secret:
        return jsonify({"error": "Webhook not configured."}), 400
    signature = request.headers.get("X-Patreon-Signature", "")
    raw_body = request.get_data(cache=False, as_text=False) or b""
    computed = hmac.new(secret.encode("utf-8"), raw_body, hashlib.md5).hexdigest()
    if not hmac.compare_digest(signature, computed):
        return jsonify({"error": "Invalid signature."}), 403
    payload = request.get_json(silent=True) or {}

    data = payload.get("data") or {}
    included = payload.get("included") or []
    member = None
    if data.get("type") in {"member", "members"}:
        member = data
    elif data.get("type") == "user":
        rel = (data.get("relationships") or {}).get("memberships", {}).get("data", [])
        member_ids = {m.get("id") for m in rel if isinstance(m, dict)}
        member = next((inc for inc in included if inc.get("type") in {"member", "members"} and inc.get("id") in member_ids), None)

    if not member:
        return jsonify({"status": "ignored"}), 200

    member_id = member.get("id")
    attrs = member.get("attributes") or {}
    patron_status = attrs.get("patron_status")
    last_charge_status = attrs.get("last_charge_status")
    relationships = member.get("relationships") or {}
    patron_rel = (relationships.get("user") or relationships.get("patron") or {}).get("data") or {}
    patreon_id = patron_rel.get("id")

    tier_title = None
    tier_ids = {
        t.get("id")
        for t in (member.get("relationships") or {}).get("currently_entitled_tiers", {}).get("data", []) or []
        if isinstance(t, dict)
    }
    if tier_ids:
        tier = next((inc for inc in included if inc.get("type") == "tier" and inc.get("id") in tier_ids), None)
        if tier:
            tier_title = (tier.get("attributes") or {}).get("title")

    is_supporter = patron_status == "active_patron" and (last_charge_status in (None, "Paid", "paid", "successful"))

    with db_session:
        user = None
        if patreon_id:
            user = User.get(patreon_id=patreon_id)
        if not user and member_id:
            user = User.get(patreon_member_id=member_id)
        if user:
            user.patreon_id = patreon_id or user.patreon_id
            user.patreon_member_id = member_id or user.patreon_member_id
            user.patreon_status = patron_status
            user.patreon_tier = tier_title
            user.patreon_is_supporter = bool(is_supporter)
            user.patreon_last_sync = datetime.datetime.utcnow()
            try:
                user.patreon_raw = json.dumps(payload)[:20000]
            except Exception:
                pass

    return jsonify({"status": "ok"})


@app.route("/banned")
def banned_page():
    reason = session.get("ban_reason", "No reason provided.")
    return render_template("banned.html", ban_reason=reason)


@app.route("/api/support/ban-appeal", methods=["POST"])
def ban_appeal():
    if not session.get("discord_user"):
        return abort(401)
    payload = request.get_json(silent=True) or {}
    message = (payload.get("message") or "").strip()
    if len(message) < 20:
        return jsonify({"error": "Please provide at least 20 characters."}), 400
    if len(message) > 2000:
        return jsonify({"error": "Message is too long."}), 400
    discord_id = session["discord_user"].get("id")
    if not discord_id:
        return abort(400)
    with db_session:
        user = User.get(discord_id=discord_id)
        if not user:
            return abort(401)
        if not user.banned:
            return jsonify({"error": "Your account is not banned."}), 400
        from .models import SupportTicket
        SupportTicket(
            user=user,
            category="ban_appeal",
            status="open",
            subject="Ban appeal",
            message=message,
        )
    return jsonify({"status": "ok"})


@app.route("/help")
def help_page():
    return render_template("help.html")


@app.route("/api/support/tickets", methods=["POST"])
def support_ticket_create():
    user_id = _require_user()
    payload = request.get_json(silent=True) or {}
    category = (payload.get("category") or "general").strip().lower()
    subject = (payload.get("subject") or "").strip()
    message = (payload.get("message") or "").strip()
    if category not in {"general", "account", "lobby", "generation", "bug", "billing"}:
        return jsonify({"error": "Invalid category."}), 400
    if len(subject) < 4 or len(subject) > 120:
        return jsonify({"error": "Subject must be 4-120 characters."}), 400
    if len(message) < 20:
        return jsonify({"error": "Please provide at least 20 characters."}), 400
    if len(message) > 4000:
        return jsonify({"error": "Message is too long."}), 400
    with db_session:
        user = User.get(discord_id=user_id)
        if not user:
            return abort(401)
        ticket = SupportTicket(
            user=user,
            category=category,
            status="open",
            subject=subject,
            message=message,
        )
        socketio.emit(
            "support_ticket_update",
            {
                "ticket_id": ticket.id,
                "status": ticket.status,
                "subject": ticket.subject or "Support ticket",
            },
            to=_user_room(user.discord_id),
        )
    return jsonify({"status": "ok"})


@app.route("/api/auth/terms", methods=["POST"])
def discord_terms():
    if not session.get("discord_user"):
        return abort(401)
    payload = request.get_json(silent=True) or {}
    if payload.get("accept") is not True:
        return abort(400)
    discord_id = session["discord_user"].get("id")
    if not discord_id:
        return abort(400)
    current_version = app.config.get("TERMS_VERSION", "v1")
    ip = request.headers.get("X-Forwarded-For", request.remote_addr)
    user_agent = request.headers.get("User-Agent")
    with db_session:
        user = User.get(discord_id=discord_id)
        if not user:
            user = User(discord_id=discord_id, terms_accepted=True, terms_accepted_at=datetime.datetime.utcnow(),
                        terms_version=current_version)
        else:
            user.terms_accepted = True
            user.terms_accepted_at = datetime.datetime.utcnow()
            user.terms_version = current_version
        user.terms_accepted_ip = ip
        user.terms_accepted_user_agent = user_agent
        session["terms_accepted"] = True
        session["terms_version"] = current_version
    return Response(status=204)


def _parse_badges(raw: str | None) -> str:
    if not raw:
        return ""
    try:
        if raw.strip().startswith("["):
            arr = json.loads(raw)
            if isinstance(arr, list):
                return ", ".join(str(x).strip() for x in arr if str(x).strip())
    except Exception:
        pass
    return raw


def _normalize_badges(raw: str) -> str:
    badges = [b.strip() for b in (raw or "").split(",") if b.strip()]
    badges = badges[:24]
    return json.dumps(badges)


def _avatars_dir() -> str:
    base = os.path.join(os.path.dirname(__file__), "static", "static", "avatars")
    os.makedirs(base, exist_ok=True)
    return base


def _save_avatar_upload(user_id: str, storage) -> str | None:
    if not storage or not getattr(storage, "filename", ""):
        return None
    filename = secure_filename(storage.filename)
    _, ext = os.path.splitext(filename.lower())
    if ext not in {".png", ".jpg", ".jpeg", ".webp", ".gif"}:
        return None
    dst_name = f"{user_id}{ext}"
    dst_path = os.path.join(_avatars_dir(), dst_name)
    storage.save(dst_path)
    return f"/static/static/avatars/{dst_name}?v={int(time.time())}"


@app.route("/account")
def account():
    if not session.get("discord_user"):
        return redirect(url_for("auth_page"))
    discord_id = session["discord_user"].get("id")
    user = None
    yaml_entries = []
    profile = {
        "bio": "",
        "badges": "",
        "has_password": False,
    }
    if discord_id:
        with db_session:
            user = User.get(discord_id=discord_id)
            if user and user.banned:
                return abort(403)
            if user:
                identity = AuthIdentity.get(user=user, provider="email")
                profile["bio"] = user.bio or ""
                profile["badges"] = _parse_badges(user.badges_json)
                profile["has_password"] = bool(identity and identity.password_hash)
                user_dir = _get_user_yaml_dir(user)
                for filename in os.listdir(user_dir):
                    if not filename.endswith(".yaml"):
                        continue
                    path = os.path.join(user_dir, filename)
                    yaml_id = filename[:-5]
                    title = filename
                    game = ""
                    player_name = ""
                    custom_flag = False
                    try:
                        with open(path, "r", encoding="utf-8") as f:
                            content = yaml.safe_load(f)
                        player_name = content.get("name", "")
                        game = content.get("game", "")
                        title = (content.get("sekailink") or {}).get("title") or title
                        custom_entry = UserYaml.get(user=user, yaml_id=yaml_id)
                        custom_flag = bool(custom_entry.custom) if custom_entry else False
                    except Exception:
                        pass
                    yaml_entries.append({
                        "id": yaml_id,
                        "title": title,
                        "game": game,
                        "player_name": player_name,
                        "custom": custom_flag,
                    })
    return render_template(
        "account.html",
        user=user,
        profile=profile,
        terms_version=app.config.get("TERMS_VERSION", "v1"),
        yamls=yaml_entries,
    )




def _coerce_bool(value):
    if isinstance(value, bool):
        return value
    if value is None:
        return None
    if isinstance(value, (int, float)):
        return bool(value)
    raw = str(value).strip().lower()
    if raw in {"1", "true", "yes", "on"}:
        return True
    if raw in {"0", "false", "no", "off"}:
        return False
    return None


def _extract_twitch_from_raw_profile(user: User) -> tuple[str, bool]:
    try:
        payload = json.loads(user.raw_profile or "{}")
        if not isinstance(payload, dict):
            return "", False
        if payload.get("provider") != "twitch":
            return "", False
        info = payload.get("payload") or {}
        if not isinstance(info, dict):
            return "", True
        twitch_name = (info.get("login") or info.get("display_name") or "").strip()
        return twitch_name, True
    except Exception:
        return "", False


@app.route("/api/me/profile", methods=["GET", "POST"])
def api_me_profile():
    user_id = _require_user()

    if request.method == "GET":
        with db_session:
            user = User.get(discord_id=user_id)
            if not user:
                return abort(401)
            twitch_identity = AuthIdentity.get(user=user, provider="twitch")
            twitch_name, raw_is_twitch = _extract_twitch_from_raw_profile(user)
            return jsonify({
                "display_name": user.global_name or user.username or "",
                "bio": user.bio or "",
                "badges": _parse_badges(user.badges_json),
                "avatar_url": _avatar_url(user),
                "twitch_username": twitch_name,
                "twitch_linked": bool(twitch_identity or raw_is_twitch),
            })

    payload = request.get_json(silent=True) or request.form or {}
    display_name = str(payload.get("display_name") or "").strip()
    bio = str(payload.get("bio") or "").strip()
    badges_raw = payload.get("badges")
    twitch_linked = _coerce_bool(payload.get("twitch_linked"))

    if display_name and len(display_name) > 64:
        return jsonify({"error": "Display name is too long."}), 400
    if len(bio) > 1200:
        return jsonify({"error": "Bio is too long."}), 400

    with db_session:
        user = User.get(discord_id=user_id)
        if not user:
            return abort(401)

        if display_name:
            user.global_name = display_name
        user.bio = bio

        if badges_raw is not None:
            if isinstance(badges_raw, list):
                badges_raw = ",".join(str(x).strip() for x in badges_raw if str(x).strip())
            user.badges_json = _normalize_badges(str(badges_raw))

        twitch_identity = AuthIdentity.get(user=user, provider="twitch")
        if twitch_linked is False and twitch_identity:
            identities_count = select(i for i in AuthIdentity if i.user == user).count()
            if identities_count <= 1:
                return jsonify({"error": "Cannot unlink the only sign-in provider."}), 400
            twitch_identity.delete()

        commit()

        twitch_identity = AuthIdentity.get(user=user, provider="twitch")
        twitch_name, raw_is_twitch = _extract_twitch_from_raw_profile(user)
        return jsonify({
            "status": "ok",
            "display_name": user.global_name or user.username or "",
            "bio": user.bio or "",
            "badges": _parse_badges(user.badges_json),
            "avatar_url": _avatar_url(user),
            "twitch_username": twitch_name,
            "twitch_linked": bool(twitch_identity or raw_is_twitch),
            "tutorial": bool(user.tutorial),
        })


@app.route("/api/account/profile", methods=["POST"])
def account_profile_update():
    user_id = _require_user()
    bio = (request.form.get("bio") or "").strip()
    badges = (request.form.get("badges") or "").strip()
    if len(bio) > 1200:
        return jsonify({"error": "Bio is too long."}), 400

    with db_session:
        user = User.get(discord_id=user_id)
        if not user:
            return abort(401)
        user.bio = bio
        user.badges_json = _normalize_badges(badges)
        avatar_file = request.files.get("avatar")
        new_avatar = _save_avatar_upload(user_id, avatar_file)
        if avatar_file and not new_avatar:
            return jsonify({"error": "Invalid avatar format."}), 400
        if new_avatar:
            user.custom_avatar_url = new_avatar
            if session.get("discord_user"):
                session["discord_user"]["avatar_url"] = new_avatar
        commit()
    return jsonify({"status": "ok"})


@app.route("/api/account/password", methods=["POST"])
def account_password_update():
    user_id = _require_user()
    current_password = request.form.get("current_password") or ""
    new_password = request.form.get("new_password") or ""
    recs = _password_recommendations(new_password)
    if recs:
        return jsonify({"error": " ".join(recs)}), 400

    with db_session:
        user = User.get(discord_id=user_id)
        if not user:
            return abort(401)
        identity = AuthIdentity.get(user=user, provider="email")
        if not identity:
            if not user.email:
                return jsonify({"error": "No email account linked to this profile."}), 400
            identity = AuthIdentity(
                user=user,
                provider="email",
                email=user.email,
                email_verified=True,
                preferred_locale="en",
            )
        if identity.password_hash and not check_password_hash(identity.password_hash, current_password):
            return jsonify({"error": "Current password is invalid."}), 400

        identity.password_hash = generate_password_hash(new_password)
        identity.last_login = datetime.datetime.utcnow()
        user.password_updated_at = datetime.datetime.utcnow()
        commit()

    return jsonify({"status": "ok"})


@app.route("/api/social/settings", methods=["GET", "POST"])
def social_settings():
    user_id = _require_user()
    if request.method == "GET":
        with db_session:
            user = User.get(discord_id=user_id)
            if not user:
                return abort(401)
            return jsonify({
                "presence_status": user.presence_status,
                "dm_policy": user.dm_policy,
            })
    payload = request.get_json(silent=True) or {}
    presence_status = (payload.get("presence_status") or "").strip().lower()
    dm_policy = (payload.get("dm_policy") or "").strip().lower()
    with db_session:
        user = User.get(discord_id=user_id)
        if not user:
            return abort(401)
        changed = False
        if presence_status in {"online", "offline", "dnd"} and presence_status != user.presence_status:
            user.presence_status = presence_status
            user.presence_updated_at = datetime.datetime.utcnow()
            changed = True
        if dm_policy in {"anyone", "friends", "none"} and dm_policy != user.dm_policy:
            user.dm_policy = dm_policy
            changed = True
        if changed:
            _emit_presence_update(user)
    return jsonify({"status": "ok"})


@app.route("/api/me")
def api_me():
    user_id = _require_user()
    with db_session:
        user = User.get(discord_id=user_id)
        if not user:
            return abort(401)
        terms_version = app.config.get("TERMS_VERSION", "v1")
        return jsonify({
            "discord_id": user.discord_id,
            "username": user.username,
            "global_name": user.global_name,
            "email": user.email,
            "avatar_url": _avatar_url(user),
            "terms_accepted": bool(user.terms_accepted),
            "terms_version": user.terms_version or terms_version,
            "terms_accepted_at": user.terms_accepted_at.isoformat() if user.terms_accepted_at else None,
            "presence_status": user.presence_status,
            "dm_policy": user.dm_policy,
            "tutorial": bool(user.tutorial),
        })


@app.route("/api/me/tutorial", methods=["POST"])
def api_me_tutorial():
    user_id = _require_user()
    payload = request.get_json(silent=True) or {}
    tutorial_value = _coerce_bool(payload.get("tutorial"))
    if tutorial_value is None:
        tutorial_value = False

    with db_session:
        user = User.get(discord_id=user_id)
        if not user:
            return abort(401)
        user.tutorial = bool(tutorial_value)
        if user.tutorial:
            user.tutorial_completed_at = None
        else:
            user.tutorial_completed_at = datetime.datetime.utcnow()
        commit()

    return jsonify({"status": "ok", "tutorial": bool(tutorial_value)})


@app.route("/api/online-users")
def api_online_users():
    """List of users currently online in SekaiLink (global presence).

    Note: requires auth to avoid leaking user presence to unauthenticated clients.
    """
    _require_user()
    with db_session:
        users = select(u for u in User if (not u.banned) and u.is_online and u.presence_status != "offline")\
            .order_by(desc(User.last_seen_at))[:250]
        data = []
        for user in users:
            data.append({
                "user_id": user.discord_id,
                "display_name": _display_name(user),
                "avatar_url": _avatar_url(user),
                "status": _effective_presence(user),
                "presence_status": user.presence_status,
                "last_seen_at": user.last_seen_at.isoformat() if user.last_seen_at else None,
            })
    return jsonify({"users": data})


@app.route("/api/social/friends")
def social_friends():
    user_id = _require_user()
    with db_session:
        user = User.get(discord_id=user_id)
        friends = select(f.friend for f in Friendship if f.user == user)
        data = []
        for friend in friends:
            status = _effective_presence(friend)
            data.append({
                "user_id": friend.discord_id,
                "display_name": _display_name(friend),
                "avatar_url": _avatar_url(friend),
                "status": status,
                "is_online": status == "online",
                "last_seen_at": friend.last_seen_at.isoformat() if friend.last_seen_at else None,
            })
    return jsonify({"friends": data})


@app.route("/api/social/requests")
def social_requests():
    user_id = _require_user()
    with db_session:
        user = User.get(discord_id=user_id)
        incoming = select(r for r in FriendRequest if r.to_user == user and r.status == "pending")\
            .order_by(desc(FriendRequest.created_at))[:200]
        outgoing = select(r for r in FriendRequest if r.from_user == user and r.status == "pending")\
            .order_by(desc(FriendRequest.created_at))[:200]
        return jsonify({
            "incoming": [
                {
                    "id": req.id,
                    "from_id": req.from_user.discord_id,
                    "from_name": _display_name(req.from_user),
                    "avatar_url": _avatar_url(req.from_user),
                    "created_at": req.created_at.isoformat(),
                } for req in incoming
            ],
            "outgoing": [
                {
                    "id": req.id,
                    "to_id": req.to_user.discord_id,
                    "to_name": _display_name(req.to_user),
                    "avatar_url": _avatar_url(req.to_user),
                    "created_at": req.created_at.isoformat(),
                } for req in outgoing
            ],
        })


@app.route("/api/social/friends/request", methods=["POST"])
def social_friend_request():
    user_id = _require_user()
    payload = request.get_json(silent=True) or {}
    target_id = (payload.get("user_id") or "").strip()
    if not target_id:
        return jsonify({"error": "Missing user id."}), 400
    if target_id == user_id:
        return jsonify({"error": "You cannot add yourself."}), 400
    with db_session:
        user = User.get(discord_id=user_id)
        target = User.get(discord_id=target_id)
        if not target:
            return jsonify({"error": "User not found."}), 404
        if _are_friends(user, target):
            return jsonify({"error": "You are already friends."}), 400
        existing = FriendRequest.get(from_user=user, to_user=target, status="pending")
        reverse = FriendRequest.get(from_user=target, to_user=user, status="pending")
        if existing:
            return jsonify({"error": "Friend request already sent."}), 400
        if reverse:
            return jsonify({"error": "This user already sent you a request."}), 400
        req = FriendRequest(from_user=user, to_user=target, status="pending")
        socketio.emit(
            "friend_request",
            {
                "id": req.id,
                "from_id": user.discord_id,
                "from_name": _display_name(user),
            },
            to=_user_room(target.discord_id),
        )
    return jsonify({"status": "ok"})


@app.route("/api/social/friends/respond", methods=["POST"])
def social_friend_respond():
    user_id = _require_user()
    payload = request.get_json(silent=True) or {}
    request_id = payload.get("request_id")
    action = (payload.get("action") or "").strip().lower()
    if action not in {"accept", "decline"}:
        return jsonify({"error": "Invalid action."}), 400
    with db_session:
        req = FriendRequest.get(id=request_id)
        if not req or req.to_user.discord_id != user_id:
            return abort(404)
        if req.status != "pending":
            return jsonify({"error": "Request already handled."}), 400
        if action == "decline":
            req.status = "declined"
            return jsonify({"status": "ok"})
        req.status = "accepted"
        if not _are_friends(req.from_user, req.to_user):
            Friendship(user=req.from_user, friend=req.to_user)
        if not _are_friends(req.to_user, req.from_user):
            Friendship(user=req.to_user, friend=req.from_user)
        socketio.emit(
            "friend_added",
            {
                "user_id": req.to_user.discord_id,
                "display_name": _display_name(req.to_user),
            },
            to=_user_room(req.from_user.discord_id),
        )
    return jsonify({"status": "ok"})


@app.route("/api/social/friends/remove", methods=["POST"])
def social_friend_remove():
    user_id = _require_user()
    payload = request.get_json(silent=True) or {}
    target_id = (payload.get("user_id") or "").strip()
    if not target_id:
        return jsonify({"error": "Missing user id."}), 400
    with db_session:
        user = User.get(discord_id=user_id)
        target = User.get(discord_id=target_id)
        if not target:
            return jsonify({"error": "User not found."}), 404
        link = Friendship.get(user=user, friend=target)
        if link:
            link.delete()
        reverse = Friendship.get(user=target, friend=user)
        if reverse:
            reverse.delete()
    return jsonify({"status": "ok"})


@app.route("/api/social/messages", methods=["GET", "POST"])
def social_messages():
    user_id = _require_user()
    if request.method == "GET":
        target_id = (request.args.get("user_id") or "").strip()
        if not target_id:
            return jsonify({"error": "Missing user id."}), 400
        with db_session:
            user = User.get(discord_id=user_id)
            target = User.get(discord_id=target_id)
            if target_id == "support" and not target:
                target = _get_support_user()
            if not target:
                return jsonify({"error": "User not found."}), 404
            messages = select(
                m for m in DirectMessage
                if (m.sender == user and m.recipient == target)
                or (m.sender == target and m.recipient == user)
            ).order_by(desc(DirectMessage.created_at))[:50]
            data = []
            for message in reversed(list(messages)):
                data.append({
                    "id": message.id,
                    "direction": "out" if message.sender == user else "in",
                    "content": message.content,
                    "created_at": message.created_at.isoformat(),
                    "read": message.read_at is not None,
                })
        return jsonify({"messages": data, "user": {"id": target.discord_id, "name": _display_name(target)}})
    payload = request.get_json(silent=True) or {}
    target_id = (payload.get("user_id") or "").strip()
    content = (payload.get("content") or "").strip()
    if not target_id or not content:
        return jsonify({"error": "Missing message content."}), 400
    if len(content) > 2000:
        return jsonify({"error": "Message is too long."}), 400
    with db_session:
        user = User.get(discord_id=user_id)
        target = User.get(discord_id=target_id)
        if target_id == "support" and not target:
            target = _get_support_user()
        if not target:
            return jsonify({"error": "User not found."}), 404
        if target.dm_policy == "none":
            return jsonify({"error": "This user does not accept messages."}), 403
        if target.dm_policy == "friends" and not _are_friends(target, user):
            return jsonify({"error": "This user only accepts messages from friends."}), 403
        msg = DirectMessage(sender=user, recipient=target, content=content)
        socketio.emit(
            "dm_new",
            {
                "from_id": user.discord_id,
                "from_name": _display_name(user),
                "content": msg.content,
                "created_at": msg.created_at.isoformat(),
            },
            to=_user_room(target.discord_id),
        )
    return jsonify({"status": "ok"})


@app.route("/api/social/messages/read", methods=["POST"])
def social_messages_read():
    user_id = _require_user()
    payload = request.get_json(silent=True) or {}
    target_id = (payload.get("user_id") or "").strip()
    if not target_id:
        return jsonify({"error": "Missing user id."}), 400
    with db_session:
        user = User.get(discord_id=user_id)
        target = User.get(discord_id=target_id)
        if not target:
            return jsonify({"error": "User not found."}), 404
        for message in select(m for m in DirectMessage if m.sender == target and m.recipient == user and m.read_at is None):
            message.read_at = datetime.datetime.utcnow()
    return jsonify({"status": "ok"})


@app.route("/api/social/unread-count")
def social_unread_count():
    user_id = _require_user()
    with db_session:
        user = User.get(discord_id=user_id)
        count_unread = count(m for m in DirectMessage if m.recipient == user and m.read_at is None)
    return jsonify({"unread": int(count_unread)})


@app.route("/admin")
def admin_page():
    _require_admin_user()
    return render_template("admin.html")


@app.route("/api/admin/users")
def admin_users():
    _require_admin_user()
    search = (request.args.get("search") or "").strip().lower()
    with db_session:
        users = select(u for u in User).order_by(desc(User.last_login))[:500]
        data = []
        for user in users:
            display_name = user.global_name or user.username or user.discord_id
            if search:
                haystack = " ".join([
                    (user.username or "").lower(),
                    (user.global_name or "").lower(),
                    (user.email or "").lower(),
                    user.discord_id.lower(),
                ])
                if search not in haystack:
                    continue
            data.append({
                "discord_id": user.discord_id,
                "display_name": display_name,
                "role": user.role,
                "banned": bool(user.banned),
                "ban_reason": user.ban_reason or "",
                "last_login": user.last_login.isoformat() if user.last_login else None,
            })
    return jsonify({"users": data})


@app.route("/api/admin/users/<discord_id>/role", methods=["POST"])
def admin_set_user_role(discord_id):
    admin_id = _require_admin_user()
    payload = request.get_json(silent=True) or {}
    role = (payload.get("role") or "").strip().lower()
    if role not in {"admin", "moderator", "player"}:
        return jsonify({"error": "Invalid role."}), 400
    with db_session:
        target = User.get(discord_id=discord_id)
        if not target:
            return abort(404)
        if target.discord_id == admin_id and role != "admin":
            return jsonify({"error": "You cannot remove your own admin role."}), 400
        target.role = role
    return jsonify({"status": "ok"})


@app.route("/api/admin/users/<discord_id>/ban", methods=["POST"])
def admin_set_user_ban(discord_id):
    admin_id = _require_admin_user()
    payload = request.get_json(silent=True) or {}
    banned = bool(payload.get("banned"))
    reason = (payload.get("reason") or "").strip()
    if len(reason) > 255:
        return jsonify({"error": "Ban reason must be 255 characters or less."}), 400
    with db_session:
        target = User.get(discord_id=discord_id)
        if not target:
            return abort(404)
        if target.discord_id == admin_id and banned:
            return jsonify({"error": "You cannot ban yourself."}), 400
        target.banned = banned
        target.ban_reason = reason or None
    return jsonify({"status": "ok"})


@app.route("/api/admin/lobbies")
def admin_lobbies():
    _require_admin_user()
    with db_session:
        lobbies = select(l for l in Lobby).order_by(desc(Lobby.last_activity))[:200]
        data = []
        for lobby in lobbies:
            member_count = count(m for m in LobbyMember if m.lobby == lobby)
            latest = select(lg for lg in LobbyGeneration if lg.lobby == lobby).order_by(desc(LobbyGeneration.created_at)).first()
            room_id = to_url(latest.room_id) if latest and latest.room_id else None
            data.append({
                "id": to_url(lobby.id),
                "name": lobby.name,
                "owner": lobby.owner.global_name or lobby.owner.username or lobby.owner.discord_id,
                "member_count": int(member_count),
                "last_activity": lobby.last_activity.isoformat(),
                "room_id": room_id,
            })
    return jsonify({"lobbies": data})


@app.route("/api/admin/lobbies/<suuid:lobby_id>")
def admin_lobby_detail(lobby_id):
    _require_admin_user()
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        latest = select(lg for lg in LobbyGeneration if lg.lobby == lobby).order_by(desc(LobbyGeneration.created_at)).first()
        room_id = to_url(latest.room_id) if latest and latest.room_id else None
        members = select(m for m in LobbyMember if m.lobby == lobby).order_by(LobbyMember.joined_at)[:200]
        messages = select(m for m in LobbyMessage if m.lobby == lobby).order_by(desc(LobbyMessage.created_at))[:8]
        settings_summary = (
            f"Release {lobby.release_mode}, Collect {lobby.collect_mode}, Remaining {lobby.remaining_mode}, "
            f"Countdown {lobby.countdown_mode}, Hint {lobby.hint_cost}%, Spoiler {lobby.spoiler}, "
            f"Item cheat {'on' if lobby.item_cheat else 'off'}, "
            f"Plando items {'on' if lobby.plando_items else 'off'}, "
            f"Bosses {'on' if lobby.plando_bosses else 'off'}, "
            f"Connections {'on' if lobby.plando_connections else 'off'}, "
            f"Text {'on' if lobby.plando_texts else 'off'}."
        )
        return jsonify({
            "id": to_url(lobby.id),
            "name": lobby.name,
            "description": lobby.description or "",
            "owner": lobby.owner.global_name or lobby.owner.username or lobby.owner.discord_id,
            "room_id": room_id,
            "settings_summary": settings_summary,
            "members": [{"name": m.user.global_name or m.user.username or m.user.discord_id} for m in members],
            "recent_messages": [
                {
                    "author": msg.author_name,
                    "content": (msg.content or "")[:140],
                    "created_at": msg.created_at.isoformat(),
                } for msg in messages
            ],
        })


@app.route("/api/admin/lobbies/<suuid:lobby_id>/close", methods=["POST"])
def admin_close_lobby(lobby_id):
    _require_admin_user()
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        from .lobbies import _shutdown_lobby_room, _add_system_message
        from . import socketio
        _add_system_message(lobby, "Lobby closed by admin.")
        socketio.emit(
            "lobby_closed",
            {"message": "Lobby closed by admin.", "redirect_url": "/"},
            to=f"lobby:{to_url(lobby.id)}",
        )
        _shutdown_lobby_room(lobby)
        lobby.delete()
    return jsonify({"status": "ok"})


@app.route("/api/admin/rooms")
def admin_rooms():
    _require_admin_user()
    from .api.monitoring import is_room_active
    with db_session:
        rooms = select(r for r in Room).order_by(desc(Room.last_activity))[:200]
        data = []
        for room in rooms:
            if room.last_port == -1:
                status = "Error"
            elif is_room_active(room):
                status = "Active"
            elif room.last_port and room.last_port > 0:
                status = "Idle"
            else:
                status = "Stopped"
            data.append({
                "id": to_url(room.id),
                "owner": to_url(room.owner) if isinstance(room.owner, UUID) else str(room.owner),
                "last_port": room.last_port or 0,
                "last_activity": room.last_activity.isoformat(),
                "seed_id": to_url(room.seed.id) if room.seed else "",
                "log_file": f"{room.id}.txt",
                "status": status,
            })
    return jsonify({"rooms": data})


@app.route("/api/admin/rooms/<suuid:room_id>/close", methods=["POST"])
def admin_close_room(room_id):
    _require_admin_user()
    with db_session:
        room = Room.get(id=room_id)
        if not room:
            return abort(404)
        Command(room=room, commandtext="/exit")
        room.last_port = 0
        room.last_activity = datetime.datetime.utcnow() - datetime.timedelta(seconds=room.timeout + 60)
    return jsonify({"status": "ok"})


@app.route("/api/admin/rooms/purge", methods=["POST"])
def admin_purge_rooms():
    admin_id = _require_admin_user()
    with db_session:
        rooms = select(r for r in Room if r.last_port is not None and r.last_port > 0)
        count = 0
        now = datetime.datetime.utcnow()
        for room in rooms:
            Command(room=room, commandtext="/exit")
            room.last_port = 0
            room.last_activity = now - datetime.timedelta(seconds=room.timeout + 60)
            count += 1
    _admin_audit(admin_id, "rooms.purge", "ok", {"closed": count})
    return jsonify({"status": "ok", "closed": count})


_ADMIN_SERVICE_UNITS = {
    "webhost": "multiworldgg-webhost",
    "workers": "multiworldgg-workers",
}


def _admin_audit(admin_id: str, action: str, status: str, details: dict[str, Any] | None = None) -> None:
    try:
        logs_dir = os.path.abspath("logs")
        os.makedirs(logs_dir, exist_ok=True)
        payload = {
            "timestamp": datetime.datetime.utcnow().isoformat() + "Z",
            "admin_id": admin_id,
            "action": action,
            "status": status,
            "details": details or {},
        }
        with open(os.path.join(logs_dir, "admin_audit.log"), "a", encoding="utf-8") as f:
            f.write(json.dumps(payload, ensure_ascii=True) + "\n")
    except Exception:
        app.logger.exception("failed to write admin audit log")


def _run_command(cmd: list[str], timeout: int = 15) -> tuple[int, str, str]:
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, check=False)
        return proc.returncode, proc.stdout.strip(), proc.stderr.strip()
    except Exception as exc:
        return 999, "", str(exc)


def _service_status(unit: str) -> dict[str, Any]:
    rc_active, out_active, err_active = _run_command(["/usr/bin/systemctl", "is-active", unit])
    rc_enabled, out_enabled, _ = _run_command(["/usr/bin/systemctl", "is-enabled", unit])
    rc_show, out_show, err_show = _run_command(["/usr/bin/systemctl", "show", unit, "--property=SubState", "--property=MainPID", "--property=ActiveEnterTimestampMonotonic"])

    substate = ""
    pid = 0
    uptime_seconds = 0
    if rc_show == 0 and out_show:
        values = {}
        for line in out_show.splitlines():
            if "=" not in line:
                continue
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
        substate = values.get("SubState", "")
        try:
            pid = int(values.get("MainPID", "0") or "0")
        except ValueError:
            pid = 0
        try:
            monotonic_us = int(values.get("ActiveEnterTimestampMonotonic", "0") or "0")
            if monotonic_us > 0:
                now_us = int(time.monotonic() * 1_000_000)
                uptime_seconds = max(0, int((now_us - monotonic_us) / 1_000_000))
        except ValueError:
            uptime_seconds = 0

    return {
        "active": out_active if out_active else ("unknown" if rc_active != 0 else ""),
        "enabled": out_enabled if out_enabled else ("unknown" if rc_enabled != 0 else ""),
        "substate": substate,
        "pid": pid,
        "uptime_seconds": uptime_seconds,
        "ok": rc_active == 0 and (out_active == "active"),
        "errors": [e for e in [err_active, err_show] if e],
    }


def _read_meminfo() -> tuple[int, int]:
    total = 0
    available = 0
    try:
        with open("/proc/meminfo", "r", encoding="utf-8") as f:
            for line in f:
                if line.startswith("MemTotal:"):
                    total = int(line.split()[1]) * 1024
                elif line.startswith("MemAvailable:"):
                    available = int(line.split()[1]) * 1024
    except Exception:
        pass
    return total, available


def _system_metrics() -> dict[str, Any]:
    load1, load5, load15 = os.getloadavg()
    mem_total, mem_available = _read_meminfo()
    disk = shutil.disk_usage("/")
    uptime_seconds = 0
    try:
        with open("/proc/uptime", "r", encoding="utf-8") as f:
            uptime_seconds = int(float(f.read().split()[0]))
    except Exception:
        uptime_seconds = 0
    return {
        "cpu_count": os.cpu_count() or 0,
        "loadavg": [round(load1, 3), round(load5, 3), round(load15, 3)],
        "uptime_seconds": uptime_seconds,
        "mem_total_bytes": mem_total,
        "mem_available_bytes": mem_available,
        "disk_total_bytes": int(disk.total),
        "disk_free_bytes": int(disk.free),
    }


def _room_status_value(room: Room) -> str:
    from .api.monitoring import is_room_active
    if room.last_port == -1:
        return "Error"
    if is_room_active(room):
        return "Active"
    if room.last_port and room.last_port > 0:
        return "Idle"
    return "Stopped"


@app.route("/api/admin/me")
def admin_me():
    admin_id = _require_admin_user()
    with db_session:
        user = User.get(discord_id=admin_id)
        if not user:
            return abort(401)
        return jsonify({
            "discord_id": user.discord_id,
            "display_name": user.global_name or user.username or user.discord_id,
            "role": user.role,
            "is_admin": user.role == "admin",
            "email": user.email or "",
            "last_login": user.last_login.isoformat() if user.last_login else None,
        })


@app.route("/api/admin/system/health")
def admin_system_health():
    admin_id = _require_admin_user()
    services: dict[str, Any] = {}
    for key, unit in _ADMIN_SERVICE_UNITS.items():
        status = _service_status(unit)
        status.update({"key": key, "unit": unit})
        services[key] = status
    payload = {
        "timestamp": datetime.datetime.utcnow().isoformat() + "Z",
        "host": app.config.get("HOST_ADDRESS") or os.uname().nodename,
        "app_version": __version__,
        "services": services,
        "metrics": _system_metrics(),
    }
    _admin_audit(admin_id, "system.health", "ok")
    return jsonify(payload)


@app.route("/api/admin/system/services/<string:service_key>/restart", methods=["POST"])
def admin_restart_service(service_key: str):
    admin_id = _require_admin_user()
    key = (service_key or "").strip().lower()
    unit = _ADMIN_SERVICE_UNITS.get(key)
    if not unit:
        return jsonify({"error": "Unknown service."}), 400

    rc, out, err = _run_command(["sudo", "-n", "/usr/bin/systemctl", "restart", unit], timeout=25)
    if rc != 0:
        _admin_audit(admin_id, "system.restart", "error", {"service": key, "stderr": err, "stdout": out, "rc": rc})
        return jsonify({"error": "restart_failed", "stdout": out, "stderr": err, "rc": rc}), 500

    _admin_audit(admin_id, "system.restart", "ok", {"service": key})
    return jsonify({"status": "ok", "service": key})


@app.route("/api/admin/system/journal")
def admin_system_journal():
    _require_admin_user()
    key = (request.args.get("service") or "webhost").strip().lower()
    unit = _ADMIN_SERVICE_UNITS.get(key)
    if not unit:
        return jsonify({"error": "Unknown service."}), 400
    lines = _parse_int_value(request.args.get("lines"), 200, 20, 2000)
    rc, out, err = _run_command(["sudo", "-n", "/usr/bin/journalctl", "-u", unit, "-n", str(lines), "--no-pager"], timeout=20)
    if rc != 0:
        return jsonify({"error": "journal_failed", "stderr": err, "stdout": out, "rc": rc}), 500
    return jsonify({"service": key, "unit": unit, "lines": lines, "content": out})


@app.route("/api/admin/system/reboot", methods=["POST"])
def admin_system_reboot():
    admin_id = _require_admin_user()
    payload = request.get_json(silent=True) or {}
    confirm = (payload.get("confirm") or "").strip().upper()
    reason = (payload.get("reason") or "").strip()
    if confirm != "REBOOT":
        return jsonify({"error": "Missing confirmation token."}), 400
    if len(reason) < 3 or len(reason) > 300:
        return jsonify({"error": "Reason must be 3-300 characters."}), 400

    rc, out, err = _run_command(["sudo", "-n", "/usr/bin/systemctl", "reboot"], timeout=5)
    if rc != 0:
        _admin_audit(admin_id, "system.reboot", "error", {"stderr": err, "stdout": out, "rc": rc, "reason": reason})
        return jsonify({"error": "reboot_failed", "stderr": err, "stdout": out, "rc": rc}), 500

    _admin_audit(admin_id, "system.reboot", "ok", {"reason": reason})
    return jsonify({"status": "ok", "message": "Reboot requested."})


@app.route("/api/admin/rooms/purge-filtered", methods=["POST"])
def admin_purge_rooms_filtered():
    admin_id = _require_admin_user()
    payload = request.get_json(silent=True) or {}
    older_than_minutes = _parse_int_value(payload.get("older_than_minutes"), 180, 0, 60 * 24 * 30)
    statuses_raw = payload.get("status")
    statuses = set()
    if isinstance(statuses_raw, list):
        statuses = {str(v).strip() for v in statuses_raw if str(v).strip()}
    game_filter = (payload.get("game") or "").strip().lower()
    owner_filter = (payload.get("owner") or "").strip().lower()

    now = datetime.datetime.utcnow()
    with db_session:
        rooms = select(r for r in Room if r.last_port is not None and r.last_port > 0)
        matched = 0
        closed = 0
        for room in rooms:
            status_value = _room_status_value(room)
            if statuses and status_value not in statuses:
                continue
            if older_than_minutes > 0:
                age = now - room.last_activity
                if age < datetime.timedelta(minutes=older_than_minutes):
                    continue
            if game_filter:
                games = [str(slot.game or "").lower() for slot in room.seed.slots]
                if not any(game_filter in g for g in games):
                    continue
            if owner_filter:
                owner_value = str(room.owner).lower()
                if owner_filter not in owner_value:
                    continue

            matched += 1
            Command(room=room, commandtext="/exit")
            room.last_port = 0
            room.last_activity = now - datetime.timedelta(seconds=room.timeout + 60)
            closed += 1

    _admin_audit(admin_id, "rooms.purge.filtered", "ok", {
        "matched": matched,
        "closed": closed,
        "older_than_minutes": older_than_minutes,
        "statuses": sorted(statuses),
        "game_filter": game_filter,
        "owner_filter": owner_filter,
    })
    return jsonify({"status": "ok", "matched": matched, "closed": closed})


@app.route("/api/admin/db/<string:entity>")
def admin_db_entity(entity: str):
    _require_admin_user()
    key = (entity or "").strip().lower()
    limit = _parse_int_value(request.args.get("limit"), 100, 1, 500)
    offset = _parse_int_value(request.args.get("offset"), 0, 0, 200000)

    with db_session:
        if key == "users":
            rows = select(u for u in User).order_by(desc(User.last_login))[offset:offset + limit]
            data = [{
                "discord_id": u.discord_id,
                "display_name": u.global_name or u.username or u.discord_id,
                "role": u.role,
                "banned": bool(u.banned),
                "email": u.email or "",
                "last_login": u.last_login.isoformat() if u.last_login else None,
                "created_at": u.created_at.isoformat() if u.created_at else None,
            } for u in rows]
            total = count(u for u in User)
        elif key == "lobbies":
            rows = select(l for l in Lobby).order_by(desc(Lobby.last_activity))[offset:offset + limit]
            data = [{
                "id": to_url(l.id),
                "name": l.name,
                "owner": l.owner.global_name or l.owner.username or l.owner.discord_id,
                "created_at": l.created_at.isoformat(),
                "last_activity": l.last_activity.isoformat(),
                "member_count": int(count(m for m in LobbyMember if m.lobby == l)),
                "is_private": bool(l.server_password),
            } for l in rows]
            total = count(l for l in Lobby)
        elif key == "rooms":
            rows = select(r for r in Room).order_by(desc(Room.last_activity))[offset:offset + limit]
            data = [{
                "id": to_url(r.id),
                "owner": str(r.owner),
                "last_port": r.last_port or 0,
                "last_activity": r.last_activity.isoformat(),
                "creation_time": r.creation_time.isoformat() if r.creation_time else None,
                "timeout": r.timeout,
                "seed_id": to_url(r.seed.id) if r.seed else "",
                "status": _room_status_value(r),
            } for r in rows]
            total = count(r for r in Room)
        elif key == "generations":
            rows = select(g for g in LobbyGeneration).order_by(desc(LobbyGeneration.created_at))[offset:offset + limit]
            data = [{
                "id": int(g.id),
                "lobby_id": to_url(g.lobby.id) if g.lobby else "",
                "status": g.status,
                "created_at": g.created_at.isoformat() if g.created_at else None,
                "completed_at": g.completed_at.isoformat() if g.completed_at else None,
                "seed_id": to_url(g.seed_id) if g.seed_id else "",
                "room_id": to_url(g.room_id) if g.room_id else "",
                "error": (g.error or "")[:500],
            } for g in rows]
            total = count(g for g in LobbyGeneration)
        elif key == "tickets":
            rows = select(t for t in SupportTicket).order_by(desc(SupportTicket.created_at))[offset:offset + limit]
            data = [{
                "id": int(t.id),
                "user": t.user.global_name or t.user.username or t.user.discord_id,
                "user_id": t.user.discord_id,
                "category": t.category,
                "status": t.status,
                "subject": t.subject or "",
                "message": (t.message or "")[:600],
                "created_at": t.created_at.isoformat() if t.created_at else None,
            } for t in rows]
            total = count(t for t in SupportTicket)
        else:
            return jsonify({"error": "Unknown entity."}), 400

    return jsonify({
        "entity": key,
        "offset": offset,
        "limit": limit,
        "total": int(total),
        "rows": data,
    })


def _format_size(num_bytes: int) -> str:
    if num_bytes < 1024:
        return f"{num_bytes} B"
    if num_bytes < 1024 * 1024:
        return f"{num_bytes / 1024:.1f} KB"
    return f"{num_bytes / (1024 * 1024):.1f} MB"


def _parse_int_value(value: Any, default: int, min_value: int, max_value: int) -> int:
    try:
        parsed = int(value)
    except (TypeError, ValueError):
        return default
    return max(min_value, min(parsed, max_value))


def _tail_file(path: str, max_lines: int = 200, max_bytes: int = 200_000) -> str:
    size = os.path.getsize(path)
    read_size = min(size, max_bytes)
    with open(path, "rb") as f:
        if read_size:
            f.seek(-read_size, os.SEEK_END)
        data = f.read().decode("utf-8", errors="replace")
    lines = data.splitlines()
    return "\n".join(lines[-max_lines:])


@app.route("/api/admin/logs")
def admin_logs():
    _require_admin_user()
    logs_dir = os.path.abspath("logs")
    entries = []
    for name in os.listdir(logs_dir):
        if not name.endswith(".txt"):
            continue
        path = os.path.join(logs_dir, name)
        if not os.path.isfile(path):
            continue
        stat = os.stat(path)
        entries.append({
            "name": name,
            "size": _format_size(stat.st_size),
            "updated_at": datetime.datetime.utcfromtimestamp(stat.st_mtime).isoformat() + "Z",
        })
    entries.sort(key=lambda e: e["updated_at"], reverse=True)
    return jsonify({"logs": entries})


@app.route("/api/admin/logs/<path:filename>")
def admin_log_view(filename):
    _require_admin_user()
    safe_name = os.path.basename(filename)
    if safe_name != filename or not safe_name.endswith(".txt"):
        return abort(400)
    logs_dir = os.path.abspath("logs")
    path = os.path.join(logs_dir, safe_name)
    if not os.path.isfile(path):
        return abort(404)
    lines = _parse_int_value(request.args.get("lines"), 200, 20, 2000)
    content = _tail_file(path, max_lines=lines)
    return jsonify({"name": safe_name, "content": content})


@app.route("/api/admin/support")
def admin_support():
    _require_admin_user()
    category = (request.args.get("category") or "").strip().lower()
    with db_session:
        tickets = select(t for t in SupportTicket).order_by(desc(SupportTicket.created_at))[:200]
        data = []
        for ticket in tickets:
            if category and ticket.category != category:
                continue
            display_name = ticket.user.global_name or ticket.user.username or ticket.user.discord_id
            data.append({
                "id": ticket.id,
                "user": display_name,
                "user_id": ticket.user.discord_id,
                "status": ticket.status,
                "category": ticket.category,
                "subject": ticket.subject or "",
                "message": (ticket.message or "")[:280],
                "created_at": ticket.created_at.isoformat(),
            })
    return jsonify({"tickets": data})


@app.route("/api/admin/support/<int:ticket_id>/status", methods=["POST"])
def admin_support_status(ticket_id):
    _require_admin_user()
    payload = request.get_json(silent=True) or {}
    status = (payload.get("status") or "").strip().lower()
    response = (payload.get("response") or "").strip()
    if status not in {"open", "closed"}:
        return jsonify({"error": "Invalid status."}), 400
    if response and len(response) > 2000:
        return jsonify({"error": "Response is too long."}), 400
    with db_session:
        ticket = SupportTicket.get(id=ticket_id)
        if not ticket:
            return abort(404)
        ticket.status = status
        if response:
            support_user = _get_support_user()
            DirectMessage(sender=support_user, recipient=ticket.user, content=response)
            socketio.emit(
                "dm_new",
                {
                    "from_id": support_user.discord_id,
                    "from_name": _display_name(support_user),
                    "content": response,
                    "created_at": datetime.datetime.utcnow().isoformat(),
                },
                to=_user_room(ticket.user.discord_id),
            )
        socketio.emit(
            "support_ticket_update",
            {
                "ticket_id": ticket.id,
                "status": ticket.status,
                "subject": ticket.subject or "Support ticket",
            },
            to=_user_room(ticket.user.discord_id),
        )
    return jsonify({"status": "ok"})

class WebWorldTheme(StrEnum):
    DIRT = "dirt"
    GRASS = "grass"
    GRASS_FLOWERS = "grassFlowers"
    ICE = "ice"
    JUNGLE = "jungle"
    OCEAN = "ocean"
    PARTY_TIME = "partyTime"
    STONE = "stone"

def get_world_theme(game_name: str) -> str:
    if game_name not in AutoWorldRegister.world_types:
        return "grass"
    chosen_theme = AutoWorldRegister.world_types[game_name].web.theme
    available_themes = [theme.value for theme in WebWorldTheme]
    if chosen_theme not in available_themes:
        warnings.warn(f"Theme '{chosen_theme}' for {game_name} not valid, switching to default 'grass' theme.")
        return "grass"
    return chosen_theme


def format_authors_string(authors: list[str]) -> str:
    """Format a list of authors with proper grammar (commas and &)."""
    if not authors:
        return ""
    if len(authors) == 1:
        return authors[0]
    if len(authors) == 2:
        return f"{authors[0]} & {authors[1]}"
    # For 3+ authors: "Author1, Author2, Author3 & Author4"
    return f"{', '.join(authors[:-1])} & {authors[-1]}"


def get_world_version(world: type(World)) -> str:
    """Get the world version from archipelago.json manifest, if it exists and is not 0.0.0."""
    try:
        import json
        import pkgutil
        
        # Get the world's module path
        world_module = world.__module__
        if world_module.startswith('worlds.'):
            # Try to load archipelago.json from the world folder
            try:
                manifest_data = pkgutil.get_data(world_module, 'archipelago.json')
                if manifest_data:
                    manifest = json.loads(manifest_data.decode('utf-8-sig'))
                    if 'world_version' in manifest and manifest['world_version']:
                        world_version = manifest['world_version']
                        # Don't show version if it's 0.0.0
                        if world_version != "0.0.0":
                            return f"{world_version}"
            except (FileNotFoundError, json.JSONDecodeError, UnicodeDecodeError):
                pass
    except (ImportError, AttributeError, OSError):
        pass
    
    return ""


def get_world_authors(world: type(World)) -> str:
    """Get the formatted authors string for a world, preferring manifest authors over the author field."""
    # Try to load manifest data first
    try:
        import json
        import os
        import pkgutil
        
        # Get the world's module path
        world_module = world.__module__
        if world_module.startswith('worlds.'):
            world_folder = world_module[7:]  # Remove 'worlds.' prefix
            world_path = os.path.join('worlds', world_folder)
            
            # Try to load archipelago.json from the world folder
            try:
                manifest_data = pkgutil.get_data(world_module, 'archipelago.json')
                if manifest_data:
                    manifest = json.loads(manifest_data.decode('utf-8-sig'))
                    if 'authors' in manifest and manifest['authors']:
                        return format_authors_string(manifest['authors'])
            except (FileNotFoundError, json.JSONDecodeError, UnicodeDecodeError):
                pass
    except (ImportError, AttributeError, OSError):
        pass
    
    # Fallback to the author field
    return getattr(world, 'author', '')


@cache.memoize(timeout=300)
def get_visible_worlds() -> dict[str, type(World)]:
    worlds = {}
    for game, world in AutoWorldRegister.world_types.items():
        if not world.hidden and game not in app.config["HIDDEN_WEBWORLDS"]:
            worlds[game] = world
    return worlds


@app.errorhandler(404)
@app.errorhandler(jinja2.exceptions.TemplateNotFound)
def page_not_found(err):
    return render_template('404.html'), 404


# Start Playing Page
@app.route('/start-playing')
@cache.cached()
def start_playing():
    return render_template(f"startPlaying.html")


@app.route('/games/<string:game>/info/<string:lang>')
@cache.cached()
def game_info(game, lang):
    """Game Info Pages"""
    try:
        theme = get_world_theme(game)
        secure_game_name = secure_filename(game)
        lang = secure_filename(lang)
        file_dir = os.path.join(app.static_folder, "generated", "docs", secure_game_name)
        file_dir_url = url_for("static", filename=f"generated/docs/{secure_game_name}")
        document = render_markdown(os.path.join(file_dir, f"{lang}_{secure_game_name}.md"), file_dir_url)
        return render_template(
            "markdown_document.html",
            title=f"{game} Guide",
            html_from_markdown=document,
            theme=theme,
        )
    except FileNotFoundError:
        return abort(404)


@app.route('/games')
@cache.cached()
def games():
    """List of supported games"""
    return render_template("supportedGames.html", worlds=get_visible_worlds(), get_world_authors=get_world_authors, get_world_version=get_world_version)


@app.route('/tutorial/<string:game>/<string:file>')
@cache.cached()
def tutorial(game: str, file: str):
    try:
        theme = get_world_theme(game)
        secure_game_name = secure_filename(game)
        file = secure_filename(file)
        file_dir = os.path.join(app.static_folder, "generated", "docs", secure_game_name)
        file_dir_url = url_for("static", filename=f"generated/docs/{secure_game_name}")
        document = render_markdown(os.path.join(file_dir, f"{file}.md"), file_dir_url)
        return render_template(
            "markdown_document.html",
            title=f"{game} Guide",
            html_from_markdown=document,
            theme=theme,
        )
    except FileNotFoundError:
        return abort(404)


@app.route('/tutorial/<string:game>/<string:file>/<string:lang>')
def tutorial_redirect(game: str, file: str, lang: str):
    """
    Permanent redirect old tutorial URLs to new ones to keep search engines happy.
    e.g. /tutorial/Archipelago/setup/en -> /tutorial/Archipelago/setup_en
    """
    return redirect(url_for("tutorial", game=game, file=f"{file}_{lang}"), code=301)


@app.route('/tutorial/')
@cache.cached()
def tutorial_landing():
    tutorials = {}
    worlds = AutoWorldRegister.world_types
    
    # Filter worlds based on hidden webworlds config
    visible_worlds = {name: world for name, world in worlds.items() 
                     if name not in app.config["HIDDEN_WEBWORLDS"]}
    
    for world_name, world_type in visible_worlds.items():
        current_world = tutorials[world_name] = {}
        if hasattr(world_type.web, 'tutorials'):
            for tutorial in world_type.web.tutorials:
                # Skip if tutorial is not a Tutorial object (e.g., if it's a string)
                if not hasattr(tutorial, 'tutorial_name'):
                    continue
                
                current_tutorial = current_world.setdefault(tutorial.tutorial_name, {
                    "description": tutorial.description, "files": {}})
                
                # Get the file name without extension for the new format
                file_key = secure_filename(tutorial.file_name).rsplit(".", 1)[0]
                
                # Create file data with both old and new link formats
                file_data = {
                    "authors": tutorial.authors,
                    "language": tutorial.language,
                    "file_name": file_key,
                    "legacy_link": tutorial.link if tutorial.link != "unused" else None
                }
                
                current_tutorial["files"][file_key] = file_data
    tutorials = {world_name: tutorials for world_name, tutorials in title_sorted(
        tutorials.items(), key=lambda element: "\x00" if element[0] == "Archipelago" else (getattr(visible_worlds[element[0]].web, 'display_name', None) or visible_worlds[element[0]].game))}
    
    # Sort the worlds dictionary by display name for consistent ordering
    sorted_worlds = dict(title_sorted(
        visible_worlds.items(), 
        key=lambda element: "\x00" if element[0] == "Archipelago" else (getattr(element[1].web, 'display_name', None) or element[1].game)
    ))
    
    return render_template("tutorialLanding.html", worlds=sorted_worlds, tutorials=tutorials)


@app.route('/faq/<string:lang>/')
@cache.cached()
def faq(lang: str):
    document = render_markdown(os.path.join(app.static_folder, "assets", "faq", secure_filename(lang)+".md"))
    return render_template(
        "markdown_document.html",
        title="Frequently Asked Questions",
        html_from_markdown=document,
    )


@app.route('/glossary/<string:lang>/')
@cache.cached()
def glossary(lang: str):
    document = render_markdown(os.path.join(app.static_folder, "assets", "glossary", secure_filename(lang)+".md"))
    return render_template(
        "markdown_document.html",
        title="Glossary",
        html_from_markdown=document,
    )


@app.route('/seed/<suuid:seed>')
def view_seed(seed: UUID):
    seed = Seed.get(id=seed)
    if not seed:
        abort(404)
    return render_template("viewSeed.html", seed=seed, slot_count=count(seed.slots))


@app.route('/new_room/<suuid:seed>')
def new_room(seed: UUID):
    seed = Seed.get(id=seed)
    if not seed:
        abort(404)
    room = Room(seed=seed, owner=session["_id"], tracker=uuid4())
    commit()
    return redirect(url_for("host_room", room=room.id))

@app.route('/downloads/')
@cache.cached()
def downloads():
    return render_template("clients.html", version=__version__)


def _resolve_legal_locale(lang: str | None = None) -> str:
    candidate = lang or request.args.get("lang")
    if not candidate:
        discord_user = session.get("discord_user") or {}
        discord_id = discord_user.get("id")
        if discord_id:
            with db_session:
                user = User.get(discord_id=discord_id)
                if user and user.preferred_locale:
                    candidate = user.preferred_locale
    return _normalize_locale(candidate)


LEGAL_LANG_LABELS = {
    "en": "English",
    "fr": "Français",
    "es": "Español",
    "ja": "日本語",
    "zh-CN": "简体中文",
    "zh-TW": "繁體中文",
}


def _render_legal_document(kind: str, lang: str | None = None):
    locale = _resolve_legal_locale(lang)
    safe_locale = secure_filename(locale)
    filename = f"{kind}_{safe_locale}.md"
    path = os.path.join(app.static_folder, "assets", "legal", filename)
    if not os.path.exists(path):
        path = os.path.join(app.static_folder, "assets", "legal", f"{kind}_en.md")
        locale = "en"

    title_map = {
        "tos": {
            "en": "Terms of Service",
            "fr": "Conditions d'utilisation",
            "es": "Términos de servicio",
            "ja": "利用規約",
            "zh-CN": "服务条款",
            "zh-TW": "服務條款",
        },
        "privacy": {
            "en": "Privacy Policy",
            "fr": "Politique de confidentialité",
            "es": "Política de privacidad",
            "ja": "プライバシーポリシー",
            "zh-CN": "隐私政策",
            "zh-TW": "隱私權政策",
        },
    }

    doc_title = title_map.get(kind, title_map["tos"]).get(locale, title_map.get(kind, title_map["tos"])["en"])
    html = render_markdown(path)
    return render_template(
        "legal_doc.html",
        title=f"{doc_title} · SekaiLink",
        doc_title=doc_title,
        doc_kind=kind,
        lang=locale,
        lang_labels=LEGAL_LANG_LABELS,
        html_from_markdown=html,
    )


@app.route('/legal/')
def legal():
    locale = _resolve_legal_locale(None)
    return render_template("legal.html", lang=locale, lang_labels=LEGAL_LANG_LABELS)


@app.route('/legal/tos')
@app.route('/legal/tos/<string:lang>')
def legal_tos(lang: str | None = None):
    return _render_legal_document("tos", lang)


@app.route('/legal/privacy')
@app.route('/legal/privacy/<string:lang>')
def legal_privacy(lang: str | None = None):
    return _render_legal_document("privacy", lang)


def _read_log(log: IO[Any], offset: int = 0) -> Iterator[bytes]:
    marker = log.read(3)  # skip optional BOM
    if marker != b'\xEF\xBB\xBF':
        log.seek(0, os.SEEK_SET)
    log.seek(offset, os.SEEK_CUR)
    yield from log
    log.close()  # free file handle as soon as possible


@app.route('/log/<suuid:room>')
def display_log(room: UUID) -> Union[str, Response, Tuple[str, int]]:
    room = Room.get(id=room)
    if room is None:
        return abort(404)
    if room.owner == session["_id"]:
        file_path = os.path.join("logs", str(room.id) + ".txt")
        try:
            log = open(file_path, "rb")
            range_header = request.headers.get("Range")
            if range_header:
                range_type, range_values = range_header.split('=')
                start, end = map(str.strip, range_values.split('-', 1))
                if range_type != "bytes" or end != "":
                    return "Unsupported range", 500
                # NOTE: we skip Content-Range in the response here, which isn't great but works for our JS
                return Response(_read_log(log, int(start)), mimetype="text/plain", status=206)
            return Response(_read_log(log), mimetype="text/plain")
        except FileNotFoundError:
            return Response(f"Logfile {file_path} does not exist. "
                            f"Likely a crash during spinup of multiworld instance or it is still spinning up.",
                            mimetype="text/plain")

    return "Access Denied", 403


@app.post("/room/<suuid:room>")
def host_room_command(room: UUID):
    room: Room = Room.get(id=room)
    if room is None:
        return abort(404)

    if room.owner == session["_id"]:
        cmd = request.form["cmd"]
        if cmd:
            Command(room=room, commandtext=cmd)
            commit()
    return redirect(url_for("host_room", room=room.id))


@app.get("/room/<suuid:room>")
def host_room(room: UUID):
    room: Room = Room.get(id=room)
    if room is None:
        return abort(404)

    now = datetime.datetime.utcnow()
    # indicate that the page should reload to get the assigned port
    should_refresh = ((not room.last_port and now - room.creation_time < datetime.timedelta(seconds=3))
                      or room.last_activity < now - datetime.timedelta(seconds=room.timeout))

    if now - room.last_activity > datetime.timedelta(minutes=1):
        # we only set last_activity if needed, otherwise parallel access on /room will cause an internal server error
        # due to "pony.orm.core.OptimisticCheckError: Object Room was updated outside of current transaction"
        room.last_activity = now  # will trigger a spinup, if it's not already running

    browser_tokens = "Mozilla", "Chrome", "Safari"
    automated = ("update" in request.args
                 or "Discordbot" in request.user_agent.string
                 or not any(browser_token in request.user_agent.string for browser_token in browser_tokens))

    def get_log(max_size: int = 0 if automated else 1024000) -> Tuple[str, int]:
        if max_size == 0:
            return "…", 0
        try:
            with open(os.path.join("logs", str(room.id) + ".txt"), "rb") as log:
                raw_size = 0
                fragments: List[str] = []
                for block in _read_log(log):
                    if raw_size + len(block) > max_size:
                        fragments.append("…")
                        break
                    raw_size += len(block)
                    fragments.append(block.decode("utf-8"))
                return "".join(fragments), raw_size
        except FileNotFoundError:
            return "", 0

    return render_template("hostRoom.html", room=room, should_refresh=should_refresh, get_log=get_log)


@app.route('/favicon.ico')
def favicon():
    return send_from_directory(os.path.join(app.root_path, "static", "static"),
                               'favicon.ico', mimetype='image/vnd.microsoft.icon')


@app.route('/assets/<path:filename>')
def serve_assets(filename):
    assets_dir = os.path.join(os.path.dirname(app.root_path), "assets")
    return send_from_directory(assets_dir, filename)


@app.route('/discord')
def discord():
    return redirect("https://discord.gg/jTaefxAEDW")


@app.route('/datapackage')
@cache.cached()
def get_datapackage():
    """A pretty print version of /api/datapackage"""
    from worlds import network_data_package
    import json
    return Response(json.dumps(network_data_package, indent=4), mimetype="text/plain")

@app.route('/datapackage/<game_name>')
@cache.cached()
def get_datapackage_single(game_name):
    """A pretty print version of /api/datapackage, filtered by an individual game"""
    from worlds import network_data_package_single_game
    import json
    pkg = network_data_package_single_game.get(game_name)
    if pkg is None:
        abort(404, description=f"Game '{game_name}' not found")
    return Response(json.dumps(pkg, indent=4), mimetype='text/plain')

@app.route('/index')
@app.route('/sitemap')
@cache.cached()
def get_sitemap():
    available_games: List[Dict[str, Union[str, bool]]] = []
    for game, world in AutoWorldRegister.world_types.items():
        if not world.hidden and not game in app.config["HIDDEN_WEBWORLDS"]:
            has_settings: bool = isinstance(world.web.options_page, bool) and world.web.options_page
            available_games.append({ 'title': game, 'has_settings': has_settings })
    return render_template("siteMap.html", games=available_games)
