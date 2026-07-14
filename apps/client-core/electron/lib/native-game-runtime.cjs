"use strict";

const createNativeGameRuntime = (deps = {}) => {
  const {
    app,
    fs,
    path,
    processRef = process,
    spawn,
    ensureDir,
    readConfig,
    getModuleManifest,
    resolveGithubRepo,
    which,
    pcPackageRuntime,
    writeLogLine = () => {},
    isPlainObject,
    normalizeIpcString,
    getRuntimeToolsDir,
    getBundledThirdPartyDir,
    ensureGithubReleaseZipInstalled,
    backupAndReplaceFile,
    findPathByBasename,
    spawnMaybeGamescope,
    nativeGameProcs,
  } = deps;
  const process = processRef;

  const getFactorioModsDir = () => {
    const cfg = readConfig();
    const overrides = cfg.games && typeof cfg.games === "object" ? cfg.games : {};
    const factorio = overrides.factorio && typeof overrides.factorio === "object" ? overrides.factorio : {};
    if (typeof factorio.mods_dir === "string" && factorio.mods_dir.trim()) {
      return factorio.mods_dir.trim();
    }
  
    // Default per-platform locations.
    if (process.platform === "win32") {
      return path.join(app.getPath("appData"), "Factorio", "mods");
    }
    // Linux/macOS default: ~/.factorio/mods
    return path.join(app.getPath("home"), ".factorio", "mods");
  };
  
  const tryLaunchFactorio = () => {
    const cfg = readConfig();
    const overrides = cfg.games && typeof cfg.games === "object" ? cfg.games : {};
    const factorio = overrides.factorio && typeof overrides.factorio === "object" ? overrides.factorio : {};
  
    const exeOverride = typeof factorio.exe_path === "string" ? factorio.exe_path.trim() : "";
    const exePath = exeOverride || which("factorio") || "";
    if (exePath && fs.existsSync(exePath)) {
      try {
        const wrapped = spawnMaybeGamescope(exePath, [], { stdio: "ignore" });
        if (!wrapped.ok) return { ok: false, error: wrapped.error || "spawn_failed" };
        nativeGameProcs.set(wrapped.proc.pid, wrapped.proc);
        wrapped.proc.on("exit", () => nativeGameProcs.delete(wrapped.proc.pid));
        return { ok: true, pid: wrapped.proc.pid, method: "exe" };
      } catch (err) {
        return { ok: false, error: String(err) };
      }
    }
  
    const steam = which("steam");
    if (steam) {
      try {
        const proc = spawn(steam, ["-applaunch", "427520"], { stdio: "ignore" });
        nativeGameProcs.set(proc.pid, proc);
        proc.on("exit", () => nativeGameProcs.delete(proc.pid));
        return { ok: true, pid: proc.pid, method: "steam" };
      } catch (err) {
        return { ok: false, error: String(err) };
      }
    }
  
    return { ok: false, error: "factorio_not_found" };
  };
  
  const getGzDoomSettings = () => {
    const cfg = readConfig();
    const games = cfg.games && typeof cfg.games === "object" ? cfg.games : {};
    const gzdoom = games.gzdoom && typeof games.gzdoom === "object" ? games.gzdoom : {};
    return {
      exe_path: typeof gzdoom.exe_path === "string" ? gzdoom.exe_path.trim() : "",
      iwad_path: typeof gzdoom.iwad_path === "string" ? gzdoom.iwad_path.trim() : "",
      gzap_pk3_path: typeof gzdoom.gzap_pk3_path === "string" ? gzdoom.gzap_pk3_path.trim() : "",
      args: Array.isArray(gzdoom.args) ? gzdoom.args.map((v) => String(v)) : [],
    };
  };
  
  const getSm64ExSettings = () => {
    const cfg = readConfig();
    const games = cfg.games && typeof cfg.games === "object" ? cfg.games : {};
    const sm64ex = games.sm64ex && typeof games.sm64ex === "object" ? games.sm64ex : {};
    return {
      // Path to the compiled game binary (recommended).
      exe_path: typeof sm64ex.exe_path === "string" ? sm64ex.exe_path.trim() : "",
      // Optional folder to search for a compiled build (fallback).
      root_dir: typeof sm64ex.root_dir === "string" ? sm64ex.root_dir.trim() : "",
      // Extra args for the binary (one per string).
      args: Array.isArray(sm64ex.args) ? sm64ex.args.map((v) => String(v)) : [],
    };
  };
  
  const getSohSettings = () => {
    const cfg = readConfig();
    const games = cfg.games && typeof cfg.games === "object" ? cfg.games : {};
    const soh = games.oot_soh && typeof games.oot_soh === "object" ? games.oot_soh : {};
    return {
      // Path to the Ship of Harkinian executable (recommended).
      exe_path: typeof soh.exe_path === "string" ? soh.exe_path.trim() : "",
      // Optional folder to search for a build/download (fallback).
      root_dir: typeof soh.root_dir === "string" ? soh.root_dir.trim() : "",
      // Auto-download latest AP-integrated release if executable is missing.
      auto_install: typeof soh.auto_install === "boolean" ? soh.auto_install : true,
      // Extra args for the binary (one per string).
      args: Array.isArray(soh.args) ? soh.args.map((v) => String(v)) : [],
    };
  };
  
  const findSohExecutableInDir = (rootDir) => {
    if (!rootDir || !fs.existsSync(rootDir)) return "";
    const exactCandidates = [
      "soh",
      "soh.exe",
      "soh.appimage",
      "ship.of.harkinian",
      "ship.of.harkinian.exe",
      "ship.of.harkinian.appimage",
      "ship of harkinian",
      "ship of harkinian.exe",
      "ship of harkinian.appimage",
    ];
    for (const candidate of exactCandidates) {
      const hit = findPathByBasename(rootDir, candidate, 8);
      if (hit) return hit;
    }
  
    const queue = [{ dir: rootDir, depth: 0 }];
    while (queue.length) {
      const cur = queue.shift();
      if (!cur) break;
      if (cur.depth > 8) continue;
      let entries = [];
      try {
        entries = fs.readdirSync(cur.dir, { withFileTypes: true });
      } catch (_err) {
        continue;
      }
      for (const entry of entries) {
        const full = path.join(cur.dir, entry.name);
        if (entry.isDirectory()) {
          queue.push({ dir: full, depth: cur.depth + 1 });
          continue;
        }
        if (!entry.isFile()) continue;
        const lower = entry.name.toLowerCase();
        const maybeExec =
          lower.endsWith(".appimage") ||
          lower.endsWith(".exe") ||
          (!lower.includes(".") && lower.length > 0);
        if (!maybeExec) continue;
        if (lower.includes("soh") || lower.includes("harkinian")) {
          return full;
        }
      }
    }
    return "";
  };
  
  const tryLaunchSoh = async (options = {}) => {
    const settings = getSohSettings();
    const manifest = getModuleManifest("oot_soh") || {};
    const launcherRepo =
      resolveGithubRepo(String(manifest.launcher_repo || "").trim()) || "HarbourMasters/Archipelago-SoH";
    const launcherAssetRegex =
      String(manifest.launcher_asset_regex || "").trim() ||
      "Ship\\.of\\.Harkinian\\.Archipelago\\..*\\.linux\\.zip$";
    const managedInstall = pcPackageRuntime?.resolveInstalled?.("ship-of-harkinian-sekailink") || null;
    const exeOverride = managedInstall?.executable || settings.exe_path;
    let rootDir = options.rootDir || settings.root_dir;
    let exePath = "";
  
    if (exeOverride && fs.existsSync(exeOverride)) {
      exePath = exeOverride;
    } else {
      if (rootDir && fs.existsSync(rootDir)) {
        exePath = findSohExecutableInDir(rootDir);
      }
      if (!exePath && settings.auto_install && !managedInstall) {
        try {
          rootDir = await ensureGithubReleaseZipInstalled(
            "oot_soh",
            launcherRepo,
            launcherAssetRegex
          );
          exePath = findSohExecutableInDir(rootDir);
        } catch (err) {
          return { ok: false, error: "soh_install_failed", detail: String(err || "") };
        }
      }
    }
  
    if (!exePath || !fs.existsSync(exePath)) {
      return { ok: false, error: "soh_not_found" };
    }
  
    try {
      fs.chmodSync(exePath, 0o755);
    } catch (_err) {
      // ignore chmod failures
    }
  
    try {
      const args = [...(settings.args || [])];
      let launchManifestPath = "";
      if (managedInstall) {
        const server = String(options.serverAddress || "").trim();
        const slot = String(options.slot || "").trim();
        if (!server || !slot) return { ok: false, error: "soh_connection_missing" };
        const launchDir = path.join(app.getPath("userData"), "pc-packages", "launch");
        ensureDir(launchDir);
        launchManifestPath = path.join(launchDir, `soh-${processRef.pid}-${Date.now()}.json`);
        fs.writeFileSync(launchManifestPath, `${JSON.stringify({
          schema: "sekailink.pc-launch/v1",
          gameId: "ship-of-harkinian",
          sessionId: String(options.sessionId || ""),
          connection: { server, slot, password: String(options.password || ""), autoConnect: true },
          paths: { data: path.dirname(exePath), logs: launchDir },
        }, null, 2)}\n`, { mode: 0o600 });
        args.push("--sekailink-launch", launchManifestPath);
      }
      const wrapped = spawnMaybeGamescope(exePath, args, { stdio: "ignore", cwd: path.dirname(exePath) });
      if (!wrapped.ok) return { ok: false, error: wrapped.error || "spawn_failed" };
      nativeGameProcs.set(wrapped.proc.pid, wrapped.proc);
      wrapped.proc.on("exit", () => {
        nativeGameProcs.delete(wrapped.proc.pid);
        if (launchManifestPath) fs.rmSync(launchManifestPath, { force: true });
      });
      if (launchManifestPath) {
        setTimeout(() => {
          try { fs.rmSync(launchManifestPath, { force: true }); } catch (error) {
            writeLogLine("warn", "soh", `launch manifest cleanup failed: ${String(error?.message || error)}`);
          }
        }, 30000).unref?.();
      }
      return { ok: true, pid: wrapped.proc.pid, method: "exe", exePath };
    } catch (err) {
      return { ok: false, error: "soh_launch_failed", detail: String(err || "") };
    }
  };

  const tryLaunch2Ship = async (options = {}) => {
    const managedInstall = pcPackageRuntime?.resolveInstalled?.("2ship2harkinian-sekailink") || null;
    const exePath = managedInstall?.executable || "";
    if (!exePath || !fs.existsSync(exePath)) {
      return { ok: false, error: "2ship_not_installed" };
    }

    const server = String(options.serverAddress || "").trim();
    const slot = String(options.slot || "").trim();
    if (!server || !slot) return { ok: false, error: "2ship_connection_missing" };

    try {
      fs.chmodSync(exePath, 0o755);
    } catch (_err) {
      // Ignore chmod failures on filesystems that do not expose Unix modes.
    }

    let launchManifestPath = "";
    try {
      const launchDir = path.join(app.getPath("userData"), "pc-packages", "launch");
      ensureDir(launchDir);
      launchManifestPath = path.join(launchDir, `2ship-${processRef.pid}-${Date.now()}.json`);
      fs.writeFileSync(launchManifestPath, `${JSON.stringify({
        schema: "sekailink.pc-launch/v1",
        gameId: "2ship2harkinian",
        sessionId: String(options.sessionId || ""),
        connection: {
          server,
          slot,
          password: String(options.password || ""),
          autoConnect: true,
        },
        paths: { data: path.dirname(exePath), logs: launchDir },
      }, null, 2)}\n`, { mode: 0o600 });

      const wrapped = spawnMaybeGamescope(exePath, ["--sekailink-launch", launchManifestPath], {
        stdio: "ignore",
        cwd: path.dirname(exePath),
      });
      if (!wrapped.ok) return { ok: false, error: wrapped.error || "spawn_failed" };
      nativeGameProcs.set(wrapped.proc.pid, wrapped.proc);
      wrapped.proc.on("exit", () => {
        nativeGameProcs.delete(wrapped.proc.pid);
        if (launchManifestPath) fs.rmSync(launchManifestPath, { force: true });
      });
      setTimeout(() => {
        try { fs.rmSync(launchManifestPath, { force: true }); } catch (error) {
          writeLogLine("warn", "2ship", `launch manifest cleanup failed: ${String(error?.message || error)}`);
        }
      }, 30000).unref?.();
      return { ok: true, pid: wrapped.proc.pid, method: "pc-package", exePath };
    } catch (err) {
      if (launchManifestPath) fs.rmSync(launchManifestPath, { force: true });
      return { ok: false, error: "2ship_launch_failed", detail: String(err || "") };
    }
  };
  
  const tryLaunchSm64Ex = (options = {}) => {
    const settings = getSm64ExSettings();
    const exeOverride = settings.exe_path;
    const rootDir = options.rootDir || settings.root_dir;
    const managedInstall = pcPackageRuntime?.resolveInstalled?.("sm64ex-sekailink") || null;
  
    let exePath = "";
    if (exeOverride && fs.existsSync(exeOverride)) {
      exePath = exeOverride;
    } else if (managedInstall?.executable && fs.existsSync(managedInstall.executable)) {
      exePath = managedInstall.executable;
    } else if (rootDir && fs.existsSync(rootDir)) {
      // Common Linux build outputs:
      // - sm64ex/build/us_pc/sm64.us.f3dex2e
      // - sm64ex/build/jp_pc/sm64.jp.f3dex2e
      exePath =
        findPathByBasename(rootDir, "sm64.us.f3dex2e", 8) ||
        findPathByBasename(rootDir, "sm64.us.f3dex2e.exe", 8) ||
        findPathByBasename(rootDir, "sm64.jp.f3dex2e", 8) ||
        findPathByBasename(rootDir, "sm64.jp.f3dex2e.exe", 8) ||
        "";
    }
  
    if (!exePath || !fs.existsSync(exePath)) {
      return { ok: false, error: "sm64ex_not_found" };
    }
  
    const fileArg = options.filePath;
    if (!fileArg || !fs.existsSync(fileArg)) {
      return { ok: false, error: "missing_slot_file" };
    }
  
    const savePath = path.join(app.getPath("userData"), "games", "sm64ex");
    ensureDir(savePath);
    const args = ["--savepath", savePath, "--sm64ap_file", fileArg, ...settings.args];
    try {
      const wrapped = spawnMaybeGamescope(exePath, args, { stdio: "ignore" });
      if (!wrapped.ok) return { ok: false, error: wrapped.error || "spawn_failed" };
      nativeGameProcs.set(wrapped.proc.pid, wrapped.proc);
      wrapped.proc.on("exit", () => nativeGameProcs.delete(wrapped.proc.pid));
      return { ok: true, pid: wrapped.proc.pid, method: managedInstall?.executable === exePath ? "pc-package" : "exe" };
    } catch (err) {
      return { ok: false, error: String(err) };
    }
  };
  
  const tryLaunchGzDoom = (options = {}) => {
    const settings = getGzDoomSettings();
    const exeOverride = settings.exe_path;
    const exePath = exeOverride || which("gzdoom") || "";
    if (!exePath || !fs.existsSync(exePath)) {
      return { ok: false, error: "gzdoom_not_found" };
    }
  
    const iwad = options.iwadPath || settings.iwad_path;
    if (!iwad || !fs.existsSync(iwad)) {
      return { ok: false, error: "iwad_missing" };
    }
  
    const gzap = settings.gzap_pk3_path;
    if (!gzap || !fs.existsSync(gzap)) {
      return { ok: false, error: "gzap_pk3_missing" };
    }
  
    const fileArg = options.filePath;
    if (!fileArg || !fs.existsSync(fileArg)) {
      return { ok: false, error: "missing_mod_file" };
    }
  
    // Load order matters: gzArchipelago first, then the generated seed pk3.
    const args = ["-iwad", iwad, "-file", gzap, fileArg, ...settings.args];
    try {
      const wrapped = spawnMaybeGamescope(exePath, args, { stdio: "ignore" });
      if (!wrapped.ok) return { ok: false, error: wrapped.error || "spawn_failed" };
      nativeGameProcs.set(wrapped.proc.pid, wrapped.proc);
      wrapped.proc.on("exit", () => nativeGameProcs.delete(wrapped.proc.pid));
      return { ok: true, pid: wrapped.proc.pid, method: "exe" };
    } catch (err) {
      return { ok: false, error: String(err) };
    }
  };
  
  const tryHandleDownloadedArtifact = async (ctx = {}) => {
    const apGameName = String(ctx.apGameName || "").trim().toLowerCase();
    const downloadedPath = String(ctx.downloadedPath || "").trim();
    const ext = String(ctx.ext || "").trim().toLowerCase();
  
    if (!downloadedPath || !fs.existsSync(downloadedPath)) return { ok: false, error: "missing_download" };
  
    // Super Mario 64 EX (PC port) uses a slot file (.apsm64ex) and the game binary consumes it via --sm64ap_file.
    // Note: the world currently only generates output for single-player seeds.
    if ((apGameName === "super mario 64" || ext === ".apsm64ex") && ext === ".apsm64ex") {
      const launch = tryLaunchSm64Ex({ filePath: downloadedPath });
      if (launch.ok) {
        emitSessionEvent({ event: "status", status: "Launching SM64EX...", pid: launch.pid, method: launch.method });
        return { ok: true, handled: true, installedPath: downloadedPath, gamePid: launch.pid, launchMethod: launch.method };
      }
      return {
        ok: true,
        handled: true,
        installedPath: downloadedPath,
        gamePid: null,
        error: launch.error,
        note:
          "SM64EX build not found. Compile via SM64AP-Launcher or a manual sm64ex build, then set Settings -> Games (Automation) -> SM64EX executable path.",
      };
    }
  
    // Factorio slot files are served as a mod zip.
    if (apGameName.startsWith("factorio") && ext === ".zip") {
      const modsDir = getFactorioModsDir();
      try {
        ensureDir(modsDir);
      } catch (err) {
        return { ok: false, error: "mods_dir_create_failed", detail: String(err), modsDir };
      }
  
      const destPath = path.join(modsDir, path.basename(downloadedPath));
      try {
        backupAndReplaceFile(downloadedPath, destPath);
      } catch (err) {
        return { ok: false, error: "mod_install_failed", detail: String(err), destPath };
      }
  
      emitSessionEvent({
        event: "status",
        status: "Installed Factorio mod.",
        modsDir,
        installedPath: destPath,
      });
  
      const launch = tryLaunchFactorio();
      if (launch.ok) {
        emitSessionEvent({ event: "status", status: "Launching Factorio...", pid: launch.pid, method: launch.method });
        return { ok: true, handled: true, installedPath: destPath, gamePid: launch.pid, launchMethod: launch.method };
      }
  
      // Still a partial automation win: mod installed.
      return { ok: true, handled: true, installedPath: destPath, gamePid: null, error: launch.error };
    }
  
    // gzDoom slot files are served as a .pk3 to load via -file, but requires an IWAD.
    if (apGameName === "gzdoom" && ext === ".pk3") {
      const launch = tryLaunchGzDoom({ filePath: downloadedPath });
      if (launch.ok) {
        emitSessionEvent({ event: "status", status: "Launching gzDoom...", pid: launch.pid, method: launch.method });
        return { ok: true, handled: true, installedPath: downloadedPath, gamePid: launch.pid, launchMethod: launch.method };
      }
      if (launch.error === "iwad_missing") {
        return {
          ok: true,
          handled: true,
          installedPath: downloadedPath,
          gamePid: null,
          error: "iwad_missing",
          note: "Missing IWAD. Set gzDoom IWAD path in Settings -> Games (Automation).",
        };
      }
      if (launch.error === "gzap_pk3_missing") {
        return {
          ok: true,
          handled: true,
          installedPath: downloadedPath,
          gamePid: null,
          error: "gzap_pk3_missing",
          note: "Missing gzArchipelago.pk3. Set it in Settings -> Games (Automation).",
        };
      }
      return { ok: true, handled: true, installedPath: downloadedPath, gamePid: null, error: launch.error };
    }
  
    // Dark Souls III slot file is a JSON blob used by the external DS3 randomizer package.
    // We can at least install the package and stage the JSON file for the user.
    if (apGameName === "dark souls iii" && ext === ".json") {
      emitSessionEvent({ event: "status", status: "Installing DS3 randomizer package...", apGameName });
      let rootDir = "";
      try {
        rootDir = await ensureGithubReleaseZipInstalled(
          "ds3",
          "nex3/Dark-Souls-III-Archipelago-client",
          "DS3\\.Archipelago.*\\.zip$"
        );
      } catch (err) {
        return { ok: true, handled: true, installedPath: downloadedPath, gamePid: null, error: "ds3_install_failed", note: String(err) };
      }
  
      const slotDir = path.join(rootDir, "slot_files");
      try {
        ensureDir(slotDir);
        const destPath = path.join(slotDir, path.basename(downloadedPath));
        backupAndReplaceFile(downloadedPath, destPath);
  
        const randomizerExe = findPathByBasename(rootDir, "ds3randomizer.exe", 7);
        const launcherBat = findPathByBasename(rootDir, "launchmod_darksouls3.bat", 7);
        const noteParts = [];
        noteParts.push("DS3 package installed; slot file staged.");
        if (randomizerExe) noteParts.push("Run DS3Randomizer.exe for one-time setup.");
        if (launcherBat) noteParts.push("Then run launchmod_darksouls3.bat to start the game.");
  
        return { ok: true, handled: true, installedPath: destPath, gamePid: null, note: noteParts.join(" ") };
      } catch (err) {
        return { ok: true, handled: true, installedPath: downloadedPath, gamePid: null, error: "ds3_stage_failed", note: String(err) };
      }
    }
  
    return { ok: false, error: "no_handler" };
  };

  return {
    getFactorioModsDir,
    tryLaunchFactorio,
    getGzDoomSettings,
    getSm64ExSettings,
    getSohSettings,
    findSohExecutableInDir,
    tryLaunchSoh,
    tryLaunch2Ship,
    tryLaunchSm64Ex,
    tryLaunchGzDoom,
    tryHandleDownloadedArtifact,
  };
};

module.exports = { createNativeGameRuntime };
