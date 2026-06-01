import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const appDir = path.resolve(__dirname, "..");
const releaseDir = path.join(appDir, "release");
const packageJson = JSON.parse(fs.readFileSync(path.join(appDir, "package.json"), "utf8"));

const today = new Date().toISOString().slice(0, 10).replace(/-/g, "");
const releaseVersion = String(process.env.SEKAILINK_RELEASE_VERSION || `${packageJson.version}-prebeta3.${today}.1`).trim();
const channel = String(process.env.SEKAILINK_RELEASE_CHANNEL || "test").trim();
const build = String(process.env.SEKAILINK_RELEASE_BUILD || "release").trim();
const dateDir = String(process.env.SEKAILINK_RELEASE_DATE || today).trim();
const baseUrl = String(process.env.SEKAILINK_RELEASE_BASE_URL || `https://sekailink.com/downloads/client/prebeta3/${dateDir}`).replace(/\/+$/, "");
const outputRoot = path.join(releaseDir, "update-bundles", dateDir);

const bundles = [
  {
    key: "linux-x64",
    source: path.join(releaseDir, "linux-unpacked"),
    platforms: ["linux", "linux-x64"],
    fallbackUrl: process.env.SEKAILINK_FALLBACK_LINUX_URL || "",
    fallbackSha256: process.env.SEKAILINK_FALLBACK_LINUX_SHA256 || "",
    fallbackArtifactType: "appimage",
  },
  {
    key: "win-x64",
    source: path.join(releaseDir, "win-unpacked"),
    platforms: ["windows", "win32-x64"],
    fallbackUrl: process.env.SEKAILINK_FALLBACK_WINDOWS_URL || "",
    fallbackSha256: process.env.SEKAILINK_FALLBACK_WINDOWS_SHA256 || "",
    fallbackArtifactType: "nsis",
  },
];

const log = (message) => process.stdout.write(`[package-update-bundles] ${message}\n`);

const sha256File = (filePath) => {
  const hash = crypto.createHash("sha256");
  const data = fs.readFileSync(filePath);
  hash.update(data);
  return hash.digest("hex");
};

const commandExists = (cmd) => {
  const res = spawnSync(process.platform === "win32" ? "where" : "which", [cmd], { stdio: "ignore" });
  return res.status === 0;
};

const zipWithPython = (sourceDir, outPath) => {
  const python = process.platform === "win32" ? "python" : "python3";
  const code = `
import os, stat, sys, zipfile
src = os.path.abspath(sys.argv[1])
out = os.path.abspath(sys.argv[2])
with zipfile.ZipFile(out, 'w', compression=zipfile.ZIP_DEFLATED) as z:
  for root, dirs, files in os.walk(src):
    dirs[:] = [d for d in dirs if d not in {'.git', '__pycache__'}]
    for name in files:
      full = os.path.join(root, name)
      rel = os.path.relpath(full, src).replace(os.sep, '/')
      info = zipfile.ZipInfo(rel)
      st = os.stat(full)
      info.external_attr = (stat.S_IMODE(st.st_mode) | stat.S_IFREG) << 16
      with open(full, 'rb') as fh:
        z.writestr(info, fh.read(), compress_type=zipfile.ZIP_DEFLATED)
`.trim();
  const res = spawnSync(python, ["-c", code, sourceDir, outPath], { stdio: "inherit" });
  if (res.status !== 0) throw new Error(`python_zip_failed:${sourceDir}`);
};

const zipDirectory = (sourceDir, outPath) => {
  fs.rmSync(outPath, { force: true });
  fs.mkdirSync(path.dirname(outPath), { recursive: true });

  if (commandExists("zip")) {
    const res = spawnSync("zip", ["-qry", outPath, "."], { cwd: sourceDir, stdio: "inherit" });
    if (res.status === 0) return;
  }

  if (process.platform === "win32") {
    const script = [
      `$src='${sourceDir.replace(/'/g, "''")}'`,
      `$out='${outPath.replace(/'/g, "''")}'`,
      "if (Test-Path -LiteralPath $out) { Remove-Item -LiteralPath $out -Force }",
      "Compress-Archive -Path (Join-Path $src '*') -DestinationPath $out -Force",
    ].join("; ");
    const res = spawnSync("powershell.exe", ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", script], {
      stdio: "inherit",
    });
    if (res.status === 0) return;
  }

  zipWithPython(sourceDir, outPath);
};

const releases = [];
const artifacts = [];

for (const bundle of bundles) {
  if (!fs.existsSync(bundle.source)) {
    log(`skip ${bundle.key}: missing ${bundle.source}`);
    continue;
  }
  const fileName = `SekaiLink-client-${releaseVersion}-${bundle.key}.zip`;
  const outPath = path.join(outputRoot, fileName);
  log(`zipping ${bundle.key}: ${bundle.source}`);
  zipDirectory(bundle.source, outPath);
  const sha256 = sha256File(outPath);
  const size = fs.statSync(outPath).size;
  const url = `${baseUrl}/${encodeURIComponent(fileName)}`;
  artifacts.push({ key: bundle.key, path: outPath, fileName, sha256, size, url });

  for (const platform of bundle.platforms) {
    const entry = {
      version: releaseVersion,
      latest: releaseVersion,
      channel,
      build,
      platform,
      artifact_type: "client-bundle",
      requires_client_updater: "bundle-v1",
      download_url: url,
      sha256,
      available: true,
    };
    if (bundle.fallbackUrl && bundle.fallbackSha256) {
      entry.fallback_download_url = bundle.fallbackUrl;
      entry.fallback_sha256 = bundle.fallbackSha256;
      entry.fallback_artifact_type = bundle.fallbackArtifactType;
    }
    releases.push(entry);
  }
}

if (!artifacts.length) {
  throw new Error("no_update_bundles_created");
}

const manifest = {
  generated_at: new Date().toISOString(),
  version: releaseVersion,
  channel,
  build,
  base_url: baseUrl,
  artifacts,
  releases,
};

const manifestPath = path.join(outputRoot, "client_release_manifest.fragment.json");
fs.writeFileSync(manifestPath, `${JSON.stringify(manifest, null, 2)}\n`, "utf8");
log(`wrote ${manifestPath}`);
