"use strict";

const createMonoRuntime = (deps = {}) => {
  const {
    fs,
    path,
    spawnSync,
    processRef = process,
    getBundledThirdPartyDir,
    findPathByBasename,
    which,
    writeLogLine,
  } = deps;
  const process = processRef;

  let _monoRuntimePromise = null;
  
  const monoProbeOk = (monoPath) => {
    const p = String(monoPath || "").trim();
    if (!p) return false;
    try {
      if (!fs.existsSync(p)) return false;
      const probe = spawnSync(p, ["--version"], { stdio: ["ignore", "ignore", "ignore"] });
      return probe.status === 0;
    } catch (_err) {
      return false;
    }
  };
  
  const getBundledMonoInstallRoot = () => {
    // SekaiLink bundles a pinned mono runtime for Linux so BizHawk can run without system package installs.
    // Expected layout: third_party/mono/sekailink-mono-linux-<arch>/bin/mono
    if (process.platform !== "linux") return "";
    const arch =
      process.arch === "x64" ? "x86_64" : process.arch === "arm64" ? "arm64" : String(process.arch || "x86_64");
    return path.join(getBundledThirdPartyDir(), "mono", `sekailink-mono-linux-${arch}`);
  };
  
  const getMonoFromInstallRoot = (rootDir) => {
    const base = String(rootDir || "").trim();
    if (!base) return "";
    const direct = path.join(base, "bin", "mono");
    if (fs.existsSync(direct)) return direct;
    // Fallback: find any "mono" binary under the extracted tree.
    const found = findPathByBasename(base, "mono", 8);
    return found && fs.existsSync(found) ? found : "";
  };
  
  const ensureMonoRuntime = async () => {
    if (_monoRuntimePromise) return _monoRuntimePromise;
    _monoRuntimePromise = (async () => {
      // 1) Explicit override
      const override = String(process.env.SEKAILINK_MONO || "").trim();
      if (override && monoProbeOk(override)) return override;
  
      // 2) Bundled mono runtime (Linux)
      const bundledRoot = getBundledMonoInstallRoot();
      if (bundledRoot && fs.existsSync(bundledRoot)) {
        const monoPath = getMonoFromInstallRoot(bundledRoot);
        try {
          if (monoPath) fs.chmodSync(monoPath, 0o755);
        } catch (_err) {
          // ignore chmod failures
        }
        if (monoPath && monoProbeOk(monoPath)) return monoPath;
        writeLogLine("error", "mono-runtime", `bundled mono not runnable: rootDir=${bundledRoot} monoPath=${monoPath}`);
      }
  
      // 3) System mono (fallback)
      const sys = which("mono");
      if (sys && monoProbeOk(sys)) return sys;
  
      throw new Error(`mono_missing: expected bundled mono at ${bundledRoot || "(n/a)"}`);
    })().catch((err) => {
      // Allow retry if the user installs mono after a failure.
      _monoRuntimePromise = null;
      throw err;
    });
    return _monoRuntimePromise;
  };

  return { ensureMonoRuntime };
};

module.exports = { createMonoRuntime };
