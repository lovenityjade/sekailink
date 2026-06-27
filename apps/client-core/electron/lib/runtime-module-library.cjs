"use strict";

const createRuntimeModuleLibrary = (deps = {}) => {
  const {
    fs,
    path,
    processRef = process,
    getRuntimeModulesDirs,
    normalizeGameId,
    normalizeIpcString,
    readConfig,
    writeConfig,
    fileExists,
    hashFile,
    getRomsDir,
    ensureDir,
    ensureMonoRuntime,
    resolveSekaiemuExecutable,
    resolveSekaiemuCorePath,
    resolveSklmiRuntimeForSekaiemu,
    resolveSklmiManifestDirForSekaiemu,
    resolveSekaiemuTrackerPackPath,
    getBundledRuntimeDir,
    getRuntimePlatformPath = () => "",
    getPopTrackerStatus,
  } = deps;
  const process = processRef;
  let _romIndexCache = null;
  let _patchIndexCache = null;

  const getModuleManifest = (moduleId) => {
    if (!moduleId) return null;
    for (const modulesDir of getRuntimeModulesDirs()) {
      const manifestPath = path.join(modulesDir, moduleId, "manifest.json");
      if (!fs.existsSync(manifestPath)) continue;
      try {
        return JSON.parse(fs.readFileSync(manifestPath, "utf-8"));
      } catch (_err) {
        return null;
      }
    }
    return null;
  };
  
  const listRuntimeModules = () => {
    const moduleIds = new Set();
    const results = [];
    for (const modulesDir of getRuntimeModulesDirs()) {
      if (!fs.existsSync(modulesDir)) continue;
      const entries = fs.readdirSync(modulesDir, { withFileTypes: true });
      for (const entry of entries) {
        if (!entry.isDirectory()) continue;
        if (moduleIds.has(entry.name)) continue;
        moduleIds.add(entry.name);
        const manifest = getModuleManifest(entry.name);
        if (!manifest) continue;
        results.push({ moduleId: entry.name, manifest });
      }
    }
    results.sort((a, b) => String(a.moduleId).localeCompare(String(b.moduleId)));
    return results;
  };
  
  const readFilePrefix = (filePath, length = 32) => {
    try {
      const fd = fs.openSync(filePath, "r");
      try {
        const buf = Buffer.alloc(Math.max(0, Math.min(4096, Number(length) || 0)));
        const n = fs.readSync(fd, buf, 0, buf.length, 0);
        return buf.slice(0, Math.max(0, n));
      } finally {
        fs.closeSync(fd);
      }
    } catch (_err) {
      return Buffer.alloc(0);
    }
  };
  
  const getRomHashOffsets = (filePath) => {
    const offsets = new Set([0]);
    const ext = path.extname(String(filePath || "")).toLowerCase();
    try {
      const size = fs.statSync(filePath).size || 0;
      // SNES copier header: 512 bytes. Archipelago strips it before hashing.
      if ((ext === ".sfc" || ext === ".smc") && size > 512 && (size % 0x8000) === 512) {
        offsets.add(512);
      }
    } catch (_err) {
      // ignore
    }
    // NES iNES header: 16 bytes when magic is present.
    if (ext === ".nes") {
      const prefix = readFilePrefix(filePath, 4);
      if (prefix.length >= 4 && prefix[0] === 0x4e && prefix[1] === 0x45 && prefix[2] === 0x53 && prefix[3] === 0x1a) {
        offsets.add(16);
      }
    }
    return Array.from(offsets.values()).sort((a, b) => a - b);
  };
  
  const findRomMatch = async (filePath, romIndex) => {
    const offsets = getRomHashOffsets(filePath);
  
    // Try md5 first (fast and used by most worlds), then sha1.
    for (const start of offsets) {
      const md5 = await hashFile(filePath, "md5", { start });
      const match = romIndex.get(String(md5 || "").toLowerCase());
      if (match) return { ...match, hash: md5, algo: "md5", start };
    }
    for (const start of offsets) {
      const sha1 = await hashFile(filePath, "sha1", { start });
      const match = romIndex.get(String(sha1 || "").toLowerCase());
      if (match) return { ...match, hash: sha1, algo: "sha1", start };
    }
    return null;
  };
  
  const getRomImportCandidatesForExtension = (ext) => {
    const normalizedExt = String(ext || "").trim().toLowerCase();
    if (!normalizedExt) return [];
    return listRuntimeModules()
      .map((info) => {
        const manifest = info.manifest || {};
        const exts = normalizeStringList(manifest?.rom_requirements?.extensions).map((entry) => entry.toLowerCase());
        if (!exts.includes(normalizedExt)) return null;
        const aliases = getRomConfigKeysForManifest(info.moduleId, manifest);
        return {
          gameId: String(manifest.game_id || info.moduleId || "").trim(),
          moduleId: String(info.moduleId || "").trim(),
          displayName: String(manifest.display_name || manifest.game_id || info.moduleId || "").trim(),
          extensions: exts,
          aliases,
        };
      })
      .filter(Boolean)
      .sort((a, b) => a.displayName.localeCompare(b.displayName));
  };
  
  const resolveRomImportTarget = async (filePath, options = {}) => {
    const romIndex = loadRomIndex();
    const ext = path.extname(filePath).toLowerCase();
    const requestedGameId = normalizeGameId(options.gameId || options.game_id || "");
    const candidates = getRomImportCandidatesForExtension(ext);
  
    if (requestedGameId) {
      const candidate = candidates.find((entry) =>
        entry.gameId === requestedGameId ||
        entry.moduleId === requestedGameId ||
        (Array.isArray(entry.aliases) && entry.aliases.includes(requestedGameId))
      );
      if (!candidate) {
        return {
          ok: false,
          error: "rom_extension_not_supported_for_game",
          expected: candidates.length
            ? candidates.map((entry) => `${entry.displayName} (${entry.extensions.join(", ")})`).join(", ")
            : "No runtime module accepts this file extension.",
        };
      }
      return { ok: true, ...candidate, matchKind: "selected" };
    }
  
    const found = await findRomMatch(filePath, romIndex);
    if (found) {
      const candidate = candidates.find((entry) => entry.gameId === found.gameId || entry.moduleId === found.moduleId);
      return {
        ok: true,
        gameId: found.gameId,
        moduleId: found.moduleId,
        displayName: candidate?.displayName || found.gameId,
        hash: found.hash,
        algo: found.algo,
        matchKind: "checksum",
      };
    }
  
    if (candidates.length === 1) {
      return { ok: true, ...candidates[0], matchKind: "extension" };
    }
  
    if (candidates.length > 1) {
      return {
        ok: false,
        error: "rom_game_ambiguous",
        expected: candidates.map((entry) => `${entry.displayName} [${entry.gameId}]`).join(", "),
        candidates,
      };
    }
  
    return { ok: false, error: "unsupported_rom_extension", expected: "A supported ROM extension for an installed runtime module." };
  };
  
  const normalizeStringList = (value) => {
    if (Array.isArray(value)) return value.map((v) => String(v));
    if (typeof value === "string") {
      const s = value.trim();
      return s ? [s] : [];
    }
    return [];
  };
  
  const loadRomIndex = () => {
    if (_romIndexCache) return _romIndexCache;
    const index = new Map();
    const seenModuleIds = new Set();
    for (const modulesDir of getRuntimeModulesDirs()) {
      if (!fs.existsSync(modulesDir)) continue;
      const entries = fs.readdirSync(modulesDir, { withFileTypes: true });
      for (const entry of entries) {
        if (!entry.isDirectory()) continue;
        if (seenModuleIds.has(entry.name)) continue;
        seenModuleIds.add(entry.name);
        const manifestPath = path.join(modulesDir, entry.name, "manifest.json");
        if (!fs.existsSync(manifestPath)) continue;
        try {
          const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf-8"));
          const req = manifest.rom_requirements || {};
          const md5s = normalizeStringList(req.md5);
          const sha1s = normalizeStringList(req.sha1);
          const gameId = String(manifest.game_id || entry.name || "").trim();
          for (const md5 of md5s) {
            index.set(md5.toLowerCase(), { gameId, moduleId: entry.name, algo: "md5" });
          }
          for (const sha1 of sha1s) {
            index.set(sha1.toLowerCase(), { gameId, moduleId: entry.name, algo: "sha1" });
          }
        } catch (_err) {
          // ignore malformed manifest
        }
      }
    }
    _romIndexCache = index;
    return index;
  };
  
  const getRomConfigKeysForManifest = (moduleId, manifest = null) => {
    const keys = new Set();
    const add = (value) => {
      const key = normalizeGameId(value);
      if (key) keys.add(key);
    };
    add(moduleId);
    add(manifest?.game_id);
    add(manifest?.game_key);
    add(manifest?.ap_world);
    for (const value of Array.isArray(manifest?.required_roms) ? manifest.required_roms : []) add(value);
    for (const value of Array.isArray(manifest?.aliases) ? manifest.aliases : []) add(value);
    return Array.from(keys.values());
  };
  
  const resolveConfiguredRomForModule = (moduleId, manifest = null) => {
    const safeModuleId = normalizeIpcString(moduleId, 200);
    const m = manifest && typeof manifest === "object"
      ? manifest
      : safeModuleId
        ? getModuleManifest(safeModuleId)
        : null;
    const config = readConfig();
    const roms = config.roms && typeof config.roms === "object" ? config.roms : {};
    for (const gameId of getRomConfigKeysForManifest(safeModuleId, m)) {
      const romPath = String(roms[gameId] || "").trim();
      if (romPath && fileExists(romPath)) return romPath;
    }
    return "";
  };
  
  const loadPatchIndex = () => {
    if (_patchIndexCache) return _patchIndexCache;
    const index = new Map();
    const collisions = [];
    const seenModuleIds = new Set();
    for (const modulesDir of getRuntimeModulesDirs()) {
      if (!fs.existsSync(modulesDir)) continue;
      const entries = fs.readdirSync(modulesDir, { withFileTypes: true });
      for (const entry of entries) {
        if (!entry.isDirectory()) continue;
        if (seenModuleIds.has(entry.name)) continue;
        seenModuleIds.add(entry.name);
        const manifestPath = path.join(modulesDir, entry.name, "manifest.json");
        if (!fs.existsSync(manifestPath)) continue;
        try {
          const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf-8"));
          const exts = normalizeStringList(manifest.patch_extensions);
          for (const ext of exts) {
            const normalized = String(ext || "").trim().toLowerCase();
            if (!normalized || !normalized.startsWith(".")) continue;
            const existing = index.get(normalized);
            if (existing) {
              collisions.push({
                ext: normalized,
                first: existing,
                second: { gameId: manifest.game_id, moduleId: entry.name },
              });
              continue;
            }
            index.set(normalized, { gameId: manifest.game_id, moduleId: entry.name });
          }
        } catch (_err) {
          // ignore malformed manifest
        }
      }
    }
    if (collisions.length) {
      console.warn("[runtime] Duplicate patch_extensions detected; resolution is ambiguous:", collisions);
    }
    _patchIndexCache = index;
    return index;
  };
  
  const resolveModuleForDownload = (downloadUrl) => {
    if (!downloadUrl) return { ok: false, error: "missing_url" };
    let pathname = "";
    try {
      pathname = new URL(downloadUrl).pathname || "";
    } catch (_err) {
      pathname = String(downloadUrl);
    }
    const ext = path.extname(pathname).toLowerCase();
    if (!ext) return { ok: false, error: "unknown_extension" };
  
    const patchIndex = loadPatchIndex();
    const found = patchIndex.get(ext);
    if (!found) return { ok: false, error: "unsupported_patch_type", ext };
    return { ok: true, moduleId: found.moduleId, gameId: found.gameId, ext };
  };
  
  const findModuleByApGameName = (apGameName) => {
    const target = String(apGameName || "").trim().toLowerCase();
    if (!target) return null;
    for (const info of listRuntimeModules()) {
      const manifest = info.manifest || {};
      const name = String(manifest.display_name || manifest.game_name || "").trim().toLowerCase();
      if (name && name === target) return info.moduleId;
    }
    return null;
  };

  const sha256File = (filePath) => {
    const crypto = require("crypto");
    const hash = crypto.createHash("sha256");
    hash.update(fs.readFileSync(filePath));
    return hash.digest("hex");
  };

  const normalizeSystemFileEntries = (manifest = {}) => {
    const entries = Array.isArray(manifest.system_files) ? manifest.system_files : [];
    return entries
      .map((entry) => {
        if (typeof entry === "string") return { name: entry, target: entry };
        if (!entry || typeof entry !== "object") return null;
        const name = String(entry.name || entry.file || entry.path || "").trim();
        const target = String(entry.target || entry.name || "").trim();
        if (!name || !target) return null;
        return {
          name,
          target,
          size: Number(entry.size || 0) || 0,
          sha256: String(entry.sha256 || "").trim().toLowerCase(),
          description: String(entry.description || "").trim(),
        };
      })
      .filter(Boolean);
  };

  const hasSystemFile = (entry) => {
    const userDataDir = process.env.SEKAILINK_USER_DATA_DIR || path.join(process.env.HOME || "", ".config", "sekailink-client");
    const roots = [
      path.join(userDataDir, "firmware"),
      path.join(getBundledRuntimeDir(), "firmware"),
      path.join(getBundledRuntimeDir(), "system"),
      getRuntimePlatformPath("firmware"),
      getRuntimePlatformPath("system"),
    ].filter(Boolean);
    for (const root of roots) {
      for (const candidate of [entry.name, entry.target].filter(Boolean)) {
        const filePath = path.join(root, candidate);
        if (!fs.existsSync(filePath)) continue;
        try {
          if (entry.size && fs.statSync(filePath).size !== entry.size) continue;
          if (entry.sha256 && sha256File(filePath) !== entry.sha256) continue;
          return true;
        } catch (_err) {
          // Keep scanning.
        }
      }
    }
    return false;
  };
  
  const validateSetupForModule = async (moduleId) => {
    const manifest = getModuleManifest(moduleId);
    if (!manifest) return { ok: false, error: "manifest_missing" };
  
    // Install per-module system/runtime dependencies during the setup phase,
    // so we fail early with actionable errors (instead of failing mid-launch).
    if (process.platform === "linux" && String(manifest.emu || "").toLowerCase() === "bizhawk") {
      try {
        await ensureMonoRuntime();
      } catch (err) {
        return { ok: false, error: "mono_missing", detail: String(err || ""), setupArea: "paths" };
      }
    }
  
    const emu = String(manifest.emu || "").trim().toLowerCase();
    if (emu === "sekaiemu" || emu === "sekaiemu-libretro") {
      if (!resolveSekaiemuExecutable()) {
        return { ok: false, error: "sekaiemu_not_found", setupArea: "paths" };
      }
      if (!resolveSekaiemuCorePath(manifest)) {
        return { ok: false, error: "sekaiemu_core_missing", setupArea: "paths" };
      }
      if (!resolveSklmiRuntimeForSekaiemu()) {
        return { ok: false, error: "sklmi_runtime_missing", setupArea: "paths" };
      }
      if (!resolveSklmiManifestDirForSekaiemu()) {
        return { ok: false, error: "sklmi_manifest_missing", setupArea: "paths" };
      }
      for (const systemFile of normalizeSystemFileEntries(manifest)) {
        if (!hasSystemFile(systemFile)) {
          return {
            ok: false,
            error: "sekaiemu_system_file_missing",
            detail: systemFile.description || systemFile.name,
            file: systemFile.name,
            setupArea: "paths",
          };
        }
      }
      const sekaiemu = manifest.sekaiemu && typeof manifest.sekaiemu === "object" ? manifest.sekaiemu : {};
      const declaredTrackerPack = String(
        sekaiemu.tracker_pack ||
          sekaiemu.tracker_pack_path ||
          manifest.tracker_pack_path ||
          ""
      ).trim();
      if (declaredTrackerPack) {
        const trackerPack = resolveSekaiemuTrackerPackPath(manifest, [
          getBundledRuntimeDir(),
          path.join(getBundledRuntimeDir(), "tracker-bundles"),
        ], { allowInstalledFallback: false });
        if (!trackerPack) {
          return { ok: false, error: "sekaiemu_tracker_pack_missing", setupArea: "paths" };
        }
      }
    }
  
    const config = readConfig();
    const roms = config.roms || {};
    const requiredRomIds = Array.isArray(manifest.required_roms)
      ? manifest.required_roms
      : manifest.game_id
        ? [manifest.game_id]
        : [];
    for (const gameId of requiredRomIds) {
      const romPath = String(roms[gameId] || "").trim();
      if (!romPath) return { ok: false, error: "rom_missing", gameId, setupArea: "roms" };
      if (!fs.existsSync(romPath)) {
        return { ok: false, error: "rom_missing", gameId, detail: "rom_path_not_found", setupArea: "roms" };
      }
      try {
        const stat = fs.statSync(romPath);
        if (!stat.isFile()) {
          return { ok: false, error: "rom_missing", gameId, detail: "rom_path_not_file", setupArea: "roms" };
        }
      } catch (_err) {
        return { ok: false, error: "rom_missing", gameId, detail: "rom_path_unreadable", setupArea: "roms" };
      }
    }
  
    // Separate PopTracker is only for legacy/non-Sekaiemu launch flows. Sekaiemu passes packs to SKLMI.
    if (manifest.tracker_pack_uid && emu !== "sekaiemu" && emu !== "sekaiemu-libretro") {
      const trackerStatus = getPopTrackerStatus();
      if (!trackerStatus.exists) return { ok: false, error: "poptracker_missing", setupArea: "paths" };
    }
  
    return { ok: true, manifest };
  };
  
  const scanRomFolder = async (folderPath) => {
    const romIndex = loadRomIndex();
    const results = [];
    if (!folderPath || !fs.existsSync(folderPath)) return results;
    const walk = (dirPath) => {
      const files = [];
      const entries = fs.readdirSync(dirPath, { withFileTypes: true });
      for (const entry of entries) {
        const fullPath = path.join(dirPath, entry.name);
        if (entry.isDirectory()) {
          files.push(...walk(fullPath));
        } else if (entry.isFile()) {
          files.push(fullPath);
        }
      }
      return files;
    };
  
    const files = walk(folderPath);
    for (const srcPath of files) {
      const ext = path.extname(srcPath).toLowerCase();
      if (!ext || ext.length > 6) continue;
      try {
        const found = await findRomMatch(srcPath, romIndex);
        if (!found) continue;
  
        const gameId = found.gameId;
        const destDir = getRomsDir();
        ensureDir(destDir);
        const destPath = path.join(destDir, `${gameId}${ext}`);
        fs.copyFileSync(srcPath, destPath);
  
        const config = readConfig();
        if (!config.roms) config.roms = {};
        const manifest = getModuleManifest(found.moduleId) || null;
        for (const key of getRomConfigKeysForManifest(found.moduleId, manifest)) {
          config.roms[key] = destPath;
        }
        writeConfig(config);
  
        results.push({ gameId, file: path.basename(srcPath) });
      } catch (_err) {
        // ignore hashing errors per file
      }
    }
    return results;
  };
  
  const importRomFile = async (filePath, options = {}) => {
    if (!filePath || !fs.existsSync(filePath)) return { ok: false, error: "missing_file" };
    const ext = path.extname(filePath).toLowerCase();
    if (!ext || ext.length > 6) return { ok: false, error: "invalid_extension" };
  
    try {
      const target = await resolveRomImportTarget(filePath, options);
      if (!target?.ok) return target;
  
      const gameId = target.gameId;
      const destDir = getRomsDir();
      ensureDir(destDir);
      const destPath = path.join(destDir, `${gameId}${ext}`);
      fs.copyFileSync(filePath, destPath);
  
      const config = readConfig();
      if (!config.roms) config.roms = {};
      const manifest = getModuleManifest(target.moduleId) || null;
      for (const key of getRomConfigKeysForManifest(target.moduleId, manifest)) {
        config.roms[key] = destPath;
      }
      writeConfig(config);
  
      return {
        ok: true,
        gameId,
        moduleId: target.moduleId,
        displayName: target.displayName,
        path: destPath,
        matchKind: target.matchKind,
        hash: target.hash,
        algo: target.algo,
      };
    } catch (err) {
      return { ok: false, error: String(err) };
    }
  };

  const clearCaches = () => {
    _romIndexCache = null;
    _patchIndexCache = null;
  };

  return {
    getModuleManifest,
    listRuntimeModules,
    getRomImportCandidatesForExtension,
    resolveRomImportTarget,
    resolveConfiguredRomForModule,
    resolveModuleForDownload,
    findModuleByApGameName,
    validateSetupForModule,
    scanRomFolder,
    importRomFile,
    clearCaches,
  };
};

module.exports = { createRuntimeModuleLibrary };
