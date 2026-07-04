const fs = require("fs");
const os = require("os");
const path = require("path");
const { spawnSync } = require("child_process");

const copyDir = (source, target) => {
  fs.rmSync(target, { recursive: true, force: true });
  fs.mkdirSync(path.dirname(target), { recursive: true });
  fs.cpSync(source, target, { recursive: true });
};

const copyFileIfExists = (source, target) => {
  if (!fs.existsSync(source)) return false;
  fs.mkdirSync(path.dirname(target), { recursive: true });
  fs.copyFileSync(source, target);
  return true;
};

const stageWindowsBootloaderDlls = (appOutDir) => {
  const bootloaderDir = path.join(appOutDir, "SekaiLink Bootloader");
  if (!fs.existsSync(path.join(bootloaderDir, "SekaiLink Bootloader.exe"))) return;

  const runtimeBinDir = path.join(appOutDir, "resources", "runtime", "platforms", "win32-x64", "bin");
  const msysRoots = [
    process.env.SEKAILINK_MSYS2_UCRT_ROOT,
    "C:\\msys64\\ucrt64",
  ].filter(Boolean);
  const required = [
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

  for (const name of required) {
    const target = path.join(bootloaderDir, name);
    if (copyFileIfExists(path.join(runtimeBinDir, name), target)) continue;
    for (const root of msysRoots) {
      if (copyFileIfExists(path.join(root, "bin", name), target)) break;
    }
  }

  copyFileIfExists(path.join(runtimeBinDir, "ca-bundle.crt"), path.join(bootloaderDir, "ca-bundle.crt"));
  for (const root of msysRoots) {
    if (copyFileIfExists(path.join(root, "etc", "ssl", "certs", "ca-bundle.crt"), path.join(bootloaderDir, "ca-bundle.crt"))) break;
  }
};

const run = (cmd, args, options = {}) => {
  const res = spawnSync(cmd, args, { stdio: "inherit", ...options });
  if (res.status !== 0) throw new Error(`${cmd} ${args.join(" ")} failed`);
};

const latestBootstrapperDir = (releaseDir) => {
  const normalizeReleaseChannel = (value) => {
    const raw = String(value || "").trim().toLowerCase();
    if (raw === "canary") return "canari";
    if (raw === "test" || raw === "stable" || raw === "release") return "canonical";
    return raw === "canari" ? "canari" : "canonical";
  };
  const root = path.join(releaseDir, "bootstrapper");
  const channelRoot = path.join(root, normalizeReleaseChannel(process.env.SEKAILINK_RELEASE_CHANNEL || "canonical"));
  const searchRoot = fs.existsSync(channelRoot) ? channelRoot : root;
  if (!fs.existsSync(searchRoot)) return "";
  const dirs = fs.readdirSync(searchRoot, { withFileTypes: true })
    .filter((entry) => entry.isDirectory() && /^\d{8}$/.test(entry.name))
    .map((entry) => entry.name)
    .sort();
  return dirs.length ? path.join(searchRoot, dirs[dirs.length - 1]) : "";
};

const findArtifact = (dir, suffix) => {
  if (!dir || !fs.existsSync(dir)) return "";
  const files = fs.readdirSync(dir)
    .filter((name) => name.startsWith("SekaiLink-bootstrapper-") && name.endsWith(suffix))
    .sort();
  return files.length ? path.join(dir, files[files.length - 1]) : "";
};

const stageNativeBootloader = (platform, appOutDir, releaseDir) => {
  const bootstrapperDir = latestBootstrapperDir(releaseDir);
  if (!bootstrapperDir) {
    throw new Error("native_bootloader_artifacts_missing: run npm run bootstrapper:pack first");
  }

  const temp = fs.mkdtempSync(path.join(os.tmpdir(), "sekailink-bootloader-stage-"));
  try {
    if (platform === "win32") {
      const zip = findArtifact(bootstrapperDir, "-windows.zip");
      if (!zip) throw new Error("native_windows_bootloader_zip_missing");
      run("unzip", ["-q", "-o", zip, "-d", temp]);
      const source = path.join(temp, "SekaiLink Bootloader");
      if (!fs.existsSync(path.join(source, "SekaiLink Bootloader.exe"))) {
        throw new Error("native_windows_bootloader_exe_missing");
      }
      copyDir(source, path.join(appOutDir, "SekaiLink Bootloader"));
      stageWindowsBootloaderDlls(appOutDir);
      return;
    }

    const tar = findArtifact(bootstrapperDir, "-linux.tar.gz");
    if (!tar) throw new Error("native_linux_bootloader_tar_missing");
    run("tar", ["-xzf", tar, "-C", temp]);
    const sourceCandidates = [
      path.join(temp, "SekaiLink Bootloader"),
      path.join(temp, "SekaiLink Bootloader Linux"),
    ];
    const source = sourceCandidates.find((candidate) => fs.existsSync(path.join(candidate, "sekailink-bootloader")));
    if (!source) throw new Error("native_linux_bootloader_exe_missing");
    copyDir(source, path.join(appOutDir, "SekaiLink Bootloader"));
    fs.chmodSync(path.join(appOutDir, "SekaiLink Bootloader", "sekailink-bootloader"), 0o755);
  } finally {
    fs.rmSync(temp, { recursive: true, force: true });
  }
};

exports.default = async function afterPack(context) {
  const appOutDir = String(context?.appOutDir || "").trim();
  if (!appOutDir) return;

  const appDir = path.resolve(__dirname, "..");
  const releaseDir = path.join(appDir, "release");
  const platform = String(context?.electronPlatformName || "").trim();
  stageNativeBootloader(platform, appOutDir, releaseDir);
};
