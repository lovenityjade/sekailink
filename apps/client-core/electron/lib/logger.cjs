function safeStringify(value) {
  try {
    return JSON.stringify(value);
  } catch (_err) {
    return String(value);
  }
}

const MAX_LOG_LINE_CHARS = 4000;
const MAX_LOG_FILE_BYTES = 8 * 1024 * 1024;
const MAX_LOG_FILES = 30;
const SENSITIVE_KEY_RE = /(pass(word)?|secret|token|auth(entication|orization)?|cookie|session|api[-_]?key|credential|private[-_]?key|refresh)/i;
const SENSITIVE_QUERY_RE = /^(token|access_token|refresh_token|auth|authorization|key|api_key|apikey|password|pass|secret|signature|sig|code|session|cookie)$/i;

function truncateLogText(value, maxLength = MAX_LOG_LINE_CHARS) {
  const text = String(value || "");
  if (text.length <= maxLength) return text;
  return `${text.slice(0, maxLength)}...[truncated ${text.length - maxLength} chars]`;
}

function scrubSecretString(value) {
  let text = String(value || "");
  if (!text) return "";
  text = text
    .replace(/(authorization\s*[:=]\s*)(bearer\s+)?[^\s,;]+/gi, "$1[REDACTED]")
    .replace(/(x-api-key\s*[:=]\s*)[^\s,;]+/gi, "$1[REDACTED]")
    .replace(/(cookie\s*[:=]\s*)[^\n\r]+/gi, "$1[REDACTED]");
  text = text.replace(/\b(password|pass|token|secret|api[_-]?key|apikey|session|auth)\s*=\s*([^\s,;]+)/gi, "$1=[REDACTED]");
  text = text.replace(/\bhttps?:\/\/[^\s"'<>]+/gi, (rawUrl) => {
    try {
      const url = new URL(rawUrl);
      for (const key of Array.from(url.searchParams.keys())) {
        if (SENSITIVE_QUERY_RE.test(key)) url.searchParams.set(key, "[REDACTED]");
      }
      return url.toString();
    } catch (_err) {
      return rawUrl;
    }
  });
  return text;
}

function scrubSecretsForLog(value, depth = 0, seen = new WeakSet()) {
  if (value == null) return value;
  if (depth > 8) return "[TRUNCATED]";
  const valueType = typeof value;
  if (valueType === "string") return scrubSecretString(value);
  if (valueType !== "object") return value;
  if (seen.has(value)) return "[CIRCULAR]";
  seen.add(value);

  if (Array.isArray(value)) {
    return value.map((entry) => scrubSecretsForLog(entry, depth + 1, seen));
  }

  const out = {};
  for (const [key, raw] of Object.entries(value)) {
    if (SENSITIVE_KEY_RE.test(key)) {
      out[key] = "[REDACTED]";
      continue;
    }
    out[key] = scrubSecretsForLog(raw, depth + 1, seen);
  }
  return out;
}

function createLogger({ app, fs, path }) {
  const nowIso = () => new Date().toISOString();
  let logStream = null;
  let logFilePath = "";
  let logStamp = "";
  let logPart = 0;
  let logBytes = 0;

  function writeConsole(level, ...args) {
    try {
      if (level === "error") console.error(...args);
      else if (level === "warn") console.warn(...args);
      else console.log(...args);
    } catch (_err) {
      // Some Wine/Electron launches expose invalid stdio handles. Logging must
      // never crash the client, especially during updater relaunches.
    }
  }

  function getLogsDir() {
    return path.join(app.getPath("userData"), "logs");
  }

  function pruneOldLogs(dir) {
    try {
      const entries = fs.readdirSync(dir)
        .filter((name) => /^sekailink-.*\.log$/i.test(name))
        .map((name) => {
          const filePath = path.join(dir, name);
          const stat = fs.statSync(filePath);
          return { filePath, mtimeMs: stat.mtimeMs };
        })
        .sort((a, b) => b.mtimeMs - a.mtimeMs);
      for (const entry of entries.slice(MAX_LOG_FILES)) {
        try {
          fs.rmSync(entry.filePath, { force: true });
        } catch (_err) {
          // Best-effort cleanup only.
        }
      }
    } catch (_err) {
      // Best-effort cleanup only.
    }
  }

  function openLogStream(reason = "started") {
    const dir = getLogsDir();
    fs.mkdirSync(dir, { recursive: true });
    if (!logStamp) logStamp = nowIso().replace(/[:.]/g, "-");
    const suffix = logPart > 0 ? `-part${String(logPart + 1).padStart(2, "0")}` : "";
    logFilePath = path.join(dir, `sekailink-${logStamp}${suffix}.log`);
    logStream = fs.createWriteStream(logFilePath, { flags: "a" });
    logBytes = 0;
    const line = `[${nowIso()}] [info] logger: ${reason} path=${logFilePath}\n`;
    logStream.write(line);
    logBytes += Buffer.byteLength(line);
    pruneOldLogs(dir);
  }

  function rotateLogStream() {
    try {
      if (logStream) {
        logStream.write(`[${nowIso()}] [info] logger: rotating max_bytes=${MAX_LOG_FILE_BYTES}\n`);
        logStream.end();
      }
    } catch (_err) {
      // ignore
    }
    logStream = null;
    logPart += 1;
    openLogStream("rotated");
  }

  function start() {
    try {
      logStamp = nowIso().replace(/[:.]/g, "-");
      logPart = 0;
      openLogStream("started");
    } catch (err) {
      logStream = null;
      logFilePath = "";
      writeConsole("error", "[logger] failed to start file logging:", err);
    }
  }

  function stop() {
    try {
      if (logStream) {
        logStream.write(`[${nowIso()}] [info] logger: stopping\n`);
        logStream.end();
        logStream = null;
      }
    } catch (_err) {
      // ignore
    }
  }

  function writeLine(level, scope, message) {
    const safeMessage = truncateLogText(scrubSecretString(message));
    const line = `[${nowIso()}] [${level}] ${scope}: ${safeMessage}`;
    try {
      if (logStream) {
        const payload = line + "\n";
        const bytes = Buffer.byteLength(payload);
        if (logBytes + bytes > MAX_LOG_FILE_BYTES) rotateLogStream();
        logStream.write(payload);
        logBytes += bytes;
      }
    } catch (_err) {
      // ignore
    }
    writeConsole(level, line);
  }

  function writeJson(scope, obj) {
    const safeObj = scrubSecretsForLog(obj);
    const line = truncateLogText(`[${nowIso()}] [json] ${scope}: ${safeStringify(safeObj)}`);
    try {
      if (logStream) {
        const payload = line + "\n";
        const bytes = Buffer.byteLength(payload);
        if (logBytes + bytes > MAX_LOG_FILE_BYTES) rotateLogStream();
        logStream.write(payload);
        logBytes += bytes;
      }
    } catch (_err) {
      // ignore
    }
    writeConsole("info", line);
  }

  function getLogFilePath() {
    return logFilePath;
  }

  return {
    start,
    stop,
    writeLine,
    writeJson,
    getLogFilePath,
    safeStringify,
  };
}

module.exports = {
  createLogger,
};
