import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const appDir = path.resolve(__dirname, "..");
const repoDir = path.resolve(appDir, "..", "..");
const releaseDir = path.join(appDir, "release");
const nativeDir = path.join(appDir, "native-bootloader");

const today = new Date().toISOString().slice(0, 10).replace(/-/g, "");
const dateDir = String(process.env.SEKAILINK_RELEASE_DATE || today).trim();
const version = String(process.env.SEKAILINK_BOOTSTRAPPER_VERSION || `1.0.0-${dateDir}`).trim();
const normalizeReleaseChannel = (value) => {
  const raw = String(value || "").trim().toLowerCase();
  if (raw === "canary") return "canari";
  if (raw === "test" || raw === "stable" || raw === "release") return "canonical";
  return raw === "canari" ? "canari" : "canonical";
};
const channel = normalizeReleaseChannel(process.env.SEKAILINK_RELEASE_CHANNEL || "canonical");
const build = String(process.env.SEKAILINK_RELEASE_BUILD || "release").trim();
const baseUrl = String(process.env.SEKAILINK_BOOTSTRAPPER_BASE_URL || `https://sekailink.com/downloads/client/bootstrapper/${channel}/${dateDir}`).replace(/\/+$/, "");
const outputDir = path.join(releaseDir, "bootstrapper", channel, dateDir);
const linuxBuildDir = path.join(outputDir, "native-build-linux");
const winBuildDir = path.join(outputDir, "native-build-windows");
const defaultMsysRoot = process.platform === "win32" ? "C:\\msys64\\ucrt64" : "/mnt/windows/msys64/ucrt64";
const msysRoot = String(process.env.SEKAILINK_MSYS2_UCRT_ROOT || defaultMsysRoot).trim();
const msysBin = path.join(msysRoot, "bin");
const requiredWindowsBootloaderDlls = [
  "libgcc_s_seh-1.dll",
  "libwinpthread-1.dll",
  "libstdc++-6.dll",
  "libcurl-4.dll",
  "libcrypto-3-x64.dll",
  "libssl-3-x64.dll",
  "SDL2.dll",
  "zlib1.dll",
  "libidn2-0.dll",
  "libintl-8.dll",
  "libnghttp2-14.dll",
  "libnghttp3-9.dll",
  "libngtcp2-16.dll",
  "libngtcp2_crypto_ossl-0.dll",
  "libpsl-5.dll",
  "libssh2-1.dll",
  "libunistring-5.dll",
  "libbrotlicommon.dll",
  "libbrotlidec.dll",
  "libbrotlienc.dll",
  "libiconv-2.dll",
  "libzstd.dll",
];

const sha256File = (filePath) => crypto.createHash("sha256").update(fs.readFileSync(filePath)).digest("hex");

const run = (cmd, args, options = {}) => {
  const res = spawnSync(cmd, args, { cwd: repoDir, stdio: "inherit", ...options });
  if (res.status !== 0) throw new Error(`${cmd} ${args.join(" ")} failed`);
};

const commandOutput = (cmd, args, options = {}) => {
  const res = spawnSync(cmd, args, { cwd: repoDir, encoding: "utf8", ...options });
  if (res.status !== 0) return "";
  return String(res.stdout || "");
};

const copyFile = (source, target) => {
  fs.mkdirSync(path.dirname(target), { recursive: true });
  fs.copyFileSync(source, target);
};

const copyDir = (source, target) => {
  fs.mkdirSync(path.dirname(target), { recursive: true });
  fs.cpSync(source, target, { recursive: true });
};

const commandExists = (cmd) => {
  const res = spawnSync(process.platform === "win32" ? "where" : "which", [cmd], { stdio: "ignore" });
  return res.status === 0;
};

