const { existsSync } = require("node:fs");
const path = require("node:path");
const { spawnSync } = require("node:child_process");

if (!existsSync(path.join(process.cwd(), ".git"))) {
  process.exit(0);
}

const huskyBin = path.join(
  process.cwd(),
  "node_modules",
  ".bin",
  process.platform === "win32" ? "husky.cmd" : "husky"
);

if (!existsSync(huskyBin)) {
  process.exit(0);
}

const result = spawnSync(huskyBin, [], {
  stdio: "inherit",
  shell: process.platform === "win32",
});

if (result.error) {
  console.warn(`husky prepare skipped: ${result.error.message}`);
  process.exit(0);
}

process.exit(result.status ?? 0);
