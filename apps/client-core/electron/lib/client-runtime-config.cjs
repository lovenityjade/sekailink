"use strict";

const createClientRuntimeConfig = (deps = {}) => {
  const {
    app,
    fs,
    os,
    path,
    processRef = globalThis.process,
    getRuntimeOverlayRoot,
    getBundledRuntimeDir,
    normalizeGameId,
  } = deps;
  const process = processRef;

  const getOverlayRuntimeDir = () => {
    return path.join(getRuntimeOverlayRoot(), "runtime");
  };
  
  const getRuntimeFilePath = (relativePath) => {
    const rel = String(relativePath || "").replace(/\\/g, "/").replace(/^\/+/, "");
    const overlayPath = path.join(getOverlayRuntimeDir(), rel);
    if (fs.existsSync(overlayPath)) return overlayPath;
    return path.join(getBundledRuntimeDir(), rel);
  };
  
  const getPythonRuntimeWheelhouseTag = () => {
    if (process.platform === "win32" && process.arch === "x64") return "win-amd64-cp312";
    return "";
  };
  
  const getPythonRuntimeWheelhouseDir = () => {
    const tag = getPythonRuntimeWheelhouseTag();
    if (!tag) return "";
    return getRuntimeFilePath(path.join("tools", "python", "wheelhouse", tag));
  };
  
  const getPythonPipInstallArgs = (specs = []) => {
    const normalizedSpecs = Array.isArray(specs)
      ? specs.map((spec) => String(spec || "").trim()).filter(Boolean)
      : [];
    const wheelhouseDir = getPythonRuntimeWheelhouseDir();
    if (wheelhouseDir && fs.existsSync(wheelhouseDir)) {
      return ["-m", "pip", "install", "--no-index", "--find-links", wheelhouseDir, ...normalizedSpecs];
    }
    if (process.platform === "win32" && app.isPackaged) {
      throw new Error(`python_wheelhouse_missing:${wheelhouseDir || "win-amd64-cp312"}`);
    }
    return ["-m", "pip", "install", ...normalizedSpecs];
  };
  
  const getRuntimeModulesDirs = () => {
    const dirs = [];
    const overlayModulesDir = path.join(getOverlayRuntimeDir(), "modules");
    const bundledModulesDir = path.join(getBundledRuntimeDir(), "modules");
    if (fs.existsSync(overlayModulesDir)) dirs.push(overlayModulesDir);
    if (fs.existsSync(bundledModulesDir)) dirs.push(bundledModulesDir);
    if (!dirs.length) dirs.push(overlayModulesDir, bundledModulesDir);
    return dirs;
  };
  
  const getRuntimeModuleDir = (moduleId) => {
    const id = normalizeGameId(moduleId);
    if (!id) return "";
    for (const dir of getRuntimeModulesDirs()) {
      const candidate = path.join(dir, id);
      if (fs.existsSync(candidate)) return candidate;
    }
    for (const dir of getRuntimeModulesDirs()) {
      if (!fs.existsSync(dir)) continue;
      const entries = fs.readdirSync(dir, { withFileTypes: true });
      for (const entry of entries) {
        if (!entry.isDirectory()) continue;
        const manifestPath = path.join(dir, entry.name, "manifest.json");
        if (!fs.existsSync(manifestPath)) continue;
        try {
          const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf-8"));
          const aliases = [
            entry.name,
            manifest.game_id,
            manifest.game_key,
            manifest.ap_world,
            manifest.display_name,
            ...(Array.isArray(manifest.aliases) ? manifest.aliases : []),
          ].map((value) => normalizeGameId(value)).filter(Boolean);
          if (aliases.includes(id)) return path.join(dir, entry.name);
        } catch (_err) {
          // ignore malformed manifests while resolving aliases
        }
      }
    }
    return "";
  };
  
  const getRuntimeModulePath = (moduleId, ...parts) => {
    const base = getRuntimeModuleDir(moduleId);
    if (base) return path.join(base, ...parts);
    const fallbackDir = path.join(getRuntimeModulesDirs()[0], String(moduleId || ""));
    return path.join(fallbackDir, ...parts);
  };
  
  const getCommonClientWrapperPath = () => {
    return getRuntimeFilePath("commonclient_wrapper.py");
  };
  
  const getBizHawkClientWrapperPath = () => {
    return getRuntimeFilePath("bizhawkclient_wrapper.py");
  };
  
  const getSniClientWrapperPath = () => {
    return getRuntimeFilePath("sniclient_wrapper.py");
  };
  
  const getArchipelagoClientWrapperPath = () => {
    return getRuntimeFilePath("apclient_common.py");
  };
  
  const getRetroarchMemoryBridgePath = () => {
    return getRuntimeFilePath("retroarch_memory_bridge.py");
  };
  
  const getArchipelagoClientRegistryPath = () => {
    return getRuntimeFilePath(path.join("game-registry", "archipelago-clients.json"));
  };
  
  const getSniBridgePath = () => {
    return getRuntimeFilePath("sni_bridge.py");
  };
  
  const getPatcherWrapperPath = () => {
    return getRuntimeFilePath("patcher_wrapper.py");
  };
  
  
  const getConfigPath = () => {
    return path.join(app.getPath("home"), ".sekailink", "config.json");
  };

  const ROM_FILE_EXTENSIONS = new Set([
    ".gb",
    ".gbc",
    ".gba",
    ".nes",
    ".sfc",
    ".smc",
    ".z64",
    ".n64",
    ".v64",
    ".iso",
    ".rvz",
    ".gcm",
  ]);

  const looksLikeRomPath = (value) => {
    if (typeof value !== "string") return false;
    const ext = path.extname(value.trim()).toLowerCase();
    return ROM_FILE_EXTENSIONS.has(ext);
  };
  
  const normalizeConfig = (value) => {
    const config = value && typeof value === "object" ? value : {};
    if (!config.roms || typeof config.roms !== "object" || Array.isArray(config.roms)) config.roms = {};
    if (!config.windowing || typeof config.windowing !== "object") config.windowing = {};
    if (!config.layout || typeof config.layout !== "object") config.layout = {};
    // Back-compat: if older builds stored gamescope at top-level, migrate to windowing.gamescope.
    if (config.gamescope && !config.windowing.gamescope) {
      config.windowing.gamescope = config.gamescope;
    }
    // Back-compat: some import helpers briefly wrote ROM paths at the
    // root of the config. Runtime validation only reads config.roms, so
    // mirror those legacy entries without deleting the original keys.
    for (const [key, value] of Object.entries(config)) {
      if (!looksLikeRomPath(value)) continue;
      const normalizedKey = normalizeGameId ? normalizeGameId(key) : String(key || "").trim();
      if (!normalizedKey || config.roms[normalizedKey]) continue;
      config.roms[normalizedKey] = value;
    }
    return config;
  };
  
  const readConfig = () => {
    const configPath = getConfigPath();
    try {
      if (!fs.existsSync(configPath)) return {};
      return normalizeConfig(JSON.parse(fs.readFileSync(configPath, "utf-8")));
    } catch (_err) {
      return {};
    }
  };
  
  const writeConfig = (config) => {
    const configPath = getConfigPath();
    ensureDir(path.dirname(configPath));
    fs.writeFileSync(configPath, JSON.stringify(normalizeConfig(config), null, 2));
  };
  
  const ensureDir = (dirPath) => {
    fs.mkdirSync(dirPath, { recursive: true });
  };
  
  const isWritablePath = (p) => {
    try {
      fs.accessSync(String(p || ""), fs.constants.W_OK);
      return true;
    } catch (_err) {
      return false;
    }
  };
  
  const getRuntimeModulesDir = () => {
    return getRuntimeModulesDirs()[0];
  };
  
  const getRomsDir = () => {
    return path.join(app.getPath("userData"), "roms");
  };
  
  const getPopTrackerPacksDir = () => {
    return path.join(app.getPath("userData"), "poptracker", "packs");
  };
  
  const getRuntimeDownloadsDir = () => {
    return path.join(app.getPath("userData"), "runtime", "downloads");
  };
  
  const getRuntimeToolsDir = () => {
    return path.join(app.getPath("userData"), "runtime", "tools");
  };
  
  const getClientUpdateDownloadsDir = () => {
    return path.join(app.getPath("downloads"), "SekaiLink");
  };
  
  const getClientUpdateStagingDir = () => {
    if (process.platform === "win32") {
      // Keep Windows bundle extraction under a short path. The update bundle
      // contains deep runtime asset paths, and AppData/Roaming staging can cross
      // the legacy MAX_PATH limit during .NET zip extraction.
      return path.join(os.tmpdir(), "skl-upd");
    }
    return path.join(app.getPath("userData"), "updates", "staging");
  };

  return {
    getOverlayRuntimeDir,
    getRuntimeFilePath,
    getPythonRuntimeWheelhouseTag,
    getPythonRuntimeWheelhouseDir,
    getPythonPipInstallArgs,
    getRuntimeModulesDirs,
    getRuntimeModuleDir,
    getRuntimeModulePath,
    getCommonClientWrapperPath,
    getBizHawkClientWrapperPath,
    getSniClientWrapperPath,
    getArchipelagoClientWrapperPath,
    getRetroarchMemoryBridgePath,
    getArchipelagoClientRegistryPath,
    getSniBridgePath,
    getPatcherWrapperPath,
    getConfigPath,
    normalizeConfig,
    readConfig,
    writeConfig,
    ensureDir,
    isWritablePath,
    getRuntimeModulesDir,
    getRomsDir,
    getPopTrackerPacksDir,
    getRuntimeDownloadsDir,
    getRuntimeToolsDir,
    getClientUpdateDownloadsDir,
    getClientUpdateStagingDir,
  };
};

module.exports = { createClientRuntimeConfig };
