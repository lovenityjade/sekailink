"use strict";

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

const createWindowingRuntime = (deps = {}) => {
  const {
    fs,
    path,
    spawn,
    spawnSync,
    screen,
    processRef = process,
    readConfig,
    writeLogLine,
  } = deps;
  const process = processRef;
  let _gamescopePathCache = undefined;
  let _wmctrlPathCache = undefined;

  const getGamescopePath = () => {
    if (_gamescopePathCache !== undefined) return _gamescopePathCache;
    const fromEnv = process.env.SEKAILINK_GAMESCOPE;
    if (fromEnv && fs.existsSync(fromEnv)) {
      _gamescopePathCache = fromEnv;
      return _gamescopePathCache;
    }
    try {
      const res = spawnSync("which", ["gamescope"], { stdio: ["ignore", "pipe", "ignore"] });
      if (res.status === 0) {
        const found = String(res.stdout || "").trim();
        _gamescopePathCache = found || null;
        return _gamescopePathCache;
      }
    } catch (_err) {
      // ignore
    }
    _gamescopePathCache = null;
    return _gamescopePathCache;
  };
  
  const spawnMaybeGamescope = (cmd, cmdArgs, spawnOpts = {}) => {
    const config = readConfig();
    const windowing = config.windowing || {};
    const gamescopeCfg = windowing.gamescope || {};
  
    if (!gamescopeCfg?.enabled) {
      return { ok: true, proc: spawn(cmd, cmdArgs, spawnOpts), wrapped: false };
    }
  
    const gamescopePath = getGamescopePath();
    const mode = gamescopeCfg?.mode || "prefer"; // "prefer" | "require"
    if (!gamescopePath) {
      if (mode === "require") return { ok: false, error: "gamescope_missing" };
      return { ok: true, proc: spawn(cmd, cmdArgs, spawnOpts), wrapped: false };
    }
  
    const gsArgs = [];
    if (gamescopeCfg?.fullscreen) gsArgs.push("-f");
    if (Number.isFinite(gamescopeCfg?.width)) gsArgs.push("-W", String(gamescopeCfg.width));
    if (Number.isFinite(gamescopeCfg?.height)) gsArgs.push("-H", String(gamescopeCfg.height));
    if (Array.isArray(gamescopeCfg?.args)) {
      for (const a of gamescopeCfg.args) {
        const s = String(a || "").trim();
        if (s) gsArgs.push(s);
      }
    }
    gsArgs.push("--", cmd, ...cmdArgs);
  
    try {
      return { ok: true, proc: spawn(gamescopePath, gsArgs, spawnOpts), wrapped: true };
    } catch (err) {
      if (mode === "require") return { ok: false, error: "gamescope_spawn_failed", detail: String(err) };
      return { ok: true, proc: spawn(cmd, cmdArgs, spawnOpts), wrapped: false };
    }
  };
  
  const getWmctrlPath = () => {
    if (_wmctrlPathCache !== undefined) return _wmctrlPathCache;
    try {
      const res = spawnSync("which", ["wmctrl"], { stdio: ["ignore", "pipe", "ignore"] });
      if (res.status === 0) {
        const found = String(res.stdout || "").trim();
        _wmctrlPathCache = found || null;
        return _wmctrlPathCache;
      }
    } catch (_err) {
      // ignore
    }
    _wmctrlPathCache = null;
    return _wmctrlPathCache;
  };
  
  const which = (cmd) => {
    try {
      const res = spawnSync("which", [cmd], { stdio: ["ignore", "pipe", "ignore"] });
      if (res.status === 0) return String(res.stdout || "").trim();
    } catch (_err) {
      // ignore
    }
    return "";
  };
  
  const wmctrlFindWindowIdByPid = async (pid, timeoutMs = 5000) => {
    const wmctrl = getWmctrlPath();
    if (!wmctrl) return null;
    const end = Date.now() + timeoutMs;
    while (Date.now() < end) {
      try {
        const res = spawnSync(wmctrl, ["-lp"], { stdio: ["ignore", "pipe", "ignore"] });
        if (res.status === 0) {
          const out = String(res.stdout || "");
          const lines = out.split(/\r?\n/);
          for (const line of lines) {
            // Example: 0x03c00007  0  12345 host Window Title
            const m = line.match(/^(0x[0-9a-fA-F]+)\s+\S+\s+(\d+)\s+/);
            if (!m) continue;
            const winId = m[1];
            const winPid = Number(m[2]);
            if (winPid === Number(pid)) return winId;
          }
        }
      } catch (_err) {
        // ignore
      }
      await sleep(250);
    }
    return null;
  };
  
  const wmctrlMoveResize = (winId, bounds) => {
    const wmctrl = getWmctrlPath();
    if (!wmctrl) return { ok: false, error: "wmctrl_missing" };
    const x = Math.max(0, Math.floor(bounds?.x ?? 0));
    const y = Math.max(0, Math.floor(bounds?.y ?? 0));
    const w = Math.max(1, Math.floor(bounds?.width ?? 1));
    const h = Math.max(1, Math.floor(bounds?.height ?? 1));
    const spec = `0,${x},${y},${w},${h}`;
    try {
      const res = spawnSync(wmctrl, ["-ir", winId, "-e", spec], { stdio: "ignore" });
      return res.status === 0 ? { ok: true } : { ok: false, error: "wmctrl_failed" };
    } catch (err) {
      return { ok: false, error: String(err) };
    }
  };
  
  const getLayoutConfig = () => {
    const config = readConfig();
    const layout = config.layout && typeof config.layout === "object" ? config.layout : {};
    const preset = typeof layout.preset === "string" ? layout.preset : "handheld";
    const mode = layout.mode === "side_by_side" || layout.mode === "separate_displays" ? layout.mode : "";
    const gameDisplay = Number.isFinite(layout.game_display) ? Number(layout.game_display) : 0;
    const trackerDisplay = Number.isFinite(layout.tracker_display) ? Number(layout.tracker_display) : 1;
    const split = Number.isFinite(layout.split) ? Math.min(0.9, Math.max(0.1, Number(layout.split))) : 0.7;
    return { preset, mode, gameDisplay, trackerDisplay, split };
  };
  
  const applyLayoutBestEffort = async (options = {}) => {
    const gamePid = options.gamePid;
    const trackerPid = options.trackerPid;
    const { preset, mode, gameDisplay, trackerDisplay, split } = getLayoutConfig();
    const displays = screen.getAllDisplays();
    if (!displays?.length) return { ok: true };
  
    // Only implemented for X11/XWayland via wmctrl. Wayland-native environments may not allow this.
    if (!getWmctrlPath()) return { ok: false, error: "wmctrl_missing" };
  
    const pickDisplay = (idx) => {
      const i = Math.max(0, Math.min(displays.length - 1, Number(idx) || 0));
      return displays[i];
    };
  
    const dGame = pickDisplay(gameDisplay);
    const dTracker = pickDisplay(trackerDisplay);
    const bGame = dGame?.workArea || dGame?.bounds;
    const bTracker = dTracker?.workArea || dTracker?.bounds;
  
    const effectiveMode =
      mode ||
      (preset === "desktop_dual" || preset === "streamer_dual" ? "separate_displays" : "side_by_side");
  
    const gameWinId = gamePid ? await wmctrlFindWindowIdByPid(gamePid, 6000) : null;
    const trackerWinId = trackerPid ? await wmctrlFindWindowIdByPid(trackerPid, 6000) : null;
  
    if (effectiveMode === "separate_displays") {
      if (gameWinId && bGame) wmctrlMoveResize(gameWinId, bGame);
      if (trackerWinId && bTracker) wmctrlMoveResize(trackerWinId, bTracker);
      return { ok: true };
    }
  
    // side_by_side on game display
    if (bGame) {
      const left = { x: bGame.x, y: bGame.y, width: Math.floor(bGame.width * split), height: bGame.height };
      const right = {
        x: bGame.x + left.width,
        y: bGame.y,
        width: Math.max(1, bGame.width - left.width),
        height: bGame.height
      };
      if (gameWinId) wmctrlMoveResize(gameWinId, left);
      if (trackerWinId) wmctrlMoveResize(trackerWinId, right);
    }
    return { ok: true };
  };
  
  return {
    getGamescopePath,
    spawnMaybeGamescope,
    getWmctrlPath,
    which,
    wmctrlFindWindowIdByPid,
    wmctrlMoveResize,
    getLayoutConfig,
    applyLayoutBestEffort,
  };
};

module.exports = { createWindowingRuntime };
