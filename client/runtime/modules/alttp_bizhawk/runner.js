// Runner contract for ALttP (BizHawk)
// Functions should be wired by the Electron main process.

export async function prepare(_ctx) {
  // TODO: download patch -> apply -> produce ROM
}

export async function launchEmu(_ctx) {
  // TODO: launch BizHawk with patched ROM and auto-load Lua connector
}

export async function launchTracker(_ctx) {
  // TODO: launch PopTracker with pack + AP auto-connect
}

export async function cleanup(_ctx) {
  // TODO: stop processes
}
