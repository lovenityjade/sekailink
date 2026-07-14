"use strict";

const DEFAULT_CATALOG_URL = "https://sekailink.com/downloads/pc-packages/v1/catalog.json";

const createPcPackageRuntime = (deps = {}) => {
  const { app, fs, path, https, spawnSync, processRef = process, ensureDir, downloadToDirWithProgress, writeLogLine = () => {} } = deps;
  const installRoot = () => path.join(app.getPath("userData"), "pc-packages");
  const catalogCachePath = () => path.join(installRoot(), "catalog-cache.json");
  const statePath = () => path.join(installRoot(), "installed.json");
  const bundledCatalogPaths = () => [
    path.join(app.getAppPath(), "public", "pc-packages", "catalog-v1.json"),
    path.join(processRef.resourcesPath || "", "public", "pc-packages", "catalog-v1.json"),
  ];

  const readJson = (filePath, fallback) => {
    try { return JSON.parse(fs.readFileSync(filePath, "utf8")); } catch (_err) { return fallback; }
  };
  const writeJson = (filePath, value) => {
    ensureDir(path.dirname(filePath));
    const temporary = `${filePath}.tmp-${processRef.pid}-${Date.now()}`;
    fs.writeFileSync(temporary, `${JSON.stringify(value, null, 2)}\n`, { mode: 0o600 });
    fs.renameSync(temporary, filePath);
  };
  const loadState = () => readJson(statePath(), { schema: "sekailink.pc-package-state/v1", packages: {} });
  const normalizePackageId = (value) => {
    const id = String(value || "").trim().toLowerCase();
    if (!/^[a-z0-9][a-z0-9._-]{0,127}$/.test(id)) throw new Error("pc_package_invalid_id");
    return id;
  };
  const safeRelativePath = (value) => {
    const normalized = String(value || "").replace(/\\/g, "/");
    if (!normalized || normalized.startsWith("/") || normalized.split("/").includes("..")) {
      throw new Error("pc_package_invalid_relative_path");
    }
    return normalized;
  };

  const fetchJson = (url, redirects = 0) => new Promise((resolve, reject) => {
    if (redirects > 5) return reject(new Error("pc_package_catalog_redirect_limit"));
    const request = https.get(url, { headers: { "User-Agent": "SekaiLink/1.0" } }, (response) => {
      if ([301, 302, 303, 307, 308].includes(response.statusCode) && response.headers.location) {
        response.resume();
        return fetchJson(new URL(response.headers.location, url).toString(), redirects + 1).then(resolve, reject);
      }
      if (response.statusCode !== 200) {
        response.resume();
        return reject(new Error(`pc_package_catalog_http_${response.statusCode || "unknown"}`));
      }
      let body = "";
      response.setEncoding("utf8");
      response.on("data", (chunk) => { body += chunk; if (body.length > 4 * 1024 * 1024) request.destroy(new Error("pc_package_catalog_too_large")); });
      response.on("end", () => { try { resolve(JSON.parse(body)); } catch (_err) { reject(new Error("pc_package_catalog_invalid_json")); } });
    });
    request.on("error", reject);
  });

  const validateCatalog = (catalog) => {
    if (!catalog || catalog.schema !== "sekailink.pc-package-catalog/v1" || !Array.isArray(catalog.packages)) {
      throw new Error("pc_package_catalog_invalid_schema");
    }
    return catalog;
  };

  const getCatalog = async ({ refresh = false } = {}) => {
    const url = String(processRef.env.SEKAILINK_PC_PACKAGE_CATALOG_URL || DEFAULT_CATALOG_URL);
    if (refresh || !fs.existsSync(catalogCachePath())) {
      try {
        const catalog = validateCatalog(await fetchJson(url));
        writeJson(catalogCachePath(), catalog);
      } catch (error) {
        if (!fs.existsSync(catalogCachePath())) {
          const bundled = bundledCatalogPaths().find((candidate) => candidate && fs.existsSync(candidate));
          if (!bundled) throw error;
          writeJson(catalogCachePath(), validateCatalog(readJson(bundled, null)));
        }
        writeLogLine("warn", "pc-packages", `catalog refresh failed, using cache: ${String(error?.message || error)}`);
      }
    }
    const catalog = validateCatalog(readJson(catalogCachePath(), null));
    const state = loadState();
    return {
      ...catalog,
      packages: catalog.packages.map((entry) => ({
        ...entry,
        availableRelease: currentRelease(entry),
        installed: state.packages[entry.id] || null,
      })),
    };
  };

  const currentRelease = (entry) => {
    const platform = `${processRef.platform}-${processRef.arch}`;
    const releases = Array.isArray(entry?.releases) ? entry.releases : [];
    return releases.find((release) => release.platform === platform && release.channel === "canonical") ||
      releases.find((release) => release.platform === platform) || null;
  };
  const safeArchiveEntries = (archivePath) => {
    const result = spawnSync("tar", ["-tf", archivePath], { encoding: "utf8", windowsHide: true });
    if (result.status !== 0) throw new Error(`pc_package_archive_list_failed: ${String(result.stderr || "").trim()}`);
    const entries = String(result.stdout || "").split(/\r?\n/).filter(Boolean);
    if (!entries.length || entries.some((entry) => path.isAbsolute(entry) || entry.replace(/\\/g, "/").split("/").includes(".."))) {
      throw new Error("pc_package_archive_unsafe_paths");
    }
    const verbose = spawnSync("tar", ["-tvf", archivePath], { encoding: "utf8", windowsHide: true });
    if (verbose.status !== 0 || String(verbose.stdout || "").split(/\r?\n/).some((line) => /^[lh]/.test(line))) {
      throw new Error("pc_package_archive_links_not_allowed");
    }
  };

  const install = async (packageId) => {
    packageId = normalizePackageId(packageId);
    const catalog = await getCatalog({ refresh: true });
    const entry = catalog.packages.find((candidate) => candidate.id === packageId);
    if (!entry) throw new Error("pc_package_not_found");
    const release = currentRelease(entry);
    if (!release || !/^https:\/\//.test(String(release.url || "")) || !/^[a-f0-9]{64}$/i.test(String(release.sha256 || "")) || !(Number(release.bytes) > 0)) {
      throw new Error("pc_package_release_unavailable");
    }

    const packageRoot = path.join(installRoot(), "packages", entry.id);
    const downloads = path.join(installRoot(), "downloads");
    const staging = path.join(packageRoot, `.staging-${Date.now()}`);
    ensureDir(downloads);
    ensureDir(staging);
    let archivePath = "";
    try {
      const downloaded = await downloadToDirWithProgress(release.url, downloads, {
        expectedSha256: release.sha256,
        requireHash: true,
        defaultName: `${entry.id}-${release.version}.tar.gz`,
      });
      archivePath = downloaded.path;
      if (Number(downloaded.bytes) !== Number(release.bytes)) throw new Error("pc_package_size_mismatch");
      safeArchiveEntries(archivePath);
      const extracted = spawnSync("tar", ["--no-same-owner", "--no-same-permissions", "-xf", archivePath, "-C", staging], { encoding: "utf8", windowsHide: true });
      if (extracted.status !== 0) throw new Error(`pc_package_extract_failed: ${String(extracted.stderr || "").trim()}`);
      const executableRelative = safeRelativePath(release.executable);
      const executable = path.join(staging, executableRelative);
      if (!fs.existsSync(executable)) throw new Error("pc_package_executable_missing");
      if (processRef.platform !== "win32") fs.chmodSync(executable, 0o755);

      const active = path.join(packageRoot, "active");
      const previous = path.join(packageRoot, "previous");
      fs.rmSync(previous, { recursive: true, force: true });
      if (fs.existsSync(active)) fs.renameSync(active, previous);
      fs.renameSync(staging, active);
      const state = loadState();
      state.packages[entry.id] = { version: release.version, platform: release.platform, installedAt: new Date().toISOString(), executable: release.executable };
      writeJson(statePath(), state);
      return { ok: true, packageId: entry.id, ...state.packages[entry.id] };
    } finally {
      fs.rmSync(staging, { recursive: true, force: true });
      if (archivePath) fs.rmSync(archivePath, { force: true });
    }
  };

  const uninstall = async (packageId) => {
    packageId = normalizePackageId(packageId);
    const state = loadState();
    delete state.packages[packageId];
    writeJson(statePath(), state);
    fs.rmSync(path.join(installRoot(), "packages", packageId), { recursive: true, force: true });
    return { ok: true, packageId };
  };

  const resolveInstalled = (packageId) => {
    packageId = normalizePackageId(packageId);
    const installed = loadState().packages[packageId];
    if (!installed) return null;
    const executableRelative = safeRelativePath(installed.executable);
    const executable = path.join(installRoot(), "packages", packageId, "active", executableRelative);
    return fs.existsSync(executable) ? { ...installed, executable } : null;
  };

  return { getCatalog, install, uninstall, resolveInstalled };
};

module.exports = { createPcPackageRuntime };
