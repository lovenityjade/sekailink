import fs from "node:fs";
import path from "node:path";
import { execFileSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const appDir = path.resolve(__dirname, "..");
const repoRoot = path.resolve(appDir, "..", "..");

const runtimeDir = path.join(repoRoot, "client", "runtime");
const bundledDir = path.join(runtimeDir, "_bundled_libs", "bizhawk");
const bundledPopTrackerDir = path.join(runtimeDir, "_bundled_libs", "poptracker");
const thirdPartyDir = path.join(repoRoot, "third_party");

const log = (msg) => process.stdout.write(`[prepare-native-deps] ${msg}\n`);

const resolveFromLdconfig = (soname) => {
  try {
    const out = execFileSync("ldconfig", ["-p"], { encoding: "utf8" });
    const re = new RegExp(`${soname.replaceAll(".", "\\.")}\\s*\\([^)]*\\)\\s*=>\\s*(\\S+)`, "i");
    const m = out.match(re);
    if (m && m[1]) return String(m[1]).trim();
  } catch (_err) {
    // ignore (ldconfig may not exist in minimal envs)
  }
  return "";
};

const firstExisting = (candidates) => {
  for (const c of candidates) {
    const p = String(c || "").trim();
    if (!p) continue;
    try {
      if (fs.existsSync(p)) return p;
    } catch (_err) {
      // ignore
    }
  }
  return "";
};

const copyLibIfPresent = (soname, destName, options = {}) => {
  const override = String(options.overridePath || "").trim();
  const destRoot = String(options.destDir || bundledDir).trim() || bundledDir;
  const fromLd = resolveFromLdconfig(soname);
  const src = firstExisting([
    override,
    fromLd,
    `/lib64/${soname}`,
    `/usr/lib64/${soname}`,
    `/lib/${soname}`,
    `/usr/lib/${soname}`,
    `/usr/lib/x86_64-linux-gnu/${soname}`,
  ]);

  if (!src) {
    log(`skip: ${soname} not found (set SEKAILINK_BUNDLE_LIBGDIPLUS_PATH to override)`);
    return { ok: false, skipped: true };
  }

  const realSrc = fs.realpathSync(src);
  fs.mkdirSync(destRoot, { recursive: true });
  const dest = path.join(destRoot, destName || soname);
  fs.copyFileSync(realSrc, dest);
  try {
    fs.chmodSync(dest, 0o755);
  } catch (_err) {
    // ignore
  }
  log(`bundled: ${soname} <- ${realSrc}`);
  return { ok: true, dest };
};

if (process.platform !== "linux") {
  log(`skip: platform=${process.platform} (linux only)`);
  process.exit(0);
}

// BizHawk on Linux runs WinForms via mono and typically requires libgdiplus.
copyLibIfPresent("libgdiplus.so.0", "libgdiplus.so.0", {
  overridePath: process.env.SEKAILINK_BUNDLE_LIBGDIPLUS_PATH || "",
  destDir: bundledDir,
});

// PopTracker on Linux often depends on SDL2_ttf/image stacks not present on all distros/handheld images.
// Bundle a focused set of libs and add them to LD_LIBRARY_PATH at launch time.
const popTrackerLibs = [
  "libSDL2-2.0.so.0",
  "libSDL2_ttf-2.0.so.0",
  "libSDL2_image-2.0.so.0",
  "libharfbuzz.so.0",
  "libfreetype.so.6",
  "libjpeg.so.62",
  "libpng16.so.16",
  "libwebp.so.7",
  "libwebpdemux.so.2",
  "libtiff.so.6",
  "libjxl.so.0.11",
  "libjxl_cms.so.0.11",
  "libbrotlidec.so.1",
  "libbrotlicommon.so.1",
  "libzstd.so.1",
];
for (const soname of popTrackerLibs) {
  copyLibIfPresent(soname, soname, { destDir: bundledPopTrackerDir });
}

// SekaiLink bundles a pinned mono runtime so BizHawk can run without system installs.
// We fail packaging early if the expected mono binary is missing.
const archRaw = process.arch === "x64" ? "x86_64" : process.arch === "arm64" ? "arm64" : process.arch;
const monoRoot = path.join(thirdPartyDir, "mono", `sekailink-mono-linux-${archRaw}`);
const monoBin = path.join(monoRoot, "bin", "mono");

const copyTree = (srcDir, destDir) => {
  fs.rmSync(destDir, { recursive: true, force: true });
  fs.mkdirSync(destDir, { recursive: true });
  const cp = spawnSync("cp", ["-a", path.join(srcDir, "."), destDir], { stdio: "inherit" });
  if (cp.status !== 0) throw new Error(`copy_failed: ${srcDir} -> ${destDir}`);
};

const extractZip = (zipPath, destDir) => {
  fs.mkdirSync(destDir, { recursive: true });
  const unzip = spawnSync("unzip", ["-q", zipPath, "-d", destDir], { stdio: "inherit" });
  if (unzip.status === 0) return;
  const py = spawnSync("python3", ["-m", "zipfile", "-e", zipPath, destDir], { stdio: "inherit" });
  if (py.status !== 0) throw new Error(`extract_failed: ${zipPath}`);
};

// Allow the build pipeline to inject the mono runtime without committing it to git.
// - SEKAILINK_MONO_PORTABLE_DIR: points to an extracted folder with bin/mono
// - SEKAILINK_MONO_PORTABLE_ZIP: points to a zip that contains top-level sekailink-mono-linux-${archRaw}/
if (!fs.existsSync(monoBin)) {
  const injectedDir = String(process.env.SEKAILINK_MONO_PORTABLE_DIR || "").trim();
  const injectedZip = String(process.env.SEKAILINK_MONO_PORTABLE_ZIP || "").trim();

  try {
    if (injectedDir && fs.existsSync(path.join(injectedDir, "bin", "mono"))) {
      log(`inject: mono from dir: ${injectedDir}`);
      copyTree(injectedDir, monoRoot);
    } else if (injectedZip && fs.existsSync(injectedZip)) {
      log(`inject: mono from zip: ${injectedZip}`);
      const tmp = fs.mkdtempSync(path.join(path.resolve(process.env.TMPDIR || "/tmp"), "sekailink-mono-"));
      extractZip(injectedZip, tmp);
      const extracted = path.join(tmp, `sekailink-mono-linux-${archRaw}`);
      if (!fs.existsSync(path.join(extracted, "bin", "mono"))) {
        throw new Error(`unexpected_zip_layout: missing ${extracted}/bin/mono`);
      }
      copyTree(extracted, monoRoot);
    }
  } catch (err) {
    log(`warn: mono inject failed: ${String(err?.message || err)}`);
  }
}

if (!fs.existsSync(monoBin)) {
  log(`error: bundled mono missing at: ${monoBin}`);
  log(`hint: extract a pinned mono portable runtime into: ${monoRoot}`);
  log(`hint: or set SEKAILINK_MONO_PORTABLE_DIR=/path/to/sekailink-mono-linux-${archRaw}`);
  log(`hint: or set SEKAILINK_MONO_PORTABLE_ZIP=/path/to/sekailink-mono-linux-${archRaw}.zip`);
  log(`hint: expected layout: ${monoRoot}/bin/mono (plus lib/ and etc/ as needed)`);
  process.exit(2);
}
log(`ok: bundled mono present: ${monoBin}`);

// Windows PopTracker runtime DLLs (needed at runtime on Windows; without them PopTracker exits with 0xc000007b).
// Even though this script runs on Linux, it also prepares the Windows extraResources tree before packaging.
try {
  const popWin64Dir = path.join(thirdPartyDir, "PopTracker", "build", "win64");
  const popExe = path.join(popWin64Dir, "poptracker.exe");
  if (fs.existsSync(popExe)) {
    const mingwBin = "/usr/x86_64-w64-mingw32/sys-root/mingw/bin";

    // NOTE: In this sandbox environment, calling objdump from Node may be blocked.
    // Keep a curated DLL list derived from PopTracker's dependency closure.
    const dlls = [
      // mingw runtime
      "libgcc_s_seh-1.dll",
      "libstdc++-6.dll",
      "libwinpthread-1.dll",

      // base deps
      "SDL2.dll",
      "SDL2_image.dll",
      "SDL2_ttf.dll",
      "zlib1.dll",
      "libssl-3-x64.dll",
      "libcrypto-3-x64.dll",

      // SDL2_image.dll
      "libtiff-5.dll",
      "libwebp-7.dll",
      "libwebpdemux-2.dll",

      // SDL2_ttf.dll
      "libfreetype-6.dll",
      "libharfbuzz-0.dll",

      // transitive deps
      "libjpeg-62.dll",
      "libsharpyuv-0.dll",
      "libbz2-1.dll",
      "libpng16-16.dll",
      "libglib-2.0-0.dll",
      "libintl-8.dll",
      "libpcre2-8-0.dll",
      "iconv.dll",
    ];

    for (const name of dlls) {
      const src = path.join(mingwBin, name);
      const dst = path.join(popWin64Dir, name);
      if (!fs.existsSync(src)) {
        log(`warn: poptracker win64 dll missing: ${src}`);
        continue;
      }
      if (fs.existsSync(dst)) continue;
      fs.copyFileSync(src, dst);
      log(`bundled: poptracker-win64 ${name} <- ${src}`);
    }
  } else {
    log(`skip: poptracker win64 exe not found at: ${popExe}`);
  }
} catch (err) {
  log(`warn: poptracker win64 dll staging failed: ${String(err?.message || err)}`);
}
