import { apiFetch } from "./api";

type DebugLogPayload = {
  message: string;
  level?: "debug" | "info" | "warn" | "error";
  category?: string;
  source?: string;
  room_id?: string;
  lobby_id?: string;
  details?: unknown;
};

const queue: DebugLogPayload[] = [];
let flushTimer: number | null = null;

const flush = async () => {
  flushTimer = null;
  const batch = queue.splice(0, 8);
  for (const item of batch) {
    try {
      await apiFetch("/api/client/debug-log", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(item),
      });
    } catch {
      // Best-effort telemetry only.
    }
  }
  if (queue.length > 0) {
    flushTimer = window.setTimeout(() => {
      void flush();
    }, 800);
  }
};

export const pushDebugLog = (payload: DebugLogPayload) => {
  if (!payload || !payload.message) return;
  queue.push(payload);
  if (queue.length > 200) {
    queue.splice(0, queue.length - 200);
  }
  if (flushTimer === null) {
    flushTimer = window.setTimeout(() => {
      void flush();
    }, 300);
  }
};
