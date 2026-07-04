const createRuntimePaths = ({ app, fs, path, dirname, processRef = process }) => {
  const firstExistingDir = (...candidates) => {
    for (const candidate of candidates) {
      const safe = String(candidate || "").trim();
      if (!safe) continue;
      try {
        if (fs.existsSync(safe) && fs.statSync(safe).isDirectory()) return safe;
      } catch (_err) {
        // keep scanning
      }
    }
    return "";
  };

  const getDevRuntimeCandidates = () => {
    const repoRuntime = path.join(dirname, "..", "..", "..", "runtime");
    const localCoreRuntime = path.join(dirname, "..", "..", "runtime");
    const siblingSekaiemuRuntime = path.join(dirname, "..", "..", "..", "sekaiemu", "host", "runtime");
    const envRuntime = String(processRef.env.SEKAILINK_RUNTIME_DIR || "").trim();
    return [envRuntime, repoRuntime, localCoreRuntime, siblingSekaiemuRuntime].filter(Boolean);
  };

  const getBundledRuntimeDir = () => {
    if (app.isPackaged) return path.join(processRef.resourcesPath, "runtime");
    const resolved = firstExistingDir(...getDevRuntimeCandidates());
    if (resolved) return resolved;
    return path.join(dirname, "..", "..", "runtime");
  };

  const getRuntimePlatformId = () => {
    const arch = processRef.arch === "x64" ? "x64" : processRef.arch;
    return `${processRef.platform}-${arch}`;
  };

  const getRuntimePlatformDir = () => path.join(getBundledRuntimeDir(), "platforms", getRuntimePlatformId());
  const getRuntimePlatformPath = (...parts) => path.join(getRuntimePlatformDir(), ...parts);

  const getPlatformRuntimeDirCandidates = (...parts) => {
    const runtimeDir = getBundledRuntimeDir();
    return [getRuntimePlatformPath(...parts), path.join(runtimeDir, ...parts)];
  };

  const getBundledThirdPartyDir = () => {
    if (app.isPackaged) return path.join(processRef.resourcesPath, "third_party");
    return path.join(dirname, "..", "..", "..", "third_party");
  };

  const isValidApRuntimeRoot = (dir) =>
    Boolean(
      dir &&
        fs.existsSync(path.join(dir, "Patch.py")) &&
        fs.existsSync(path.join(dir, "BaseClasses.py")) &&
        fs.existsSync(path.join(dir, "worlds", "Files.py"))
    );

  const getBundledApRootDir = () => {
    const envApRoot = String(processRef.env.SEKAILINK_AP_ROOT || "").trim();
    if ((!app.isPackaged || processRef.env.SEKAILINK_ALLOW_EXTERNAL_AP_ROOT === "1") && isValidApRuntimeRoot(envApRoot)) {
      return envApRoot;
    }
    const candidates = app.isPackaged
      ? [path.join(processRef.resourcesPath, "runtime", "ap"), path.join(processRef.resourcesPath, "ap")]
      : [
          path.join(dirname, "..", "..", "runtime", "ap"),
          path.join(dirname, "..", "..", "..", "runtime", "ap"),
          path.join(dirname, "..", "..", ".."),
        ];
    const resolved = firstExistingDir(...candidates.filter(isValidApRuntimeRoot));
    if (resolved) return resolved;
    return app.isPackaged ? path.join(processRef.resourcesPath, "runtime", "ap") : path.join(dirname, "..", "..", "runtime", "ap");
  };

  const getRuntimeOverlayRoot = () => path.join(app.getPath("userData"), "runtime", "overlay");
  const getOverlayApRootDir = () => path.join(getRuntimeOverlayRoot(), "ap");

  const getEffectiveApRootDirs = () => {
    const dirs = [];
    const overlay = getOverlayApRootDir();
    if (isValidApRuntimeRoot(overlay)) dirs.push(overlay);
    const bundled = getBundledApRootDir();
    if (isValidApRuntimeRoot(bundled)) dirs.push(bundled);
    return dirs;
  };

  const withApPythonEnv = (baseEnv = {}) => {
    const apRoots = getEffectiveApRootDirs();
    const apRoot = apRoots[0] || getBundledApRootDir();
    const prior = baseEnv.PYTHONPATH || processRef.env.PYTHONPATH || "";
    const extraImportRoots = [];
    for (const root of apRoots) {
      const dk64Root = path.join(root, "worlds", "dk64");
      if (fs.existsSync(path.join(dk64Root, "randomizer"))) extraImportRoots.push(dk64Root);
    }
    const injected = [...apRoots, ...extraImportRoots].filter(Boolean).join(path.delimiter);
    return {
      ...baseEnv,
      SEKAILINK_AP_ROOT: apRoot,
      PYTHONPATH: prior ? `${injected}${path.delimiter}${prior}` : injected,
      PYTHONNOUSERSITE: "1",
      PYTHONDONTWRITEBYTECODE: "1",
      SKIP_REQUIREMENTS_UPDATE: "1",
    };
  };

  return {
    firstExistingDir,
    getDevRuntimeCandidates,
    getBundledRuntimeDir,
    getRuntimePlatformId,
    getRuntimePlatformDir,
    getRuntimePlatformPath,
    getPlatformRuntimeDirCandidates,
    getBundledThirdPartyDir,
    isValidApRuntimeRoot,
    getBundledApRootDir,
    getRuntimeOverlayRoot,
    getOverlayApRootDir,
    getEffectiveApRootDirs,
    withApPythonEnv,
  };
};

module.exports = { createRuntimePaths };