const zipFiles = (items, outPath) => {
  fs.rmSync(outPath, { force: true });
  fs.mkdirSync(path.dirname(outPath), { recursive: true });
  if (commandExists("zip")) {
    run("zip", ["-qry", outPath, ...items.map((item) => item.name)], { cwd: items[0].cwd });
    return;
  }
  const code = `
import os, sys, zipfile
out = os.path.abspath(sys.argv[1])
items = sys.argv[2:]
with zipfile.ZipFile(out, 'w', compression=zipfile.ZIP_DEFLATED) as z:
  for item in items:
    base = os.path.basename(item)
    if os.path.isdir(item):
      for root, dirs, files in os.walk(item):
        for f in files:
          full = os.path.join(root, f)
          rel = os.path.join(base, os.path.relpath(full, item))
          z.write(full, rel)
    else:
      z.write(item, base)
`.trim();
  run("python3", ["-c", code, outPath, ...items.map((item) => path.join(item.cwd, item.name))]);
};

const tarGzFiles = (items, outPath) => {
  fs.rmSync(outPath, { force: true });
  fs.mkdirSync(path.dirname(outPath), { recursive: true });
  if (commandExists("tar")) {
    run("tar", ["-czf", outPath, ...items.map((item) => item.name)], { cwd: items[0].cwd });
    return;
  }
  zipFiles(items, outPath.replace(/\.tar\.gz$/, ".zip"));
};

const buildLinux = () => {
  fs.rmSync(linuxBuildDir, { recursive: true, force: true });
  run("cmake", ["-S", nativeDir, "-B", linuxBuildDir, "-DCMAKE_BUILD_TYPE=Release"]);
  run("cmake", ["--build", linuxBuildDir, "-j4"]);
};

const buildWindows = () => {
  fs.rmSync(winBuildDir, { recursive: true, force: true });
  run("cmake", [
    "-S", nativeDir,
    "-B", winBuildDir,
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_SYSTEM_NAME=Windows",
    "-DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc",
    "-DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++",
    `-DSEKAILINK_MSYS2_UCRT_ROOT=${msysRoot}`,
  ]);
  run("cmake", ["--build", winBuildDir, "-j4"]);
};

const systemDll = (name) => {
  const n = name.toLowerCase();
  return n === "kernel32.dll" ||
    n === "shell32.dll" ||
    n === "user32.dll" ||
    n === "gdi32.dll" ||
    n === "advapi32.dll" ||
    n === "ole32.dll" ||
    n === "oleaut32.dll" ||
    n === "version.dll" ||
    n === "uuid.dll" ||
    n === "winmm.dll" ||
    n === "imm32.dll" ||
    n === "setupapi.dll" ||
    n === "crypt32.dll" ||
    n === "bcrypt.dll" ||
    n === "ws2_32.dll" ||
    n === "wldap32.dll" ||
    n === "secur32.dll" ||
    n === "iphlpapi.dll" ||
    n.startsWith("api-ms-win-") ||
    n.startsWith("ext-ms-win-");
};

const importedDlls = (filePath) => {
  const text = commandOutput("x86_64-w64-mingw32-objdump", ["-p", filePath]);
  const names = [];
  for (const line of text.split(/\r?\n/)) {
    const match = line.match(/DLL Name:\s*(.+)$/);
    if (match) names.push(match[1].trim());
  }
  return names;
};

const copyWindowsDllClosure = (seed, targetDir) => {
  const queue = [seed];
  const seen = new Set();
  const copyDll = (dll) => {
    const key = dll.toLowerCase();
    if (seen.has(key)) return "";
    const source = path.join(msysBin, dll);
    if (!fs.existsSync(source)) {
      throw new Error(`missing_windows_dll:${dll}`);
    }
    seen.add(key);
    const target = path.join(targetDir, dll);
    copyFile(source, target);
    return target;
  };

  while (queue.length) {
    const current = queue.shift();
    for (const dll of importedDlls(current)) {
      if (systemDll(dll)) continue;
      const target = copyDll(dll);
      if (!target) continue;
      queue.push(target);
    }
  }

  for (const dll of requiredWindowsBootloaderDlls) {
    const target = copyDll(dll);
    if (target) queue.push(target);
  }
  return [...seen].sort();
};

fs.mkdirSync(outputDir, { recursive: true });
const shouldBuildWindows = process.platform === "win32" || process.env.SEKAILINK_BOOTSTRAPPER_BUILD_WINDOWS === "1";
const shouldBuildLinux = process.platform !== "win32" || process.env.SEKAILINK_BOOTSTRAPPER_BUILD_LINUX === "1";
if (shouldBuildLinux) buildLinux();
if (shouldBuildWindows) buildWindows();

