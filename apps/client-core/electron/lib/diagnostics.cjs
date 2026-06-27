const createDiagnostics = ({
  app,
  fs,
  http,
  https,
  os,
  path,
  processRef = process,
  logger,
  isPlainObject,
  isSafeExternalUrl,
  normalizeIpcString,
  getBootstrapInstallState,
  getArchipelagoClientStats = () => ({}),
  getArchipelagoTraceTail = () => "",
  getMainWindow = () => null,
  nowIso = () => new Date().toISOString(),
}) => {
  let logFilePath = "";

  const startFileLogging = () => {
    logger.start();
    logFilePath = logger.getLogFilePath();
  };

  const getLatestLogFilePath = () => {
    try {
      if (logFilePath && fs.existsSync(logFilePath)) return logFilePath;
      const logsDir = path.join(app.getPath("userData"), "logs");
      if (!fs.existsSync(logsDir)) return "";
      const entries = fs
        .readdirSync(logsDir)
        .filter((name) => name.startsWith("sekailink-") && name.endsWith(".log"))
        .map((name) => {
          const full = path.join(logsDir, name);
          let mtimeMs = 0;
          try {
            mtimeMs = fs.statSync(full).mtimeMs;
          } catch (_err) {
            mtimeMs = 0;
          }
          return { full, mtimeMs };
        })
        .sort((a, b) => b.mtimeMs - a.mtimeMs);
      return entries[0]?.full || "";
    } catch (_err) {
      return "";
    }
  };

  const readFileTailUtf8 = (filePath, maxBytes = 200 * 1024) => {
    try {
      const stat = fs.statSync(filePath);
      const size = Number(stat.size || 0);
      if (!Number.isFinite(size) || size <= 0) return "";
      const tailBytes = Math.max(1024, Math.min(Number(maxBytes || 0), size));
      const fd = fs.openSync(filePath, "r");
      try {
        const buffer = Buffer.alloc(tailBytes);
        const start = Math.max(0, size - tailBytes);
        const bytesRead = fs.readSync(fd, buffer, 0, tailBytes, start);
        return buffer.slice(0, bytesRead).toString("utf8");
      } finally {
        fs.closeSync(fd);
      }
    } catch (_err) {
      return "";
    }
  };

  const collectDiagnosticsReport = async (options = {}) => {
    const includeLogTail = options?.includeLogTail !== false;
    const maxLogBytesRaw = Number(options?.maxLogBytes || 200 * 1024);
    const maxLogBytes = Number.isFinite(maxLogBytesRaw) ? Math.min(1024 * 1024, Math.max(4096, maxLogBytesRaw)) : 200 * 1024;
    const latest = getLatestLogFilePath();
    const logTail = includeLogTail && latest ? readFileTailUtf8(latest, maxLogBytes) : "";
    return {
      reportId: `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`,
      createdAt: nowIso(),
      app: {
        version: app.getVersion ? String(app.getVersion() || "") : "",
        platform: processRef.platform,
        arch: processRef.arch,
        electron: processRef.versions?.electron,
        chrome: processRef.versions?.chrome,
        node: processRef.versions?.node,
      },
      log: {
        path: latest || "",
        tail: logTail || "",
      },
      meta: isPlainObject(options?.meta) ? options.meta : {},
    };
  };

  const collectBugReportArtifacts = async (options = {}) => {
    const includeScreenshot = options?.includeScreenshot !== false;
    const maxLogBytesRaw = Number(options?.maxLogBytes || 700 * 1024);
    const maxLogBytes = Number.isFinite(maxLogBytesRaw) ? Math.min(1024 * 1024, Math.max(32 * 1024, maxLogBytesRaw)) : 700 * 1024;
    const latest = logFilePath && fs.existsSync(logFilePath) ? logFilePath : getLatestLogFilePath();
    const logTail = latest ? readFileTailUtf8(latest, maxLogBytes) : "";
    const apClientTraceTail = getArchipelagoTraceTail();
    const logsText = apClientTraceTail ? `${logTail || ""}\n\n[Archipelago client trace]\n${apClientTraceTail}`.trim() : logTail;

    let screenshotBase64 = "";
    const mainWindow = getMainWindow();
    if (includeScreenshot && mainWindow && !mainWindow.isDestroyed()) {
      try {
        const image = await mainWindow.capturePage();
        screenshotBase64 = image.toPNG().toString("base64");
      } catch (_err) {
        screenshotBase64 = "";
      }
    }

    let diskTotal = 0;
    let diskFree = 0;
    try {
      const installDir = path.dirname(processRef.execPath);
      if (typeof fs.statfsSync === "function") {
        const info = fs.statfsSync(installDir);
        const bsize = Number(info?.bsize || 0);
        const blocks = Number(info?.blocks || 0);
        const bavail = Number(info?.bavail || 0);
        if (bsize > 0 && blocks > 0) diskTotal = Math.max(0, Math.floor(bsize * blocks));
        if (bsize > 0 && bavail > 0) diskFree = Math.max(0, Math.floor(bsize * bavail));
      }
    } catch (_err) {
      diskTotal = 0;
      diskFree = 0;
    }

    const cpus = Array.isArray(os.cpus?.()) ? os.cpus() : [];
    const cpuModel = cpus[0] && typeof cpus[0].model === "string" ? cpus[0].model : "";
    const bootstrap = getBootstrapInstallState() || {};
    const apStats = getArchipelagoClientStats() || {};
    return {
      screenshotBase64,
      logsText: logsText || "",
      systemInfo: {
        os: os.type(),
        os_release: os.release(),
        platform: processRef.platform,
        arch: processRef.arch,
        hostname: os.hostname(),
        cpu_model: cpuModel,
        cpu_count: cpus.length || 0,
        memory_total: Number(os.totalmem?.() || 0),
        memory_free: Number(os.freemem?.() || 0),
        disk_total: diskTotal,
        disk_free: diskFree,
      },
      appInfo: {
        sekailink_version: app.getVersion ? String(app.getVersion() || "") : "",
        bootstrapper_version: String(bootstrap.releaseVersion || ""),
        bootstrap_channel: String(bootstrap.channel || ""),
        bootstrap_build: String(bootstrap.buildType || ""),
        electron: processRef.versions?.electron || "",
        chrome: processRef.versions?.chrome || "",
        node: processRef.versions?.node || "",
        archipelago_client_processes: Number(apStats.processes || 0),
        archipelago_client_trace_events: Number(apStats.traceEvents || 0),
      },
    };
  };

  const postJson = async (targetUrl, body, extraHeaders = {}, timeoutMs = 10000) => {
    return new Promise((resolve) => {
      try {
        const parsed = new URL(String(targetUrl || ""));
        const transport = parsed.protocol === "http:" ? http : https;
        const payload = Buffer.from(JSON.stringify(body || {}), "utf8");
        const req = transport.request(
          parsed,
          {
            method: "POST",
            headers: {
              "Content-Type": "application/json",
              "Content-Length": String(payload.length),
              "User-Agent": "SekaiLink-CrashReporter/1.0",
              ...extraHeaders,
            },
            timeout: Math.max(1000, Number(timeoutMs || 0)),
          },
          (res) => {
            let responseBody = "";
            res.on("data", (chunk) => {
              responseBody += String(chunk || "");
            });
            res.on("end", () => {
              const status = Number(res.statusCode || 0);
              resolve({ ok: status >= 200 && status < 300, status, body: responseBody });
            });
          }
        );
        req.on("timeout", () => req.destroy(new Error("request_timeout")));
        req.on("error", (err) => resolve({ ok: false, status: 0, error: String(err || "request_error"), body: "" }));
        req.write(payload);
        req.end();
      } catch (err) {
        resolve({ ok: false, status: 0, error: String(err || "invalid_request"), body: "" });
      }
    });
  };

  const submitDiagnosticsReport = async (options = {}) => {
    const uploadUrl = normalizeIpcString(options?.uploadUrl, 2048);
    if (!uploadUrl || !isSafeExternalUrl(uploadUrl)) return { ok: false, error: "invalid_upload_url" };
    const report = isPlainObject(options?.report) ? options.report : await collectDiagnosticsReport(options);
    const authToken = normalizeIpcString(options?.authToken, 4096);
    const headers = {};
    if (authToken) headers.Authorization = `Bearer ${authToken}`;
    const timeoutMsRaw = Number(options?.timeoutMs || 12000);
    const timeoutMs = Number.isFinite(timeoutMsRaw) ? Math.max(2000, timeoutMsRaw) : 12000;
    const result = await postJson(uploadUrl, report, headers, timeoutMs);
    if (!result.ok) return { ok: false, error: result.error || `upload_http_${result.status}`, status: result.status || 0 };
    return { ok: true, status: result.status || 200 };
  };

  return {
    startFileLogging,
    getLatestLogFilePath,
    readFileTailUtf8,
    collectDiagnosticsReport,
    collectBugReportArtifacts,
    postJson,
    submitDiagnosticsReport,
  };
};

module.exports = { createDiagnostics };
