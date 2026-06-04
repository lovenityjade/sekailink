import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const appDir = path.resolve(__dirname, "..");
const releaseDir = path.join(appDir, "release");
const bootstrapperDir = path.join(appDir, "bootstrapper");

const today = new Date().toISOString().slice(0, 10).replace(/-/g, "");
const dateDir = String(process.env.SEKAILINK_RELEASE_DATE || today).trim();
const version = String(process.env.SEKAILINK_BOOTSTRAPPER_VERSION || `0.1.0-${dateDir}`).trim();
const baseUrl = String(process.env.SEKAILINK_BOOTSTRAPPER_BASE_URL || `https://sekailink.com/downloads/client/bootstrapper/${dateDir}`).replace(/\/+$/, "");
const outputDir = path.join(releaseDir, "bootstrapper", dateDir);

const sha256File = (filePath) => crypto.createHash("sha256").update(fs.readFileSync(filePath)).digest("hex");

const copyFile = (source, target) => {
  fs.mkdirSync(path.dirname(target), { recursive: true });
  fs.copyFileSync(source, target);
};

const commandExists = (cmd) => {
  const res = spawnSync(process.platform === "win32" ? "where" : "which", [cmd], { stdio: "ignore" });
  return res.status === 0;
};

const zipFiles = (files, outPath) => {
  fs.rmSync(outPath, { force: true });
  fs.mkdirSync(path.dirname(outPath), { recursive: true });
  if (commandExists("zip")) {
    const res = spawnSync("zip", ["-qry", outPath, ...files.map((item) => item.name)], {
      cwd: files[0].cwd,
      stdio: "inherit",
    });
    if (res.status === 0) return;
  }
  const python = process.platform === "win32" ? "python" : "python3";
  const code = `
import os, sys, zipfile
out = os.path.abspath(sys.argv[1])
items = sys.argv[2:]
with zipfile.ZipFile(out, 'w', compression=zipfile.ZIP_DEFLATED) as z:
  for item in items:
    z.write(item, os.path.basename(item))
`.trim();
  const res = spawnSync(python, ["-c", code, outPath, ...files.map((item) => path.join(item.cwd, item.name))], {
    stdio: "inherit",
  });
  if (res.status !== 0) throw new Error("zip_failed");
};

const tarGzFiles = (files, outPath) => {
  fs.rmSync(outPath, { force: true });
  fs.mkdirSync(path.dirname(outPath), { recursive: true });
  if (commandExists("tar")) {
    const res = spawnSync("tar", ["-czf", outPath, ...files.map((item) => item.name)], {
      cwd: files[0].cwd,
      stdio: "inherit",
    });
    if (res.status === 0) return;
  }
  zipFiles(files, outPath.replace(/\.tar\.gz$/, ".zip"));
};

fs.mkdirSync(outputDir, { recursive: true });

const staged = path.join(outputDir, "staged");
fs.rmSync(staged, { recursive: true, force: true });
fs.mkdirSync(staged, { recursive: true });

for (const name of ["SekaiLink-bootstrapper.cmd", "SekaiLink-bootstrapper.ps1", "sekailink-bootstrapper.sh", "README.md"]) {
  copyFile(path.join(bootstrapperDir, name), path.join(staged, name));
}
try {
  fs.chmodSync(path.join(staged, "sekailink-bootstrapper.sh"), 0o755);
} catch {
  // ignore
}

const windowsZip = path.join(outputDir, `SekaiLink-bootstrapper-${version}-windows.zip`);
const linuxTar = path.join(outputDir, `SekaiLink-bootstrapper-${version}-linux.tar.gz`);
zipFiles(
  [
    { cwd: staged, name: "SekaiLink-bootstrapper.cmd" },
    { cwd: staged, name: "SekaiLink-bootstrapper.ps1" },
    { cwd: staged, name: "README.md" },
  ],
  windowsZip
);
tarGzFiles(
  [
    { cwd: staged, name: "sekailink-bootstrapper.sh" },
    { cwd: staged, name: "README.md" },
  ],
  linuxTar
);

const artifacts = [];
for (const filePath of [windowsZip, linuxTar]) {
  if (!fs.existsSync(filePath)) continue;
  const fileName = path.basename(filePath);
  artifacts.push({
    fileName,
    sha256: sha256File(filePath),
    size: fs.statSync(filePath).size,
    url: `${baseUrl}/${encodeURIComponent(fileName)}`,
  });
}

const manifest = {
  generated_at: new Date().toISOString(),
  version,
  channel: "test",
  build: "release",
  base_url: baseUrl,
  artifacts,
};
fs.writeFileSync(path.join(outputDir, `sekailink-bootstrapper-release-${dateDir}.json`), `${JSON.stringify(manifest, null, 2)}\n`, "utf8");
fs.rmSync(staged, { recursive: true, force: true });

for (const artifact of artifacts) {
  process.stdout.write(`[package-bootstrapper] ${artifact.fileName} ${artifact.sha256} ${artifact.size}\n`);
}
