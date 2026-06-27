import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

export const windowsRuntimePlatform = "win32-x64";
export const windowsRuntimeManifestName = "runtime-manifest.json";

export const windowsRuntimeDlls = [
  "libarchive-13.dll",
  "libgcc_s_seh-1.dll",
  "libstdc++-6.dll",
  "libwinpthread-1.dll",
  "SDL2.dll",
  "SDL2_image.dll",
  "libb2-1.dll",
  "libbz2-1.dll",
  "libcrypto-3-x64.dll",
  "libexpat-1.dll",
  "libiconv-2.dll",
  "liblz4.dll",
  "liblzma-5.dll",
  "zlib1.dll",
  "libzstd.dll",
  "libssl-3-x64.dll",
  "libpng16-16.dll",
  "libwebp-7.dll",
  "libwebpdemux-2.dll",
  "libsharpyuv-0.dll",
  "libjxl.dll",
  "libjxl_cms.dll",
  "libbrotlidec.dll",
  "libbrotlicommon.dll",
  "libavif-16.dll",
  "libtiff-6.dll",
  "libjpeg-8.dll",
  "libdav1d-7.dll",
  "librav1e.dll",
  "libdeflate.dll",
  "libjbig-0.dll",
  "libLerc.dll",
  "libyuv.dll",
  "libbrotlienc.dll",
  "libhwy.dll",
  "liblcms2-2.dll",
  "libSvtAv1Enc-4.dll",
  "libaom.dll",
];

export const windowsRuntimeRequiredFiles = [
  "bin/sekaiemu_libretro_spike.exe",
  "bin/sekailink_sklmi_runtime.exe",
  "bin/luajit.exe",
  "bin/lua51.dll",
  "bin/tracker_poptracker_eval.lua",
  ...windowsRuntimeDlls.map((name) => `bin/${name}`),
];

export const windowsRuntimeRequiredDirs = [
  "bin/tracker_poptracker_eval_parts",
];

export const windowsRuntimeBuildTimeSource = "MSYS2 UCRT64";

export const windowsRuntimeManifestPath = (runtimeRoot, platform = windowsRuntimePlatform) =>
  path.join(runtimeRoot, "platforms", platform, windowsRuntimeManifestName);

export const sha256File = (filePath) => {
  const hash = crypto.createHash("sha256");
  hash.update(fs.readFileSync(filePath));
  return hash.digest("hex");
};

export const buildWindowsRuntimeManifest = ({ runtimeRoot, platform = windowsRuntimePlatform, sourceBinDir = "" }) => {
  const platformRoot = path.join(runtimeRoot, "platforms", platform);
  const fileEntries = windowsRuntimeRequiredFiles.map((relativePath) => {
    const abs = path.join(platformRoot, relativePath);
    const present = fs.existsSync(abs) && fs.statSync(abs).isFile();
    return {
      path: relativePath.replaceAll("\\", "/"),
      required: true,
      sha256: present ? sha256File(abs) : null,
    };
  });

  return {
    schema: "sekailink.runtime.windows.v1",
    platform,
    buildTimeDependencySource: windowsRuntimeBuildTimeSource,
    sourceBinDir,
    generatedAt: new Date().toISOString(),
    notes: [
      "These files are staged at build time. End users must not install MSYS2, LuaJIT, Python, or extra DLLs.",
      "ALTTP may be used as the release fixture, but runtime semantics remain generic APWorld and PopTracker pack compatibility.",
    ],
    files: fileEntries,
    directories: windowsRuntimeRequiredDirs.map((relativePath) => ({
      path: relativePath.replaceAll("\\", "/"),
      required: true,
    })),
  };
};

export const writeWindowsRuntimeManifest = ({ runtimeRoot, platform = windowsRuntimePlatform, sourceBinDir = "" }) => {
  const manifestPath = windowsRuntimeManifestPath(runtimeRoot, platform);
  fs.mkdirSync(path.dirname(manifestPath), { recursive: true });
  fs.writeFileSync(
    manifestPath,
    `${JSON.stringify(buildWindowsRuntimeManifest({ runtimeRoot, platform, sourceBinDir }), null, 2)}\n`,
  );
  return manifestPath;
};
