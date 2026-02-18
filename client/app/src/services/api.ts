const envBase = import.meta.env.VITE_API_BASE_URL as string | undefined;

export const API_BASE_URL = envBase && envBase !== "same" ? envBase : "";
export const DESKTOP_TOKEN_KEY = "skl_desktop_token";

export const getDesktopToken = () => {
  try {
    return window.localStorage.getItem(DESKTOP_TOKEN_KEY);
  } catch {
    return null;
  }
};

export const setDesktopToken = (token: string | null) => {
  try {
    if (!token) {
      window.localStorage.removeItem(DESKTOP_TOKEN_KEY);
      return;
    }
    window.localStorage.setItem(DESKTOP_TOKEN_KEY, token);
  } catch {
    // Ignore storage errors.
  }
};

export const apiUrl = (path: string) => {
  if (!API_BASE_URL) return path;
  return `${API_BASE_URL.replace(/\/$/, "")}${path.startsWith("/") ? path : `/${path}`}`;
};

export const apiFetch = async (path: string, init?: RequestInit) => {
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
  const locale =
    (typeof window !== "undefined" && window.localStorage.getItem("skl_locale")) ||
    (typeof navigator !== "undefined" ? navigator.language : "en");
  if (token && !headers.has("Authorization")) {
    headers.set("Authorization", `Bearer ${token}`);
  }
  if (locale && !headers.has("X-SekaiLink-Locale")) {
    headers.set("X-SekaiLink-Locale", locale);
  }
  if (locale && !headers.has("Accept-Language")) {
    headers.set("Accept-Language", locale);
  }
  const response = await fetch(url, {
    credentials: "include",
    headers,
    ...init
  });
  return response;
};

export const apiFetchWithTimeout = async (path: string, timeoutMs = 6000, init?: RequestInit) => {
  const controller = new AbortController();
  const timer = window.setTimeout(() => controller.abort(), timeoutMs);
  try {
    const res = await apiFetch(path, { ...init, signal: controller.signal });
    return res;
  } finally {
    window.clearTimeout(timer);
  }
};

export const apiJson = async <T>(path: string, init?: RequestInit): Promise<T> => {
  const response = await apiFetch(path, init);
  if (!response.ok) {
    const errorText = await response.text();
    throw new Error(errorText || response.statusText);
  }
  return response.json() as Promise<T>;
};
