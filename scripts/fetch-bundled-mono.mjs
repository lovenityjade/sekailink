import fs from "node:fs";
import path from "node:path";
import https from "node:https";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const repoRoot = path.resolve(__dirname, "..");

const log = (msg) => process.stdout.write(`[fetch-bundled-mono] ${msg}\n`);

const token =
  process.env.SEKAILINK_GITHUB_TOKEN ||
  process.env.GITHUB_TOKEN ||
  process.env.GH_TOKEN ||
  "";

const arch = (() => {
  const m = spawnSync("uname", ["-m"], { encoding: "utf8" });
  const raw = String(m.stdout || "").trim();
  if (raw === "x86_64" || raw === "amd64") return "x86_64";
  if (raw === "aarch64" || raw === "arm64") return "arm64";
  return raw || "x86_64";
})();

const destRoot = path.join(repoRoot, "third_party", "mono", `sekailink-mono-linux-${arch}`);
const destMono = path.join(destRoot, "bin", "mono");

const ghRepo = process.env.SEKAILINK_MONO_GH_REPO || "lovenityjade/sekailink";
const assetRe = new RegExp(process.env.SEKAILINK_MONO_ASSET_REGEX || `sekailink-mono-linux-${arch}\\.zip$`, "i");

const githubHeaders = (accept) => {
  const h = {
    "User-Agent": "SekaiLink",
    Accept: accept || "application/vnd.github+json",
  };
  if (token) h.Authorization = `Bearer ${token}`;
  return h;
};

const httpGetJson = (url) =>
  new Promise((resolve, reject) => {
    const req = https.get(
      url,
      {
        headers: githubHeaders("application/vnd.github+json"),
      },
      (res) => {
        let data = "";
        res.on("data", (c) => (data += String(c)));
        res.on("end", () => {
          if (res.statusCode && res.statusCode >= 400) {
            const err = new Error(`HTTP ${res.statusCode}: ${data.slice(0, 200)}`);
            err.statusCode = res.statusCode;
            err.body = data;
            return reject(err);
          }
          try {
            resolve(JSON.parse(data));
          } catch (err) {
            reject(err);
          }
        });
      }
    );
    req.on("error", reject);
  });

const downloadToFile = (url, outPath, headers = {}, depth = 0) =>
  new Promise((resolve, reject) => {
    if (depth > 6) return reject(new Error("too_many_redirects"));
    fs.mkdirSync(path.dirname(outPath), { recursive: true });
    const out = fs.createWriteStream(outPath);
    const req = https.get(url, { headers: { "User-Agent": "SekaiLink", ...headers } }, (res) => {
      if (res.statusCode && res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
        out.close();
        res.resume();
        return resolve(downloadToFile(res.headers.location, outPath, headers, depth + 1));
      }
      if (res.statusCode !== 200) {
        out.close();
        return reject(new Error(`Download failed: ${res.statusCode}`));
      }
      res.pipe(out);
      out.on("finish", () => out.close(() => resolve(true)));
    });
    req.on("error", (err) => {
      out.close();
      reject(err);
    });
  });

const extractZip = (zipPath, destDir) => {
  fs.mkdirSync(destDir, { recursive: true });
  const unzip = spawnSync("unzip", ["-q", zipPath, "-d", destDir], { stdio: "inherit" });
  if (unzip.status === 0) return;
  // Fallback: python zipfile
  const py = spawnSync(
    "python3",
    [
      "-c",
      [
        "import sys, zipfile",
        "zip_path=sys.argv[1]; out_dir=sys.argv[2]",
        "with zipfile.ZipFile(zip_path) as z: z.extractall(out_dir)",
      ].join("\n"),
      zipPath,
      destDir,
    ],
    { stdio: "inherit" }
  );
  if (py.status !== 0) throw new Error("extract_failed");
};

const main = async () => {
  if (process.platform !== "linux") {
    log(`skip: platform=${process.platform} (linux only)`);
    return;
  }

  if (fs.existsSync(destMono)) {
    log(`ok: already present: ${destMono}`);
    return;
  }

  log(`dest: ${destRoot}`);
  log(`repo: ${ghRepo}`);
  log(`asset regex: ${assetRe}`);
  log(`auth: ${token ? "token" : "none"} (set SEKAILINK_GITHUB_TOKEN/GH_TOKEN if repo is private)`);

  let rel = null;
  try {
    rel = await httpGetJson(`https://api.github.com/repos/${ghRepo}/releases/latest`);
  } catch (err) {
    // GitHub returns 404 for private repos without auth, and also if there is no "latest" release.
    if (err && err.statusCode === 404) {
      const list = await httpGetJson(`https://api.github.com/repos/${ghRepo}/releases?per_page=10`);
      if (Array.isArray(list) && list.length) rel = list[0];
    }
    if (!rel) throw err;
  }

  const assets = Array.isArray(rel.assets) ? rel.assets : [];
  const hit = assets.find((a) => assetRe.test(String(a?.name || "")));
  if (!hit) throw new Error("no_matching_release_asset");

  const tmpDir = fs.mkdtempSync(path.join(path.resolve(process.env.TMPDIR || "/tmp"), "sekailink-mono-"));
  const zipPath = path.join(tmpDir, String(hit.name || `sekailink-mono-linux-${arch}.zip`));

  log(`downloading: ${hit.name}`);
  if (token && hit.url) {
    // Private repo-safe: use the asset API URL with Accept: application/octet-stream.
    await downloadToFile(hit.url, zipPath, githubHeaders("application/octet-stream"));
  } else if (hit.browser_download_url) {
    await downloadToFile(hit.browser_download_url, zipPath, { "User-Agent": "SekaiLink" });
  } else {
    throw new Error("asset_download_url_missing");
  }

  const extractDir = path.join(tmpDir, "extract");
  extractZip(zipPath, extractDir);

  // Zip contains a top-level folder named sekailink-mono-linux-${arch}.
  const srcRoot = path.join(extractDir, `sekailink-mono-linux-${arch}`);
  if (!fs.existsSync(srcRoot)) throw new Error(`unexpected_zip_layout: missing ${srcRoot}`);

  fs.rmSync(destRoot, { recursive: true, force: true });
  fs.mkdirSync(destRoot, { recursive: true });
  // Copy directory tree without trying to be clever about hardlinks.
  spawnSync("cp", ["-a", path.join(srcRoot, "."), destRoot], { stdio: "inherit" });

  if (!fs.existsSync(destMono)) throw new Error(`mono_missing_after_copy: ${destMono}`);
  try {
    fs.chmodSync(destMono, 0o755);
  } catch {
    // ignore
  }

  const probe = spawnSync(destMono, ["--version"], { stdio: "inherit" });
  if (probe.status !== 0) throw new Error("mono_probe_failed");

  log(`ok: installed: ${destMono}`);
};

main().catch((err) => {
  log(`error: ${String(err?.message || err)}`);
  process.exit(1);
});
