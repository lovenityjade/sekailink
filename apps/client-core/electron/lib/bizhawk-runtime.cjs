"use strict";

const createBizHawkRuntime = ({
  app,
  fs,
  path,
  dirname,
  processRef = process,
  getBundledThirdPartyDir,
  getRuntimeToolsDir,
  getBundledRuntimeDir,
  getBundledApRootDir,
  getRuntimeModulePath,
  getModuleManifest,
  ensureMonoRuntime,
  ensureDir,
  isWritablePath,
  spawnMaybeGamescope,
  terminateChildProcess,
  triggerCoupledRuntimeTeardown,
  emitSessionEvent = () => {},
  writeLogLine = () => {},
  bizhawkProcs = new Map(),
}) => {
  const getBizHawkBaseDir = () => {
    const dirName = processRef.platform === "win32" ? "BizHawk-2.10-win-x64" : "BizHawk-2.10-linux-x64";
    return path.join(getBundledThirdPartyDir(), "emulators", dirName);
  };

  const getBizHawkInstalledDir = () => {
    const dirName = processRef.platform === "win32" ? "BizHawk-2.10-win-x64" : "BizHawk-2.10-linux-x64";
    return path.join(getRuntimeToolsDir(), "bizhawk", dirName);
  };

  const BIZHAWK_REQUIRED_ASSEMBLY_VERSION = "2.10.0.0";

  const readBizHawkAssemblyVersion = (baseDir) => {
    try {
      const dllPath = path.join(String(baseDir || ""), "dll", "BizHawk.Common.dll");
      if (!fs.existsSync(dllPath)) return null;
      const blob = fs.readFileSync(dllPath);
      const text = blob.toString("latin1");
      const matches = text.match(/\b2\.\d+\.\d+\.\d+\b/g) || [];
      if (!matches.length) return null;
      if (matches.includes(BIZHAWK_REQUIRED_ASSEMBLY_VERSION)) return BIZHAWK_REQUIRED_ASSEMBLY_VERSION;
      return String(matches[0]);
    } catch (_err) {
      return null;
    }
  };

  const verifyBizHawkVersion = (baseDir, sourceLabel = "runtime") => {
    const found = readBizHawkAssemblyVersion(baseDir);
    if (found === BIZHAWK_REQUIRED_ASSEMBLY_VERSION) return { ok: true, version: found };
    const detail = `expected ${BIZHAWK_REQUIRED_ASSEMBLY_VERSION}, found ${found || "unknown"} (${sourceLabel})`;
    writeLogLine("error", "bizhawk", `version mismatch: ${detail}`);
    return { ok: false, error: "bizhawk_version_mismatch", detail };
  };

  const stageBizHawkToDir = (sourceDir, destDir) => {
    try {
      if (!fs.existsSync(sourceDir)) {
        return { ok: false, error: "bizhawk_missing", detail: `source_missing: ${sourceDir}` };
      }
      fs.rmSync(destDir, { recursive: true, force: true });
      fs.mkdirSync(destDir, { recursive: true });
      fs.cpSync(sourceDir, destDir, { recursive: true });
      if (processRef.platform !== "win32") {
        try {
          fs.chmodSync(path.join(destDir, "EmuHawkMono.sh"), 0o755);
        } catch (_err) {
          // ignore
        }
      }
      return { ok: true, baseDir: destDir };
    } catch (err) {
      const detail = String(err || "");
      writeLogLine("error", "bizhawk", `staging failed: ${detail}`);
      return { ok: false, error: "bizhawk_stage_failed", detail };
    }
  };

  const ensureBizHawkInstalled = async () => {
    const dest = getBizHawkInstalledDir();
    const override = String(processRef.env.SEKAILINK_BIZHAWK || "").trim();
    if (override) {
      const overridePath = path.resolve(override);
      const overrideDir = path.dirname(overridePath);
      if (!fs.existsSync(overridePath)) {
        return { ok: false, error: "bizhawk_missing", detail: `override_missing: ${overridePath}` };
      }
      if (isWritablePath(overrideDir)) {
        const verify = verifyBizHawkVersion(overrideDir, "override");
        if (!verify.ok) return verify;
        return { ok: true, baseDir: overrideDir, emuPath: overridePath };
      }
      emitSessionEvent({ event: "status", status: "Preparing BizHawk runtime (read-only override detected)...", tool: "bizhawk" });
      writeLogLine("warn", "bizhawk", `override is read-only; staging runtime to writable dir: ${overrideDir} -> ${dest}`);
      const staged = stageBizHawkToDir(overrideDir, dest);
      if (!staged.ok) return staged;
      const verify = verifyBizHawkVersion(dest, "override-staged");
      if (!verify.ok) return verify;
      return { ok: true, baseDir: dest, emuPath: path.join(dest, path.basename(overridePath)) };
    }

    const bundled = getBizHawkBaseDir();
    const bundledVerify = verifyBizHawkVersion(bundled, "bundled");
    if (!bundledVerify.ok) return bundledVerify;
    if (!app.isPackaged) return { ok: true, baseDir: bundled };

    const marker = path.join(dest, processRef.platform === "win32" ? "EmuHawk.exe" : "EmuHawkMono.sh");
    const stampPath = path.join(dest, ".sekailink-bizhawk-stamp.json");
    const bundledMarker = path.join(bundled, processRef.platform === "win32" ? "EmuHawk.exe" : "EmuHawkMono.sh");
    const bundledMtime = fs.existsSync(bundledMarker) ? Number(fs.statSync(bundledMarker).mtimeMs || 0) : 0;
    const runtimeVersion = app.getVersion();
    if (fs.existsSync(marker) && fs.existsSync(stampPath)) {
      try {
        const stamp = JSON.parse(fs.readFileSync(stampPath, "utf8"));
        if (String(stamp?.version || "") === runtimeVersion && Number(stamp?.bundledMtime || 0) === bundledMtime) {
          const verify = verifyBizHawkVersion(dest, "staged");
          if (!verify.ok) return verify;
          return { ok: true, baseDir: dest };
        }
        writeLogLine("info", "bizhawk", "runtime stamp changed, refreshing staged BizHawk files");
      } catch (_err) {
        writeLogLine("warn", "bizhawk", "invalid BizHawk runtime stamp, restaging");
      }
    } else if (fs.existsSync(marker)) {
      writeLogLine("info", "bizhawk", "staged BizHawk exists without stamp, refreshing once");
    }

    emitSessionEvent({ event: "status", status: "Installing BizHawk runtime (one-time)...", tool: "bizhawk" });
    writeLogLine("info", "bizhawk", `staging runtime to writable dir: ${dest}`);
    const staged = stageBizHawkToDir(bundled, dest);
    if (!staged.ok) return staged;
    const verify = verifyBizHawkVersion(dest, "staged");
    if (!verify.ok) return verify;
    try {
      fs.writeFileSync(stampPath, JSON.stringify({ version: runtimeVersion, bundledMtime }, null, 2), "utf8");
    } catch (_err) {
      // non-fatal
    }
    return staged;
  };

  const ensureBizHawkConfig = (baseDir) => {
    try {
      const dir = String(baseDir || "").trim() || getBizHawkBaseDir();
      const configPath = path.join(dir, "config.ini");
      let config = {};
      if (fs.existsSync(configPath)) {
        try {
          config = JSON.parse(fs.readFileSync(configPath, "utf-8"));
        } catch (_err) {
          config = {};
        }
      }

      config.RunInBackground = true;
      config.AutosaveSaveRAM = true;
      config.FlushSaveRamFrames = 300;
      config.BackupSaveram = true;
      config.LastWrittenFrom = "2.10";
      if (!config.LastWrittenFromDetailed) config.LastWrittenFromDetailed = "2.10";
      config.PreferredCores = config.PreferredCores && typeof config.PreferredCores === "object" ? config.PreferredCores : {};
      config.PreferredCores.NES = "NesHawk";

      fs.writeFileSync(configPath, JSON.stringify(config, null, 2));
      return { ok: true };
    } catch (err) {
      return { ok: false, error: String(err) };
    }
  };

  const ensureBizHawkLuaCompat = (baseDir) => {
    try {
      const dir = String(baseDir || "").trim() || getBizHawkBaseDir();
      const luaDir = path.join(dir, "Lua");
      const apRoot = getBundledApRootDir();
      const apLuaDir = path.join(apRoot, "data", "lua");
      const requiredLuaFiles = [
        "lua_5_3_compat.lua",
        "base64.lua",
        "json.lua",
        "socket.lua",
      ];
      const missing = [];

      for (const file of requiredLuaFiles) {
        const destPath = path.join(luaDir, file);
        if (fs.existsSync(destPath)) continue;

        const candidates = [
          path.join(apLuaDir, file),
          getRuntimeModulePath("bizhawk_base", "lua", file),
          path.join(getBizHawkBaseDir(), "Lua", file),
        ];

        let copied = false;
        for (const src of candidates) {
          if (!fs.existsSync(src)) continue;
          ensureDir(luaDir);
          fs.copyFileSync(src, destPath);
          copied = true;
          break;
        }
        if (!copied) missing.push(file);
      }

      const rootX64Dir = path.join(dir, "x64");
      const luaX64Dir = path.join(luaDir, "x64");
      const srcX64Dir = path.join(apLuaDir, "x64");
      const neededNative =
        processRef.platform === "win32"
          ? ["socket-windows-5-4.dll", "socket-windows-5-1.dll"]
          : ["socket-linux-5-4.so", "socket-linux-5-1.so"];
      for (const file of neededNative) {
        const src = path.join(srcX64Dir, file);
        if (!fs.existsSync(src)) {
          missing.push(`x64/${file}`);
          continue;
        }
        for (const targetDir of [rootX64Dir, luaX64Dir]) {
          const destPath = path.join(targetDir, file);
          if (fs.existsSync(destPath)) continue;
          ensureDir(targetDir);
          fs.copyFileSync(src, destPath);
          try {
            fs.chmodSync(destPath, 0o755);
          } catch (_err) {
            // ignore chmod failures
          }
        }
      }

      if (missing.length) {
        return {
          ok: false,
          error: "lua_compat_missing",
          detail: `missing lua runtime files for ${dir}: ${missing.join(", ")}`,
        };
      }
      return { ok: true };
    } catch (err) {
      return { ok: false, error: "lua_compat_copy_failed", detail: String(err || "") };
    }
  };

  const stageBizHawkConnectorLua = (sourceLuaPath, baseDir, moduleId = "") => {
    try {
      const src = String(sourceLuaPath || "").trim();
      if (!src || !fs.existsSync(src)) {
        return { ok: false, error: "lua_not_found", detail: `source connector missing: ${src}` };
      }
      const dir = String(baseDir || "").trim() || getBizHawkBaseDir();
      const luaDir = path.join(dir, "Lua");
      ensureDir(luaDir);
      const safeId = String(moduleId || "module").replace(/[^a-zA-Z0-9_-]/g, "_") || "module";
      const stagedPath = path.join(luaDir, `sekailink_connector_${safeId}.lua`);
      fs.copyFileSync(src, stagedPath);
      return { ok: true, luaPath: stagedPath };
    } catch (err) {
      return { ok: false, error: "lua_stage_failed", detail: String(err || "") };
    }
  };

  const getBundledBizHawkLibsDir = () => {
    return path.join(getBundledRuntimeDir(), "_bundled_libs", "bizhawk");
  };

  const launchBizHawk = async (options = {}) => {
    const modulesBase = path.join(dirname, "..", "..", "runtime", "modules");
    const install = await ensureBizHawkInstalled();
    if (!install.ok) return { ok: false, error: install.error || "bizhawk_stage_failed", detail: install.detail || "" };

    const baseDir = String(options.baseDir || "").trim() || install.baseDir;
    const defaultEmuName = processRef.platform === "win32" ? "EmuHawk.exe" : "EmuHawkMono.sh";
    const emuPath = options.emuPath || install.emuPath || path.join(baseDir, defaultEmuName);
    const romPath = options.romPath;
    let luaPath = options.luaPath;
    if (!luaPath && options.moduleId) {
      const manifest = getModuleManifest(options.moduleId);
      const rel = options.luaRelPath || manifest?.lua_connector || path.join("lua", "connector.lua");
      luaPath = path.join(modulesBase, options.moduleId, rel);
    }

    if (!romPath) return { ok: false, error: "missing_rom" };
    if (!luaPath) return { ok: false, error: "missing_lua" };
    if (!fs.existsSync(luaPath)) return { ok: false, error: "lua_not_found" };

    if (!fs.existsSync(emuPath)) return { ok: false, error: "bizhawk_missing" };
    let monoBin = "";
    if (processRef.platform !== "win32") {
      try {
        monoBin = await ensureMonoRuntime();
      } catch (err) {
        const detail = String(err || "");
        writeLogLine("error", "bizhawk", `mono ensure failed: ${detail}`);
        return { ok: false, error: "mono_missing", detail };
      }
      try {
        fs.chmodSync(emuPath, 0o755);
      } catch (_err) {
        // ignore chmod failures
      }
    }

    const configResult = ensureBizHawkConfig(baseDir);
    if (!configResult.ok) return configResult;
    const luaCompatResult = ensureBizHawkLuaCompat(baseDir);
    if (!luaCompatResult.ok) return luaCompatResult;
    const stagedLuaResult = stageBizHawkConnectorLua(luaPath, baseDir, options.moduleId || "manual");
    if (!stagedLuaResult.ok) return stagedLuaResult;
    luaPath = stagedLuaResult.luaPath;

    const args = [`--lua=${luaPath}`, romPath];
    const spawnEnv = { ...processRef.env };
    try {
      const logsDir = path.join(app.getPath("userData"), "logs");
      ensureDir(logsDir);
      spawnEnv.SEKAILINK_LUA_LOG_PATH = path.join(logsDir, "bizhawk-lua-connector.log");
    } catch (_err) {
      // best-effort only
    }

    if (processRef.platform !== "win32") {
      try {
        const monoHome = path.resolve(path.dirname(monoBin), "..");
        const portableCfgDir = path.join(monoHome, "etc");
        if (fs.existsSync(path.join(portableCfgDir, "mono"))) {
          spawnEnv.MONO_CFG_DIR = portableCfgDir;
        }
        const gacA = path.join(monoHome, "lib", "mono", "gac");
        const gacB = path.join(monoHome, "lib64", "mono", "gac");
        if (fs.existsSync(gacA) || fs.existsSync(gacB)) {
          spawnEnv.MONO_GAC_PREFIX = monoHome;
        }
        const monoLibDirs = [path.join(monoHome, "lib"), path.join(monoHome, "lib64")].filter((d) => fs.existsSync(d));
        if (monoLibDirs.length) {
          const prior = spawnEnv.LD_LIBRARY_PATH || "";
          spawnEnv.LD_LIBRARY_PATH = prior ? `${monoLibDirs.join(":")}:${prior}` : monoLibDirs.join(":");
        }
      } catch (_err) {
        // best-effort
      }

      if (processRef.platform === "linux") {
        const fallbackLibDirs = ["/usr/lib64", "/usr/lib"].filter((d) => fs.existsSync(d));
        if (fallbackLibDirs.length) {
          const prior = spawnEnv.LD_LIBRARY_PATH || "";
          const missing = fallbackLibDirs.filter((d) => !prior.split(":").includes(d));
          if (missing.length) {
            spawnEnv.LD_LIBRARY_PATH = prior ? `${prior}:${missing.join(":")}` : missing.join(":");
          }
        }
      }

      const extraLibDir = getBundledBizHawkLibsDir();
      if (fs.existsSync(extraLibDir)) {
        const prior = spawnEnv.LD_LIBRARY_PATH || "";
        spawnEnv.LD_LIBRARY_PATH = prior ? `${extraLibDir}:${prior}` : extraLibDir;
      }
      spawnEnv.SEKAILINK_MONO = monoBin;
    }

    const wrapped = spawnMaybeGamescope(emuPath, args, { stdio: "ignore", env: spawnEnv });
    if (!wrapped.ok) return wrapped;
    const proc = wrapped.proc;
    bizhawkProcs.set(proc.pid, proc);
    proc.on("exit", (code, signal) => {
      const exitedPid = proc.pid;
      bizhawkProcs.delete(exitedPid);
      triggerCoupledRuntimeTeardown("bizhawk", exitedPid, { code, signal });
    });
    return { ok: true, pid: wrapped.proc.pid, gamescope: Boolean(wrapped.wrapped) };
  };

  const stopBizHawk = async (pid) => {
    if (pid && bizhawkProcs.has(pid)) {
      const proc = bizhawkProcs.get(pid);
      await terminateChildProcess(proc, "bizhawk", { graceMs: 1100 });
      bizhawkProcs.delete(pid);
      return { ok: true };
    }
    return { ok: false, error: "not_running" };
  };

  return {
    getBizHawkBaseDir,
    getBizHawkInstalledDir,
    ensureBizHawkInstalled,
    ensureBizHawkConfig,
    ensureBizHawkLuaCompat,
    stageBizHawkConnectorLua,
    launchBizHawk,
    stopBizHawk,
  };
};

module.exports = { createBizHawkRuntime };
