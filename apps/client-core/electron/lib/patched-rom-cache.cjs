function createPatchedRomCacheStore(options) {
  const fs = options.fs;
  const path = options.path;
  const crypto = options.crypto;
  const ensureDir = options.ensureDir;
  const nowIso = options.nowIso;
  const hashFile = options.hashFile;
  const cachePath = String(options.cachePath || "");

  let cacheData = null;

  const load = () => {
    if (cacheData) return cacheData;
    try {
      if (cachePath && fs.existsSync(cachePath)) {
        const parsed = JSON.parse(fs.readFileSync(cachePath, "utf-8"));
        if (parsed && typeof parsed === "object" && parsed.entries && typeof parsed.entries === "object") {
          if (!parsed.urlEntries || typeof parsed.urlEntries !== "object") parsed.urlEntries = {};
          cacheData = parsed;
          return cacheData;
        }
      }
    } catch (_err) {
      // ignore malformed cache and recreate
    }
    cacheData = { entries: {}, urlEntries: {} };
    return cacheData;
  };

  const save = () => {
    try {
      ensureDir(path.dirname(cachePath));
      fs.writeFileSync(cachePath, JSON.stringify(load(), null, 2), "utf-8");
    } catch (_err) {
      // best-effort cache persistence
    }
  };

  const makeKey = ({ moduleId, patchPath, patchHash }) => {
    const ext = path.extname(String(patchPath || "")).toLowerCase();
    return `${String(moduleId || "")}|${ext}|${String(patchHash || "")}`;
  };

  const makeUrlKey = ({ moduleId, downloadUrl }) => {
    const url = String(downloadUrl || "").trim();
    const urlHash = url ? crypto.createHash("md5").update(url).digest("hex") : "";
    return `${String(moduleId || "")}|url|${urlHash}`;
  };

  const forgetMissingEntry = (key) => {
    const cache = load();
    if (cache.entries[key]) {
      delete cache.entries[key];
      save();
    }
  };

  const resolveByPatch = async ({ moduleId, patchPath }) => {
    if (!moduleId || !patchPath || !fs.existsSync(patchPath)) {
      return { ok: false, error: "missing_args" };
    }
    const patchHash = await hashFile(patchPath, "md5");
    const key = makeKey({ moduleId, patchPath, patchHash });
    const cache = load();
    const ext = path.extname(String(patchPath || "")).toLowerCase();
    const entry = cache.entries[key];

    if (entry && entry.outputPath && fs.existsSync(entry.outputPath)) {
      return { ok: true, key, patchHash, outputPath: entry.outputPath, cacheHit: "exact", shouldReuse: true };
    }

    const legacyPrefix = `${String(moduleId || "")}|${ext}|${String(patchHash || "")}|cfg:`;
    for (const [legacyKey, legacyEntry] of Object.entries(cache.entries || {})) {
      if (!String(legacyKey || "").startsWith(legacyPrefix)) continue;
      if (!legacyEntry || !legacyEntry.outputPath) continue;
      if (!fs.existsSync(legacyEntry.outputPath)) continue;
      cache.entries[key] = {
        ...legacyEntry,
        moduleId: String(moduleId || ""),
        patchHash: String(patchHash || ""),
        updatedAt: nowIso(),
      };
      delete cache.entries[legacyKey];
      save();
      return {
        ok: true,
        key,
        patchHash,
        outputPath: legacyEntry.outputPath,
        cacheHit: "legacy_migrated",
        shouldReuse: true,
      };
    }

    if (entry && (!entry.outputPath || !fs.existsSync(entry.outputPath))) {
      forgetMissingEntry(key);
    }

    return { ok: false, key, patchHash, error: "cache_miss", shouldReuse: false };
  };

  const resolveByUrl = ({ moduleId, downloadUrl }) => {
    if (!moduleId || !downloadUrl) return { ok: false, error: "missing_args" };
    const key = makeUrlKey({ moduleId, downloadUrl });
    const cache = load();
    const entry = cache.urlEntries?.[key];
    if (entry && entry.outputPath && fs.existsSync(entry.outputPath)) {
      return { ok: true, key, outputPath: entry.outputPath, cacheHit: "url", shouldReuse: true };
    }
    if (entry && (!entry.outputPath || !fs.existsSync(entry.outputPath))) {
      delete cache.urlEntries[key];
      save();
    }
    return { ok: false, key, error: "cache_miss", shouldReuse: false };
  };

  const remember = ({ key, moduleId, patchPath, patchHash, outputPath, downloadUrl }) => {
    if (!key || !outputPath) return;
    const cache = load();
    cache.entries[key] = {
      moduleId: String(moduleId || ""),
      patchName: path.basename(String(patchPath || "")),
      patchHash: String(patchHash || ""),
      outputPath: String(outputPath),
      updatedAt: nowIso(),
    };

    if (downloadUrl) {
      const urlKey = makeUrlKey({ moduleId, downloadUrl });
      cache.urlEntries[urlKey] = {
        moduleId: String(moduleId || ""),
        downloadUrl: String(downloadUrl || ""),
        outputPath: String(outputPath),
        updatedAt: nowIso(),
      };
    }

    const rows = Object.entries(cache.entries);
    if (rows.length > 400) {
      rows
        .sort((a, b) => String(a[1]?.updatedAt || "").localeCompare(String(b[1]?.updatedAt || "")))
        .slice(0, rows.length - 400)
        .forEach(([rowKey]) => delete cache.entries[rowKey]);
    }

    const urlRows = Object.entries(cache.urlEntries || {});
    if (urlRows.length > 400) {
      urlRows
        .sort((a, b) => String(a[1]?.updatedAt || "").localeCompare(String(b[1]?.updatedAt || "")))
        .slice(0, urlRows.length - 400)
        .forEach(([rowKey]) => delete cache.urlEntries[rowKey]);
    }

    save();
  };

  const listEntries = () => {
    const cache = load();
    const rows = [];
    for (const [key, entry] of Object.entries(cache.entries || {})) {
      const outputPath = String(entry?.outputPath || "");
      rows.push({
        key,
        moduleId: String(entry?.moduleId || ""),
        patchName: String(entry?.patchName || ""),
        patchHash: String(entry?.patchHash || ""),
        outputPath,
        exists: outputPath ? fs.existsSync(outputPath) : false,
        updatedAt: String(entry?.updatedAt || ""),
      });
    }
    rows.sort((a, b) => String(b.updatedAt || "").localeCompare(String(a.updatedAt || "")));
    return rows;
  };

  const clear = ({ removeFiles = false } = {}) => {
    const cache = load();
    let removedFiles = 0;
    let failedFiles = 0;
    if (removeFiles) {
      const outputPaths = new Set();
      for (const entry of Object.values(cache.entries || {})) {
        if (entry?.outputPath) outputPaths.add(String(entry.outputPath));
      }
      for (const entry of Object.values(cache.urlEntries || {})) {
        if (entry?.outputPath) outputPaths.add(String(entry.outputPath));
      }
      for (const outputPath of outputPaths) {
        try {
          if (outputPath && fs.existsSync(outputPath)) {
            fs.rmSync(outputPath, { force: true });
            removedFiles += 1;
          }
        } catch (_err) {
          failedFiles += 1;
        }
      }
    }
    cacheData = { entries: {}, urlEntries: {} };
    save();
    return { removedFiles, failedFiles };
  };

  return {
    load,
    save,
    resolveByPatch,
    resolveByUrl,
    remember,
    listEntries,
    clear,
  };
}

module.exports = {
  createPatchedRomCacheStore,
};
