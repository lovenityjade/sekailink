import { apiJson } from "./api";

type BoxArtMapResponse =
  | { boxarts?: Record<string, string> }
  | { images?: Record<string, string> }
  | { items?: Array<{ game_id?: string; gameId?: string; image_url?: string; imageUrl?: string; url?: string }> };

const normalizeMap = (data: BoxArtMapResponse): Record<string, string> => {
  const out: Record<string, string> = {};
  if (data && typeof data === "object") {
    const m1 = (data as any).boxarts;
    const m2 = (data as any).images;
    if (m1 && typeof m1 === "object") Object.assign(out, m1);
    if (m2 && typeof m2 === "object") Object.assign(out, m2);
    const items = Array.isArray((data as any).items) ? (data as any).items : [];
    for (const item of items) {
      const gameId = String(item?.game_id || item?.gameId || "").trim();
      const url = String(item?.image_url || item?.imageUrl || item?.url || "").trim();
      if (gameId && url) out[gameId] = url;
    }
  }
  return out;
};

export const fetchGameBoxArtMap = async (gameIds: string[]): Promise<Record<string, string>> => {
  const ids = Array.from(new Set((gameIds || []).map((v) => String(v || "").trim()).filter(Boolean)));
  if (!ids.length) return {};
  const encoded = encodeURIComponent(ids.join(","));

  const endpoints = [
    `/api/client/gamebox?game_ids=${encoded}`,
    `/api/client/boxart?game_ids=${encoded}`,
  ];

  for (const path of endpoints) {
    try {
      const data = await apiJson<BoxArtMapResponse>(path);
      const mapped = normalizeMap(data);
      if (Object.keys(mapped).length) return mapped;
    } catch {
      // Try next endpoint shape/path.
    }
  }
  return {};
};

