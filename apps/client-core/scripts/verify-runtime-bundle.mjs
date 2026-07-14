#!/usr/bin/env node
import fs from "node:fs";
import path from "node:path";
import { spawnSync } from "node:child_process";

const args = process.argv.slice(2);
const catalogPath = new URL("../src/data/sekailinkGameCatalog.ts", import.meta.url);

const readArg = (name, fallback = "") => {
  const index = args.indexOf(name);
  if (index === -1) return fallback;
  return args[index + 1] || "";
};

const runtimeRoot = path.resolve(readArg("--root", path.resolve(process.cwd(), "../../runtime")));
const platform = readArg("--platform", `${process.platform}-${process.arch === "x64" ? "x64" : process.arch}`);

const errors = [];
const warnings = [];

const isWindows = platform.startsWith("win32-");
const platformRoot = path.join(runtimeRoot, "platforms", platform);

const exists = (p) => {
  try {
    return fs.existsSync(p);
  } catch {
    return false;
  }
};

const isDir = (p) => {
  try {
    return fs.statSync(p).isDirectory();
  } catch {
    return false;
  }
};

const isFile = (p) => {
  try {
    return fs.statSync(p).isFile();
  } catch {
    return false;
  }
};

const readJson = (p) => {
  try {
    return JSON.parse(fs.readFileSync(p, "utf8").replace(/^\uFEFF/, ""));
  } catch (err) {
    errors.push(`invalid_json:${p}:${String(err?.message || err)}`);
    return null;
  }
};

const readForcedUnavailableCatalogKeys = () => {
  try {
    const source = fs.readFileSync(catalogPath, "utf8");
    return new Set(
      Array.from(source.matchAll(/\{\s*key:\s*['"]([^'"]+)['"][^}]*forceUnavailable:\s*true[^}]*\}/g))
        .map((match) => match[1])
        .filter(Boolean),
    );
  } catch (err) {
    warnings.push(`catalog_force_unavailable_unreadable:${catalogPath.pathname}:${String(err?.message || err)}`);
    return new Set();
  }
};

const fileMagic = (p) => {
  try {
    return fs.readFileSync(p).subarray(0, 4);
  } catch {
    return Buffer.alloc(0);
  }
};

const fileKind = (p) => {
  const magic = fileMagic(p);
  if (magic.length >= 4 && magic[0] === 0x7f && magic[1] === 0x45 && magic[2] === 0x4c && magic[3] === 0x46) {
    return "elf";
  }
  if (magic.length >= 2 && magic[0] === 0x4d && magic[1] === 0x5a) {
    return "pe";
  }
  return "unknown";
};

const windowsSystemDlls = new Set(
  [
    "advapi32.dll",
    "bcrypt.dll",
    "comctl32.dll",
    "comdlg32.dll",
    "crypt32.dll",
    "gdi32.dll",
    "imm32.dll",
    "iphlpapi.dll",
    "kernel32.dll",
    "msvcrt.dll",
    "ntdll.dll",
    "ole32.dll",
    "oleaut32.dll",
    "opengl32.dll",
    "rpcrt4.dll",
    "setupapi.dll",
    "shell32.dll",
    "shlwapi.dll",
    "user32.dll",
    "version.dll",
    "winmm.dll",
    "ws2_32.dll",
  ].map((name) => name.toLowerCase()),
);

const isWindowsApiSetDll = (name) => /^api-ms-win-/i.test(String(name || ""));

const readWindowsDllImports = (p) => {
  if (!isWindows) return [];
  const candidates = [
    "C:\\msys64\\ucrt64\\bin\\objdump.exe",
    "C:\\msys64\\usr\\bin\\objdump.exe",
    "/usr/bin/x86_64-w64-mingw32-objdump",
    "x86_64-w64-mingw32-objdump",
    "objdump",
  ];
  for (const cmd of candidates) {
    const res = spawnSync(cmd, ["-p", p], { encoding: "utf8" });
    if (res.status !== 0 || !res.stdout) continue;
    return Array.from(String(res.stdout).matchAll(/DLL Name:\s*(.+)/g))
      .map((match) => String(match[1] || "").trim())
      .filter(Boolean);
  }
  warnings.push("windows_import_check_skipped:objdump_unavailable");
  return [];
};

