import { trace, traceError } from "./trace";

const envBase = import.meta.env.VITE_API_BASE_URL as string | undefined;
const DEFAULT_API_BASE_URL = "https://sekailink.com";

export const API_BASE_URL =
  envBase?.trim() === "same" ? "" : envBase?.trim() || DEFAULT_API_BASE_URL;
export const DESKTOP_TOKEN_KEY = "skl_desktop_token";
export const WEB_AUTH_TOKEN_KEY = "sekailink_session_token";
export const WEB_AUTH_USER_KEY = "sekailink_user";
export const WEB_AUTH_SESSION_KEY = "sekailink_session";
export const DEVICE_ID_KEY = "skl_device_id";
let desktopTokenMemory = "";

const logApiTrace = (payload: Record<string, unknown>) => {
  try {
    void (window as any)?.sekailink?.logToMain?.({
      source: "api-fetch",
      ...payload,
    });
  } catch {
    // ignore logging failures
  }
};

const TRACE_API_PREFIXES = [
  "/api/identity",
  "/api/lobbies",
  "/api/chat",
  "/api/social",
  "/api/client",
];
const API_TRACE_ENABLED = import.meta.env.VITE_SKL_API_TRACE === "1";
const API_SLOW_TRACE_MS = 750;
const CURRENT_USER_CACHE_TTL_MS = 30 * 1000;
let apiRequestSeq = 0;
let currentUserCache: {
  value?: CurrentUser;
  expiresAt: number;
  inFlight?: Promise<CurrentUser>;
} = { expiresAt: 0 };

export const apiErrorMessage = (raw: unknown, fallback = "Request failed.") => {
  const text = typeof raw === "string" ? raw.trim() : "";
  if (!text) return fallback;
  try {
    const parsed = JSON.parse(text);
    const code = String(parsed?.error || "").trim();
    if (code === "unauthorized") return "Session expired or unauthorized.";
    if (code === "not_implemented") return "This SekaiLink endpoint is not implemented yet.";
    if (code === "not_found" || code === "route_not_found") return "This SekaiLink endpoint is not available yet.";
    if (code === "maintenance") return "This SekaiLink service is currently in maintenance.";
    if (code) return code;
  } catch {
    // Raw server text, not JSON.
  }
  if (/^<!doctype html|^<html/i.test(text)) return "SekaiLink service returned an unexpected web page instead of API data.";
  return text;
};

export const errorFromResponse = async (response: Response, fallback = "Request failed.") => {
  const errorText = await response.text().catch(() => "");
  const message = apiErrorMessage(errorText, response.statusText || fallback);
  return new Error(message);
};

export const getDesktopToken = () => {
  if (desktopTokenMemory) return desktopTokenMemory;
  try {
    const stored =
      window.localStorage.getItem(DESKTOP_TOKEN_KEY) ||
      window.localStorage.getItem(WEB_AUTH_TOKEN_KEY);
    if (stored) desktopTokenMemory = stored;
    return stored;
  } catch {
    return desktopTokenMemory || null;
  }
};

export const setDesktopToken = (token: string | null) => {
  desktopTokenMemory = token || "";
  currentUserCache = { expiresAt: 0 };
  try {
    if (!token) {
      window.localStorage.removeItem(DESKTOP_TOKEN_KEY);
      window.localStorage.removeItem(WEB_AUTH_TOKEN_KEY);
      return;
    }
    window.localStorage.setItem(DESKTOP_TOKEN_KEY, token);
    window.localStorage.setItem(WEB_AUTH_TOKEN_KEY, token);
  } catch {
    // Ignore storage errors.
  }
};

export const setWebAuthCache = (payload: { user?: unknown; session?: unknown } | null) => {
  try {
    if (!payload) {
      window.localStorage.removeItem(WEB_AUTH_USER_KEY);
      window.localStorage.removeItem(WEB_AUTH_SESSION_KEY);
      return;
    }
    if (payload.user) window.localStorage.setItem(WEB_AUTH_USER_KEY, JSON.stringify(payload.user));
    if (payload.session) window.localStorage.setItem(WEB_AUTH_SESSION_KEY, JSON.stringify(payload.session));
  } catch {
    // Ignore storage errors.
  }
};

const generateDeviceId = () => {
  const bytes = new Uint8Array(16);
  if (typeof window !== "undefined" && window.crypto?.getRandomValues) {
    window.crypto.getRandomValues(bytes);
  } else {
    for (let i = 0; i < bytes.length; i += 1) {
      bytes[i] = Math.floor(Math.random() * 256);
    }
  }
  return Array.from(bytes).map((b) => b.toString(16).padStart(2, "0")).join("");
};

