"use strict";

const createLinkedWorldRuntime = (deps = {}) => {
  const {
    app,
    fs,
    path,
    spawn,
    spawnSync,
    readline,
    processRef = process,
    dirname,
    secureIpcHandle,
    isPlainObject,
    normalizeGameId,
    normalizeIpcString,
    normalizeIpcPath,
    findFreeLocalPort,
    writeLogLine,
    writeLogJson,
    emitSessionEvent,
    nativeGameProcs,
    linkedWorldProcs,
  } = deps;
  const __dirname = dirname;
  const process = processRef;

  const getLinkedWorldInstallRoot = () => path.join(app.getPath("userData"), "linkedworlds");
  
  const getLinkedWorldRuntimeRoot = (worldId) => {
    const safeId = normalizeGameId(worldId);
    return path.join(app.getPath("home"), ".local", "share", "sekailink", "linkedworlds", safeId || "wind-waker");
  };
  
  const resolveBundledLinkedWorldArchive = (worldId, version = "") => {
    const safeId = normalizeGameId(worldId);
    const safeVersion = normalizeIpcString(version, 64) || "";
    if (!safeId) return "";
    const archiveName = safeVersion ? `${safeId}-${safeVersion}.linkedworld` : `${safeId}.linkedworld`;
    const envKey = `SEKAILINK_${safeId.replace(/[^a-z0-9]/gi, "_").toUpperCase()}_LINKEDWORLD`;
    const candidates = [
      normalizeIpcPath(process.env[envKey] || ""),
      normalizeIpcPath(process.env.SEKAILINK_LINKEDWORLD_ARCHIVE || ""),
      path.join(process.resourcesPath || "", "linkedworlds", archiveName),
      path.join(__dirname, "..", "..", "linkedworlds", archiveName),
      path.join(app.getPath("home"), "sekailink-emu-repo", "dist", "linkedworlds", archiveName),
      path.join(app.getPath("home"), "sekailink", "dist", "linkedworlds", archiveName),
    ].filter(Boolean);
    for (const candidate of candidates) {
      try {
        if (fs.existsSync(candidate) && fs.statSync(candidate).isFile()) return candidate;
      } catch (_err) {
        // keep looking
      }
    }
    return "";
  };
  
  const resolveBundledWindWakerLinkedWorldArchive = () => {
    const candidates = [
      normalizeIpcPath(process.env.SEKAILINK_WIND_WAKER_LINKEDWORLD || ""),
      resolveBundledLinkedWorldArchive("wind-waker", "0.1.0-dev"),
      path.join(process.resourcesPath || "", "linkedworlds", "wind-waker-0.1.0-dev.linkedworld"),
      path.join(__dirname, "..", "..", "linkedworlds", "wind-waker-0.1.0-dev.linkedworld"),
      path.join(app.getPath("home"), "sekailink-emu-repo", "dist", "linkedworlds", "wind-waker-0.1.0-dev.linkedworld"),
      path.join(app.getPath("home"), "sekailink", "dist", "linkedworlds", "wind-waker-0.1.0-dev.linkedworld"),
    ].filter(Boolean);
    for (const candidate of candidates) {
      try {
        if (fs.existsSync(candidate) && fs.statSync(candidate).isFile()) return candidate;
      } catch (_err) {
        // keep looking
      }
    }
    return "";
  };
  
  const readLinkedWorldManifest = (worldRoot) => {
    try {
      const manifestPath = path.join(worldRoot, "linkedworld.json");
      if (!fs.existsSync(manifestPath)) return null;
      return JSON.parse(fs.readFileSync(manifestPath, "utf8"));
    } catch (_err) {
      return null;
    }
  };
  
  const chmodExecutableIfPresent = (targetPath) => {
    try {
      if (!targetPath || !fs.existsSync(targetPath)) return;
      const stat = fs.statSync(targetPath);
      fs.chmodSync(targetPath, stat.mode | 0o111);
    } catch (_err) {
      // best-effort on non-POSIX targets
    }
  };
  
  const describeWindWakerLinkedWorld = () => {
    const archivePath = resolveBundledWindWakerLinkedWorldArchive();
    const root = path.join(getLinkedWorldInstallRoot(), "wind-waker");
    const manifest = readLinkedWorldManifest(root);
    const runtimeRoot = getLinkedWorldRuntimeRoot("wind-waker");
    const dolphinPath = path.join(root, "runtime", process.platform === "win32" ? "windows" : "linux", process.platform === "win32" ? "dolphin-emu-nogui.exe" : "dolphin-emu-nogui");
    const serverPath = path.join(root, "runtime", process.platform === "win32" ? "windows" : "linux", process.platform === "win32" ? "sekailink-wind-waker-ap-server.exe" : "sekailink-wind-waker-ap-server");
    const aptwwPath = path.join(root, "seeds", "Jade-WW.aptww");
    const trackerPath = path.join(root, "tracker", "ww-poptracker-1.3.0.zip");
    const randomizedIsoPath = path.join(runtimeRoot, "seeds", "Jade-WW.GZLE99.iso");
    const errors = [];
    if (!manifest) errors.push("manifest_missing");
    if (!fs.existsSync(dolphinPath)) errors.push("dolphin_missing");
    if (!fs.existsSync(serverPath)) errors.push("local_server_missing");
    if (!fs.existsSync(aptwwPath)) errors.push("seed_missing");
    if (!fs.existsSync(trackerPath)) errors.push("tracker_pack_missing");
    if (!fs.existsSync(randomizedIsoPath)) errors.push("randomized_iso_missing");
    return {
      id: "wind-waker",
      installed: Boolean(manifest),
      installPath: root,
      manifest: manifest || undefined,
      ready: errors.length === 0,
      errors,
      archiveAvailable: Boolean(archivePath),
      archivePath,
      runtimeRoot,
      paths: {
        dolphinPath,
        serverPath,
        aptwwPath,
        trackerPath,
        randomizedIsoPath,
        userDir: path.join(runtimeRoot, "dolphin-user"),
        logDir: path.join(runtimeRoot, "logs"),
      },
    };
  };
  
  const installWindWakerLinkedWorldTest = async () => {
    const archivePath = resolveBundledWindWakerLinkedWorldArchive();
    if (!archivePath) return { ok: false, error: "linkedworld_archive_missing" };
    const installRoot = getLinkedWorldInstallRoot();
    const targetRoot = path.join(installRoot, "wind-waker");
    fs.mkdirSync(installRoot, { recursive: true });
    fs.rmSync(targetRoot, { recursive: true, force: true });
    const res = spawnSync("tar", ["--zstd", "-xf", archivePath, "-C", installRoot], {
      encoding: "utf8",
      timeout: 120000,
    });
    if (res.status !== 0) {
      return {
        ok: false,
        error: "linkedworld_extract_failed",
        detail: String(res.stderr || res.stdout || `tar exited ${res.status}`),
      };
    }
    const world = describeWindWakerLinkedWorld();
    chmodExecutableIfPresent(world.paths?.dolphinPath);
    chmodExecutableIfPresent(world.paths?.serverPath);
    writeLogJson("linkedworld.install", {
      id: "wind-waker",
      archivePath,
      installPath: targetRoot,
      ready: world.ready,
      errors: world.errors,
    });
    return { ok: true, world };
  };
  
  const installLinkedWorld = async (options = {}) => {
    const safe = isPlainObject(options) ? options : {};
    const id = normalizeGameId(safe.id || safe.worldId || "");
    if (!id) return { ok: false, error: "invalid_linkedworld_id" };
    if (id !== "wind-waker") return { ok: false, error: "linkedworld_unsupported" };
    return installWindWakerLinkedWorldTest();
  };
  
  const validateLinkedWorld = async (options = {}) => {
    const safe = isPlainObject(options) ? options : {};
    const id = normalizeGameId(safe.id || safe.worldId || "");
    if (!id) return { ok: false, error: "invalid_linkedworld_id" };
    if (id !== "wind-waker") return { ok: false, error: "linkedworld_unsupported" };
    return { ok: true, world: describeWindWakerLinkedWorld() };
  };
  
  const launchLinkedWorld = async (options = {}) => {
    const safe = isPlainObject(options) ? options : {};
    const id = normalizeGameId(safe.id || safe.worldId || "wind-waker");
    const mode = normalizeIpcString(safe.mode || "solo", 32) || "solo";
    if (id !== "wind-waker") return { ok: false, error: "linkedworld_unsupported" };
    if (mode !== "solo" && mode !== "offline") return { ok: false, error: "linkedworld_mode_unsupported" };
    return launchWindWakerLinkedWorldSolo(safe);
  };
  
  const attachLinkedWorldProcessLog = (worldId, name, proc) => {
    const prefix = `linkedworld:${worldId}:${name}`;
    const attach = (stream, level) => {
      if (!stream) return;
      const rl = readline.createInterface({ input: stream });
      rl.on("line", (line) => {
        const text = String(line || "").trim();
        if (text) writeLogLine(level, prefix, text);
      });
      proc.once("close", () => rl.close());
    };
    attach(proc.stdout, "info");
    attach(proc.stderr, "warn");
  };
  
  const launchWindWakerLinkedWorldSolo = async (options = {}) => {
    let world = describeWindWakerLinkedWorld();
    if (!world.installed || options.reinstall === true) {
      const installed = await installWindWakerLinkedWorldTest();
      if (!installed.ok) return installed;
      world = describeWindWakerLinkedWorld();
    }
    if (!world.ready) {
      return { ok: false, error: "linkedworld_not_ready", detail: world.errors.join(","), world };
    }
    const port = Number.isFinite(Number(options.port)) && Number(options.port) > 0
      ? Math.round(Number(options.port))
      : await findFreeLocalPort(38281);
    fs.mkdirSync(world.paths.logDir, { recursive: true });
    fs.mkdirSync(world.paths.userDir, { recursive: true });
  
    const serverArgs = [
      `--port=${port}`,
      "--seconds=0",
      `--linkedworld-root=${world.installPath}`,
      `--aptww=${world.paths.aptwwPath}`,
      "--slot=Jade-WW",
    ];
    const serverProc = spawn(world.paths.serverPath, serverArgs, {
      cwd: world.installPath,
      stdio: ["ignore", "pipe", "pipe"],
      env: { ...process.env },
    });
    attachLinkedWorldProcessLog("wind-waker", "server", serverProc);
  
    await new Promise((resolve) => setTimeout(resolve, 550));
    if (serverProc.exitCode !== null) {
      return { ok: false, error: "linkedworld_server_exited", detail: `exit=${serverProc.exitCode}` };
    }
  
    const dolphinArgs = [
      "--platform=x11",
      `--user=${world.paths.userDir}`,
      `--sekailink-manifest=${path.join(world.installPath, "linkedworld.json")}`,
      "--sekailink-mode=offline",
      `--sekailink-log-dir=${world.paths.logDir}`,
      `--sekailink-tracker-pack=${world.paths.trackerPath}`,
      "--sekailink-slot=Jade-WW",
      "--sekailink-server=127.0.0.1",
      `--sekailink-port=${port}`,
      `--exec=${world.paths.randomizedIsoPath}`,
    ];
    const dolphinProc = spawn(world.paths.dolphinPath, dolphinArgs, {
      cwd: world.installPath,
      stdio: ["ignore", "pipe", "pipe"],
      env: { ...process.env },
    });
    attachLinkedWorldProcessLog("wind-waker", "dolphin", dolphinProc);
    linkedWorldProcs.set(dolphinProc.pid, { worldId: "wind-waker", game: dolphinProc, server: serverProc, port });
    nativeGameProcs.set(dolphinProc.pid, dolphinProc);
  
    emitSessionEvent({
      event: "status",
      status: "Wind Waker LinkedWorld launched.",
      moduleId: "wind-waker",
      gamePid: dolphinProc.pid,
      serverPid: serverProc.pid,
      port,
    });
    writeLogJson("linkedworld.launch", {
      id: "wind-waker",
      gamePid: dolphinProc.pid,
      serverPid: serverProc.pid,
      port,
      dolphinArgs,
      serverArgs,
    });
  
    dolphinProc.once("exit", (code, signal) => {
      writeLogJson("linkedworld.exit", { id: "wind-waker", gamePid: dolphinProc.pid, code, signal });
      linkedWorldProcs.delete(dolphinProc.pid);
      nativeGameProcs.delete(dolphinProc.pid);
      try {
        if (!serverProc.killed) serverProc.kill("SIGTERM");
      } catch (_err) {
        // ignore shutdown race
      }
    });
    serverProc.once("exit", (code, signal) => {
      writeLogJson("linkedworld.server.exit", { id: "wind-waker", serverPid: serverProc.pid, code, signal });
    });
  
    return { ok: true, gamePid: dolphinProc.pid, serverPid: serverProc.pid, port, world };
  };
  
  secureIpcHandle("linkedworld:list", async () => {
    return { ok: true, worlds: [describeWindWakerLinkedWorld()] };
  });
  
  secureIpcHandle("linkedworld:validate", async (_event, options) => {
    return validateLinkedWorld(options);
  });
  
  secureIpcHandle("linkedworld:install", async (_event, options) => {
    return installLinkedWorld(options);
  });
  
  secureIpcHandle("linkedworld:launch", async (_event, options) => {
    return launchLinkedWorld(options);
  });
  
  secureIpcHandle("linkedworld:installWindWakerTest", async () => {
    return installWindWakerLinkedWorldTest();
  });
  
  secureIpcHandle("linkedworld:launchSolo", async (_event, options) => {
    return launchLinkedWorld({ ...(isPlainObject(options) ? options : {}), mode: "solo" });
  });

  return {
    describeWindWakerLinkedWorld,
    installLinkedWorld,
    validateLinkedWorld,
    launchLinkedWorld,
  };
};

module.exports = { createLinkedWorldRuntime };
