const envBase = import.meta.env.VITE_API_BASE_URL as string | undefined;

export const API_BASE_URL = envBase && envBase !== "same" ? envBase : "";
export const ADMIN_TOKEN_KEY = "skl_admin_desktop_token";

export const getAdminToken = () => {
  try {
    return window.localStorage.getItem(ADMIN_TOKEN_KEY);
  } catch {
    return null;
  }
};

export const setAdminToken = (token: string | null) => {
  try {
    if (!token) {
      window.localStorage.removeItem(ADMIN_TOKEN_KEY);
      return;
    }
    window.localStorage.setItem(ADMIN_TOKEN_KEY, token);
  } catch {
    // ignore storage failures
  }
};

export const apiUrl = (path: string) => {
  if (!API_BASE_URL) return path;
  return `${API_BASE_URL.replace(/\/$/, "")}${path.startsWith("/") ? path : `/${path}`}`;
};

export const apiFetch = async (path: string, init?: RequestInit) => {
  const token = getAdminToken();
  let url = apiUrl(path);
  if (token && API_BASE_URL && window.location.protocol === "file:") {
    try {
      const next = new URL(url, window.location.origin);
      if (!next.searchParams.has("token")) next.searchParams.set("token", token);
      url = next.toString();
    } catch {
      // ignore URL fallback failures
    }
  }
  const headers = new Headers(init?.headers || undefined);
  if (token && !headers.has("Authorization")) {
    headers.set("Authorization", `Bearer ${token}`);
  }
  return fetch(url, {
    credentials: "include",
    headers,
    ...init
  });
};

export const apiJson = async <T>(path: string, init?: RequestInit): Promise<T> => {
  const response = await apiFetch(path, init);
  if (!response.ok) {
    const text = await response.text();
    throw new Error(text || response.statusText || `HTTP ${response.status}`);
  }
  return response.json() as Promise<T>;
};
