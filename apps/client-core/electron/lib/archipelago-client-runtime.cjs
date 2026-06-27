"use strict";

const createArchipelagoClientRuntime = (deps = {}) => {
  const {
    fs,
    path,
    crypto,
    readline,
    spawn,
    processRef = process,
    getMainWindow = () => null,
    COMMONCLIENT_EVENT_CHANNEL,
    BIZHAWKCLIENT_EVENT_CHANNEL,
    ARCHIPELAGOCLIENT_EVENT_CHANNEL,
    writeLogLine,
    writeLogJson,
    nowIso,
    isPlainObject,
    normalizeIpcString,
    normalizeIpcPath,
    withApPythonEnv,
    getEffectiveApRootDirs,
    ensurePythonRuntime,
    getCommonClientWrapperPath,
    getBizHawkClientWrapperPath,
    getSniClientWrapperPath,
    getArchipelagoClientWrapperPath,
    getRetroarchMemoryBridgePath,
    getArchipelagoClientRegistryPath,
    getSniBridgePath,
    waitForTcpPort,
    isPidAlive,
    terminateChildProcess,
    purgeStaleSniBridgePortHolders,
    probeSniBridge,
    archipelagoClientProcs,
    archipelagoClientReadlines,
    archipelagoClientSessions,
    retroarchMemoryBridgeProcs,
  } = deps;
  const process = processRef;
  let commonClientProc = null;
  let commonClientRl = null;
  let bizhawkClientProc = null;
  let bizhawkClientRl = null;
  let bizhawkClientKind = "bizhawk";
  let sniBridgeProc = null;
  let sniBridgeEndpoint = "";
  const mmbn3BridgeProcs = new Map();
  const archipelagoClientTrace = [];

  const emitCommonClientEvent = (payload) => {
    const win = getMainWindow();
    if (!win || win.isDestroyed()) return;
    writeLogJson("commonclient", payload);
    win.webContents.send(COMMONCLIENT_EVENT_CHANNEL, payload);
  };
  
  const emitBizHawkClientEvent = (payload) => {
    const win = getMainWindow();
    if (!win || win.isDestroyed()) return;
    writeLogJson("bizhawkclient", payload);
    win.webContents.send(BIZHAWKCLIENT_EVENT_CHANNEL, payload);
  };
  
  const scrubTraceValue = (value, depth = 0) => {
    if (value == null) return value;
    if (depth > 4) return "[TRUNCATED]";
    if (typeof value === "string") {
      return value
        .replace(/\b(password|pass|token|secret|api[_-]?key|session|auth)\s*=\s*([^\s,;]+)/gi, "$1=[REDACTED]")
        .slice(0, 5000);
    }
    if (typeof value !== "object") return value;
    if (Array.isArray(value)) return value.slice(0, 50).map((entry) => scrubTraceValue(entry, depth + 1));
    const out = {};
    for (const [key, raw] of Object.entries(value)) {
      if (/(pass(word)?|secret|token|auth|cookie|session|api[-_]?key|credential|private[-_]?key)/i.test(key)) {
        out[key] = "[REDACTED]";
      } else {
        out[key] = scrubTraceValue(raw, depth + 1);
      }
    }
    return out;
  };
  
  const recordArchipelagoClientTrace = (payload) => {
    try {
      archipelagoClientTrace.push({
        at: nowIso(),
        ...scrubTraceValue(payload),
      });
      while (archipelagoClientTrace.length > 160) archipelagoClientTrace.shift();
    } catch (_err) {
      // Diagnostics must never affect runtime behavior.
    }
  };
  
  const formatArchipelagoClientTraceTail = () => {
    if (!archipelagoClientTrace.length) return "";
    return archipelagoClientTrace
      .slice(-80)
      .map((entry) => JSON.stringify(entry))
      .join("\n");
  };
  
  const emitArchipelagoClientEvent = (payload) => {
    recordArchipelagoClientTrace(payload);
    const win = getMainWindow();
    if (!win || win.isDestroyed()) return;
    writeLogJson("archipelagoclient", payload);
    win.webContents.send(ARCHIPELAGOCLIENT_EVENT_CHANNEL, payload);
  };
  
  const emitTrackerClientLog = (level, message) => {
    const lvl = String(level || "info").toLowerCase();
    const msg = String(message || "").trim();
    if (!msg) return;
    emitBizHawkClientEvent({ event: "log", level: lvl, logger: "Tracker", message: msg });
  };
  
  const sendCommonClientCommand = async (command) => {
    if (!commonClientProc || !command) return { ok: false, error: "not_running" };
    try {
      commonClientProc.stdin.write(JSON.stringify(command) + "\n");
      return { ok: true };
    } catch (err) {
      return { ok: false, error: String(err) };
    }
  };
  
  const stopCommonClient = async () => {
    if (!commonClientProc) return;
    const proc = commonClientProc;
    try {
      proc.stdin.write(JSON.stringify({ cmd: "shutdown" }) + "\n");
    } catch (_err) {
      // ignore
    }
    writeLogLine("info", "commonclient", `stopping pid=${proc.pid || ""}`);
    await terminateChildProcess(proc, "commonclient", { graceMs: 1400 });
    commonClientProc = null;
    if (commonClientRl) {
      commonClientRl.close();
      commonClientRl = null;
    }
  };
  
  const sendBizHawkClientCommand = async (command) => {
    if (!bizhawkClientProc || !command) return { ok: false, error: "not_running" };
    try {
      bizhawkClientProc.stdin.write(JSON.stringify(command) + "\n");
      return { ok: true };
    } catch (err) {
      return { ok: false, error: String(err) };
    }
  };
  
  const readArchipelagoClientRegistry = () => {
    const registryPath = getArchipelagoClientRegistryPath();
    try {
      const raw = fs.readFileSync(registryPath, "utf8");
      const parsed = JSON.parse(raw);
      return { ok: true, registry: parsed, path: registryPath };
    } catch (err) {
      return { ok: false, error: String(err?.message || err || "registry_read_failed"), path: registryPath };
    }
  };
  
  const normalizeArchipelagoClientLookupKey = (value) =>
    String(value || "").trim().toLowerCase().replace(/[^a-z0-9]+/g, "_").replace(/^_+|_+$/g, "");
  
  const normalizeRuntimeSessionKey = (moduleId, slot) => {
    const moduleKey = normalizeArchipelagoClientLookupKey(moduleId || "unknown");
    const slotKey = normalizeArchipelagoClientLookupKey(slot || "unknown");
    return `${moduleKey}:${slotKey}`;
  };
  
  const resolveArchipelagoClientSpec = (options = {}) => {
    const registryRes = readArchipelagoClientRegistry();
    if (!registryRes.ok) return { ok: false, error: registryRes.error || "registry_read_failed" };
    const registry = registryRes.registry || {};
    const clients = Array.isArray(registry.clients) ? registry.clients : [];
    const rawKey = options.gameKey || options.game_key || options.world || options.game || "";
    const lookupKey = normalizeArchipelagoClientLookupKey(rawKey);
    if (!lookupKey) return { ok: true, registry, client: null };
    const client = clients.find((entry) => {
      const keys = [
        entry?.game_key,
        entry?.world,
        entry?.game,
        entry?.client_file,
      ].map(normalizeArchipelagoClientLookupKey);
      return keys.includes(lookupKey);
    });
    if (!client) return { ok: false, error: "archipelago_client_not_registered", gameKey: rawKey };
    return { ok: true, registry, client };
  };
  
  const normalizeArchipelagoClientKind = (kind) => {
    const value = String(kind || "").trim().toLowerCase();
    if (["text", "bizhawk", "sni", "oot", "dolphin", "module", "mmbn3"].includes(value)) return value;
    return "";
  };
  
  const normalizeClientMemoryBridgeKind = (value) => {
    const normalized = String(value || "").trim().toLowerCase().replace(/[-\s]+/g, "_");
    if (["retroarch", "retroarch_udp", "ra_udp"].includes(normalized)) return "retroarch_udp";
    return "";
  };

  const moduleNameFromClientFile = (clientFile) => {
    const normalized = normalizeIpcString(clientFile || "", 400)
      .replace(/\\/g, "/")
      .replace(/\.py$/i, "")
      .replace(/\/__init__$/i, "");
    if (!normalized || !normalized.startsWith("worlds/")) return "";
    return normalized.split("/").filter(Boolean).join(".");
  };
  
  const startRetroarchMemoryBridge = async (clientId, options = {}) => {
    const id = normalizeIpcString(clientId, 120);
    if (!id) return { ok: false, error: "invalid_client_id" };
    if (retroarchMemoryBridgeProcs.has(id)) return { ok: true, alreadyRunning: true };
    const memorySocket = normalizeIpcString(options.memorySocket || options.memorySocketPath || "", 500);
    if (!memorySocket) return { ok: false, error: "memory_socket_missing" };
    const python = await ensurePythonRuntime();
    const bridgePath = getRetroarchMemoryBridgePath();
    if (!fs.existsSync(bridgePath)) return { ok: false, error: "retroarch_memory_bridge_missing", detail: bridgePath };
    const host = normalizeIpcString(options.host || "127.0.0.1", 80) || "127.0.0.1";
    const port = Number(options.port || 55355);
    const args = [bridgePath, "--memory-socket", memorySocket, "--host", host, "--port", String(port)];
    const proc = spawn(python, args, {
      stdio: ["ignore", "pipe", "pipe"],
      env: withApPythonEnv(process.env),
      windowsHide: true,
    });
    retroarchMemoryBridgeProcs.set(id, proc);
    writeLogLine("info", "retroarch-bridge", `spawned client=${id} pid=${proc.pid || ""} endpoint=${host}:${port} memory=${memorySocket}`);
    proc.stdout.on("data", (chunk) => {
      for (const line of String(chunk || "").split(/\r?\n/)) {
        const trimmed = line.trim();
        if (trimmed) writeLogLine("info", "retroarch-bridge", trimmed);
      }
    });
    proc.stderr.on("data", (chunk) => {
      const text = String(chunk || "").trim();
      if (text) writeLogLine("warn", "retroarch-bridge", text);
    });
    proc.on("exit", (code, signal) => {
      retroarchMemoryBridgeProcs.delete(id);
      writeLogLine("info", "retroarch-bridge", `exit client=${id} code=${code ?? "null"} signal=${signal || "none"}`);
    });
    await new Promise((resolve) => setTimeout(resolve, 180));
    return { ok: true, pid: proc.pid || 0, host, port };
  };
  
  const stopRetroarchMemoryBridge = async (clientId) => {
    const id = normalizeIpcString(clientId, 120);
    const proc = retroarchMemoryBridgeProcs.get(id);
    if (!proc) return { ok: false, error: "not_running" };
    await terminateChildProcess(proc, "retroarch-bridge", { graceMs: 700 });
    retroarchMemoryBridgeProcs.delete(id);
    return { ok: true };
  };

  const getMmbn3BridgePath = () => {
    const apRoot = getEffectiveApRootDirs()[0] || "";
    return apRoot ? path.join(apRoot, "..", "mmbn3_bridge.py") : "";
  };

  const startMmbn3Bridge = async (clientId, options = {}) => {
    const id = normalizeIpcString(clientId, 120);
    if (!id) return { ok: false, error: "invalid_client_id" };
    if (mmbn3BridgeProcs.has(id)) return { ok: true, alreadyRunning: true };
    const memorySocket = normalizeIpcString(options.memorySocket || options.memorySocketPath || "", 500);
    if (!memorySocket) return { ok: false, error: "memory_socket_missing" };
    const bridgePath = getMmbn3BridgePath();
    if (!bridgePath || !fs.existsSync(bridgePath)) return { ok: false, error: "mmbn3_bridge_missing", detail: bridgePath };
    const python = await ensurePythonRuntime();
    const args = [
      bridgePath,
      "--memory-socket", memorySocket,
      "--host", "127.0.0.1",
      "--port", "28922",
      "--player-name", normalizeIpcString(options.playerName || "", 120),
    ];
    const proc = spawn(python, args, {
      stdio: ["ignore", "pipe", "pipe"],
      env: withApPythonEnv(process.env),
      windowsHide: true,
    });
    mmbn3BridgeProcs.set(id, proc);
    writeLogLine("info", "mmbn3-bridge", `spawned client=${id} pid=${proc.pid || ""} memory=${memorySocket}`);
    proc.stdout.on("data", (chunk) => {
      for (const line of String(chunk || "").split(/\r?\n/)) {
        const trimmed = line.trim();
        if (trimmed) writeLogLine("info", "mmbn3-bridge", trimmed);
      }
    });
    proc.stderr.on("data", (chunk) => {
      const text = String(chunk || "").trim();
      if (text) writeLogLine("warn", "mmbn3-bridge", text);
    });
    proc.on("exit", (code, signal) => {
      mmbn3BridgeProcs.delete(id);
      writeLogLine("warn", "mmbn3-bridge", `exit client=${id} code=${code ?? "null"} signal=${signal || "none"}`);
    });
    await new Promise((resolve) => setTimeout(resolve, 180));
    if (!isPidAlive(proc.pid || 0)) {
      mmbn3BridgeProcs.delete(id);
      return { ok: false, error: "mmbn3_bridge_spawn_exited" };
    }
    return { ok: true, pid: proc.pid || 0 };
  };

  const stopMmbn3Bridge = async (clientId) => {
    const id = normalizeIpcString(clientId, 120);
    const proc = mmbn3BridgeProcs.get(id);
    if (!proc) return { ok: false, error: "not_running" };
    await terminateChildProcess(proc, "mmbn3-bridge", { graceMs: 700 });
    mmbn3BridgeProcs.delete(id);
    return { ok: true };
  };
  
  const startArchipelagoClient = async (options = {}) => {
    const specRes = resolveArchipelagoClientSpec(options);
    if (!specRes.ok) {
      recordArchipelagoClientTrace({ event: "launch_failed", stage: "resolve_spec", error: specRes.error, options });
      return specRes;
    }
    const clientSpec = specRes.client;
    const status = normalizeIpcString(clientSpec?.status || "", 80).toLowerCase();
    if (["generator_only", "runtime_unavailable", "tracker_only"].includes(status)) {
      recordArchipelagoClientTrace({
        event: "launch_failed",
        stage: "client_status",
        error: "archipelago_client_runtime_unavailable",
        status,
        client: clientSpec || null,
      });
      return {
        ok: false,
        error: "archipelago_client_runtime_unavailable",
        status,
        detail: clientSpec?.notes || "",
        client: clientSpec || null,
      };
    }
    const kind = normalizeArchipelagoClientKind(options.kind || options.wrapper || clientSpec?.wrapper);
    if (!kind) {
      recordArchipelagoClientTrace({ event: "launch_failed", stage: "kind", error: "invalid_client_kind", client: clientSpec || null });
      return { ok: false, error: "invalid_client_kind" };
    }
    const defaultClientId = clientSpec?.game_key ? `${clientSpec.game_key}-${Date.now()}` : `${kind}-${Date.now()}`;
    const clientId = normalizeIpcString(options.clientId || defaultClientId, 120).replace(/[^a-z0-9_.:-]+/gi, "_");
    if (archipelagoClientProcs.has(clientId)) return { ok: true, alreadyRunning: true, clientId };
    const sessionMeta = {
      moduleId: normalizeIpcString(options.moduleId || options.module_id || clientSpec?.game_key || "", 160),
      slot: normalizeIpcString(options.slot || options.name || "", 160),
      gameKey: normalizeIpcString(options.gameKey || options.game_key || clientSpec?.game_key || "", 160),
      kind,
      launchedAt: nowIso(),
    };
  
    let python = "";
    try {
      python = await ensurePythonRuntime();
    } catch (err) {
      const detail = String(err?.message || err || "python_runtime_failed");
      recordArchipelagoClientTrace({ event: "launch_failed", stage: "python_runtime", error: detail, clientId, kind, client: clientSpec || null });
      return { ok: false, error: "python_runtime_failed", detail };
    }
    const wrapperPath = getArchipelagoClientWrapperPath();
    if (!fs.existsSync(wrapperPath)) {
      recordArchipelagoClientTrace({ event: "launch_failed", stage: "wrapper", error: "apclient_wrapper_missing", detail: wrapperPath, clientId, kind, client: clientSpec || null });
      return { ok: false, error: "apclient_wrapper_missing", detail: wrapperPath };
    }
  
    if (kind === "sni") {
      const bridgeRes = await startSniBridge({
        host: "127.0.0.1",
        port: 23074,
        memorySocket: options.memorySocketPath || options.memorySocket || "",
        luaHost: "127.0.0.1",
        luaPortStart: 43055,
        luaPortEnd: 43060,
      });
      if (!bridgeRes?.ok) {
        writeLogLine("warn", "archipelagoclient", `sni bridge failed before client launch: ${bridgeRes?.error || "unknown"}`);
      }
    }
  
    const memoryBridgeKind = normalizeClientMemoryBridgeKind(options.memoryBridge || clientSpec?.memory_bridge || clientSpec?.memoryBridge);
    if (memoryBridgeKind === "retroarch_udp") {
      const bridgeRes = await startRetroarchMemoryBridge(clientId, {
        memorySocket: options.memorySocketPath || options.memorySocket || "",
        port: clientSpec?.retroarch_port || clientSpec?.retroarchPort || 55355,
      });
      if (!bridgeRes?.ok) {
        recordArchipelagoClientTrace({ event: "launch_failed", stage: "retroarch_memory_bridge", error: bridgeRes?.error || "retroarch_bridge_failed", clientId, kind, client: clientSpec || null });
        return { ok: false, error: bridgeRes?.error || "retroarch_bridge_failed", detail: bridgeRes?.detail || "" };
      }
    }
    if (kind === "mmbn3") {
      const bridgeRes = await startMmbn3Bridge(clientId, {
        memorySocket: options.memorySocketPath || options.memorySocket || "",
        playerName: options.slot || options.name || "",
      });
      if (!bridgeRes?.ok) {
        recordArchipelagoClientTrace({ event: "launch_failed", stage: "mmbn3_bridge", error: bridgeRes?.error || "mmbn3_bridge_failed", detail: bridgeRes?.detail || "", clientId, kind, client: clientSpec || null });
        return { ok: false, error: bridgeRes?.error || "mmbn3_bridge_failed", detail: bridgeRes?.detail || "" };
      }
    }
  
    let directScriptPath = "";
    if (kind === "mmbn3") {
      directScriptPath = path.join(getEffectiveApRootDirs()[0] || "", "MMBN3Client.py");
      if (!fs.existsSync(directScriptPath)) {
        recordArchipelagoClientTrace({ event: "launch_failed", stage: "mmbn3_client", error: "mmbn3_client_missing", detail: directScriptPath, clientId, kind, client: clientSpec || null });
        return { ok: false, error: "mmbn3_client_missing", detail: directScriptPath };
      }
    }
    const args = kind === "mmbn3" ? [directScriptPath] : [wrapperPath, "--kind", kind];
    const moduleName = normalizeIpcString(options.module || options.clientModule || clientSpec?.module || "", 300);
    if ((kind === "dolphin" || kind === "module") && !moduleName) {
      recordArchipelagoClientTrace({ event: "launch_failed", stage: "module", error: `${kind}_module_required`, clientId, kind, client: clientSpec || null });
      return { ok: false, error: `${kind}_module_required` };
    }
    if (kind === "dolphin" && clientSpec?.status === "overlay_required" && !options.allowOverlayRequired) {
      const moduleParts = moduleName ? moduleName.split(".") : [];
      const moduleExists = moduleParts.length > 0 && getEffectiveApRootDirs().some((root) =>
        fs.existsSync(path.join(root, ...moduleParts) + ".py")
      );
      if (!moduleExists) {
        recordArchipelagoClientTrace({ event: "launch_failed", stage: "module", error: "dolphin_client_overlay_required", clientId, kind, gameKey: clientSpec.game_key || "", module: moduleName });
        return {
          ok: false,
          error: "dolphin_client_overlay_required",
          gameKey: clientSpec.game_key || "",
          module: moduleName,
        };
      }
    }
    if (moduleName && kind !== "mmbn3") args.push("--module", moduleName);
    const clientModuleName = moduleNameFromClientFile(options.clientFile || options.client_file || clientSpec?.client_file || "");
    const clientPlatform = normalizeIpcString(clientSpec?.platform || options.platform || "", 40).toUpperCase();
    const shouldUseScopedClientModule = clientModuleName && (
      kind === "sni" ||
      (kind === "bizhawk" && clientPlatform === "GBA")
    );
    if (shouldUseScopedClientModule) args.push("--client-module", clientModuleName);
    const address = normalizeIpcString(options.address || options.serverAddress || "", 300);
    if (address) args.push("--connect", address);
    const slot = normalizeIpcString(options.slot || options.name || "", 160);
    if (slot && clientSpec?.pass_slot !== false && kind !== "mmbn3") args.push("--slot", slot);
    const password = options.password == null ? "" : normalizeIpcString(options.password, 300);
    if (password) args.push("--password", password);
    const sniAddress = normalizeIpcString(options.sniAddress || "", 300);
    if (sniAddress) args.push("--sni-address", sniAddress);
    const patchPath = normalizeIpcPath(options.patchPath || options.patch || "");
    if (patchPath && clientSpec?.pass_patch !== false && kind !== "mmbn3") args.push("--patch", patchPath);
    const romPath = normalizeIpcPath(options.romPath || options.rom || "");
    if (romPath && kind !== "mmbn3") args.push("--rom", romPath);
  
    const proc = spawn(python, args, {
      stdio: ["pipe", "pipe", "pipe"],
      env: withApPythonEnv(process.env),
      windowsHide: true,
    });
    archipelagoClientProcs.set(clientId, proc);
    archipelagoClientSessions.set(clientId, sessionMeta);
    recordArchipelagoClientTrace({ event: "spawn", clientId, kind, client: clientSpec || null, module: moduleName, pid: proc.pid || 0 });
    writeLogLine("info", "archipelagoclient", `spawned id=${clientId} kind=${kind} game=${clientSpec?.game_key || ""} pid=${proc.pid || ""} python=${python}`);
  
    const rl = readline.createInterface({ input: proc.stdout });
    archipelagoClientReadlines.set(clientId, rl);
    rl.on("line", (line) => {
      const trimmed = String(line || "").trim();
      if (!trimmed) return;
      try {
        const data = JSON.parse(trimmed);
        const payload = { clientId, kind, client: clientSpec || null, ...data };
        emitArchipelagoClientEvent(payload);
      } catch (_err) {
        const payload = { clientId, kind, client: clientSpec || null, event: "log", level: "warn", logger: "ArchipelagoClient", message: trimmed };
        emitArchipelagoClientEvent(payload);
      }
    });
    proc.stderr.on("data", (chunk) => {
      const payload = {
        clientId,
        kind,
        client: clientSpec || null,
        event: "log",
        level: "error",
        logger: "ArchipelagoClient",
        message: String(chunk || "").trim(),
      };
      emitArchipelagoClientEvent(payload);
    });
    proc.on("exit", (code, signal) => {
      archipelagoClientProcs.delete(clientId);
      archipelagoClientSessions.delete(clientId);
      stopRetroarchMemoryBridge(clientId).catch((_err) => {});
      stopMmbn3Bridge(clientId).catch((_err) => {});
      const activeRl = archipelagoClientReadlines.get(clientId);
      if (activeRl) activeRl.close();
      archipelagoClientReadlines.delete(clientId);
      const payload = { clientId, kind, client: clientSpec || null, event: "exit", code, signal };
      emitArchipelagoClientEvent(payload);
    });
  
    return { ok: true, clientId, kind, client: clientSpec || null, module: moduleName, pid: proc.pid || 0 };
  };
  
  const sendArchipelagoClientCommand = async (clientId, command) => {
    const id = normalizeIpcString(clientId, 120);
    const proc = archipelagoClientProcs.get(id);
    if (!proc || !command) return { ok: false, error: "not_running" };
    try {
      proc.stdin.write(`${JSON.stringify(command)}\n`);
      return { ok: true };
    } catch (err) {
      return { ok: false, error: String(err?.message || err) };
    }
  };
  
  const stopArchipelagoClient = async (clientId) => {
    const id = normalizeIpcString(clientId, 120);
    const proc = archipelagoClientProcs.get(id);
    if (!proc) return { ok: false, error: "not_running" };
    try {
      proc.stdin.write(`${JSON.stringify({ cmd: "shutdown" })}\n`);
    } catch (_err) {
      // ignore shutdown write failures
    }
    await terminateChildProcess(proc, "archipelagoclient", { graceMs: 1400 });
    await stopRetroarchMemoryBridge(id).catch((_err) => {});
    archipelagoClientProcs.delete(id);
    archipelagoClientSessions.delete(id);
    const rl = archipelagoClientReadlines.get(id);
    if (rl) rl.close();
    archipelagoClientReadlines.delete(id);
    return { ok: true };
  };
  
  const stopArchipelagoClientsForSession = async (moduleId, slot, reason = "session_cleanup") => {
    const sessionKey = normalizeRuntimeSessionKey(moduleId, slot);
    const matches = [];
    for (const [clientId, meta] of Array.from(archipelagoClientSessions.entries())) {
      if (normalizeRuntimeSessionKey(meta?.moduleId, meta?.slot) === sessionKey) {
        matches.push(clientId);
      }
    }
    if (!matches.length) return { ok: true, stopped: 0 };
    writeLogLine("info", "archipelagoclient", `session cleanup reason=${reason} key=${sessionKey} clients=${matches.join(",")}`);
    for (const clientId of matches) {
      await stopArchipelagoClient(clientId);
    }
    return { ok: true, stopped: matches.length };
  };
  
  const startSniBridge = async (options = {}) => {
    const memorySocket = normalizeIpcString(options.memorySocket || options.memorySocketPath || "", 500);
    const endpointKey = memorySocket || `legacy:${String(options.luaHost || "127.0.0.1")}:${Number(options.luaPortStart || 43055)}-${Number(options.luaPortEnd || 43060)}`;
    if (sniBridgeProc) {
      if (sniBridgeEndpoint === endpointKey) return { ok: true, alreadyRunning: true };
      const oldProc = sniBridgeProc;
      writeLogLine("info", "sni-bridge", `restarting for endpoint change old=${sniBridgeEndpoint || "none"} new=${endpointKey}`);
      sniBridgeProc = null;
      sniBridgeEndpoint = "";
      await terminateChildProcess(oldProc, "sni-bridge", { graceMs: 500 }).catch((_err) => {});
      await purgeStaleSniBridgePortHolders(23074, 0);
    }
    const python = await ensurePythonRuntime();
    const bridgePath = getSniBridgePath();
    if (!bridgePath || !fs.existsSync(bridgePath)) {
      return { ok: false, error: "sni_bridge_missing", detail: `missing: ${bridgePath}` };
    }
  
    const host = String(options.host || "127.0.0.1");
    const port = Number(options.port || 23074);
    const luaHost = String(options.luaHost || "127.0.0.1");
    const luaPortStart = Number(options.luaPortStart || 43055);
    const luaPortEnd = Number(options.luaPortEnd || 43060);
    const cleanup = await purgeStaleSniBridgePortHolders(port, 0);
    if (!cleanup.ok) {
      return {
        ok: false,
        error: "sni_bridge_port_in_use",
        detail: `port ${port} in use by non-SekaiLink process`,
      };
    }
    const args = [
      bridgePath,
      "--host", host,
      "--port", `${port}`,
      "--lua-host", luaHost,
      "--lua-port-start", `${luaPortStart}`,
      "--lua-port-end", `${luaPortEnd}`,
    ];
    if (memorySocket) args.push("--memory-socket", memorySocket);
  
    sniBridgeProc = spawn(python, args, {
      stdio: ["ignore", "pipe", "pipe"],
      env: withApPythonEnv(process.env),
      windowsHide: true,
    });
    sniBridgeEndpoint = endpointKey;
    writeLogLine("info", "sni-bridge", `spawned pid=${sniBridgeProc.pid || ""} python=${python} script=${bridgePath} endpoint=${endpointKey}`);
  
    sniBridgeProc.stdout.on("data", (chunk) => {
      const text = String(chunk || "").trim();
      if (!text) return;
      for (const line of text.split(/\r?\n/).filter(Boolean).slice(-4)) {
        writeLogLine("info", "sni-bridge", line);
      }
    });
    sniBridgeProc.stderr.on("data", (chunk) => {
      const text = String(chunk || "").trim();
      if (!text) return;
      for (const line of text.split(/\r?\n/).filter(Boolean).slice(-4)) {
        writeLogLine("warn", "sni-bridge", line);
      }
    });
    sniBridgeProc.on("exit", (code, signal) => {
      writeLogLine("warn", "sni-bridge", `exited code=${code ?? "null"} signal=${signal || "none"}`);
      sniBridgeProc = null;
      sniBridgeEndpoint = "";
    });
  
    const ok = await waitForTcpPort(host, port, 6000);
    if (!ok) {
      const spawned = sniBridgeProc;
      if (spawned) await terminateChildProcess(spawned, "sni-bridge", { graceMs: 400 });
      sniBridgeProc = null;
      sniBridgeEndpoint = "";
      return { ok: false, error: "sni_bridge_start_timeout" };
    }
    const spawnedPid = Number(sniBridgeProc?.pid || 0);
    if (!(spawnedPid > 0 && isPidAlive(spawnedPid))) {
      sniBridgeProc = null;
      sniBridgeEndpoint = "";
      return { ok: false, error: "sni_bridge_spawn_exited" };
    }
    const probe = probeSniBridge(python, host, port, 2500);
    if (!probe.ok) {
      const spawned = sniBridgeProc;
      if (spawned) await terminateChildProcess(spawned, "sni-bridge", { graceMs: 400 });
      sniBridgeProc = null;
      sniBridgeEndpoint = "";
      return { ok: false, error: probe.error || "sni_bridge_probe_failed", detail: probe.detail || "" };
    }
    return { ok: true, host, port };
  };
  
  const stopSniBridge = async () => {
    if (!sniBridgeProc) return;
    const proc = sniBridgeProc;
    try {
      await terminateChildProcess(proc, "sni-bridge", { graceMs: 1000 });
    } catch (_err) {
      // ignore
    }
    sniBridgeProc = null;
    sniBridgeEndpoint = "";
    await purgeStaleSniBridgePortHolders(23074, 0);
  };
  
  const stopBizHawkClient = async () => {
    if (!bizhawkClientProc) return;
    const proc = bizhawkClientProc;
    try {
      proc.stdin.write(JSON.stringify({ cmd: "shutdown" }) + "\n");
    } catch (_err) {
      // ignore
    }
    writeLogLine("info", "bizhawkclient", `stopping pid=${proc.pid || ""}`);
    await terminateChildProcess(proc, "bizhawkclient", { graceMs: 1400 });
    bizhawkClientProc = null;
    bizhawkClientKind = "bizhawk";
    await stopSniBridge();
    if (bizhawkClientRl) {
      bizhawkClientRl.close();
      bizhawkClientRl = null;
    }
  };
  
  const startCommonClient = async (options = {}) => {
    if (commonClientProc) return { ok: true, alreadyRunning: true };
  
    const python = await ensurePythonRuntime();
    const wrapperPath = getCommonClientWrapperPath();
    // Security: avoid putting secrets in argv. We send connection details via stdin JSON commands.
    // Also avoid passing address/slot to keep the startup argv minimal and consistent.
    const args = [wrapperPath];
  
    commonClientProc = spawn(python, args, {
      stdio: ["pipe", "pipe", "pipe"],
      env: withApPythonEnv(process.env)
    });
    writeLogLine("info", "commonclient", `spawned pid=${commonClientProc.pid || ""} python=${python} wrapper=${wrapperPath}`);
  
    commonClientRl = readline.createInterface({ input: commonClientProc.stdout });
    commonClientRl.on("line", (line) => {
      const trimmed = line.trim();
      if (!trimmed) return;
      try {
        const data = JSON.parse(trimmed);
        emitCommonClientEvent(data);
      } catch (_err) {
        emitCommonClientEvent({ event: "error", message: "Invalid JSON from CommonClient" });
      }
    });
  
    commonClientProc.stderr.on("data", (chunk) => {
      emitCommonClientEvent({ event: "log", level: "error", logger: "CommonClient", message: String(chunk).trim() });
    });
  
    commonClientProc.on("exit", (code, signal) => {
      emitCommonClientEvent({ event: "exit", code, signal });
      commonClientProc = null;
      if (commonClientRl) {
        commonClientRl.close();
        commonClientRl = null;
      }
    });
  
    return { ok: true };
  };
  
  const startBizHawkClient = async (options = {}) => {
    const requestedKind = String(options.clientKind || "").trim().toLowerCase();
    const kind = requestedKind === "sni" ? "sni" : "bizhawk";
    if (bizhawkClientProc) {
      // If the running client wrapper kind doesn't match the requested one,
      // restart cleanly to avoid running SNI logic during non-SNES launches
      // (causes CARTROM/CARTRAM domain spam in BizHawk logs on NES/GBA/etc).
      if (bizhawkClientKind !== kind) {
        await stopBizHawkClient();
      } else {
        return { ok: true, alreadyRunning: true };
      }
    }
  
    const python = await ensurePythonRuntime();
    const wrapperPath = kind === "sni" ? getSniClientWrapperPath() : getBizHawkClientWrapperPath();
    const args = [wrapperPath];
  
    bizhawkClientProc = spawn(python, args, {
      stdio: ["pipe", "pipe", "pipe"],
      env: withApPythonEnv(process.env)
    });
    bizhawkClientKind = kind;
    writeLogLine("info", "bizhawkclient", `spawned pid=${bizhawkClientProc.pid || ""} python=${python} wrapper=${wrapperPath}`);
  
    bizhawkClientRl = readline.createInterface({ input: bizhawkClientProc.stdout });
    bizhawkClientRl.on("line", (line) => {
      const trimmed = String(line || "").trim();
      if (!trimmed) return;
      try {
        const data = JSON.parse(trimmed);
        emitBizHawkClientEvent(data);
      } catch (_err) {
        writeLogLine("warn", "bizhawkclient", `non-json stdout: ${trimmed}`);
        emitBizHawkClientEvent({ event: "log", level: "warn", logger: "BizHawkClient", message: trimmed });
      }
    });
  
    bizhawkClientProc.stderr.on("data", (chunk) => {
      emitBizHawkClientEvent({ event: "log", level: "error", logger: "BizHawkClient", message: String(chunk).trim() });
    });
  
    bizhawkClientProc.on("exit", (code, signal) => {
      emitBizHawkClientEvent({ event: "exit", code, signal });
      bizhawkClientProc = null;
      if (bizhawkClientRl) {
        bizhawkClientRl.close();
        bizhawkClientRl = null;
      }
    });
  
    // For now, the caller should send a "connect" JSON command after spawning.
    return { ok: true };
  };

  const getArchipelagoClientStats = () => ({
    processes: archipelagoClientProcs.size,
    traceEvents: archipelagoClientTrace.length,
  });

  const stopAllArchipelagoClients = async () => {
    for (const clientId of Array.from(archipelagoClientProcs.keys())) {
      await stopArchipelagoClient(clientId);
    }
  };

  const stopAllRetroarchMemoryBridges = async () => {
    for (const clientId of Array.from(retroarchMemoryBridgeProcs.keys())) {
      await stopRetroarchMemoryBridge(clientId).catch((_err) => {});
    }
  };

  return {
    getArchipelagoClientStats,
    formatArchipelagoClientTraceTail,
    readArchipelagoClientRegistry,
    emitTrackerClientLog,
    startCommonClient,
    sendCommonClientCommand,
    stopCommonClient,
    startBizHawkClient,
    sendBizHawkClientCommand,
    stopBizHawkClient,
    startArchipelagoClient,
    sendArchipelagoClientCommand,
    stopArchipelagoClient,
    stopArchipelagoClientsForSession,
    startRetroarchMemoryBridge,
    stopRetroarchMemoryBridge,
    stopAllArchipelagoClients,
    stopAllRetroarchMemoryBridges,
    startSniBridge,
    stopSniBridge,
  };
};

module.exports = { createArchipelagoClientRuntime };