export const getDeviceId = () => {
  try {
    let value = window.localStorage.getItem(DEVICE_ID_KEY);
    if (!value) {
      value = generateDeviceId();
      window.localStorage.setItem(DEVICE_ID_KEY, value);
    }
    return value;
  } catch {
    return "";
  }
};

export const apiUrl = (path: string) => {
  if (!API_BASE_URL) return path;
  return `${API_BASE_URL.replace(/\/$/, "")}${path.startsWith("/") ? path : `/${path}`}`;
};

export const apiAssetUrl = (value: unknown) => {
  const raw = typeof value === "string" ? value.trim() : "";
  if (!raw) return "";
  if (/^(https?:|data:|blob:)/i.test(raw)) return raw;
  if (raw.startsWith("/")) return apiUrl(raw);
  return raw;
};

export const isUsableAvatarUrl = (value: unknown) => {
  const raw = typeof value === "string" ? value.trim() : "";
  if (!raw) return false;
  if (/^data:image\/[a-z0-9.+-]+;base64,/i.test(raw)) {
    const payload = raw.slice(raw.indexOf(",") + 1);
    return payload.length >= 128 && /^[a-z0-9+/=\s]+$/i.test(payload);
  }
  return /^(https?:|blob:)/i.test(raw) || raw.startsWith("/");
};

export const apiFetch = async (path: string, init?: RequestInit) => {
  const requestId = ++apiRequestSeq;
  const startedAt = performance.now();
  const token = getDesktopToken();
  let url = apiUrl(path);
  if (token && API_BASE_URL && window.location.protocol === "file:") {
    try {
      const next = new URL(url, window.location.origin);
      if (!next.searchParams.has("token")) {
        next.searchParams.set("token", token);
      }
      url = next.toString();
    } catch {
      // Ignore token fallback.
    }
  }
  const headers = new Headers(init?.headers || undefined);
  const isDesktopRuntime = typeof window !== "undefined" && Boolean((window as any).sekailink);
  const isCrossOriginApi = (() => {
    try {
      return new URL(url, window.location.href).origin !== window.location.origin;
    } catch {
      return Boolean(API_BASE_URL);
    }
  })();
  const locale =
    (typeof window !== "undefined" && window.localStorage.getItem("skl_locale")) ||
    (typeof navigator !== "undefined" ? navigator.language : "en");
  if (token && !headers.has("Authorization")) {
    headers.set("Authorization", `Bearer ${token}`);
  }
  if (locale && !headers.has("X-SekaiLink-Locale") && !(isDesktopRuntime && isCrossOriginApi)) {
    headers.set("X-SekaiLink-Locale", locale);
  }
  if (locale && !headers.has("Accept-Language")) {
    headers.set("Accept-Language", locale);
  }
  const deviceId = getDeviceId();
  if (deviceId && !headers.has("X-SekaiLink-Device-Id")) {
    headers.set("X-SekaiLink-Device-Id", deviceId);
  }
  // Desktop runtime: prefer bearer token when available, otherwise allow cookies during auth callback/session bootstrap.
  if (isDesktopRuntime && !isCrossOriginApi && !headers.has("X-SekaiLink-Client")) {
    headers.set("X-SekaiLink-Client", "desktop");
  }
  const credentials: RequestCredentials =
    isDesktopRuntime && isCrossOriginApi
      ? "omit"
      : isDesktopRuntime
        ? (token ? "omit" : "include")
        : "include";
  const defaultTracePath =
    isDesktopRuntime && TRACE_API_PREFIXES.some((prefix) => String(path || "").startsWith(prefix));
  if (API_TRACE_ENABLED) {
    trace("api", "request_start", {
      requestId,
      path,
      url,
      method: String(init?.method || "GET").toUpperCase(),
      hasToken: Boolean(token),
      hasDesktopClientHeader: headers.get("X-SekaiLink-Client") === "desktop",
      hasDeviceId: Boolean(headers.get("X-SekaiLink-Device-Id")),
      crossOrigin: isCrossOriginApi,
      credentials,
    });
    if (API_TRACE_ENABLED) {
      logApiTrace({
        event: "request",
        requestId,
        path,
        method: String(init?.method || "GET").toUpperCase(),
        hasToken: Boolean(token),
        hasDesktopClientHeader: headers.get("X-SekaiLink-Client") === "desktop",
        hasDeviceId: Boolean(headers.get("X-SekaiLink-Device-Id")),
      });
    }
  }
  let response: Response;
  try {
    response = await fetch(url, {
      ...init,
      credentials,
      headers,
    });
  } catch (error) {
    if (API_TRACE_ENABLED || defaultTracePath) {
      traceError("api", "request_failed", error, {
        requestId,
        path,
        method: String(init?.method || "GET").toUpperCase(),
        durationMs: Math.round(performance.now() - startedAt),
      });
    }
    throw error;
  }
  const durationMs = Math.round(performance.now() - startedAt);
  const shouldTraceComplete = API_TRACE_ENABLED || !response.ok || (defaultTracePath && durationMs >= API_SLOW_TRACE_MS);
  if (shouldTraceComplete) {
    let bodyPreview = "";
    if (API_TRACE_ENABLED || !response.ok) {
      try {
        bodyPreview = (await response.clone().text()).slice(0, 300);
      } catch {
        bodyPreview = "";
      }
    }
    trace("api", "request_complete", {
      requestId,
      path,
      method: String(init?.method || "GET").toUpperCase(),
      status: response.status,
      ok: response.ok,
      durationMs,
      bodyPreview: response.ok ? "" : bodyPreview,
    }, response.ok ? "info" : "warn");
    if (API_TRACE_ENABLED) {
      logApiTrace({
        event: "response",
        requestId,
        path,
        method: String(init?.method || "GET").toUpperCase(),
        status: response.status,
        ok: response.ok,
        bodyPreview,
      });
    }
  }
  return response;
};

