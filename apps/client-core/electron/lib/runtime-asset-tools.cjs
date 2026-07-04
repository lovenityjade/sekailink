"use strict";

const createRuntimeAssetTools = (deps = {}) => {
  const {
    fs,
    path,
    http,
    https,
    processRef = process,
    spawnSync,
    sanitizeHeaders,
    resolveRedirectUrl,
    downloadToFile,
    getRuntimeToolsDir,
    ensureDir,
  } = deps;
  const process = processRef;

  const findPathByBasename = (rootDir, wantedLower, maxDepth = 6) => {
    const wanted = String(wantedLower || "").toLowerCase();
    if (!wanted || !rootDir || !fs.existsSync(rootDir)) return "";
  
    const queue = [{ dir: rootDir, depth: 0 }];
    while (queue.length) {
      const cur = queue.shift();
      if (!cur) break;
      if (cur.depth > maxDepth) continue;
  
      let entries = [];
      try {
        entries = fs.readdirSync(cur.dir, { withFileTypes: true });
      } catch (_err) {
        continue;
      }
  
      for (const entry of entries) {
        const full = path.join(cur.dir, entry.name);
        if (entry.isFile()) {
          if (entry.name.toLowerCase() === wanted) return full;
        } else if (entry.isDirectory()) {
          queue.push({ dir: full, depth: cur.depth + 1 });
        }
      }
    }
    return "";
  };
  
  const httpGetJson = (url, options = {}) => {
    return new Promise((resolve, reject) => {
      const client = url.startsWith("https://") ? https : http;
      const headers = {
        "User-Agent": "SekaiLink",
        "Accept": "application/vnd.github+json",
        ...sanitizeHeaders(options.headers || {}),
      };
      if (options.accept) {
        headers.Accept = String(options.accept);
      }
      const req = client.get(url, {
        headers
      }, (res) => {
        if (res.statusCode && res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
          const redirected = resolveRedirectUrl(url, res.headers.location);
          res.resume();
          return resolve(httpGetJson(redirected, options));
        }
        let data = "";
        res.on("data", (chunk) => {
          data += String(chunk);
        });
        res.on("end", () => {
          if (res.statusCode && res.statusCode >= 400) {
            return reject(new Error(`HTTP ${res.statusCode}: ${data.slice(0, 200)}`));
          }
          try {
            resolve(JSON.parse(data));
          } catch (err) {
            reject(err);
          }
        });
      });
      req.on("error", reject);
    });
  };
  
  const ensureGithubReleaseZipInstalled = async (toolId, repo, assetRegex) => {
    const id = String(toolId || "").trim();
    if (!id) throw new Error("missing_tool_id");
    const baseDir = path.join(getRuntimeToolsDir(), id);
    ensureDir(baseDir);
  
    const markerPath = path.join(baseDir, "install.json");
    if (fs.existsSync(markerPath)) {
      try {
        const marker = JSON.parse(fs.readFileSync(markerPath, "utf-8"));
        const rootDir = String(marker?.rootDir || "");
        if (rootDir && fs.existsSync(rootDir)) return rootDir;
      } catch (_err) {
        // ignore marker parse errors
      }
    }
  
    const resolvedRepo = resolveGithubRepo(repo) || repo;
    if (!resolvedRepo) throw new Error("invalid_repo");
    const assetInfo = await resolveGithubAssetInfo(resolvedRepo, assetRegex || "\\.zip$");
    if (!assetInfo?.url) throw new Error("missing_release_asset");
    if (!assetInfo.expectedSha256) throw new Error("missing_release_asset_hash");
    const downloadUrl = assetInfo.url;
  
    const zipName = path.basename(new URL(downloadUrl).pathname || `${id}.zip`);
    const zipPath = path.join(baseDir, zipName);
    await downloadToFile(downloadUrl, zipPath, {
      expectedSha256: assetInfo.expectedSha256,
      requireHash: true,
    });
  
    const extractDir = path.join(baseDir, "extract");
    if (fs.existsSync(extractDir)) fs.rmSync(extractDir, { recursive: true, force: true });
    extractZip(zipPath, extractDir);
    const rootDir = resolvePackRoot(extractDir);
  
    fs.writeFileSync(markerPath, JSON.stringify({
      toolId: id,
      repo: resolvedRepo,
      downloadUrl,
      sha256: assetInfo.expectedSha256,
      installed_at: new Date().toISOString(),
      rootDir,
    }, null, 2));
  
    return rootDir;
  };
  
  const backupAndReplaceFile = (srcPath, destPath) => {
    ensureDir(path.dirname(destPath));
    if (fs.existsSync(destPath)) {
      const ts = new Date().toISOString().replace(/[:.]/g, "-");
      const bak = `${destPath}.bak.${ts}`;
      try {
        fs.renameSync(destPath, bak);
      } catch (_err) {
        // If rename fails (cross-device, perms), try a copy + unlink.
        try {
          fs.copyFileSync(destPath, bak);
          fs.unlinkSync(destPath);
        } catch (_err2) {
          // ignore
        }
      }
    }
    fs.copyFileSync(srcPath, destPath);
  };

  const isUrl = (value) => /^https?:\/\//i.test(String(value || ""));
  
  
  const resolveGithubRepo = (source) => {
    if (!source) return null;
    if (/^[^/]+\/[^/]+$/.test(source)) return source;
    const match = String(source).match(/github\.com\/([^/]+\/[^/]+)/i);
    return match ? match[1] : null;
  };
  
  const resolveGithubAssetInfo = async (repo, assetRegex) => {
    const data = await httpGetJson(`https://api.github.com/repos/${repo}/releases/latest`);
    const assets = Array.isArray(data?.assets) ? data.assets : [];
    if (!assets.length) return null;
    const regex = assetRegex ? new RegExp(assetRegex, "i") : null;
    const candidates = regex ? assets.filter((asset) => regex.test(asset.name)) : assets;
    const target = candidates.find((asset) => asset.browser_download_url) || assets[0];
    if (!target?.browser_download_url) return null;
    const digest = String(target?.digest || "").trim().toLowerCase();
    const expectedSha256 = digest.startsWith("sha256:") ? digest.slice("sha256:".length) : "";
    return {
      url: target.browser_download_url,
      expectedSha256: normalizeSha256(expectedSha256),
      assetName: String(target?.name || ""),
    };
  };
  
  const extractZip = (zipPath, destDir) => {
    ensureDir(destDir);
    if (process.platform === "win32") {
      // Secure extraction with path traversal checks.
      const esc = (value) => String(value || "").replace(/'/g, "''");
      const script = `
  Add-Type -AssemblyName System.IO.Compression.FileSystem
  $zipPath='${esc(zipPath)}'
  $destDir='${esc(destDir)}'
  [System.IO.Directory]::CreateDirectory($destDir) | Out-Null
  $destRoot = [System.IO.Path]::GetFullPath($destDir + [System.IO.Path]::DirectorySeparatorChar)
  $zip = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
  try {
    foreach ($entry in $zip.Entries) {
      $name = ($entry.FullName -replace '\\\\','/').Trim()
      if ([string]::IsNullOrWhiteSpace($name)) { continue }
      if ($name.StartsWith('/') -or $name.StartsWith('\\\\') -or ($name -match '^[A-Za-z]:')) { throw 'zip_path_traversal' }
      $parts = $name.Split('/')
      if ($parts -contains '..') { throw 'zip_path_traversal' }
      $target = [System.IO.Path]::GetFullPath((Join-Path $destDir $name))
      if (-not $target.StartsWith($destRoot, [System.StringComparison]::OrdinalIgnoreCase)) { throw 'zip_path_traversal' }
      if ($name.EndsWith('/')) {
        [System.IO.Directory]::CreateDirectory($target) | Out-Null
        continue
      }
      $parent = [System.IO.Path]::GetDirectoryName($target)
      if ($parent) { [System.IO.Directory]::CreateDirectory($parent) | Out-Null }
      [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target, $true)
    }
  } finally {
    $zip.Dispose()
  }
  `.trim();
      const ps = spawnSync(
        "powershell",
        ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", script],
        { encoding: "utf8", windowsHide: true }
      );
      if (ps.status === 0) return;
      const details = [ps.stderr, ps.stdout].filter(Boolean).join("\n").trim();
      throw new Error(`zip_extract_failed:${details.slice(0, 900) || `powershell_exit_${ps.status}`}`);
    }
  
    // POSIX fallback: safe extraction with explicit checks and symlink blocking.
    const python = getPythonCommand();
    const code = `
  import os, stat, sys, zipfile
  zip_path = sys.argv[1]
  dest_dir = os.path.abspath(sys.argv[2])
  os.makedirs(dest_dir, exist_ok=True)
  dest_root = dest_dir + os.sep
  with zipfile.ZipFile(zip_path) as z:
    for info in z.infolist():
      name = (info.filename or '').replace('\\\\\\\\', '/').strip()
      if not name:
        continue
      if name.startswith('/') or name.startswith('\\\\\\\\') or ':' in name.split('/')[0]:
        raise RuntimeError('zip_path_traversal')
      parts = [p for p in name.split('/') if p]
      if '..' in parts:
        raise RuntimeError('zip_path_traversal')
      mode_type = (info.external_attr >> 16) & 0o170000
      if mode_type == stat.S_IFLNK:
        raise RuntimeError('zip_symlink_forbidden')
      out_path = os.path.abspath(os.path.join(dest_dir, name))
      if not (out_path == dest_dir or out_path.startswith(dest_root)):
        raise RuntimeError('zip_path_traversal')
      if info.is_dir() or name.endswith('/'):
        os.makedirs(out_path, exist_ok=True)
        continue
      parent = os.path.dirname(out_path)
      os.makedirs(parent, exist_ok=True)
      with z.open(info) as src, open(out_path, 'wb') as dst:
        dst.write(src.read())
      mode = (info.external_attr >> 16) & 0o777
      if mode:
        try:
          os.chmod(out_path, mode)
        except OSError:
          pass
  `.trim();
    const res = spawnSync(python, ["-c", code, zipPath, destDir], { encoding: "utf8" });
    if (res.status !== 0) {
      const details = [res.stderr, res.stdout].filter(Boolean).join("\n").trim();
      throw new Error(`zip_extract_failed:${details.slice(0, 900) || `python_exit_${res.status}`}`);
    }
  };
  
  const resolvePackRoot = (destDir) => {
    const entries = fs.readdirSync(destDir, { withFileTypes: true });
    const dirs = entries.filter((entry) => entry.isDirectory());
    if (dirs.length === 1 && entries.length === 1) {
      return path.join(destDir, dirs[0].name);
    }
    return destDir;
  };

  return {
    findPathByBasename,
    httpGetJson,
    ensureGithubReleaseZipInstalled,
    backupAndReplaceFile,
    isUrl,
    resolveGithubRepo,
    resolveGithubAssetInfo,
    extractZip,
    resolvePackRoot,
  };
};

module.exports = { createRuntimeAssetTools };
