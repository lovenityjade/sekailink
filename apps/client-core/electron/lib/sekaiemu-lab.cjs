const createSekaiemuLab = ({
  app,
  fs,
  path,
  net,
  Buffer,
  getSekaiemuSettings,
  getRuntimePlatformPath,
  getBundledRuntimeDir,
  dirExists,
  fileExists,
  pathExists,
  ensureDir,
  isPlainObject,
  normalizeIpcPath,
  nativeGameProcs,
  sekaiemuChatBridges,
  terminateChildProcess,
  secureIpcHandle,
  tryLaunchSekaiemu,
}) => {
  const sessions = new Map();

  const inferCoreSystemLabel = (corePath) => {
    const name = path.basename(String(corePath || "")).toLowerCase();
    if (name.includes("bsnes") || name.includes("snes9x") || name.includes("snes")) return "SNES";
    if (name.includes("mgba")) return "GBA";
    if (name.includes("gambatte")) return "GB/GBC";
    if (name.includes("fce") || name.includes("nestopia")) return "NES";
    if (name.includes("mupen") || name.includes("parallel")) return "N64";
    if (name.includes("genesis") || name.includes("picodrive") || name.includes("smsplus")) return "SEGA";
    return "Libretro";
  };

  const listCoreRoots = () => {
    const settings = getSekaiemuSettings();
    const home = app.getPath("home");
    return [
      ...settings.core_dirs,
      getRuntimePlatformPath("cores"),
      path.join(getBundledRuntimeDir(), "cores"),
      path.join(getBundledRuntimeDir(), "libretro"),
      process.platform === "win32" ? "" : "/usr/lib64/libretro",
      process.platform === "win32" ? "" : "/usr/lib/libretro",
      path.join(home, ".config", "retroarch", "cores"),
      ...(app.isPackaged ? [] : [
        path.join(home, "Projects", "Sekaiemu-Libretro-Spike-Codex", "workspace", "src"),
      ]),
    ].filter(Boolean);
  };

  const listCores = () => {
    const ext = process.platform === "win32" ? ".dll" : process.platform === "darwin" ? ".dylib" : ".so";
    const seen = new Set();
    const cores = [];
    const scan = (root, depth = 0) => {
      if (!dirExists(root) || depth > 3) return;
      let entries = [];
      try {
        entries = fs.readdirSync(root, { withFileTypes: true });
      } catch (_err) {
        return;
      }
      for (const entry of entries) {
        const full = path.join(root, entry.name);
        if (entry.isDirectory()) {
          scan(full, depth + 1);
          continue;
        }
        if (!entry.isFile() && !entry.isSymbolicLink()) continue;
        const lower = entry.name.toLowerCase();
        if (!lower.endsWith(ext) || !lower.includes("libretro")) continue;
        if (seen.has(full)) continue;
        seen.add(full);
        cores.push({
          id: path.basename(entry.name, ext),
          name: entry.name,
          path: full,
          system: inferCoreSystemLabel(full),
        });
      }
    };
    const roots = listCoreRoots();
    for (const root of roots) scan(root);
    cores.sort((a, b) => `${a.system}:${a.name}`.localeCompare(`${b.system}:${b.name}`));
    return { ok: true, cores, roots };
  };

  const sendMemoryRequest = (endpoint, requests, timeoutMs = 1800) => new Promise((resolve) => {
    const rawEndpoint = String(endpoint || "").trim();
    if (!rawEndpoint) {
      resolve({ ok: false, error: "memory_socket_missing" });
      return;
    }
    const payload = `${JSON.stringify(requests)}\n`;
    let settled = false;
    let buffer = "";
    const connectOptions = rawEndpoint.startsWith("tcp://")
      ? (() => {
          const url = new URL(rawEndpoint);
          return { host: url.hostname || "127.0.0.1", port: Number(url.port || 0) };
        })()
      : { path: rawEndpoint };
    const socket = net.createConnection(connectOptions, () => {
      socket.write(payload);
    });
    const timer = setTimeout(() => finish({ ok: false, error: "memory_socket_timeout" }), timeoutMs);
    const finish = (result) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      try {
        socket.destroy();
      } catch (_err) {
        // ignore close failures
      }
      resolve(result);
    };
    socket.setEncoding("utf8");
    socket.on("data", (chunk) => {
      buffer += chunk;
      const newline = buffer.indexOf("\n");
      if (newline < 0) return;
      const line = buffer.slice(0, newline).trim();
      try {
        finish({ ok: true, responses: JSON.parse(line) });
      } catch (err) {
        finish({ ok: false, error: "memory_socket_bad_json", detail: String(err || "") });
      }
    });
    socket.on("error", (err) => finish({ ok: false, error: "memory_socket_error", detail: String(err?.message || err || "") }));
  });

  const rememberSession = (pid, session) => {
    const safePid = Number(pid);
    if (!Number.isFinite(safePid) || safePid <= 0) return;
    sessions.set(safePid, { ...session, pid: safePid });
  };

  const forgetSession = (pid) => {
    const safePid = Number(pid);
    if (!Number.isFinite(safePid) || safePid <= 0) return;
    sessions.delete(safePid);
  };

  const getSessionForPid = (pid) => {
    const safePid = Number(pid);
    if (!Number.isFinite(safePid) || safePid <= 0) return null;
    return sessions.get(safePid) || null;
  };

  const registerIpcHandlers = () => {
    secureIpcHandle("sekaiemu:lab:listCores", async () => listCores());

    secureIpcHandle("sekaiemu:lab:launch", async (_event, options) => {
      const safe = isPlainObject(options) ? options : {};
      const romPath = normalizeIpcPath(safe.romPath);
      const corePath = normalizeIpcPath(safe.corePath);
      const trackerPackPath = normalizeIpcPath(safe.trackerPackPath || "");
      const layoutPreview = Boolean(safe.layoutPreview);
      const previewManifest = layoutPreview
        ? {
            game_id: "alttp",
            display_name: "A Link to the Past Layout Preview",
            emu: "sekaiemu",
            sekaiemu: {
              tracker_pack: "tracker-bundles/alttp-native",
            },
          }
        : {
            game_id: "runtime-lab",
            display_name: "Sekaiemu Runtime Lab",
            emu: "sekaiemu",
            sekaiemu: {},
          };
      return tryLaunchSekaiemu({
        moduleId: "runtime-lab",
        manifest: previewManifest,
        romPath,
        corePath,
        trackerPackPath,
        layoutPreview,
        startFullscreen: Boolean(safe.fullscreen || safe.startFullscreen),
        trackerVariant: String(safe.trackerVariant || safe.trackerDisplayMode || "").trim(),
        serverAddress: String(safe.apHost || "").trim(),
        slot: String(safe.apSlot || "").trim(),
        password: String(safe.apPass || ""),
      });
    });

    secureIpcHandle("sekaiemu:stop", async (_event, pid) => {
      const safePid = Number(pid);
      if (!Number.isFinite(safePid) || safePid <= 0) return { ok: false, error: "invalid_pid" };
      const proc = nativeGameProcs.get(safePid);
      if (!proc) return { ok: false, error: "not_running" };
      await terminateChildProcess(proc, "sekaiemu", { graceMs: 900 });
      nativeGameProcs.delete(safePid);
      forgetSession(safePid);
      const activeBridge = sekaiemuChatBridges.get(safePid);
      activeBridge?.stop?.();
      sekaiemuChatBridges.delete(safePid);
      return { ok: true };
    });

    secureIpcHandle("sekaiemu:getSession", async (_event, pid) => {
      const safePid = Number(pid);
      const session = getSessionForPid(safePid);
      return {
        ok: Boolean(session && nativeGameProcs.has(safePid)),
        running: Boolean(session && nativeGameProcs.has(safePid)),
        session,
        error: session ? "" : "not_running",
      };
    });

    secureIpcHandle("sekaiemu:setInputState", async () => ({ ok: false, error: "input_ipc_not_available" }));
    secureIpcHandle("sekaiemu:setPaused", async () => ({ ok: false, error: "pause_ipc_not_available" }));
    secureIpcHandle("sekaiemu:reset", async () => ({ ok: false, error: "reset_ipc_not_available" }));

    secureIpcHandle("sekaiemu:memory:domains", async (_event, options) => {
      const safe = isPlainObject(options) ? options : {};
      const session = getSessionForPid(safe.pid);
      if (!session) return { ok: false, error: "not_running" };
      const res = await sendMemoryRequest(session.memorySocketPath, [{ type: "DOMAINS" }]);
      if (!res.ok) return res;
      const domains = Array.isArray(res.responses) ? res.responses.find((entry) => entry?.type === "DOMAINS_RESPONSE")?.value : null;
      return { ok: true, domains: Array.isArray(domains) ? domains : [], memorySocketPath: session.memorySocketPath };
    });

    secureIpcHandle("sekaiemu:memory:read", async (_event, options) => {
      const safe = isPlainObject(options) ? options : {};
      const session = getSessionForPid(safe.pid);
      if (!session) return { ok: false, error: "not_running" };
      const domain = String(safe.domain || "system_ram").trim();
      const address = Math.max(0, Number(safe.address || 0) || 0);
      const size = Math.max(1, Math.min(65536, Number(safe.size || 256) || 256));
      const res = await sendMemoryRequest(session.memorySocketPath, [{ type: "READ", domain, address, size }]);
      if (!res.ok) return res;
      const response = Array.isArray(res.responses) ? res.responses.find((entry) => entry?.type === "READ_RESPONSE" || entry?.type === "ERROR") : null;
      if (!response || response.type === "ERROR") return { ok: false, error: String(response?.value || "read_failed") };
      const data = Buffer.from(String(response.value || ""), "base64");
      return {
        ok: true,
        domain,
        address,
        size: data.length,
        data: Array.from(data.values()),
        hex: data.toString("hex").replace(/(.{2})/g, "$1 ").trim(),
      };
    });

    secureIpcHandle("sekaiemu:memory:dump", async (_event, options) => {
      const safe = isPlainObject(options) ? options : {};
      const session = getSessionForPid(safe.pid);
      if (!session) return { ok: false, error: "not_running" };
      const domain = String(safe.domain || "system_ram").trim();
      const domainRes = await sendMemoryRequest(session.memorySocketPath, [{ type: "DOMAINS" }]);
      if (!domainRes.ok) return domainRes;
      const domains = Array.isArray(domainRes.responses) ? domainRes.responses.find((entry) => entry?.type === "DOMAINS_RESPONSE")?.value : [];
      const descriptor = Array.isArray(domains) ? domains.find((entry) => entry?.name === domain) : null;
      const size = Math.max(1, Math.min(32 * 1024 * 1024, Number(safe.size || descriptor?.size || 0) || 0));
      if (!size) return { ok: false, error: "domain_size_unknown" };
      const readRes = await sendMemoryRequest(session.memorySocketPath, [{ type: "READ", domain, address: 0, size }], 5000);
      if (!readRes.ok) return readRes;
      const response = Array.isArray(readRes.responses) ? readRes.responses.find((entry) => entry?.type === "READ_RESPONSE" || entry?.type === "ERROR") : null;
      if (!response || response.type === "ERROR") return { ok: false, error: String(response?.value || "dump_failed") };
      const data = Buffer.from(String(response.value || ""), "base64");
      const dumpDir = path.join(app.getPath("userData"), "sekaiemu-lab", "dumps");
      ensureDir(dumpDir);
      const safeDomain = domain.replace(/[^a-z0-9_.-]+/gi, "_") || "memory";
      const outPath = path.join(dumpDir, `${safeDomain}-${new Date().toISOString().replace(/[:.]/g, "-")}.bin`);
      fs.writeFileSync(outPath, data);
      return { ok: true, path: outPath, domain, size: data.length };
    });
  };

  return {
    rememberSession,
    forgetSession,
    getSessionForPid,
    registerIpcHandlers,
  };
};

module.exports = { createSekaiemuLab };
