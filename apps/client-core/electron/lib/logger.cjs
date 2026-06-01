function safeStringify(value) {
  try {
    return JSON.stringify(value);
  } catch (_err) {
    return String(value);
  }
}

const SENSITIVE_KEY_RE = /(pass(word)?|secret|token|auth(entication|orization)?|cookie|session|api[-_]?key|credential|private[-_]?key|refresh)/i;
const SENSITIVE_QUERY_RE = /^(token|access_token|refresh_token|auth|authorization|key|api_key|apikey|password|pass|secret|signature|sig|code|session|cookie)$/i;

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

  function getLogsDir() {
    return path.join(app.getPath("userData"), "logs");
  }

  function start() {
    try {
      const dir = getLogsDir();
      fs.mkdirSync(dir, { recursive: true });
      const stamp = nowIso().replace(/[:.]/g, "-");
      logFilePath = path.join(dir, `sekailink-${stamp}.log`);
      logStream = fs.createWriteStream(logFilePath, { flags: "a" });
      logStream.write(`[${nowIso()}] [info] logger: started path=${logFilePath}\n`);
    } catch (err) {
      logStream = null;
      logFilePath = "";
      console.error("[logger] failed to start file logging:", err);
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
    const safeMessage = scrubSecretString(message);
    const line = `[${nowIso()}] [${level}] ${scope}: ${safeMessage}`;
    if (level === "error") console.error(line);
    else if (level === "warn") console.warn(line);
    else console.log(line);
    try {
      if (logStream) logStream.write(line + "\n");
    } catch (_err) {
      // ignore
    }
  }

  function writeJson(scope, obj) {
    const safeObj = scrubSecretsForLog(obj);
    const line = `[${nowIso()}] [json] ${scope}: ${safeStringify(safeObj)}`;
    console.log(line);
    try {
      if (logStream) logStream.write(line + "\n");
    } catch (_err) {
      // ignore
    }
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
