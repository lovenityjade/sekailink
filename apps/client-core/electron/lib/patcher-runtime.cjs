"use strict";

const createPatcherRuntime = (deps = {}) => {
  const {
    app,
    fs,
    path,
    crypto,
    spawn,
    processRef = process,
    createPatchedRomCacheStore,
    ensurePythonRuntime,
    getPatcherWrapperPath,
    verifyPatcherPythonWorld,
    normalizeIpcString,
    normalizeIpcPath,
    isPlainObject,
    getModuleManifest,
    resolveConfiguredRomForModule,
    downloadToFile,
    normalizeSha256,
    ensureDir,
    withApPythonEnv,
    writeLogJson,
    nowIso,
  } = deps;
  const process = processRef;

  const runPatcher = async (options = {}) => {
    const python = await ensurePythonRuntime();
    const wrapperPath = getPatcherWrapperPath();
    if (!fs.existsSync(wrapperPath)) return { ok: false, error: "patcher_wrapper_missing" };
    const moduleId = normalizeIpcString(options?.moduleId, 200);
    const manifest = isPlainObject(options?.manifest)
      ? options.manifest
      : moduleId
        ? getModuleManifest(moduleId)
        : null;
    const patchWorldModule = normalizeIpcString(
      options?.worldModule || manifest?.patch_world_module || manifest?.patcher_world_module || "",
      300
    );
    const worldProbe = await verifyPatcherPythonWorld(python, patchWorldModule);
    if (!worldProbe.ok) return { ok: false, error: `python_world_probe_failed: ${worldProbe.error || "unknown"}` };
  
    const userDataDir = app.getPath("userData");
    const patchDir = path.join(userDataDir, "runtime", "patches");
    const romDir = path.join(userDataDir, "runtime", "roms");
    const configPath =
      options.configPath ||
      path.join(app.getPath("home"), ".sekailink", "config.json");
  
    let patchPath = options.patchPath;
    if (!patchPath && options.patchUrl) {
      ensureDir(patchDir);
      const urlObj = new URL(options.patchUrl);
      const fileName = path.basename(urlObj.pathname || "patch.ap");
      patchPath = path.join(patchDir, fileName);
      try {
        await downloadToFile(options.patchUrl, patchPath, {
          expectedSha256: normalizeSha256(options.patchSha256 || options.sha256),
        });
      } catch (err) {
        return { ok: false, error: String(err) };
      }
    }
    if (!patchPath) return { ok: false, error: "missing_patch" };
  
    const args = [wrapperPath, "--patch", patchPath];
    if (configPath) args.push("--config", configPath);
    if (patchWorldModule) args.push("--world-module", patchWorldModule);
    const romPath = normalizeIpcPath(options.romPath || options.rom || resolveConfiguredRomForModule(moduleId, manifest));
    if (romPath) args.push("--rom", romPath);
    args.push("--out-dir", options.outDir || romDir);
  
    return new Promise((resolve) => {
      const proc = spawn(python, args, {
        stdio: ["ignore", "pipe", "pipe"],
        env: withApPythonEnv(process.env)
      });
      let stdout = "";
      let stderr = "";
      proc.stdout.on("data", (chunk) => {
        stdout += String(chunk);
      });
      proc.stderr.on("data", (chunk) => {
        stderr += String(chunk);
      });
      proc.on("exit", () => {
        const line = stdout.trim().split("\n").filter(Boolean).pop();
        if (!line) {
          resolve({ ok: false, error: stderr || "no_output" });
          return;
        }
        try {
          const data = JSON.parse(line);
          resolve(data);
        } catch (err) {
          resolve({ ok: false, error: String(err) });
        }
      });
    });
  };
  
  const hashFile = (filePath, algo = "md5", options = {}) => {
    return new Promise((resolve, reject) => {
      const hash = crypto.createHash(algo);
      const stream = fs.createReadStream(filePath, {
        start: Number.isFinite(options?.start) ? Math.max(0, Number(options.start)) : undefined,
      });
      stream.on("error", reject);
      stream.on("data", (chunk) => hash.update(chunk));
      stream.on("end", () => resolve(hash.digest("hex")));
    });
  };
  
  
  const getDefaultPatcherConfigPath = () => {
    return path.join(app.getPath("home"), ".sekailink", "config.json");
  };
  
  const getPatchedRomCachePath = () => {
    return path.join(app.getPath("userData"), "runtime", "patched-rom-cache.json");
  };
  
  const patchedRomCache = createPatchedRomCacheStore({
    fs,
    path,
    crypto,
    ensureDir,
    nowIso,
    hashFile,
    cachePath: getPatchedRomCachePath(),
  });
  
  const resolvePatchedRomPlan = async (options = {}) => {
    const moduleId = normalizeIpcString(options?.moduleId, 200);
    const patchPath = normalizeIpcPath(options?.patchPath);
    if (!moduleId || !patchPath) {
      return { ok: false, error: "missing_args" };
    }
  
    const cached = await patchedRomCache.resolveByPatch({ moduleId, patchPath });
    if (cached.ok && cached.outputPath) {
      const plan = {
        ok: true,
        moduleId,
        patchPath,
        patchHash: cached.patchHash || "",
        action: "reuse",
        shouldReuse: true,
        cached: true,
        outputPath: cached.outputPath,
        cacheHit: cached.cacheHit || "exact",
      };
      writeLogJson("patch-plan", plan);
      return plan;
    }
  
    const plan = {
      ok: true,
      moduleId,
      patchPath,
      patchHash: cached.patchHash || "",
      action: "patch",
      shouldReuse: false,
      cached: false,
      cacheHit: cached.cacheHit || "",
    };
    writeLogJson("patch-plan", plan);
    return plan;
  };

  return {
    runPatcher,
    hashFile,
    getDefaultPatcherConfigPath,
    patchedRomCache,
    resolvePatchedRomPlan,
  };
};

module.exports = { createPatcherRuntime };