const verifyWindowsDllImports = (label, binaryPath) => {
  if (!isWindows || !binaryPath) return;
  for (const imported of readWindowsDllImports(binaryPath)) {
    const lower = imported.toLowerCase();
    if (windowsSystemDlls.has(lower) || isWindowsApiSetDll(imported)) continue;
    const localPath = path.join(path.dirname(binaryPath), imported);
    const platformBinPath = path.join(platformRoot, "bin", imported);
    const runtimeBinPath = path.join(runtimeRoot, "bin", imported);
    if (!isFile(localPath) && !isFile(platformBinPath) && !isFile(runtimeBinPath)) {
      errors.push(`missing_imported_dll_${label}:${imported}:required_by:${binaryPath}`);
    }
  }
};

const requirePath = (label, p, predicate = exists) => {
  if (!predicate(p)) {
    errors.push(`missing_${label}:${p}`);
    return false;
  }
  return true;
};

const candidate = (...parts) => [
  path.join(platformRoot, ...parts),
  path.join(runtimeRoot, ...parts),
];

const firstFile = (...pathsToTry) => pathsToTry.find((p) => isFile(p)) || "";
const firstDir = (...pathsToTry) => pathsToTry.find((p) => isDir(p)) || "";
const firstPath = (...pathsToTry) => pathsToTry.find((p) => exists(p)) || "";

const requireBinary = (label, pathsToTry, expectedKind) => {
  const p = firstFile(...pathsToTry);
  if (!p) {
    errors.push(`missing_${label}:${pathsToTry.join("|")}`);
    return "";
  }
  const kind = fileKind(p);
  if (kind !== expectedKind) {
    errors.push(`invalid_${label}_format:${p}:expected_${expectedKind}:got_${kind}`);
  }
  verifyWindowsDllImports(label, p);
  return p;
};

const resolveRuntimePath = (declared) => {
  const safe = String(declared || "").trim();
  if (!safe) return "";
  if (path.isAbsolute(safe)) return exists(safe) ? safe : "";
  const direct = firstPath(path.join(runtimeRoot, safe), path.join(platformRoot, safe));
  if (direct) return direct;
  if (safe.toLowerCase().endsWith(".zip")) {
    return firstPath(
      path.join(runtimeRoot, safe.slice(0, -4)),
      path.join(platformRoot, safe.slice(0, -4)),
    );
  }
  return "";
};

const libretroCoreNames = (coreId) => {
  const ext = isWindows ? ".dll" : ".so";
  const names = (...baseNames) => baseNames.map((name) => `${name}${ext}`);
  const aliases = {
    bsnes: names("bsnes_mercury_performance_libretro", "bsnes_mercury_balanced_libretro", "bsnes_libretro", "snes9x_libretro"),
    snes: names("bsnes_mercury_performance_libretro", "bsnes_mercury_balanced_libretro", "snes9x_libretro"),
    snes9x: names("snes9x_libretro", "bsnes_mercury_performance_libretro", "bsnes_mercury_balanced_libretro"),
    gba: names("mgba_libretro"),
    mgba: names("mgba_libretro"),
    gb: names("gambatte_libretro", "mgba_libretro"),
    gbc: names("gambatte_libretro", "mgba_libretro"),
    nes: names("fceumm_libretro", "nestopia_libretro"),
    fceumm: names("fceumm_libretro"),
    nestopia: names("nestopia_libretro", "fceumm_libretro"),
    n64: names("mupen64plus_next_libretro"),
    mupen64plus_next: names("mupen64plus_next_libretro"),
  };
  return aliases[String(coreId || "").trim().toLowerCase()] || (coreId ? [String(coreId)] : []);
};

requirePath("runtime_root", runtimeRoot, isDir);
requirePath("modules_dir", path.join(runtimeRoot, "modules"), isDir);
requirePath("ap_patch", path.join(runtimeRoot, "ap", "Patch.py"), isFile);
requirePath("sklmi_manifest_dir", firstDir(...candidate("sklmi", "manifests"), path.join(runtimeRoot, "manifests", "sklmi"), path.join(runtimeRoot, "manifests")), isDir);

