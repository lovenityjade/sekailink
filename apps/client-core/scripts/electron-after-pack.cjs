const fs = require("fs");
const path = require("path");

exports.default = async function afterPack(context) {
  const appOutDir = String(context?.appOutDir || "").trim();
  if (!appOutDir) return;

  const appDir = path.resolve(__dirname, "..");
  const bootstrapperDir = path.join(appDir, "bootstrapper");
  const platform = String(context?.electronPlatformName || "").trim();
  const files = platform === "win32"
    ? ["SekaiLink-bootstrapper.cmd", "SekaiLink-bootstrapper.ps1"]
    : ["sekailink-bootstrapper.sh"];

  for (const name of files) {
    const source = path.join(bootstrapperDir, name);
    if (!fs.existsSync(source)) continue;
    const target = path.join(appOutDir, name);
    fs.copyFileSync(source, target);
    if (name.endsWith(".sh")) {
      try {
        fs.chmodSync(target, 0o755);
      } catch (_err) {
        // best effort for non-POSIX build hosts
      }
    }
  }
};
