"use strict";

const createDownloadTools = ({
  fs,
  path,
  http,
  https,
  crypto,
  processRef = process,
  ensureDir,
  writeLogLine = () => {},
}) => {
  const computeFileHash = (filePath, algo = "sha256") => {
    return new Promise((resolve, reject) => {
      const hash = crypto.createHash(algo);
      const stream = fs.createReadStream(filePath);
      stream.on("data", (chunk) => hash.update(chunk));
      stream.on("error", reject);
      stream.on("end", () => resolve(hash.digest("hex")));
    });
  };

  const normalizeSha256 = (value) => {
    const v = String(value || "").trim().toLowerCase();
    return /^[a-f0-9]{64}$/.test(v) ? v : "";
  };

  const verifyDownloadedFileHash = async (filePath, options = {}) => {
    const expectedSha256 = normalizeSha256(options.expectedSha256 || options.sha256);
    if (!expectedSha256) {
      if (options.requireHash) throw new Error("download_hash_required");
      return { ok: true, skipped: true };
    }
    const actual = await computeFileHash(filePath, "sha256");
    if (actual.toLowerCase() !== expectedSha256) {
      throw new Error(`download_hash_mismatch: expected=${expectedSha256} actual=${actual}`);
    }
    return { ok: true, sha256: actual };
  };

  const downloadToFile = (url, destPath, options = {}) => {
    const expectedSha256 = normalizeSha256(options.expectedSha256 || options.sha256);
    return new Promise((resolve, reject) => {
      ensureDir(path.dirname(destPath));
      const file = fs.createWriteStream(destPath);
      const client = String(url).startsWith("https:") ? https : http;
      const req = client.get(url, { headers: { "User-Agent": "SekaiLink/1.0" } }, (res) => {
        if ([301, 302, 303, 307, 308].includes(res.statusCode) && res.headers.location) {
          file.close(() => fs.rmSync(destPath, { force: true }));
          const next = new URL(res.headers.location, url).toString();
          downloadToFile(next, destPath, { ...options, expectedSha256 }).then(resolve, reject);
          return;
        }
        if (res.statusCode !== 200) {
          file.close(() => fs.rmSync(destPath, { force: true }));
          reject(new Error(`download_failed_http_${res.statusCode || "unknown"}`));
          return;
        }
        res.pipe(file);
        file.on("finish", async () => {
          file.close(async () => {
            try {
              await verifyDownloadedFileHash(destPath, { expectedSha256, requireHash: options.requireHash });
              resolve(destPath);
            } catch (err) {
              try {
                fs.rmSync(destPath, { force: true });
              } catch (_rmErr) {
                // ignore cleanup failures
              }
              reject(err);
            }
          });
        });
      });
      req.on("error", (err) => {
        try {
          file.close(() => fs.rmSync(destPath, { force: true }));
        } catch (_closeErr) {
          // ignore cleanup failures
        }
        reject(err);
      });
    });
  };

  const sanitizeFilename = (value) => {
    const raw = String(value || "").trim();
    const base = path.basename(raw.replace(/\\/g, "/"));
    const cleaned = base.replace(/[^a-zA-Z0-9._-]+/g, "_").replace(/^_+|_+$/g, "");
    return cleaned || "download.bin";
  };

  const parseContentDispositionFilename = (headerValue) => {
    const header = String(headerValue || "");
    const utfMatch = header.match(/filename\\*=UTF-8''([^;]+)/i);
    if (utfMatch) {
      try {
        return decodeURIComponent(utfMatch[1].trim().replace(/^"|"$/g, ""));
      } catch (_err) {
        return utfMatch[1].trim().replace(/^"|"$/g, "");
      }
    }
    const match = header.match(/filename=([^;]+)/i);
    if (!match) return "";
    return match[1].trim().replace(/^"|"$/g, "");
  };

  const uniquePathInDir = (dirPath, baseName) => {
    const safeBase = sanitizeFilename(baseName);
    let candidate = path.join(dirPath, safeBase);
    if (!fs.existsSync(candidate)) return candidate;
    const ext = path.extname(safeBase);
    const stem = path.basename(safeBase, ext);
    for (let i = 1; i < 10000; i += 1) {
      candidate = path.join(dirPath, `${stem}-${i}${ext}`);
      if (!fs.existsSync(candidate)) return candidate;
    }
    return path.join(dirPath, `${stem}-${Date.now()}${ext}`);
  };

  const resolveRedirectUrl = (baseUrl, location) => {
    try {
      return new URL(location, baseUrl).toString();
    } catch (_err) {
      return "";
    }
  };

  const sanitizeHeaders = (value) => {
    const out = {};
    if (!value || typeof value !== "object") return out;
    for (const [key, raw] of Object.entries(value)) {
      const k = String(key || "").trim();
      if (!k || /[\r\n:]/.test(k)) continue;
      if (raw === undefined || raw === null) continue;
      out[k] = String(raw);
    }
    return out;
  };

  const downloadToDir = (url, destDir, options = {}, depth = 0) => {
    return new Promise((resolve, reject) => {
      if (depth > 8) {
        reject(new Error("too_many_redirects"));
        return;
      }
      ensureDir(destDir);
      const client = String(url).startsWith("https:") ? https : http;
      const req = client.get(url, { headers: { "User-Agent": "SekaiLink/1.0", ...sanitizeHeaders(options.headers) } }, (res) => {
        if ([301, 302, 303, 307, 308].includes(res.statusCode) && res.headers.location) {
          const next = resolveRedirectUrl(url, res.headers.location);
          if (!next) {
            reject(new Error("invalid_redirect"));
            return;
          }
          res.resume();
          downloadToDir(next, destDir, options, depth + 1).then(resolve, reject);
          return;
        }
        if (res.statusCode !== 200) {
          res.resume();
          reject(new Error(`download_failed_http_${res.statusCode || "unknown"}`));
          return;
        }
        const nameFromHeader = parseContentDispositionFilename(res.headers["content-disposition"]);
        const nameFromUrl = path.basename(new URL(url).pathname || "");
        const targetPath = uniquePathInDir(destDir, nameFromHeader || nameFromUrl || options.defaultName || "download.bin");
        const file = fs.createWriteStream(targetPath);
        res.pipe(file);
        file.on("finish", () => {
          file.close(async () => {
            try {
              await verifyDownloadedFileHash(targetPath, options);
              resolve({ path: targetPath, filename: path.basename(targetPath), url });
            } catch (err) {
              try {
                fs.rmSync(targetPath, { force: true });
              } catch (_rmErr) {
                // ignore cleanup failures
              }
              reject(err);
            }
          });
        });
        file.on("error", reject);
      });
      req.on("error", reject);
    });
  };

  const downloadToDirWithProgress = (url, destDir, options = {}, depth = 0) => {
    return new Promise((resolve, reject) => {
      if (depth > 8) {
        reject(new Error("too_many_redirects"));
        return;
      }
      ensureDir(destDir);
      const client = String(url).startsWith("https:") ? https : http;
      const req = client.get(url, { headers: { "User-Agent": "SekaiLink/1.0", ...sanitizeHeaders(options.headers) } }, (res) => {
        if ([301, 302, 303, 307, 308].includes(res.statusCode) && res.headers.location) {
          const next = resolveRedirectUrl(url, res.headers.location);
          if (!next) {
            reject(new Error("invalid_redirect"));
            return;
          }
          res.resume();
          downloadToDirWithProgress(next, destDir, options, depth + 1).then(resolve, reject);
          return;
        }
        if (res.statusCode !== 200) {
          res.resume();
          reject(new Error(`download_failed_http_${res.statusCode || "unknown"}`));
          return;
        }
        const nameFromHeader = parseContentDispositionFilename(res.headers["content-disposition"]);
        const nameFromUrl = path.basename(new URL(url).pathname || "");
        const targetPath = uniquePathInDir(destDir, nameFromHeader || nameFromUrl || options.defaultName || "download.bin");
        const total = Number(res.headers["content-length"] || 0) || 0;
        let received = 0;
        const file = fs.createWriteStream(targetPath);
        res.on("data", (chunk) => {
          received += chunk.length;
          if (typeof options.onProgress === "function") {
            try {
              options.onProgress({ received, total, percent: total > 0 ? received / total : 0, path: targetPath });
            } catch (err) {
              writeLogLine("warn", "download", `progress callback failed: ${String(err || "")}`);
            }
          }
        });
        res.pipe(file);
        file.on("finish", () => {
          file.close(async () => {
            try {
              await verifyDownloadedFileHash(targetPath, options);
              resolve({ path: targetPath, filename: path.basename(targetPath), url, bytes: received, total });
            } catch (err) {
              try {
                fs.rmSync(targetPath, { force: true });
              } catch (_rmErr) {
                // ignore cleanup failures
              }
              reject(err);
            }
          });
        });
        file.on("error", reject);
      });
      req.on("error", reject);
    });
  };

  return {
    computeFileHash,
    verifyDownloadedFileHash,
    downloadToFile,
    sanitizeFilename,
    parseContentDispositionFilename,
    uniquePathInDir,
    downloadToDir,
    resolveRedirectUrl,
    sanitizeHeaders,
    downloadToDirWithProgress,
    normalizeSha256,
  };
};

module.exports = { createDownloadTools };
