const createBootstrapperControl = ({ app, crypto, fs, path, spawn, processRef = process }) => {
  const readJsonFileSafe = (filePath) => {
    try {
      if (!filePath || !fs.existsSync(filePath)) return null;
      const raw = fs.readFileSync(filePath, "utf8");
      const parsed = JSON.parse(raw);
      if (parsed && typeof parsed === "object") return parsed;
    } catch (_err) {
      // ignore
    }
    return null;
  };

  const base64UrlDecodeUtf8 = (value) => {
    const raw = String(value || "").replace(/-/g, "+").replace(/_/g, "/");
    const padded = raw + "===".slice((raw.length + 3) % 4);
    return Buffer.from(padded, "base64").toString("utf8");
  };

  const getBootstrapLaunchTokenStatus = () => {
    const tokenRaw = String(processRef.env.SEKAILINK_BOOTSTRAP_TOKEN || "").trim();
    if (!tokenRaw || !tokenRaw.includes(".")) return { ok: false, present: false, error: "bootstrap_token_missing" };
    const [body, sig] = tokenRaw.split(".", 2);
    if (!body || !sig || !/^[a-f0-9]{64}$/i.test(sig)) return { ok: false, present: true, error: "bootstrap_token_invalid_format" };

    const installDirHint = String(processRef.env.SEKAILINK_BOOTSTRAP_INSTALL_DIR || "").trim();
    const stateDirHint = String(processRef.env.SEKAILINK_BOOTSTRAP_STATE_DIR || "").trim();
    const exeDir = path.dirname(processRef.execPath);
    const secretCandidates = [
      stateDirHint ? path.join(stateDirHint, "launcher-secret.key") : "",
      installDirHint ? path.join(installDirHint, ".sekailink", "launcher-secret.key") : "",
      path.join(exeDir, ".sekailink", "launcher-secret.key"),
    ].filter(Boolean);

    let secret = "";
    for (const candidate of secretCandidates) {
      try {
        if (fs.existsSync(candidate)) secret = String(fs.readFileSync(candidate, "utf8") || "").trim();
      } catch (_err) {
        // ignore
      }
      if (secret.length >= 32) break;
    }
    if (secret.length < 32) return { ok: false, present: true, error: "bootstrap_secret_missing" };

    const expectedSig = crypto.createHmac("sha256", secret).update(body).digest("hex");
    if (expectedSig.toLowerCase() !== sig.toLowerCase()) return { ok: false, present: true, error: "bootstrap_token_bad_signature" };

    let payload = null;
    try {
      payload = JSON.parse(base64UrlDecodeUtf8(body));
    } catch (_err) {
      payload = null;
    }
    if (!payload || typeof payload !== "object") return { ok: false, present: true, error: "bootstrap_token_bad_payload" };
    if (String(payload.iss || "") !== "sekailink-bootstrapper") return { ok: false, present: true, error: "bootstrap_token_bad_issuer" };
    if (String(payload.aud || "") !== "sekailink-client") return { ok: false, present: true, error: "bootstrap_token_bad_audience" };

    const now = Date.now();
    const iat = Number(payload.iat || 0);
    const exp = Number(payload.exp || 0);
    if (!Number.isFinite(iat) || !Number.isFinite(exp) || exp <= now || iat > now + 10000) {
      return { ok: false, present: true, error: "bootstrap_token_expired" };
    }
    if (exp - iat > 10 * 60 * 1000) return { ok: false, present: true, error: "bootstrap_token_ttl_rejected" };

    return { ok: true, present: true, error: "" };
  };

  const validateBootstrapLaunchToken = () => {
    const runtimeLabDirect = String(processRef.env.SEKAILINK_RUNTIME_LAB || "").trim() === "1";
    const requireBootstrap = app.isPackaged && String(processRef.env.SEKAILINK_REQUIRE_BOOTSTRAP || "0") !== "0";
    const token = getBootstrapLaunchTokenStatus();
    if (runtimeLabDirect || !requireBootstrap) {
      return {
        ok: true,
        enforced: false,
        token_present: Boolean(token.present),
        token_valid: Boolean(token.ok),
        error: runtimeLabDirect ? "" : (token.ok ? "" : String(token.error || "")),
        runtime_lab_direct: runtimeLabDirect,
      };
    }
    if (!token.ok) {
      return {
        ok: false,
        enforced: true,
        token_present: Boolean(token.present),
        token_valid: false,
        error: String(token.error || "bootstrap_token_invalid"),
      };
    }
    return { ok: true, enforced: true, token_present: true, token_valid: true, error: "" };
  };

  const getBootstrapInstallState = () => {
    const explicitStateDir = String(processRef.env.SEKAILINK_BOOTSTRAP_STATE_DIR || "").trim();
    const explicitInstallDir = String(processRef.env.SEKAILINK_BOOTSTRAP_INSTALL_DIR || "").trim();
    const exeDir = path.dirname(processRef.execPath);
    const candidateStateFiles = [
      explicitStateDir ? path.join(explicitStateDir, "install-state.json") : "",
      explicitInstallDir ? path.join(explicitInstallDir, ".sekailink", "install-state.json") : "",
      path.join(exeDir, ".sekailink", "install-state.json"),
    ].filter(Boolean);
    for (const candidate of candidateStateFiles) {
      const parsed = readJsonFileSafe(candidate);
      if (parsed) return parsed;
    }
    return null;
  };

  const resolveBootstrapperExecutable = () => {
    const envExplicit = String(processRef.env.SEKAILINK_BOOTSTRAPPER_PATH || "").trim();
    if (envExplicit && fs.existsSync(envExplicit)) return envExplicit;

    const installState = getBootstrapInstallState();
    const stateInstallDir = String(installState?.installDir || processRef.env.SEKAILINK_BOOTSTRAP_INSTALL_DIR || "").trim();
    const exeDir = path.dirname(processRef.execPath);
    const roots = [stateInstallDir, exeDir].filter(Boolean);
    const candidates = [];

    for (const root of roots) {
      if (processRef.platform === "win32") {
        candidates.push(path.join(root, "SekaiLink Bootloader", "SekaiLink Bootloader.exe"));
        candidates.push(path.join(root, "SekaiLink-bootstrapper.cmd"));
        candidates.push(path.join(root, "SekaiLink-bootstrapper.ps1"));
        candidates.push(path.join(root, "SekaiLink-bootstrapper.exe"));
        candidates.push(path.join(root, "SekaiLink Bootstrapper.exe"));
      } else {
        candidates.push(path.join(root, "SekaiLink Bootloader", "sekailink-bootloader"));
        candidates.push(path.join(root, "SekaiLink Bootloader Linux", "sekailink-bootloader"));
        candidates.push(path.join(root, "sekailink-bootstrapper.sh"));
        candidates.push(path.join(root, "SekaiLink-bootstrapper.AppImage"));
        candidates.push(path.join(root, "SekaiLink Bootstrapper.AppImage"));
      }
    }

    for (const candidate of candidates) {
      if (fs.existsSync(candidate)) return candidate;
    }

    for (const root of roots) {
      try {
        const entries = fs.readdirSync(root, { withFileTypes: true });
        for (const entry of entries) {
          if (entry.isDirectory()) {
            const nested = processRef.platform === "win32"
              ? path.join(root, entry.name, "SekaiLink Bootloader.exe")
              : path.join(root, entry.name, "sekailink-bootloader");
            if (fs.existsSync(nested)) return nested;
            continue;
          }
          if (!entry.isFile()) continue;
          const lower = String(entry.name || "").toLowerCase();
          if (!lower.includes("sekailink-bootstrapper") && !lower.includes("sekailink bootloader")) continue;
          if (!(lower.endsWith(".cmd") || lower.endsWith(".ps1") || lower.endsWith(".sh") || lower.endsWith(".exe") || lower.endsWith(".appimage"))) continue;
          const full = path.join(root, entry.name);
          if (fs.existsSync(full)) return full;
        }
      } catch (_err) {
        // ignore
      }
    }
    return "";
  };

  const getBootstrapperDownloadUrl = () => {
    if (processRef.platform === "win32") {
      return "https://sekailink.com/downloads/client/bootstrapper/latest/SekaiLink-bootstrapper-windows.zip";
    }
    return "https://sekailink.com/downloads/client/bootstrapper/latest/SekaiLink-bootstrapper-linux.tar.gz";
  };

  const spawnDetachedBootstrapper = (bootstrapperPath) => {
    const p = String(bootstrapperPath || "").trim();
    const ext = path.extname(p).toLowerCase();
    let command = p;
    let args = [];

    if (processRef.platform === "win32" && (ext === ".cmd" || ext === ".bat")) {
      command = "cmd.exe";
      args = ["/d", "/s", "/c", `"${p.replace(/"/g, '""')}"`];
    } else if (processRef.platform === "win32" && ext === ".ps1") {
      command = "powershell.exe";
      args = ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", p];
    } else if (processRef.platform !== "win32" && ext === ".sh") {
      try {
        fs.chmodSync(p, fs.statSync(p).mode | 0o111);
      } catch (_err) {
        // best effort
      }
      command = "/bin/bash";
      args = [p];
    }

    return spawn(command, args, {
      detached: true,
      stdio: "ignore",
      windowsHide: true,
      cwd: path.dirname(p),
      env: { ...processRef.env, SEKAILINK_UPDATE_TRIGGER: "client" },
    });
  };

  const launchBootstrapperAndQuit = async () => {
    const bootstrapperPath = resolveBootstrapperExecutable();
    if (!bootstrapperPath) return { ok: false, error: "bootstrapper_not_found" };
    try {
      const child = spawnDetachedBootstrapper(bootstrapperPath);
      child.unref();
      setTimeout(() => app.quit(), 25);
      return { ok: true, path: bootstrapperPath };
    } catch (err) {
      return { ok: false, error: String(err || "bootstrapper_launch_failed") };
    }
  };

  return {
    getBootstrapLaunchTokenStatus,
    validateBootstrapLaunchToken,
    getBootstrapInstallState,
    resolveBootstrapperExecutable,
    getBootstrapperDownloadUrl,
    spawnDetachedBootstrapper,
    launchBootstrapperAndQuit,
  };
};

module.exports = { createBootstrapperControl };