export const apiFetchWithTimeout = async (path: string, timeoutMs = 6000, init?: RequestInit) => {
  const controller = new AbortController();
  const timer = window.setTimeout(() => controller.abort(), timeoutMs);
  try {
    const res = await apiFetch(path, { ...init, signal: controller.signal });
    return res;
  } catch (error) {
    if (controller.signal.aborted) {
      traceError("api", "request_timeout", error, { path, timeoutMs });
    }
    throw error;
  } finally {
    window.clearTimeout(timer);
  }
};

export const apiJson = async <T>(path: string, init?: RequestInit): Promise<T> => {
  const response = await apiFetch(path, init);
  if (!response.ok) {
    throw await errorFromResponse(response);
  }
  return response.json() as Promise<T>;
};

export interface CurrentUser {
  user_id?: string;
  discord_id?: string;
  username?: string;
  global_name?: string;
  display_name?: string;
  avatar_url?: string;
  presence_status?: string;
  email?: string;
  email_verified?: boolean;
  locale?: string;
  role?: string;
  permissions?: string[];
  terms_accepted?: boolean;
  terms_version?: string;
  terms_accepted_at?: string | null;
}

export const normalizeIdentityUser = (payload: any): CurrentUser | null => {
  const user = payload?.user && typeof payload.user === "object" ? payload.user : payload;
  if (!user || typeof user !== "object") return null;
  const username = String(user.username || "").trim();
  const displayName = String(user.display_name || user.global_name || username || "").trim();
  const userId = String(user.user_id || user.discord_id || username || "").trim();
  if (!userId && !username && !displayName) return null;
  return {
    user_id: userId,
    discord_id: userId,
    username,
    global_name: displayName,
    display_name: displayName,
    avatar_url: apiAssetUrl(user.avatar_url),
    presence_status: typeof user.presence_status === "string" ? user.presence_status : "online",
    email: typeof user.email === "string" ? user.email : "",
    email_verified: Boolean(user.email_verified),
    locale: typeof user.locale === "string" ? user.locale : "",
    role: typeof user.role === "string" ? user.role : "",
    permissions: Array.isArray(user.permissions) ? user.permissions.map((p: unknown) => String(p)) : [],
    terms_accepted: Boolean(user.terms_accepted),
    terms_version: typeof user.terms_version === "string" ? user.terms_version : "",
    terms_accepted_at: typeof user.terms_accepted_at === "string" ? user.terms_accepted_at : null,
  };
};

export const apiCurrentUser = async (): Promise<CurrentUser> => {
  const now = Date.now();
  if (currentUserCache.value && currentUserCache.expiresAt > now) return currentUserCache.value;
  if (currentUserCache.inFlight) return currentUserCache.inFlight;
  currentUserCache.inFlight = (async () => {
    const payload = await apiJson<any>("/api/identity/me");
    const user = normalizeIdentityUser(payload);
    if (!user) throw new Error("invalid_identity_payload");
    currentUserCache.value = user;
    currentUserCache.expiresAt = Date.now() + CURRENT_USER_CACHE_TTL_MS;
    return user;
  })();
  try {
    return await currentUserCache.inFlight;
  } finally {
    currentUserCache.inFlight = undefined;
  }
};
