"use strict";

const createClientUpdateRuntime = (deps = {}) => {
  const {
    app,
    fs,
    path,
    os,
    crypto,
    spawn,
    shell,
    processRef = process,
    normalizeIpcString,
    normalizeGameId,
    normalizeSha256,
    sanitizeFilename,
    downloadToDirWithProgress,
    hashFile,
    extractZip,
    httpGetJson,
    ensureDir,
    isWritablePath,
    getBootstrapInstallState,
    getClientUpdateDownloadsDir,
    getClientUpdateStagingDir,
    getRuntimeOverlayRoot,
    getRuntimeDownloadsDir,
    emitUpdaterEvent,
    emitSessionEvent,
    writeLogLine,
    nowIso,
    clearRuntimeCaches = () => {},
  } = deps;

  const activeUpdaterDownloads = new Map();
  const updaterDownloadResults = new Map();
  const updaterDownloadWaiters = new Map();

  const normalizeArtifactType = (value) => {
    return String(value || "")
      .trim()
      .toLowerCase()
      .replace(/_/g, "-");
  };
  
  const decodeSignature = (value) => {
    const raw = String(value || "").trim();
    if (!raw) return null;
    if (/^[a-f0-9]+$/i.test(raw) && raw.length % 2 === 0) {
      try {
        return Buffer.from(raw, "hex");
      } catch (_err) {
        return null;
      }
    }
    try {
      return Buffer.from(raw, "base64");
    } catch (_err) {
      return null;
    }
  };
  
  const verifyFileSignature = (filePath, signatureValue, publicKeyPem) => {
    try {
      const sig = decodeSignature(signatureValue);
      const pub = String(publicKeyPem || "").trim();
      if (!sig || !pub) return { ok: false, error: "missing_signature_material" };
      const payload = fs.readFileSync(filePath);
      const valid = crypto.verify(null, payload, pub, sig);
      return valid ? { ok: true } : { ok: false, error: "signature_invalid" };
    } catch (err) {
      return { ok: false, error: String(err || "signature_verify_failed") };
    }
  };
  
  const settleUpdaterDownload = (downloadId, result) => {
    if (!downloadId) return;
    const payload = result && typeof result === "object" ? result : { ok: false, error: "download_failed" };
    updaterDownloadResults.set(downloadId, payload);
    const waiters = updaterDownloadWaiters.get(downloadId);
    if (Array.isArray(waiters) && waiters.length) {
      for (const waiter of waiters) {
        try {
          waiter(payload);
        } catch (_err) {
          // ignore waiter failures
        }
      }
    }
    updaterDownloadWaiters.delete(downloadId);
  };
  
  const waitForUpdaterDownload = async (downloadId, timeoutMs = 0) => {
    const id = String(downloadId || "").trim();
    if (!id) return { ok: false, error: "missing_download_id" };
    if (updaterDownloadResults.has(id)) {
      const done = updaterDownloadResults.get(id);
      updaterDownloadResults.delete(id);
      return done;
    }
    return new Promise((resolve) => {
      let timer = null;
      const done = (value) => {
        try {
          clearTimeout(timer);
        } catch (_err) {
          // ignore
        }
        updaterDownloadResults.delete(id);
        resolve(value && typeof value === "object" ? value : { ok: false, error: "download_failed" });
      };
  
      const list = updaterDownloadWaiters.get(id) || [];
      list.push(done);
      updaterDownloadWaiters.set(id, list);
  
      if (timeoutMs > 0) {
        timer = setTimeout(() => {
          const waiters = updaterDownloadWaiters.get(id) || [];
          updaterDownloadWaiters.set(id, waiters.filter((fn) => fn !== done));
          done({ ok: false, error: "download_timeout" });
        }, timeoutMs);
        if (typeof timer.unref === "function") timer.unref();
      }
    });
  };
  
  const startClientUpdaterDownload = async (options) => {
    const downloadUrl = String(options?.downloadUrl || options?.url || "").trim();
    if (!downloadUrl) return { ok: false, error: "missing_download_url" };
    let parsedUrl = null;
    try {
      parsedUrl = new URL(downloadUrl);
    } catch (_err) {
      return { ok: false, error: "invalid_download_url" };
    }
    if (!/^https?:$/i.test(parsedUrl.protocol)) {
      return { ok: false, error: "unsupported_download_protocol" };
    }
    if (activeUpdaterDownloads.size > 0) {
      return { ok: false, error: "download_in_progress" };
    }
  
    const expectedSha256 = normalizeSha256(options?.sha256);
    if (!expectedSha256) {
      return { ok: false, error: "missing_expected_hash" };
    }
    const expectedSignature = normalizeIpcString(options?.signature, 4096);
    const publicKeyPem = normalizeIpcString(
      options?.publicKey || process.env.SEKAILINK_UPDATE_PUBLIC_KEY || "",
      8192
    );
    if (expectedSignature && !publicKeyPem) {
      return { ok: false, error: "missing_signature_public_key" };
    }
    const hintedVersion = sanitizeFilename(String(options?.version || options?.latest || "").trim());
    const pathExt = sanitizeFilename(path.extname(parsedUrl.pathname || ""));
    const defaultBasename = hintedVersion ? `SekaiLink-client-${hintedVersion}${pathExt || ""}` : "SekaiLink-client-update";
    const authToken = String(options?.authToken || "").trim();
    const downloadId = `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;
    const destDir = getClientUpdateDownloadsDir();
    const headers = {
      "User-Agent": "SekaiLink-Updater/1.0",
      "Accept": "application/zip,application/octet-stream,application/x-msdownload,application/x-iso9660-image,*/*",
    };
    if (authToken) headers.Authorization = `Bearer ${authToken}`;
  
    activeUpdaterDownloads.set(downloadId, { startedAt: Date.now() });
    emitUpdaterEvent({
      event: "download-started",
      downloadId,
      version: hintedVersion || "",
      status: "Downloading update...",
    });
  
    (async () => {
      try {
        const downloaded = await downloadToDirWithProgress(downloadUrl, destDir, {
          defaultBasename,
          headers,
          onProgress: ({ receivedBytes, totalBytes, percent }) => {
            emitUpdaterEvent({
              event: "download-progress",
              downloadId,
              receivedBytes,
              totalBytes,
              percent,
            });
          },
        });
  
        const actualSha256 = String(await hashFile(downloaded.path, "sha256")).toLowerCase();
        if (actualSha256 !== expectedSha256) {
          try {
            fs.rmSync(downloaded.path, { force: true });
          } catch (_err) {
            // ignore
          }
          throw new Error("checksum_mismatch");
        }
  
        if (expectedSignature) {
          const verified = verifyFileSignature(downloaded.path, expectedSignature, publicKeyPem);
          if (!verified.ok) {
            try {
              fs.rmSync(downloaded.path, { force: true });
            } catch (_err) {
              // ignore
            }
            throw new Error(verified.error || "signature_invalid");
          }
        }
  
        emitUpdaterEvent({
          event: "download-complete",
          downloadId,
          path: downloaded.path,
          filename: downloaded.filename,
          totalBytes: downloaded.totalBytes || 0,
          receivedBytes: downloaded.bytes || 0,
          sha256: actualSha256 || expectedSha256 || "",
        });
        settleUpdaterDownload(downloadId, {
          ok: true,
          downloadId,
          path: downloaded.path,
          filename: downloaded.filename,
          totalBytes: downloaded.totalBytes || 0,
          receivedBytes: downloaded.bytes || 0,
          sha256: actualSha256 || expectedSha256 || "",
        });
      } catch (err) {
        const message = String(err || "download_failed");
        emitUpdaterEvent({
          event: "download-error",
          downloadId,
          error: message,
        });
        settleUpdaterDownload(downloadId, { ok: false, downloadId, error: message });
      } finally {
        activeUpdaterDownloads.delete(downloadId);
      }
    })();
  
    return { ok: true, downloadId };
  };
  
  const runDetachedSelfUpdateAndQuit = async (options = {}) => {
    const sourcePath = String(options.sourcePath || "").trim();
    const targetPath = String(options.targetPath || "").trim();
    if (!sourcePath || !targetPath) return { ok: false, error: "missing_paths" };
    if (!fs.existsSync(sourcePath)) return { ok: false, error: "source_missing" };
  
    const samePath = path.resolve(sourcePath) === path.resolve(targetPath);
    if (samePath) {
      try {
        app.relaunch();
        setTimeout(() => app.quit(), 25);
        return { ok: true, method: "relaunch" };
      } catch (err) {
        return { ok: false, error: String(err || "relaunch_failed") };
      }
    }
  
    try {
      fs.accessSync(path.dirname(targetPath), fs.constants.W_OK);
    } catch (_err) {
      return { ok: false, error: "target_not_writable" };
    }
  
    if (process.platform === "win32") {
      try {
        // Windows builds are shipped as NSIS installers. Apply update silently to avoid user interaction,
        // then quit so NSIS can replace the installed app files.
        const installDir = path.dirname(targetPath);
        const quotePs = (value) => String(value || "").replace(/'/g, "''");
        // Delay the installer start to avoid "app is still running" detection and file locks.
        // Force /D to keep upgrades in-place (otherwise some NSIS configs can re-install elsewhere).
        const script = [
          `$installer='${quotePs(sourcePath)}'`,
          `$exe='${quotePs(targetPath)}'`,
          `$dir='${quotePs(installDir)}'`,
          "Start-Sleep -Milliseconds 800",
          "try {",
          "  Start-Process -FilePath $installer -ArgumentList @('/S', ('/D=' + $dir)) -Wait | Out-Null",
          "} catch {",
          "  exit 1",
          "}",
          "Start-Sleep -Milliseconds 250",
          "$launcher=Join-Path $dir 'SekaiLink-bootstrapper.cmd'",
          "try {",
          "  if (Test-Path -LiteralPath $launcher) {",
          "    Start-Process -FilePath $launcher -WorkingDirectory $dir | Out-Null",
          "  } else {",
          "    Start-Process -FilePath $exe -WorkingDirectory $dir | Out-Null",
          "  }",
          "} catch { }",
        ].join("; ");
        const proc = spawn("powershell.exe", ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script], {
          detached: true,
          stdio: "ignore",
          windowsHide: true,
        });
        proc.unref();
        setTimeout(() => app.quit(), 50);
        return { ok: true, method: "nsis-silent", installerPath: sourcePath, installDir };
      } catch (err) {
        return { ok: false, error: String(err || "nsis_silent_failed") };
      }
    }
  
    if (process.platform === "linux") {
      try {
        const script = [
          "src=\"$1\"",
          "dst=\"$2\"",
          "tmp=\"${dst}.new\"",
          "bak=\"${dst}.bak\"",
          "for i in $(seq 1 120); do",
          "  cp -f \"$src\" \"$tmp\" 2>/dev/null || { sleep 0.25; continue; }",
          "  chmod +x \"$tmp\" 2>/dev/null || { sleep 0.25; continue; }",
          "  if [ -f \"$dst\" ]; then cp -f \"$dst\" \"$bak\" 2>/dev/null || true; fi",
          "  if mv -f \"$tmp\" \"$dst\" 2>/dev/null; then",
          "    nohup \"$dst\" >/dev/null 2>&1 &",
          "    launch_rc=$?",
          "    if [ \"$launch_rc\" -eq 0 ]; then exit 0; fi",
          "    if [ -f \"$bak\" ]; then mv -f \"$bak\" \"$dst\" 2>/dev/null || true; fi",
          "  fi",
          "  sleep 0.25",
          "done",
          "exit 1",
        ].join("\n");
        const proc = spawn("/bin/bash", ["-lc", script, "--", sourcePath, targetPath], {
          detached: true,
          stdio: "ignore",
        });
        proc.unref();
        setTimeout(() => app.quit(), 50);
        return { ok: true, method: "self-replace", targetPath };
      } catch (err) {
        return { ok: false, error: String(err || "self_replace_failed") };
      }
    }
  
    return { ok: false, error: "platform_not_supported" };
  };
  
  const getRunningClientBinaryPath = () => {
    if (process.platform === "linux") {
      const appImage = String(process.env.APPIMAGE || "").trim();
      if (appImage && fs.existsSync(appImage)) return appImage;
    }
    return String(process.execPath || "").trim();
  };
  
  const getClientBundleInstallDir = () => {
    const installState = getBootstrapInstallState() || {};
    const explicitInstallDir = String(installState.installDir || process.env.SEKAILINK_BOOTSTRAP_INSTALL_DIR || "").trim();
    if (explicitInstallDir && fs.existsSync(explicitInstallDir)) return explicitInstallDir;
    if (process.platform === "linux" && String(process.env.APPIMAGE || "").trim()) return "";
    const exePath = String(process.execPath || "").trim();
    return exePath ? path.dirname(exePath) : "";
  };
  
  const findClientBundleExecutable = (bundleRoot) => {
    const root = String(bundleRoot || "").trim();
    if (!root || !fs.existsSync(root)) return "";
    const candidates = process.platform === "win32"
      ? ["SekaiLink Client.exe", "SekaiLink-client.exe", "sekailink-client.exe"]
      : ["sekailink-client", "SekaiLink Client", "SekaiLink-client"];
    for (const candidate of candidates) {
      const full = path.join(root, candidate);
      if (fs.existsSync(full)) return candidate;
    }
    try {
      const entries = fs.readdirSync(root, { withFileTypes: true });
      for (const entry of entries) {
        if (!entry.isFile()) continue;
        const lower = entry.name.toLowerCase();
        if (process.platform === "win32") {
          if (lower.endsWith(".exe") && lower.includes("sekailink")) return entry.name;
        } else if (lower.includes("sekailink")) {
          return entry.name;
        }
      }
    } catch (_err) {
      // ignore
    }
    return "";
  };
  
  const resolveClientBundleRoot = (extractDir) => {
    let root = String(extractDir || "").trim();
    if (!root || !fs.existsSync(root)) return "";
    try {
      const entries = fs.readdirSync(root, { withFileTypes: true }).filter((entry) => !entry.name.startsWith("__MACOSX"));
      if (entries.length === 1 && entries[0].isDirectory()) {
        root = path.join(root, entries[0].name);
      }
    } catch (_err) {
      return "";
    }
    return root;
  };
  
  const validateClientBundleRoot = (bundleRoot) => {
    const root = String(bundleRoot || "").trim();
    if (!root || !fs.existsSync(root)) return { ok: false, error: "bundle_root_missing" };
    if (!fs.existsSync(path.join(root, "resources", "app.asar"))) return { ok: false, error: "bundle_app_missing" };
    const executableRel = findClientBundleExecutable(root);
    if (!executableRel) return { ok: false, error: "bundle_executable_missing" };
    return { ok: true, executableRel };
  };
  
  const quoteShSingle = (value) => `'${String(value || "").replace(/'/g, "'\\''")}'`;
  
  const runDetachedBundleSwapAndQuit = async ({ bundleRoot, installDir, executableRel, statePath }) => {
    const src = String(bundleRoot || "").trim();
    const dst = String(installDir || "").trim();
    const exeRel = String(executableRel || "").trim();
    const pendingState = String(statePath || "").trim();
    if (!src || !dst || !exeRel) return { ok: false, error: "missing_bundle_swap_args" };
    if (!fs.existsSync(src)) return { ok: false, error: "bundle_root_missing" };
    if (!fs.existsSync(dst)) return { ok: false, error: "install_dir_missing" };
    if (pendingState && !fs.existsSync(pendingState)) return { ok: false, error: "install_state_missing" };
    if (!isWritablePath(dst)) return { ok: false, error: "install_dir_not_writable" };
  
    if (process.platform === "win32") {
      const esc = (value) => String(value || "").replace(/'/g, "''");
      const script = [
        `$src='${esc(src)}'`,
        `$dst='${esc(dst)}'`,
        `$exeRel='${esc(exeRel)}'`,
        `$state='${esc(pendingState)}'`,
        "Start-Sleep -Milliseconds 900",
        "for ($i = 0; $i -lt 120; $i++) {",
        "  try {",
        "    Get-ChildItem -LiteralPath $dst -Force | Where-Object { $_.Name -ne '.sekailink' } | Remove-Item -Recurse -Force -ErrorAction Stop",
        "    Copy-Item -Path (Join-Path $src '*') -Destination $dst -Recurse -Force -ErrorAction Stop",
        "    if ($state -and (Test-Path -LiteralPath $state)) {",
        "      $stateDir = Join-Path $dst '.sekailink'",
        "      New-Item -ItemType Directory -Path $stateDir -Force | Out-Null",
        "      Copy-Item -LiteralPath $state -Destination (Join-Path $stateDir 'install-state.json') -Force -ErrorAction Stop",
        "    }",
        "    $exe = Join-Path $dst $exeRel",
        "    if (-not (Test-Path -LiteralPath $exe)) { throw 'updated_executable_missing' }",
        "    $launcher = Join-Path $dst 'SekaiLink-bootstrapper.cmd'",
        "    if (Test-Path -LiteralPath $launcher) {",
        "      Start-Process -FilePath $launcher -WorkingDirectory $dst | Out-Null",
        "    } else {",
        "      Start-Process -FilePath $exe -WorkingDirectory $dst | Out-Null",
        "    }",
        "    exit 0",
        "  } catch {",
        "    Start-Sleep -Milliseconds 250",
        "  }",
        "}",
        "exit 1",
      ].join("; ");
      const proc = spawn("powershell.exe", ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", script], {
        detached: true,
        stdio: "ignore",
        windowsHide: true,
      });
      proc.unref();
      setTimeout(() => app.quit(), 50);
      return { ok: true, method: "client-bundle", installDir: dst };
    }
  
    if (process.platform === "linux") {
      const script = [
        `src=${quoteShSingle(src)}`,
        `dst=${quoteShSingle(dst)}`,
        `exe_rel=${quoteShSingle(exeRel)}`,
        `state=${quoteShSingle(pendingState)}`,
        "sleep 0.9",
        "for i in $(seq 1 120); do",
        "  if find \"$dst\" -mindepth 1 -maxdepth 1 ! -name .sekailink -exec rm -rf {} + 2>/dev/null && cp -a \"$src\"/. \"$dst\"/ 2>/dev/null; then",
        "    if [ -n \"$state\" ] && [ -f \"$state\" ]; then mkdir -p \"$dst/.sekailink\" && cp -f \"$state\" \"$dst/.sekailink/install-state.json\" 2>/dev/null || exit 1; fi",
        "    chmod +x \"$dst/$exe_rel\" 2>/dev/null || true",
        "    if [ -x \"$dst/sekailink-bootstrapper.sh\" ]; then",
        "      nohup \"$dst/sekailink-bootstrapper.sh\" >/dev/null 2>&1 &",
        "    else",
        "      nohup \"$dst/$exe_rel\" >/dev/null 2>&1 &",
        "    fi",
        "    exit 0",
        "  fi",
        "  sleep 0.25",
        "done",
        "exit 1",
      ].join("\n");
      const proc = spawn("/bin/bash", ["-lc", script], {
        detached: true,
        stdio: "ignore",
      });
      proc.unref();
      setTimeout(() => app.quit(), 50);
      return { ok: true, method: "client-bundle", installDir: dst };
    }
  
    return { ok: false, error: "platform_not_supported" };
  };
  
  const applyClientBundleUpdate = async (zipPath, options = {}) => {
    const p = String(zipPath || "").trim();
    if (!p || !fs.existsSync(p)) return { ok: false, error: "file_missing" };
    const installDir = getClientBundleInstallDir();
    if (!installDir) return { ok: false, error: "bundle_install_dir_unavailable" };
    const versionHint = sanitizeFilename(String(options?.version || options?.latest || Date.now()).trim()) || String(Date.now());
    const stagingRoot = path.join(getClientUpdateStagingDir(), `client-bundle-${versionHint}-${Date.now()}`);
    const extractDir = path.join(stagingRoot, "extract");
    try {
      emitUpdaterEvent({ event: "install-started", artifactType: "client-bundle", status: "Installing update..." });
      extractZip(p, extractDir);
      const bundleRoot = resolveClientBundleRoot(extractDir);
      const validated = validateClientBundleRoot(bundleRoot);
      if (!validated.ok) return validated;
      const installStatePath = path.join(stagingRoot, "install-state.json");
      const nextVersion = String(options?.version || options?.latest || "").trim();
      const nextChannel = String(options?.channel || "").trim().toLowerCase();
      const nextBuild = String(options?.build || "").trim().toLowerCase();
      fs.writeFileSync(installStatePath, JSON.stringify({
        version: nextVersion,
        manifestVersion: nextVersion,
        channel: nextChannel,
        build: nextBuild,
        platform: process.platform,
        arch: process.arch,
        artifactType: "client-bundle",
        installDir,
        updatedAt: nowIso(),
      }, null, 2));
      const applied = await runDetachedBundleSwapAndQuit({
        bundleRoot,
        installDir,
        executableRel: validated.executableRel,
        statePath: installStatePath,
      });
      if (applied?.ok) {
        emitUpdaterEvent({ event: "update-restarting", artifactType: "client-bundle", status: "Restarting..." });
        writeLogLine("info", "updater", `client bundle update scheduled installDir=${installDir}`);
        return { ok: true, ...applied };
      }
      return { ok: false, error: String(applied?.error || "bundle_apply_failed") };
    } catch (err) {
      writeLogLine("warn", "updater", `client bundle update failed: ${String(err || "")}`);
      return { ok: false, error: String(err || "bundle_apply_failed") };
    }
  };
  
  const openDownloadedUpdaterFile = async (targetPath, options = {}) => {
    const p = String(targetPath || "").trim();
    if (!p) return { ok: false, error: "missing_path" };
    if (!fs.existsSync(p)) return { ok: false, error: "file_missing" };
    const fileNameLower = path.basename(p).toLowerCase();
    const ext = path.extname(p).toLowerCase();
    const artifactType = normalizeArtifactType(options?.artifactType || options?.artifact_type || "");
    const isClientBundleZip = ext === ".zip" && artifactType === "client-bundle";
    const isLinuxAppImage = process.platform === "linux" && (ext === ".appimage" || fileNameLower.includes(".appimage"));
    const isWindowsExe = process.platform === "win32" && ext === ".exe";
  
    if (isClientBundleZip) {
      return applyClientBundleUpdate(p, options);
    }
  
    if (isLinuxAppImage || isWindowsExe) {
      try {
        if (isLinuxAppImage) {
          // Downloads are often non-executable by default; AppImage must be executable.
          const stat = fs.statSync(p);
          const nextMode = stat.mode | 0o111;
          if ((stat.mode & 0o111) !== 0o111) fs.chmodSync(p, nextMode);
        }
        const runningBinary = getRunningClientBinaryPath();
        if (!runningBinary) return { ok: false, error: "current_binary_missing" };
        const applied = await runDetachedSelfUpdateAndQuit({
          sourcePath: p,
          targetPath: runningBinary,
        });
        if (applied?.ok) {
          writeLogLine("info", "updater", `self-update scheduled target=${runningBinary}`);
          return { ok: true, ...applied };
        }
        writeLogLine("warn", "updater", `self-update failed: ${String(applied?.error || "")}`);
        return { ok: false, error: String(applied?.error || "self_update_failed") };
      } catch (err) {
        writeLogLine("warn", "updater", `self-update prep failed: ${String(err || "")}`);
        return { ok: false, error: String(err || "self_update_failed") };
      }
    }
    try {
      const err = await shell.openPath(p);
      if (err) return { ok: false, error: err };
      return { ok: true };
    } catch (error) {
      return { ok: false, error: String(error) };
    }
  };
  
  const downloadAndApplyClientUpdate = async (options = {}) => {
    // Safety guard: if the UI ends up with a stale embedded version string,
    // it might try to "update" to the version we are already running, causing a loop.
    try {
      const targetVersion = String(options?.version || "").trim();
      const runningVersion = app.getVersion ? String(app.getVersion() || "").trim() : "";
      if (targetVersion && runningVersion && targetVersion === runningVersion) {
        writeLogLine("info", "updater", `skip apply: already running version=${runningVersion}`);
        return { ok: false, error: "already_latest", version: runningVersion };
      }
    } catch (_err) {
      // ignore
    }
  
    const runOne = async (updateOptions = {}) => {
      const started = await startClientUpdaterDownload(updateOptions || {});
      if (!started?.ok) return started;
      const timeoutMs = Number(updateOptions?.timeoutMs || 25 * 60 * 1000);
      const completed = await waitForUpdaterDownload(started.downloadId, Number.isFinite(timeoutMs) ? timeoutMs : 0);
      if (!completed?.ok) {
        return { ok: false, error: completed?.error || "download_failed", downloadId: started.downloadId };
      }
      const apply = await openDownloadedUpdaterFile(completed.path, updateOptions || {});
      if (!apply?.ok) {
        return { ok: false, error: apply?.error || "apply_failed", downloadId: started.downloadId, path: completed.path };
      }
      return { ok: true, downloadId: started.downloadId, path: completed.path, ...apply };
    };
  
    const primary = await runOne(options || {});
    if (primary?.ok) return primary;
  
    const fallbackDownloadUrl = String(options?.fallbackDownloadUrl || options?.fallback_download_url || "").trim();
    const fallbackSha256 = normalizeSha256(options?.fallbackSha256 || options?.fallback_sha256 || "");
    if (!fallbackDownloadUrl || !fallbackSha256) return primary;
  
    emitUpdaterEvent({
      event: "download-fallback",
      error: String(primary?.error || "primary_update_failed"),
      status: "Trying fallback update...",
    });
  
    const fallbackOptions = {
      ...options,
      downloadUrl: fallbackDownloadUrl,
      url: fallbackDownloadUrl,
      sha256: fallbackSha256,
      signature: options?.fallbackSignature || options?.fallback_signature || "",
      publicKey: options?.fallbackPublicKey || options?.fallback_public_key || options?.publicKey || "",
      artifactType: options?.fallbackArtifactType || options?.fallback_artifact_type || "",
      artifact_type: options?.fallbackArtifactType || options?.fallback_artifact_type || "",
    };
    return runOne(fallbackOptions);
  };
  
  const normalizeSyncStateKey = (value) => {
    const raw = String(value || "").trim().toLowerCase();
    if (!raw) return "client";
    const safe = raw.replace(/[^a-z0-9._-]+/g, "-").replace(/^-+|-+$/g, "");
    return safe || "client";
  };
  
  const getIncrementalSyncStatePath = (stateKey = "client") => {
    const key = normalizeSyncStateKey(stateKey);
    if (key === "client") return path.join(getRuntimeOverlayRoot(), "client-sync-state.json");
    return path.join(getRuntimeOverlayRoot(), `client-sync-state-${key}.json`);
  };
  
  const loadIncrementalSyncState = (stateKey = "client") => {
    const statePath = getIncrementalSyncStatePath(stateKey);
    try {
      if (!fs.existsSync(statePath)) return { files: {} };
      const parsed = JSON.parse(fs.readFileSync(statePath, "utf-8"));
      if (parsed && typeof parsed === "object" && parsed.files && typeof parsed.files === "object") {
        return parsed;
      }
    } catch (_err) {
      // ignore malformed state
    }
    return { files: {} };
  };
  
  const saveIncrementalSyncState = (state, stateKey = "client") => {
    const statePath = getIncrementalSyncStatePath(stateKey);
    ensureDir(path.dirname(statePath));
    fs.writeFileSync(statePath, JSON.stringify(state, null, 2), "utf-8");
  };
  
  const normalizeClientRelativePath = (value) => {
    const raw = String(value || "").replace(/\\/g, "/").trim();
    if (!raw) return "";
    const noLead = raw.replace(/^\/+/, "");
    if (!noLead) return "";
    if (noLead.includes("..")) return "";
    if (noLead.startsWith("./")) return "";
    return noLead;
  };
  
  const joinUrlPath = (baseUrl, relativePath) => {
    try {
      return new URL(relativePath, baseUrl).toString();
    } catch (_err) {
      return "";
    }
  };
  
  const isIncrementalSyncPathAllowed = (relativePath) => {
    const rel = normalizeClientRelativePath(relativePath);
    if (!rel) return false;
    return rel.startsWith("runtime/") || rel.startsWith("ap/");
  };
  
  const getClientServerBaseUrl = () => {
    const state = getBootstrapInstallState() || {};
    const fromState = String(state?.serverUrl || "").trim();
    if (fromState) return fromState;
    const fromEnv = String(process.env.SEKAILINK_SERVER_URL || "").trim();
    if (fromEnv) return fromEnv;
    return "https://sekailink.com";
  };
  
  const getClientPlatformKey = () => {
    if (process.platform === "win32") return "windows";
    if (process.platform === "darwin") return "macos";
    return "linux";
  };
  
  const getClientReleaseTrack = () => {
    const state = getBootstrapInstallState() || {};
    const channel = String(state?.channel || "stable").trim().toLowerCase() || "stable";
    const build = String(state?.build || state?.buildType || "release").trim().toLowerCase() || "release";
    return { channel, build };
  };
  
  const getRuntimePackApiUrl = (params = {}) => {
    const base = getClientServerBaseUrl();
    const url = new URL("/api/client/runtime-pack-manifest", base);
    url.searchParams.set("platform", getClientPlatformKey());
    const track = getClientReleaseTrack();
    url.searchParams.set("channel", track.channel);
    url.searchParams.set("build", track.build);
    if (params.moduleId) url.searchParams.set("module_id", String(params.moduleId));
    if (params.gameId) url.searchParams.set("game_id", String(params.gameId));
    return url.toString();
  };
  
  const getRuntimePackStaticIndexUrl = () => {
    return new URL("/runtime-packs/index.json", getClientServerBaseUrl()).toString();
  };
  
  const runtimePackMatchesModule = (pack, moduleId, gameId = "") => {
    const safeModuleId = normalizeGameId(moduleId || "");
    const safeGameId = normalizeGameId(gameId || "");
    const packModuleId = normalizeGameId(pack?.moduleId || pack?.module_id || "");
    const packGameId = normalizeGameId(pack?.gameId || pack?.game_id || "");
    if (packModuleId && packModuleId !== safeModuleId) return false;
    if (safeGameId && packGameId && packGameId !== safeGameId) return false;
    return true;
  };
  
  const normalizeRuntimePackEntry = (raw, apiBaseUrl = "") => {
    const row = raw && typeof raw === "object" ? raw : {};
    const id = normalizeGameId(row.id || row.pack_id || row.key || "");
    const required = row.required !== false;
    const stateKey = normalizeSyncStateKey(row.state_key || row.stateKey || id || "pack");
    let manifestUrl = String(row.manifest_url || row.manifestUrl || row.url || "").trim();
    if (manifestUrl && !/^https?:\/\//i.test(manifestUrl)) {
      manifestUrl = joinUrlPath(apiBaseUrl || getClientServerBaseUrl(), manifestUrl);
    }
    if (!id || !manifestUrl) return null;
    return {
      id,
      kind: normalizeGameId(row.kind || row.type || "runtime-pack") || "runtime-pack",
      moduleId: normalizeGameId(row.module_id || row.moduleId || ""),
      gameId: normalizeGameId(row.game_id || row.gameId || ""),
      required,
      manifestUrl,
      stateKey,
      name: String(row.name || row.label || id),
    };
  };
  
  const parseRuntimePackPayload = (payload, baseUrl, moduleId, gameId = "") => {
    const packsRaw = Array.isArray(payload?.packs) ? payload.packs : [];
    const packs = [];
    for (const raw of packsRaw) {
      if (!runtimePackMatchesModule(raw, moduleId, gameId)) continue;
      const normalized = normalizeRuntimePackEntry(raw, baseUrl);
      if (normalized) packs.push(normalized);
    }
    return packs;
  };
  
  const fetchRuntimePacksFromStaticIndex = async (moduleId, gameId = "") => {
    const indexUrl = getRuntimePackStaticIndexUrl();
    const payload = await httpGetJson(indexUrl, { accept: "application/json" });
    const packs = parseRuntimePackPayload(payload, indexUrl, moduleId, gameId);
    return { apiUrl: indexUrl, packs, source: "static-index" };
  };
  
  const fetchRuntimePacksForModule = async (moduleId, gameId = "") => {
    const apiUrl = getRuntimePackApiUrl({ moduleId, gameId });
    try {
      const payload = await httpGetJson(apiUrl, { accept: "application/json" });
      const packs = parseRuntimePackPayload(payload, apiUrl, moduleId, gameId);
      return { apiUrl, packs, source: "client-api" };
    } catch (err) {
      const detail = String(err || "");
      if (!/HTTP\s+404\b/.test(detail)) throw err;
      writeLogLine("info", "runtime-pack", `client API pack manifest missing; trying static index module=${normalizeGameId(moduleId || "")}`);
      return fetchRuntimePacksFromStaticIndex(moduleId, gameId);
    }
  };
  
  const ensureRuntimePacksForModule = async ({ moduleId, gameId = "" } = {}) => {
    const safeModuleId = normalizeGameId(moduleId || "");
    if (!safeModuleId) return { ok: true, packs: [], applied: 0, skipped: 0, failed: 0 };
    let packs = [];
    try {
      const fetched = await fetchRuntimePacksForModule(safeModuleId, gameId);
      packs = Array.isArray(fetched?.packs) ? fetched.packs : [];
    } catch (err) {
      const detail = String(err || "");
      if (/HTTP\s+404\b/.test(detail)) {
        writeLogLine("info", "runtime-pack", `no remote runtime packs module=${safeModuleId}`);
        return { ok: true, packs: [], applied: 0, skipped: 0, failed: 0 };
      }
      writeLogLine("warn", "runtime-pack", `manifest fetch failed module=${safeModuleId} detail=${detail}`);
      // Non-blocking if no pack metadata is available.
      return { ok: true, packs: [], applied: 0, skipped: 0, failed: 0, warning: "pack_manifest_unavailable" };
    }
    if (!packs.length) return { ok: true, packs: [], applied: 0, skipped: 0, failed: 0 };
  
    let applied = 0;
    let skipped = 0;
    let failed = 0;
  
    for (const pack of packs) {
      const isTrackerPack = String(pack.kind || "").includes("tracker");
      const checkingStatus = isTrackerPack
        ? `Checking tracker pack: ${pack.name}...`
        : `Checking runtime pack: ${pack.name}...`;
      const downloadingStatus = isTrackerPack
        ? `Downloading tracker pack: ${pack.name}...`
        : `Downloading runtime pack: ${pack.name}...`;
      emitSessionEvent({
        event: "status",
        status: checkingStatus,
        moduleId: safeModuleId,
        packId: pack.id,
      });
      const stateKey = pack.stateKey || `pack-${pack.id}`;
      const syncRes = await syncIncrementalClientFiles({
        manifestUrl: pack.manifestUrl,
        stateKey,
        pruneMissing: true,
        downloadStatus: downloadingStatus,
        downloadStatusContext: {
          moduleId: safeModuleId,
          packId: pack.id,
          packKind: pack.kind,
        },
      });
      if (syncRes?.ok) {
        applied += 1;
        continue;
      }
      failed += 1;
      writeLogLine(
        pack.required ? "error" : "warn",
        "runtime-pack",
        `pack sync failed module=${safeModuleId} pack=${pack.id} required=${pack.required} error=${String(syncRes?.error || "")}`
      );
      if (pack.required) {
        return {
          ok: false,
          error: "runtime_pack_sync_failed",
          moduleId: safeModuleId,
          packId: pack.id,
          detail: String(syncRes?.error || ""),
          packs,
          applied,
          skipped,
          failed,
        };
      }
      skipped += 1;
    }
  
    return { ok: true, packs, applied, skipped, failed };
  };
  
  const syncIncrementalClientFiles = async (options) => {
    const manifestUrl = String(options?.manifestUrl || options?.url || "").trim();
    if (!manifestUrl) return { ok: false, error: "missing_manifest_url" };
    let parsedManifestUrl = null;
    try {
      parsedManifestUrl = new URL(manifestUrl);
    } catch (_err) {
      return { ok: false, error: "invalid_manifest_url" };
    }
    if (!/^https?:$/i.test(parsedManifestUrl.protocol)) {
      return { ok: false, error: "unsupported_manifest_protocol" };
    }
    const stateKey = normalizeSyncStateKey(options?.stateKey || "client");
    const syncKey = `incremental-sync:${stateKey}`;
    if (activeUpdaterDownloads.has(syncKey)) {
      return { ok: false, error: "sync_in_progress" };
    }
  
    const authToken = String(options?.authToken || "").trim();
    const manifestHeaders = {
      "User-Agent": "SekaiLink-IncrementalUpdater/1.0",
      "Accept": "application/json",
    };
    const fileHeaders = {
      "User-Agent": "SekaiLink-IncrementalUpdater/1.0",
      "Accept": "application/octet-stream,*/*",
    };
    if (authToken) {
      manifestHeaders.Authorization = `Bearer ${authToken}`;
      fileHeaders.Authorization = `Bearer ${authToken}`;
    }
  
    activeUpdaterDownloads.set(syncKey, { startedAt: Date.now() });
    emitUpdaterEvent({
      event: "incremental-sync-started",
      manifestUrl,
      stateKey,
    });
  
    try {
      const manifest = await httpGetJson(manifestUrl, { headers: manifestHeaders, accept: "application/json" });
      const files = Array.isArray(manifest?.files) ? manifest.files : [];
      const baseUrl = String(manifest?.base_url || "").trim() || manifestUrl;
      const manifestVersion = String(manifest?.version || "").trim();
      const overlayRoot = getRuntimeOverlayRoot();
      ensureDir(overlayRoot);
  
      const prevState = loadIncrementalSyncState(stateKey);
      const nextState = {
        version: manifestVersion,
        updatedAt: nowIso(),
        files: {},
      };
      const pruneMissing = options?.pruneMissing !== false && manifest?.prune !== false;
  
      let processed = 0;
      let changed = 0;
      let deleted = 0;
      let downloadedBytes = 0;
      let downloadStatusEmitted = false;
      const total = files.length;
  
      for (const rawEntry of files) {
        const entry = rawEntry && typeof rawEntry === "object" ? rawEntry : {};
        const relPath = normalizeClientRelativePath(entry.path || entry.relative_path || "");
        const sha256 = normalizeSha256(entry.sha256);
        if (!relPath || !sha256 || !isIncrementalSyncPathAllowed(relPath)) {
          continue;
        }
        processed += 1;
        const targetPath = path.join(overlayRoot, relPath);
        const itemUrl = String(entry.url || "").trim() || joinUrlPath(baseUrl, relPath);
        if (!itemUrl) continue;
  
        let needsDownload = true;
        const priorHash = String(prevState?.files?.[relPath] || "").toLowerCase();
        if (priorHash === sha256 && fs.existsSync(targetPath)) {
          needsDownload = false;
        } else if (fs.existsSync(targetPath)) {
          try {
            const existingHash = String(await hashFile(targetPath, "sha256")).toLowerCase();
            if (existingHash === sha256) needsDownload = false;
          } catch (_err) {
            needsDownload = true;
          }
        }
  
        if (needsDownload) {
          if (!downloadStatusEmitted && options?.downloadStatus) {
            downloadStatusEmitted = true;
            emitSessionEvent({
              event: "status",
              status: String(options.downloadStatus),
              ...(options.downloadStatusContext && typeof options.downloadStatusContext === "object"
                ? options.downloadStatusContext
                : {}),
            });
          }
          emitUpdaterEvent({
            event: "incremental-sync-file",
            status: "downloading",
            path: relPath,
            processed,
            total,
          });
          const tmpDir = path.join(getRuntimeDownloadsDir(), "client-sync");
          const downloaded = await downloadToDirWithProgress(itemUrl, tmpDir, {
            headers: fileHeaders,
            defaultBasename: path.basename(relPath) || "sync.bin",
          });
          const downloadedHash = String(await hashFile(downloaded.path, "sha256")).toLowerCase();
          if (downloadedHash !== sha256) {
            try {
              fs.rmSync(downloaded.path, { force: true });
            } catch (_err) {
              // ignore
            }
            throw new Error(`incremental_checksum_mismatch:${relPath}`);
          }
          ensureDir(path.dirname(targetPath));
          fs.copyFileSync(downloaded.path, targetPath);
          try {
            fs.rmSync(downloaded.path, { force: true });
          } catch (_err) {
            // ignore
          }
          changed += 1;
          downloadedBytes += Number(downloaded.bytes || 0);
        }
  
        nextState.files[relPath] = sha256;
        emitUpdaterEvent({
          event: "incremental-sync-file",
          status: needsDownload ? "updated" : "unchanged",
          path: relPath,
          processed,
          total,
        });
      }
  
      if (pruneMissing) {
        const prevFiles = prevState?.files && typeof prevState.files === "object" ? prevState.files : {};
        for (const relPath of Object.keys(prevFiles)) {
          if (nextState.files[relPath]) continue;
          const targetPath = path.join(overlayRoot, normalizeClientRelativePath(relPath));
          try {
            if (targetPath && fs.existsSync(targetPath)) {
              fs.rmSync(targetPath, { force: true });
              deleted += 1;
              emitUpdaterEvent({
                event: "incremental-sync-file",
                status: "deleted",
                path: relPath,
                processed,
                total,
                stateKey,
              });
            }
          } catch (_err) {
            // ignore cleanup errors
          }
        }
      }
  
      saveIncrementalSyncState(nextState, stateKey);
      // Incremental sync can update tool runtimes under runtime/. Clear any process-lifetime caches
      // so subsequent launches restage from the updated overlay without requiring a client restart.
      clearRuntimeCaches();
      emitUpdaterEvent({
        event: "incremental-sync-complete",
        changed,
        deleted,
        processed,
        total,
        downloadedBytes,
        stateKey,
      });
      return { ok: true, changed, deleted, processed, total, downloadedBytes };
    } catch (err) {
      const message = String(err || "incremental_sync_failed");
      emitUpdaterEvent({
        event: "incremental-sync-error",
        error: message,
        stateKey,
      });
      return { ok: false, error: message };
    } finally {
      activeUpdaterDownloads.delete(syncKey);
    }
  };

  return {
    getClientBundleInstallDir,
    startClientUpdaterDownload,
    downloadAndApplyClientUpdate,
    openDownloadedUpdaterFile,
    syncIncrementalClientFiles,
    ensureRuntimePacksForModule,
  };
};

module.exports = { createClientUpdateRuntime };
