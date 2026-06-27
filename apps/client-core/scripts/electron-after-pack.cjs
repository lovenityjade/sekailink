const fs = require("fs");
const os = require("os");
const path = require("path");
const { spawnSync } = require("child_process");

const copyDir = (source, target) => {
  fs.rmSync(target, { recursive: true, force: true });
  fs.mkdirSync(path.dirname(target), { recursive: true });
  fs.cpSync(source, target, { recursive: true });
};

const run = (cmd, args, options = {}) => {
  const res = spawnSync(cmd, args, { stdio: "inherit", ...options });
  if (res.status !== 0) throw new Error(`${cmd} ${args.join(" ")} failed`);
};

const latestBootstrapperDir = (releaseDir) => {
  const root = path.join(releaseDir, "bootstrapper");
  if (!fs.existsSync(root)) return "";
  const dirs = fs.readdirSync(root, { withFileTypes: true })
    .filter((entry) => entry.isDirectory() && /^\d{8}$/.test(entry.name))
    .map((entry) => entry.name)
    .sort();
  return dirs.length ? path.join(root, dirs[dirs.length - 1]) : "";
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