const expectedKind = isWindows ? "pe" : "elf";
const sekaiemuName = isWindows ? "sekaiemu_libretro_spike.exe" : "sekaiemu_libretro_spike";
const sklmiName = isWindows ? "sekailink_sklmi_runtime.exe" : "sekailink_sklmi_runtime";

const modulesDir = path.join(runtimeRoot, "modules");
const modules = isDir(modulesDir)
  ? fs.readdirSync(modulesDir, { withFileTypes: true }).filter((entry) => entry.isDirectory()).map((entry) => entry.name)
  : [];
const forcedUnavailableModuleIds = readForcedUnavailableCatalogKeys();

let hasSekaiemuModule = false;

for (const moduleId of modules) {
  const manifestPath = path.join(modulesDir, moduleId, "manifest.json");
  if (!requirePath("module_manifest", manifestPath, isFile)) continue;
  const manifest = readJson(manifestPath);
  if (!manifest) continue;
  const gameId = String(manifest.game_id || moduleId).trim();
  const availabilityStatus = String(manifest.availability_status || "").trim().toLowerCase();
  if (
    forcedUnavailableModuleIds.has(moduleId) ||
    forcedUnavailableModuleIds.has(gameId) ||
    ["unavailable", "disabled", "in_learning"].includes(availabilityStatus)
  ) {
    warnings.push(`skip_force_unavailable_module:${moduleId}`);
    continue;
  }

  const emu = String(manifest.emu || "").trim().toLowerCase();
  const sekaiemu = manifest.sekaiemu && typeof manifest.sekaiemu === "object" ? manifest.sekaiemu : {};
  if (emu === "sekaiemu" || emu === "sekaiemu-libretro") {
    hasSekaiemuModule = true;
    const profile = String(sekaiemu.profile || "").trim();
    if (profile) {
      const resolvedProfile = resolveRuntimePath(profile);
      if (!resolvedProfile || !isFile(resolvedProfile)) errors.push(`missing_sekaiemu_profile:${moduleId}:${profile}`);
    }

    const declaredTrackerPack = String(
      sekaiemu.tracker_pack ||
        sekaiemu.tracker_pack_path ||
        manifest.tracker_pack_path ||
        "",
    ).trim();
    if (declaredTrackerPack) {
      const resolvedTrackerPack = resolveRuntimePath(declaredTrackerPack);
      if (!resolvedTrackerPack) errors.push(`missing_sekaiemu_tracker_pack:${moduleId}:${declaredTrackerPack}`);
    }

    const corePath = String(sekaiemu.core_path || manifest.libretro_core_path || "").trim();
    const coreCandidates = corePath
      ? [resolveRuntimePath(corePath)].filter(Boolean)
      : libretroCoreNames(sekaiemu.core_id || manifest.libretro_core_id || manifest?.driver?.core_id)
          .flatMap((name) => candidate("cores", name));
    requireBinary(`libretro_core_${moduleId}`, coreCandidates, expectedKind);
  }

  if (manifest.tracker_pack_uid || manifest.tracker_pack_url) {
    warnings.push(
      `external_tracker_pack_source_not_bundled:${moduleId}:${String(
        manifest.tracker_pack_url || manifest.tracker_pack_uid || "",
      ).trim()}`,
    );
  }
}

if (hasSekaiemuModule) {
  requireBinary("sekaiemu", candidate("bin", sekaiemuName), expectedKind);
  requireBinary("sklmi_runtime", candidate("bin", sklmiName), expectedKind);
}

if (isWindows) {
  for (const linuxOnly of [
    path.join(runtimeRoot, "bin", "sekaiemu_libretro_spike"),
    path.join(runtimeRoot, "bin", "sekailink_sklmi_runtime"),
    path.join(runtimeRoot, "poptracker", "poptracker"),
  ]) {
    if (isFile(linuxOnly) && fileKind(linuxOnly) === "elf") {
      warnings.push(`linux_binary_present_in_windows_bundle:${linuxOnly}`);
    }
  }
}

if (warnings.length) {
  for (const warning of warnings) console.warn(`[verify-runtime-bundle] warn ${warning}`);
}

if (errors.length) {
  for (const error of errors) console.error(`[verify-runtime-bundle] error ${error}`);
  process.exit(1);
}

console.log(`[verify-runtime-bundle] ok root=${runtimeRoot} platform=${platform}`);
