type TraceLevel = "debug" | "info" | "warn" | "error";

type TracePayload = Record<string, unknown>;

const isSensitiveKey = (key: string) =>
  /(authorization|password|secret|cookie|jwt)/i.test(key) ||
  /(^|[_-])(token|pass|session)([_-]|$)/i.test(key);
const MAX_STRING = 600;
const MAX_ARRAY = 24;
const MAX_DEPTH = 4;

let sequence = 0;

const sanitize = (value: unknown, depth = 0): unknown => {
  if (value === null || value === undefined) return value;
  if (typeof value === "string") {
    return value.length > MAX_STRING ? `${value.slice(0, MAX_STRING)}...<truncated>` : value;
  }
  if (typeof value === "number" || typeof value === "boolean") return value;
  if (typeof value === "bigint") return value.toString();
  if (value instanceof Error) {
    return {
      name: value.name,
      message: value.message,
      stack: value.stack ? sanitize(value.stack, depth + 1) : "",
    };
  }
  if (depth >= MAX_DEPTH) return "<max-depth>";
  if (Array.isArray(value)) {
    return value.slice(0, MAX_ARRAY).map((entry) => sanitize(entry, depth + 1));
  }
  if (typeof value === "object") {
    const output: TracePayload = {};
    for (const [key, entry] of Object.entries(value as TracePayload)) {
      output[key] = isSensitiveKey(key) ? "<redacted>" : sanitize(entry, depth + 1);
    }
    return output;
  }
  return String(value);
};

const shouldMirrorToConsole = (level: TraceLevel) => {
  if (level === "warn" || level === "error") return true;
  try {
    return window.localStorage.getItem("skl.trace.console") === "1";
  } catch {
    return false;
  }
};

export const trace = (source: string, event: string, payload: TracePayload = {}, level: TraceLevel = "info") => {
  const entry = {
    type: "client.trace",
    seq: ++sequence,
    at: new Date().toISOString(),
    level,
    source,
    event,
    payload: sanitize(payload),
  };

  try {
    void window.sekailink?.logToMain?.(entry);
  } catch {
    // Logging must never affect the UI.
  }

  if (shouldMirrorToConsole(level)) {
    const method = level === "error" ? "error" : level === "warn" ? "warn" : "debug";
    try {
      // Keep console output compact; full details go to the Electron log.
      console[method](`[trace:${source}] ${event}`, entry.payload);
    } catch {
      // ignore
    }
  }
};

export const traceError = (source: string, event: string, error: unknown, payload: TracePayload = {}) => {
  trace(source, event, { ...payload, error: sanitize(error) }, "error");
};

export const createTrace = (source: string) => ({
  debug: (event: string, payload?: TracePayload) => trace(source, event, payload, "debug"),
  info: (event: string, payload?: TracePayload) => trace(source, event, payload, "info"),
  warn: (event: string, payload?: TracePayload) => trace(source, event, payload, "warn"),
  error: (event: string, error: unknown, payload?: TracePayload) => traceError(source, event, error, payload),
});
