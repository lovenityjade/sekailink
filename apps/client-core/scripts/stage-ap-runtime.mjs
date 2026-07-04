import fs from "node:fs";
import path from "node:path";
import crypto from "node:crypto";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const appDir = path.resolve(__dirname, "..");
const coreRoot = path.resolve(appDir, "..");
const runtimeDir = path.join(coreRoot, "runtime");
const destRoot = path.join(runtimeDir, "ap");

const worldDirs = [
  "_bizhawk",
  "generic",
  "alttp",
  "dkc",
  "dkc2",
  "dkc3",
  "earthbound",
  "ff4fe",
  "ffta",
  "kdl3",
  "lufia2ac",
  "marioland2",
  "metroidfusion",
  "mm2",
  "mm3",
  "mzm",
  "oot",
  "pokemon_crystal",
  "pokemon_emerald",
  "pokemon_frlg",
  "pokemon_rb",
  "sm",
  "smw",
  "smz3",
  "tloz",
  "tloz_oos",
  "tmc",
  "wl",
  "wl4",
  "zelda2",
];

const topLevelFiles = [
  "BaseClasses.py",
  "Fill.py",
  "Generate.py",
  "Launcher.py",
  "Main.py",
  "ModuleUpdate.py",
  "MultiServer.py",
  "NetUtils.py",
  "Options.py",
  "Patch.py",
  "Utils.py",
  "settings.py",
  "host.yaml",
  "requirements.txt",
  "LICENSE",
  "LICENSE-original.md",
  "README.md",
];

const topLevelDirs = [
  "lib",
  "rule_builder",
];

const log = (msg) => process.stdout.write(`[stage-ap-runtime] ${msg}\n`);

const isValidApRoot = (dir) =>
  Boolean(
    dir &&
      fs.existsSync(path.join(dir, "Patch.py")) &&
      fs.existsSync(path.join(dir, "BaseClasses.py")) &&
      fs.existsSync(path.join(dir, "worlds", "Files.py")) &&
      fs.existsSync(path.join(dir, "worlds", "alttp")) &&
      fs.existsSync(path.join(dir, "worlds", "earthbound"))
  );

const shouldSkip = (srcPath) => {
  const normalized = srcPath.replace(/\\/g, "/");
  return (
    normalized.includes("/.git/") ||
    normalized.includes("/__pycache__/") ||
    normalized.includes("/test/") ||
    normalized.includes("/tests/") ||
    normalized.endsWith(".pyc") ||
    normalized.endsWith(".pyo")
  );
};

const copyFile = (src, dest) => {
  fs.mkdirSync(path.dirname(dest), { recursive: true });
  fs.copyFileSync(src, dest);
};

const copyTree = (srcDir, destDir) => {
  if (!fs.existsSync(srcDir)) return false;
  for (const entry of fs.readdirSync(srcDir, { withFileTypes: true })) {
    const src = path.join(srcDir, entry.name);
    if (shouldSkip(src)) continue;
    const dest = path.join(destDir, entry.name);
    if (entry.isDirectory()) {
      copyTree(src, dest);
    } else if (entry.isFile()) {
      copyFile(src, dest);
    }
  }
  return true;
};

const hashFile = (filePath) => {
  const hash = crypto.createHash("sha256");
  hash.update(fs.readFileSync(filePath));
  return hash.digest("hex");
};

const hashText = (value) => crypto.createHash("sha256").update(String(value || "")).digest("hex");

const findSourceRoot = () => {
  const envSource = String(process.env.SEKAILINK_AP_SOURCE_DIR || "").trim();
  if (envSource) return envSource;
  if (isValidApRoot(destRoot)) return "";
  return "";
};

const main = () => {
  const sourceRoot = findSourceRoot();
  const requireBundle = String(process.env.SEKAILINK_REQUIRE_AP_BUNDLE || "").trim() === "1";

  if (!sourceRoot) {
    if (isValidApRoot(destRoot)) {
      log(`ok: existing runtime/ap bundle present: ${destRoot}`);
      return;
    }
    const msg = "missing AP/MWGG source; set SEKAILINK_AP_SOURCE_DIR to a root containing Patch.py and worlds/";
    if (requireBundle) throw new Error(msg);
    log(`warn: ${msg}`);
    return;
  }

  if (!isValidApRoot(sourceRoot)) {
    throw new Error(`invalid AP/MWGG source root: ${sourceRoot}`);
  }

  fs.rmSync(destRoot, { recursive: true, force: true });
  fs.mkdirSync(destRoot, { recursive: true });

  const discoveredTopLevelPy = fs
    .readdirSync(sourceRoot, { withFileTypes: true })
    .filter((entry) => entry.isFile() && entry.name.endsWith(".py"))
    .map((entry) => entry.name);
  const filesToCopy = Array.from(new Set([...topLevelFiles, ...discoveredTopLevelPy]));

  for (const file of filesToCopy) {
    const src = path.join(sourceRoot, file);
    if (fs.existsSync(src) && fs.statSync(src).isFile()) {
      copyFile(src, path.join(destRoot, file));
    }
  }

  for (const dir of topLevelDirs) {
    copyTree(path.join(sourceRoot, dir), path.join(destRoot, dir));
  }

  copyTree(path.join(sourceRoot, "data"), path.join(destRoot, "data"));

  const worldsDest = path.join(destRoot, "worlds");
  fs.mkdirSync(worldsDest, { recursive: true });
  for (const file of ["__init__.py", "Files.py", "AutoWorld.py", "AutoSNIClient.py", "LauncherComponents.py"]) {
    const src = path.join(sourceRoot, "worlds", file);
    if (fs.existsSync(src) && fs.statSync(src).isFile()) {
      copyFile(src, path.join(worldsDest, file));
    }
  }

  const copiedWorlds = [];
  for (const world of worldDirs) {
    const copied = copyTree(path.join(sourceRoot, "worlds", world), path.join(worldsDest, world));
    if (copied) copiedWorlds.push(world);
  }

  const manifest = {
    generated_by: "sekailink-core/client-app/scripts/stage-ap-runtime.mjs",
    source_label: path.basename(sourceRoot),
    source_root_sha256: hashText(sourceRoot),
    top_level_files: filesToCopy.filter((file) => fs.existsSync(path.join(destRoot, file))),
    top_level_dirs: topLevelDirs.filter((dir) => fs.existsSync(path.join(destRoot, dir))),
    worlds: copiedWorlds,
    patch_sha256: hashFile(path.join(destRoot, "Patch.py")),
    files_sha256: hashFile(path.join(destRoot, "worlds", "Files.py")),
  };
  fs.writeFileSync(path.join(destRoot, "sekailink-ap-runtime.json"), `${JSON.stringify(manifest, null, 2)}\n`, "utf8");

  if (!isValidApRoot(destRoot)) {
    throw new Error(`staged AP/MWGG bundle is incomplete: ${destRoot}`);
  }
  log(`ok: staged AP/MWGG runtime at ${destRoot} (${copiedWorlds.length} worlds)`);
};

main();