const staged = path.join(outputDir, "staged");
fs.rmSync(staged, { recursive: true, force: true });
fs.mkdirSync(staged, { recursive: true });

const winStage = path.join(staged, "SekaiLink Bootloader");
const linuxStage = path.join(staged, "SekaiLink Bootloader Linux");
if (shouldBuildWindows) fs.mkdirSync(winStage, { recursive: true });
if (shouldBuildLinux) fs.mkdirSync(linuxStage, { recursive: true });

const winExe = path.join(winBuildDir, "SekaiLink Bootloader.exe");
const linuxExe = path.join(linuxBuildDir, "sekailink-bootloader");
let copiedDlls = [];
if (shouldBuildWindows) {
  copyFile(winExe, path.join(winStage, "SekaiLink Bootloader.exe"));
  copiedDlls = copyWindowsDllClosure(path.join(winStage, "SekaiLink Bootloader.exe"), winStage);
  const caBundleCandidates = [
    path.join(msysRoot, "etc", "ssl", "certs", "ca-bundle.crt"),
    path.join(msysRoot, "ssl", "certs", "ca-bundle.crt"),
    path.resolve(msysRoot, "..", "usr", "ssl", "certs", "ca-bundle.crt"),
  ];
  const caBundle = caBundleCandidates.find((candidate) => fs.existsSync(candidate)) || "";
  if (fs.existsSync(caBundle)) {
    copyFile(caBundle, path.join(winStage, "ca-bundle.crt"));
  } else {
    throw new Error(`missing_ca_bundle:${caBundleCandidates.join(",")}`);
  }
  copyFile(path.join(appDir, "bootstrapper", "README.md"), path.join(winStage, "README.md"));
}
if (shouldBuildLinux) {
  copyFile(linuxExe, path.join(linuxStage, "sekailink-bootloader"));
  fs.chmodSync(path.join(linuxStage, "sekailink-bootloader"), 0o755);
  copyFile(path.join(appDir, "bootstrapper", "README.md"), path.join(linuxStage, "README.md"));
}

const windowsZip = path.join(outputDir, `SekaiLink-bootstrapper-${version}-windows.zip`);
const linuxTar = path.join(outputDir, `SekaiLink-bootstrapper-${version}-linux.tar.gz`);
if (shouldBuildWindows) zipFiles([{ cwd: staged, name: "SekaiLink Bootloader" }], windowsZip);
if (shouldBuildLinux) tarGzFiles([{ cwd: staged, name: "SekaiLink Bootloader Linux" }], linuxTar);

const artifacts = [];
for (const filePath of [windowsZip, linuxTar].filter((candidate) => fs.existsSync(candidate))) {
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
  channel,
  build,
  implementation: "native-cpp-sdl2-miniz",
  base_url: baseUrl,
  windows_dlls: copiedDlls,
  artifacts,
};
const manifestFileName = `sekailink-bootstrapper-release-${dateDir}.json`;
fs.writeFileSync(path.join(outputDir, manifestFileName), `${JSON.stringify(manifest, null, 2)}\n`, "utf8");

const latestDir = path.join(releaseDir, "bootstrapper", channel, "latest");
fs.rmSync(latestDir, { recursive: true, force: true });
fs.mkdirSync(latestDir, { recursive: true });
for (const artifact of artifacts) {
  copyFile(path.join(outputDir, artifact.fileName), path.join(latestDir, artifact.fileName));
}
fs.writeFileSync(path.join(latestDir, "sekailink-bootstrapper-release-latest.json"), `${JSON.stringify(manifest, null, 2)}\n`, "utf8");
fs.rmSync(staged, { recursive: true, force: true });

for (const artifact of artifacts) {
  process.stdout.write(`[package-bootstrapper] ${artifact.fileName} ${artifact.sha256} ${artifact.size}\n`);
}
process.stdout.write(`[package-bootstrapper] windows dlls: ${copiedDlls.join(", ")}\n`);
