const createBootstrapperControl = ({
  app,
  crypto,
  fs,
  path,
  spawn,
  spawnSync,
  processRef = process,
  httpGetJson,
  downloadToDirWithProgress,
  extractZip,
  ensureDir = (dir) => fs.mkdirSync(dir, { recursive: true }),
  writeLogLine = () => {},
}) => {
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

  const normalizeReleaseChannel = (value) => {
    const raw = String(value || "").trim().toLowerCase();
    if (raw === "canary") return "canari";
    if (raw === "stable" || raw === "release" || raw === "test") return "canonical";
    return raw === "canari" ? "canari" : "canonical";
  };

  const getBootloaderStateDir = () => {
    if (processRef.platform === "win32") {
      const appdata = String(processRef.env.APPDATA || "").trim();
      const home = String(processRef.env.USERPROFILE || "").trim();
      return path.join(appdata || path.join(home || processRef.cwd(), "AppData", "Roaming"), "sekailink-bootloader");
    }
    const xdgState = String(processRef.env.XDG_STATE_HOME || "").trim();
    const home = String(processRef.env.HOME || "").trim();
    return path.join(xdgState || path.join(home || processRef.cwd(), ".local", "state"), "sekailink-bootloader");
  };

  const getReleaseChannelPreferencePath = () => path.join(getBootloaderStateDir(), "release-channel.json");
  const getBootstrapperUpdateStatePath = () => path.join(getBootloaderStateDir(), "bootstrapper-state.json");

  const getPreferredReleaseChannel = () => {
    const envChannel = String(processRef.env.SEKAILINK_RELEASE_CHANNEL || "").trim();
    if (envChannel) return normalizeReleaseChannel(envChannel);
    const parsed = readJsonFileSafe(getReleaseChannelPreferencePath());
    if (parsed) return normalizeReleaseChannel(parsed.channel);
    const state = getBootstrapInstallState();
    return normalizeReleaseChannel(state?.channel || "canonical");
  };

  const setPreferredReleaseChannel = (channel) => {
    const next = normalizeReleaseChannel(channel);
    const stateDir = getBootloaderStateDir();
    ensureDir(stateDir);
    fs.writeFileSync(getReleaseChannelPreferencePath(), JSON.stringify({
      channel: next,
      updated_at: new Date().toISOString(),
      reboot_required: true,
    }, null, 2), "utf8");
    writeLogLine("info", "bootstrapper", `release channel preference set channel=${next}`);
    return { ok: true, channel: next, rebootRequired: true, path: getReleaseChannelPreferencePath() };
  };

  const getPlatformId = () => processRef.platform === "win32" ? "win32-x64" : "linux-x64";

  const getBootstrapperManifestUrl = (channel = getPreferredReleaseChannel()) => {
    const base = "https://sekailink.com";
    const url = new URL("/api/client/bootstrapper-latest", base);
    url.searchParams.set("channel", normalizeReleaseChannel(channel));
    url.searchParams.set("platform", getPlatformId());
    url.searchParams.set("build", "release");
    return url.toString();
  };

  const getBootstrapperStaticManifestUrl = (channel = getPreferredReleaseChannel()) =>
    `https://sekailink.com/downloads/client/bootstrapper/${normalizeReleaseChannel(channel)}/latest/sekailink-bootstrapper-release-latest.json`;

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
    const channel = getPreferredReleaseChannel();
    if (processRef.platform === "win32") {
      return `https://sekailink.com/downloads/client/bootstrapper/${channel}/latest/SekaiLink-bootstrapper-windows.zip`;
    }
    return `https://sekailink.com/downloads/client/bootstrapper/${channel}/latest/SekaiLink-bootstrapper-linux.tar.gz`;
  };

  const normalizeBootstrapperArtifact = (manifest, channel) => {
    const artifacts = Array.isArray(manifest?.artifacts) ? manifest.artifacts : [];
    const platformNeedle = processRef.platform === "win32" ? "windows" : "linux";
    const artifact = artifacts.find((entry) => {
      const name = String(entry?.fileName || entry?.file_name || entry?.name || entry?.url || "").toLowerCase();
      return name.includes(platformNeedle);
    }) || artifacts[0];
    if (!artifact) return null;
    const downloadUrl = String(artifact.url || artifact.download_url || artifact.downloadUrl || "").trim();
    const sha256 = String(artifact.sha256 || "").trim().toLowerCase();
    if (!downloadUrl || !/^[a-f0-9]{64}$/.test(sha256)) return null;
    return {
      version: String(manifest?.version || "").trim(),
      channel: normalizeReleaseChannel(manifest?.channel || channel),
      downloadUrl,
      sha256,
      fileName: String(artifact.fileName || artifact.file_name || path.basename(new URL(downloadUrl).pathname) || "").trim(),
    };
  };

  const fetchBootstrapperLatest = async (channel = getPreferredReleaseChannel()) => {
    if (typeof httpGetJson !== "function") return { ok: false, error: "bootstrapper_manifest_fetch_unavailable" };
    const primaryUrl = getBootstrapperManifestUrl(channel);
    try {
      const manifest = await httpGetJson(primaryUrl, { accept: "application/json" });
      return { ok: true, source: "api", url: primaryUrl, manifest };
    } catch (err) {
      const fallbackUrl = getBootstrapperStaticManifestUrl(channel);
      try {
        const manifest = await httpGetJson(fallbackUrl, { accept: "application/json" });
        return { ok: true, source: "static", url: fallbackUrl, manifest, primaryError: String(err || "") };
      } catch (fallbackErr) {
        return { ok: false, error: String(fallbackErr || err || "bootstrapper_manifest_fetch_failed"), url: primaryUrl, fallbackUrl };
      }
    }
  };

  const copyExtractedBootloader = (stagingDir, installRoot) => {
    const wanted = processRef.platform === "win32" ? "SekaiLink Bootloader" : "SekaiLink Bootloader Linux";
    const direct = path.join(stagingDir, wanted);
    const source = fs.existsSync(direct)
      ? direct
      : (fs.readdirSync(stagingDir, { withFileTypes: true })
          .filter((entry) => entry.isDirectory())
          .map((entry) => path.join(stagingDir, entry.name))
          .find((candidate) => {
            const lower = path.basename(candidate).toLowerCase();
            return lower.includes("sekailink") && (lower.includes("bootloader") || lower.includes("bootstrapper"));
          }) || "");
    if (!source) return { ok: false, error: "bootstrapper_extract_missing_payload" };
    const target = path.join(installRoot, path.basename(source));
    fs.cpSync(source, target, { recursive: true, force: true });
    if (processRef.platform !== "win32") {
      const exe = path.join(target, "sekailink-bootloader");
      if (fs.existsSync(exe)) fs.chmodSync(exe, fs.statSync(exe).mode | 0o111);
    }
    return { ok: true, source, target };
  };

  const applyBootstrapperArchive = (archivePath, installRoot, stagingDir) => {
    const lower = path.basename(archivePath).toLowerCase();
    fs.rmSync(stagingDir, { recursive: true, force: true });
    ensureDir(stagingDir);
    if (lower.endsWith(".zip")) {
      if (typeof extractZip !== "function") return { ok: false, error: "zip_extract_unavailable" };
      extractZip(archivePath, stagingDir);
    } else if (lower.endsWith(".tar.gz") || lower.endsWith(".tgz")) {
      const res = spawnSync ? spawnSync("tar", ["-xzf", archivePath, "-C", stagingDir], { stdio: "ignore" }) : null;
      if (!res || res.status !== 0) return { ok: false, error: "tar_extract_failed" };
    } else {
      return { ok: false, error: "unsupported_bootstrapper_archive" };
    }
    return copyExtractedBootloader(stagingDir, installRoot);
  };

  const checkAndApplyBootstrapperUpdate = async (options = {}) => {
    if (!app.isPackaged && !options.force) {
      return { ok: true, updated: false, skipped: true, reason: "not_packaged" };
    }
    const channel = normalizeReleaseChannel(options.channel || getPreferredReleaseChannel());
    const latest = await fetchBootstrapperLatest(channel);
    if (!latest.ok) return latest;
    const artifact = normalizeBootstrapperArtifact(latest.manifest, channel);
    if (!artifact?.version) return { ok: false, error: "bootstrapper_manifest_incomplete", source: latest.source };
    const current = readJsonFileSafe(getBootstrapperUpdateStatePath()) || {};
    if (!options.force && String(current.version || "") === artifact.version && normalizeReleaseChannel(current.channel) === artifact.channel) {
      return { ok: true, updated: false, version: artifact.version, channel: artifact.channel, source: latest.source };
    }
    if (typeof downloadToDirWithProgress !== "function") return { ok: false, error: "bootstrapper_download_unavailable" };

    const installState = getBootstrapInstallState() || {};
    const installRoot = String(installState.installDir || processRef.env.SEKAILINK_BOOTSTRAP_INSTALL_DIR || path.dirname(processRef.execPath) || "").trim();
    if (!installRoot) return { ok: false, error: "bootstrapper_install_root_missing" };
    const workRoot = path.join(getBootloaderStateDir(), "bootstrapper-update-work");
    const downloadsDir = path.join(workRoot, "downloads");
    const stagingDir = path.join(workRoot, "extract");
    fs.rmSync(workRoot, { recursive: true, force: true });
    ensureDir(downloadsDir);
    writeLogLine("info", "bootstrapper", `checking bootstrapper update channel=${artifact.channel} version=${artifact.version}`);
    const downloaded = await downloadToDirWithProgress(artifact.downloadUrl, downloadsDir, {
      sha256: artifact.sha256,
      requireHash: true,
      defaultName: artifact.fileName || "sekailink-bootstrapper.bin",
    });
    const applied = applyBootstrapperArchive(downloaded.path, installRoot, stagingDir);
    if (!applied.ok) return applied;
    ensureDir(path.dirname(getBootstrapperUpdateStatePath()));
    fs.writeFileSync(getBootstrapperUpdateStatePath(), JSON.stringify({
      version: artifact.version,
      channel: artifact.channel,
      platform: getPlatformId(),
      source: latest.source,
      manifest_url: latest.url,
      artifact_url: artifact.downloadUrl,
      installed_at: new Date().toISOString(),
      target: applied.target,
    }, null, 2), "utf8");
    fs.rmSync(workRoot, { recursive: true, force: true });
    writeLogLine("info", "bootstrapper", `bootstrapper updated version=${artifact.version} channel=${artifact.channel} target=${applied.target}`);
    return { ok: true, updated: true, version: artifact.version, channel: artifact.channel, target: applied.target, source: latest.source };
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
    getPreferredReleaseChannel,
    setPreferredReleaseChannel,
    getBootstrapperManifestUrl,
    getBootstrapperStaticManifestUrl,
    fetchBootstrapperLatest,
    checkAndApplyBootstrapperUpdate,
    resolveBootstrapperExecutable,
    getBootstrapperDownloadUrl,
    spawnDetachedBootstrapper,
    launchBootstrapperAndQuit,
  };
};

module.exports = { createBootstrapperControl };
