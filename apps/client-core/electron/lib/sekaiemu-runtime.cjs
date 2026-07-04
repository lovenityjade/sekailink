"use strict";

const createSekaiemuRuntime = (deps = {}) => {
  const {
    app,
    fs,
    os,
    path,
    crypto,
    processRef = globalThis.process,
    readConfig,
    ensureDir,
    writeLogLine,
    nowIso,
    isPlainObject,
    getRuntimePlatformPath,
    getBundledRuntimeDir,
    getRuntimeFilePath,
    getOverlayRuntimeDir,
    getInstalledTrackerPack,
    getTrackerVariant,
    moduleUsesBizHawkProtocolClient,
    findFreeLocalPort,
    findFreeLocalPortInRange,
    spawnMaybeGamescope,
    startSekaiemuChatBridge,
    stopArchipelagoClientsForSession,
    triggerCoupledRuntimeTeardown = () => {},
    nativeGameProcs,
    sekaiemuChatBridges,
    rememberSekaiemuSession = () => {},
    forgetSekaiemuSession = () => {},
    emitSessionEvent = () => {},
  } = deps;
  const process = processRef;

  const getSekaiemuSettings = () => {
    const config = readConfig();
    const games = config.games && typeof config.games === "object" ? config.games : {};
    const layout = config.layout && typeof config.layout === "object" ? config.layout : {};
    const layoutSekaiemu = layout.sekaiemu && typeof layout.sekaiemu === "object" ? layout.sekaiemu : {};
    const layoutInput = layout.input && typeof layout.input === "object" ? layout.input : {};
    const sekaiemu = games.sekaiemu && typeof games.sekaiemu === "object" ? games.sekaiemu : {};
    const frontend = {
      ...(sekaiemu.frontend && typeof sekaiemu.frontend === "object" ? sekaiemu.frontend : {}),
      ...(layoutSekaiemu.hud_buttons_visible !== undefined ? { client_core_hud_buttons_visible: layoutSekaiemu.hud_buttons_visible } : {}),
      ...(layoutSekaiemu.background_gamepad_input !== undefined ? { background_gamepad_input: layoutSekaiemu.background_gamepad_input } : {}),
      ...(layoutSekaiemu.chatbox_visible !== undefined ? { chat_overlay_enabled: false } : {}),
    };
    const cores = sekaiemu.cores && typeof sekaiemu.cores === "object" ? sekaiemu.cores : {};
    return {
      exe_path: String(sekaiemu.exe_path || process.env.SEKAILINK_SEKAIEMU_PATH || "").trim(),
      root_dir: String(sekaiemu.root_dir || process.env.SEKAILINK_SEKAIEMU_ROOT || "").trim(),
      core_dirs: Array.isArray(sekaiemu.core_dirs) ? sekaiemu.core_dirs.map((entry) => String(entry || "").trim()).filter(Boolean) : [],
      system_dir: String(sekaiemu.system_dir || "").trim(),
      save_dir: String(sekaiemu.save_dir || "").trim(),
      log_dir: String(sekaiemu.log_dir || "").trim(),
      args: Array.isArray(sekaiemu.args) ? sekaiemu.args.map((entry) => String(entry || "").trim()).filter(Boolean) : [],
      frontend,
      cores,
      input: layoutInput,
    };
  };
  
  const boolConfigValue = (value, fallback) => {
    if (typeof value === "boolean") return value ? "true" : "false";
    if (typeof value === "string") {
      const lower = value.trim().toLowerCase();
      if (["1", "true", "yes", "on"].includes(lower)) return "true";
      if (["0", "false", "no", "off"].includes(lower)) return "false";
    }
    return fallback ? "true" : "false";
  };
  
  const enumConfigValue = (value, allowed, fallback) => {
    const clean = String(value || "").trim().toLowerCase();
    return allowed.includes(clean) ? clean : fallback;
  };

  const windowModeConfigValue = (frontend) => {
    const clean = String(frontend?.window_mode || "").trim().toLowerCase();
    if (["window", "borderless-window", "fullscreen"].includes(clean)) return clean;
    if (boolConfigValue(frontend?.start_fullscreen, false) === "true") return "fullscreen";
    return "borderless-window";
  };

  const normalizeConfigKey = (value) => {
    let output = "";
    for (const char of String(value || "")) {
      output += /[A-Za-z0-9]/.test(char) ? char.toLowerCase() : "_";
    }
    return output.replace(/_+$/g, "");
  };

  const appendLogTail = (current, chunk, maxLength = 8000) => {
    const next = `${current || ""}${String(chunk || "")}`;
    return next.length > maxLength ? next.slice(next.length - maxLength) : next;
  };

  const oneLine = (value, maxLength = 1200) => {
    const clean = String(value || "").replace(/\s+/g, " ").trim();
    return clean.length > maxLength ? `${clean.slice(0, maxLength)}...` : clean;
  };

  const logProcessStream = (stream, level, scope, prefix, tailRef) => {
    if (!stream || typeof stream.on !== "function") return;
    let lastLine = "";
    let repeatCount = 0;
    let reportedRepeatCount = 0;
    let lastRepeatFlush = 0;

    const flushRepeats = (force = false) => {
      if (repeatCount <= 3) return;
      const repeatedTotal = repeatCount - 3;
      const repeatedDelta = repeatedTotal - reportedRepeatCount;
      if (repeatedDelta <= 0) return;
      const now = Date.now();
      if (!force && now - lastRepeatFlush < 5000) return;
      writeLogLine?.(level, scope, `${prefix}: suppressed ${repeatedDelta} repeated lines: ${lastLine}`);
      reportedRepeatCount = repeatedTotal;
      lastRepeatFlush = now;
    };

    const logLine = (line) => {
      const clean = oneLine(line);
      if (!clean) return;
      if (clean === lastLine) {
        repeatCount += 1;
        if (repeatCount <= 3) {
          writeLogLine?.(level, scope, `${prefix}: ${clean}`);
        } else {
          flushRepeats(false);
        }
        return;
      }
      flushRepeats(true);
      lastLine = clean;
      repeatCount = 1;
      reportedRepeatCount = 0;
      lastRepeatFlush = 0;
      writeLogLine?.(level, scope, `${prefix}: ${clean}`);
    };

    stream.setEncoding?.("utf8");
    stream.on("data", (chunk) => {
      tailRef.value = appendLogTail(tailRef.value, chunk);
      const lines = String(chunk || "").split(/\r?\n/).map((line) => line.trim()).filter(Boolean);
      for (const line of lines.slice(-12)) {
        logLine(line);
      }
    });
    stream.once?.("end", () => flushRepeats(true));
    stream.once?.("close", () => flushRepeats(true));
  };

  const describePathForLaunch = (targetPath) => {
    const safe = String(targetPath || "").trim();
    if (!safe) return "empty";
    try {
      const stat = fs.statSync(safe);
      const type = stat.isFile() ? "file" : stat.isDirectory() ? "dir" : "other";
      const mode = (stat.mode & 0o777).toString(8);
      return `${type},size=${stat.size},mode=${mode}`;
    } catch (err) {
      const code = String(err?.code || "").trim();
      return code ? `missing_or_blocked:${code}` : "missing_or_blocked";
    }
  };

  const describeWindowsRuntimeDlls = (exePath) => {
    if (process.platform !== "win32" || !exePath) return "";
    const binDir = path.dirname(exePath);
    const required = [
      "SDL2.dll",
      "libgcc_s_seh-1.dll",
      "libstdc++-6.dll",
      "libwinpthread-1.dll",
    ];
    const missing = required.filter((name) => !fileExists(path.join(binDir, name)));
    return missing.length ? `; missing_dlls=${missing.join(",")}` : "; missing_dlls=none";
  };

  const formatLaunchSpawnError = (err, context = {}) => {
    const parts = [
      `message=${oneLine(err?.message || err || "spawn_failed", 500)}`,
      err?.code ? `code=${err.code}` : "",
      err?.errno !== undefined ? `errno=${err.errno}` : "",
      err?.syscall ? `syscall=${err.syscall}` : "",
      `platform=${process.platform}-${process.arch}`,
      `packaged=${app.isPackaged ? "true" : "false"}`,
      `exe=${context.exePath || ""} [${describePathForLaunch(context.exePath)}]`,
      `core=${context.corePath || ""} [${describePathForLaunch(context.corePath)}]`,
      `rom=${context.romPath || ""} [${describePathForLaunch(context.romPath)}]`,
      `system=${context.systemDir || ""} [${describePathForLaunch(context.systemDir)}]`,
      `save=${context.saveDir || ""} [${describePathForLaunch(context.saveDir)}]`,
      `log=${context.logDir || ""} [${describePathForLaunch(context.logDir)}]`,
      `arg_count=${Array.isArray(context.args) ? context.args.length : 0}`,
      describeWindowsRuntimeDlls(context.exePath),
      process.platform === "win32" && String(err?.code || "").toUpperCase() === "UNKNOWN"
        ? "win_hint=Windows CreateProcess failed; check Security quarantine/Controlled Folder Access or a missing runtime DLL"
        : "",
    ].filter(Boolean);
    return parts.join("; ");
  };

  const waitForSpawnFailure = (proc, timeoutMs = 250) => new Promise((resolve) => {
    if (!proc || typeof proc.once !== "function") {
      resolve(null);
      return;
    }
    let settled = false;
    const finish = (err) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      resolve(err || null);
    };
    const timer = setTimeout(() => finish(null), timeoutMs);
    proc.once("error", finish);
  });

  const cleanupSklmiCompanionForSocket = (memorySocketPath, reason = "sekaiemu_exit") => {
    const socket = String(memorySocketPath || "").trim();
    if (!socket || process.platform === "win32") return;
    let procEntries = [];
    try {
      procEntries = fs.readdirSync("/proc", { withFileTypes: true });
    } catch (_err) {
      return;
    }
    for (const entry of procEntries) {
      if (!entry.isDirectory() || !/^\d+$/.test(entry.name)) continue;
      const pid = Number(entry.name);
      if (!Number.isFinite(pid) || pid <= 0 || pid === process.pid) continue;
      let cmdline = "";
      try {
        cmdline = fs.readFileSync(path.join("/proc", entry.name, "cmdline"), "utf8").replace(/\0/g, " ");
      } catch (_err) {
        continue;
      }
      if (!cmdline.includes("sekailink_sklmi_runtime") || !cmdline.includes(socket)) continue;
      try {
        process.kill(pid, "SIGTERM");
        writeLogLine?.("info", "sekaiemu", `stopped orphan SKLMI companion pid=${pid} reason=${reason}`);
        setTimeout(() => {
          try {
            process.kill(pid, "SIGKILL");
          } catch (_err) {
            // Already gone.
          }
        }, 1200).unref?.();
      } catch (err) {
        writeLogLine?.("warn", "sekaiemu", `orphan SKLMI cleanup failed pid=${pid}: ${String(err?.message || err || "")}`);
      }
    }
  };

  const sklmiBridgeIdForGame = (gameName) => {
    const clean = String(gameName || "").trim().toLowerCase();
    if (clean === "earthbound") return "earthbound-phase1";
    if (clean === "a link to the past") return "alttp-phase1";
    if (clean === "the legend of zelda") return "tloz-phase1";
    return "";
  };
  
  const writeSekaiemuRuntimeConfig = (saveDir, settings) => {
    const configDir = path.join(saveDir, "frontend-config");
    try {
      ensureDir(configDir);
      const frontend = settings.frontend && typeof settings.frontend === "object" ? settings.frontend : {};
      const settingsMode = enumConfigValue(frontend.settings_mode, ["easy", "advanced"], "easy");
      const trackerDisplayMode = "separate-window";
      const volume = Math.max(0, Math.min(150, Number(frontend.master_volume_percent ?? 35) || 0));
      const sekaiemuCfg = [
        "# Sekaiemu frontend settings generated by SekaiLink Client Core.",
        `settings_mode=${settingsMode}`,
        `chat_overlay_enabled=${boolConfigValue(frontend.chat_overlay_enabled, true)}`,
        `notifications_enabled=${boolConfigValue(frontend.notifications_enabled, true)}`,
        `activity_feed_enabled=${boolConfigValue(frontend.activity_feed_enabled, false)}`,
        `client_core_hud_buttons_visible=${boolConfigValue(frontend.client_core_hud_buttons_visible ?? frontend.hud_buttons_visible, true)}`,
        `bridge_terminal_enabled=${boolConfigValue(frontend.bridge_terminal_enabled, false)}`,
        `background_gamepad_input=${boolConfigValue(frontend.background_gamepad_input ?? frontend.system_wide_gamepad_input ?? frontend.background_controller_input, false)}`,
        `window_mode=${windowModeConfigValue(frontend)}`,
        `master_volume_percent=${volume}`,
        `tracker_display_mode=${trackerDisplayMode}`,
        "tracker_screen_visible=false",
        "tracker_auto_follow=false",
        "",
      ].join("\n");
      fs.writeFileSync(path.join(configDir, "sekaiemu.cfg"), sekaiemuCfg, "utf8");
  
      const cores = settings.cores && typeof settings.cores === "object" ? settings.cores : {};
      const coreLines = ["# Sekaiemu core preferences generated by SekaiLink Client Core."];
      for (const [rawKey, rawValue] of Object.entries(cores).sort(([a], [b]) => a.localeCompare(b))) {
        const key = String(rawKey || "").trim();
        if (!/^[A-Za-z0-9._-]+$/.test(key)) continue;
        const value = String(rawValue ?? "").replace(/[\r\n]/g, " ").trim();
        if (!value) continue;
        coreLines.push(`${key}=${value}`);
      }
      coreLines.push("");
      fs.writeFileSync(path.join(configDir, "cores.cfg"), coreLines.join("\n"), "utf8");
      writeLogLine("info", "sekaiemu", `runtime frontend config written: ${configDir}`);
      return { ok: true, dir: configDir };
    } catch (err) {
      const detail = String(err || "");
      writeLogLine("warn", "sekaiemu", `runtime frontend config write failed: ${detail}`);
      return { ok: false, error: "sekaiemu_runtime_config_write_failed", detail };
    }
  };

  const sekaiemuInputDefaults = {
    dpad_up: { keyboard: "key:Up", controller: "button:dpup" },
    dpad_down: { keyboard: "key:Down", controller: "button:dpdown" },
    dpad_left: { keyboard: "key:Left", controller: "button:dpleft" },
    dpad_right: { keyboard: "key:Right", controller: "button:dpright" },
    a: { keyboard: "key:X", controller: "button:a" },
    b: { keyboard: "key:Z", controller: "button:b" },
    x: { keyboard: "key:S", controller: "button:x" },
    y: { keyboard: "key:A", controller: "button:y" },
    l: { keyboard: "key:Q", controller: "button:leftshoulder" },
    r: { keyboard: "key:W", controller: "button:rightshoulder" },
    start: { keyboard: "key:Return", controller: "button:start" },
    select: { keyboard: "key:Right Shift", controller: "button:back" },
    analog_left: { keyboard: "key:Left", controller: "axis-:leftx" },
    analog_right: { keyboard: "key:Right", controller: "axis+:leftx" },
    analog_up: { keyboard: "key:Up", controller: "axis-:lefty" },
    analog_down: { keyboard: "key:Down", controller: "axis+:lefty" },
  };

  const gamepadButtonIndexToSdl = {
    0: "a",
    1: "b",
    2: "x",
    3: "y",
    4: "leftshoulder",
    5: "rightshoulder",
    8: "back",
    9: "start",
    10: "leftstick",
    11: "rightstick",
    12: "dpup",
    13: "dpdown",
    14: "dpleft",
    15: "dpright",
  };

  const gamepadAxisIndexToSdl = {
    0: "leftx",
    1: "lefty",
    2: "rightx",
    3: "righty",
  };

  const controlLabelToSekaiemuKey = {
    "D-Pad Up": "dpad_up",
    "D-Pad Down": "dpad_down",
    "D-Pad Left": "dpad_left",
    "D-Pad Right": "dpad_right",
    A: "a",
    B: "b",
    X: "x",
    Y: "y",
    L: "l",
    R: "r",
    Start: "start",
    Select: "select",
    "Stick Up": "analog_up",
    "Stick Down": "analog_down",
    "Stick Left": "analog_left",
    "Stick Right": "analog_right",
  };

  const controlCoreIdForSekaiemuCore = (corePath, manifest) => {
    const coreId = String(
      manifest?.sekaiemu?.core_id ||
      manifest?.libretro_core_id ||
      manifest?.driver?.core_id ||
      path.basename(String(corePath || ""), path.extname(String(corePath || "")))
    ).toLowerCase();
    const gameId = String(manifest?.game_id || manifest?.id || "").toLowerCase();
    const haystack = `${coreId} ${gameId}`;
    if (/(mupen|parallel|n64|z64|oot|dk64)/.test(haystack)) return "n64";
    if (/(mgba|gba|game_boy_advance|zeromission|zero_mission|fire_red|firered)/.test(haystack)) return "gba";
    if (/(gambatte|sameboy|gbc|gb|game_boy|ladx)/.test(haystack)) return "gbc";
    if (/(fceumm|nestopia|nes|tloz)/.test(haystack)) return "nes";
    return "snes";
  };

  const clientBindingToSekaiemuControllerToken = (binding) => {
    const clean = String(binding || "").trim().toLowerCase();
    if (/^(button|axis[+-]):[a-z0-9_+-]+$/i.test(String(binding || "").trim())) {
      return String(binding || "").trim();
    }
    if (/^sdl:(button|axis[+-]):[a-z0-9_+-]+$/i.test(String(binding || "").trim())) {
      return String(binding || "").trim().slice(4);
    }
    const buttonMatch = clean.match(/^(?:gamepad:)?button:(\d+)$/);
    if (buttonMatch) {
      const button = gamepadButtonIndexToSdl[Number(buttonMatch[1])];
      return button ? `button:${button}` : "";
    }
    const axisMatch = clean.match(/^(?:gamepad:)?axis:(\d+)([+-])$/);
    if (axisMatch) {
      const axis = gamepadAxisIndexToSdl[Number(axisMatch[1])];
      return axis ? `axis${axisMatch[2]}:${axis}` : "";
    }
    return "";
  };

  const sekaiemuControllerTokenToClientBinding = (token) => {
    const clean = String(token || "").trim();
    return clean ? `sdl:${clean}` : "";
  };

  const clientControlLabelForSekaiemuKey = (key) => {
    const entries = Object.entries(controlLabelToSekaiemuKey);
    const found = entries.find(([, value]) => value === key);
    return found ? found[0] : "";
  };

  const parseSekaiemuInputConfigMappings = (configPath) => {
    const mappings = {};
    const text = fs.readFileSync(configPath, "utf8");
    for (const rawLine of text.split(/\r?\n/)) {
      const line = rawLine.trim();
      if (!line || line.startsWith("#")) continue;
      const equals = line.indexOf("=");
      if (equals <= 0) continue;
      const key = line.slice(0, equals).trim();
      const value = line.slice(equals + 1).trim();
      const match = key.match(/^([a-z0-9_]+)\.controller$/i);
      if (!match) continue;
      const label = clientControlLabelForSekaiemuKey(match[1].toLowerCase());
      const binding = sekaiemuControllerTokenToClientBinding(value);
      if (label && binding) mappings[label] = binding;
    }
    return mappings;
  };

  const writeSekaiemuCoreOptionOverrides = (saveDir, corePath, romPath, manifest = {}) => {
    try {
      if (!saveDir || !corePath || !romPath) return { ok: true, skipped: true };
      const sekaiemu = manifest.sekaiemu && typeof manifest.sekaiemu === "object" ? manifest.sekaiemu : {};
      const coreId = String(
        sekaiemu.core_id ||
          manifest.libretro_core_id ||
          manifest?.driver?.core_id ||
          ""
      ).trim().toLowerCase();
      const coreName = path.basename(corePath, path.extname(corePath)).toLowerCase();
      const overrides = {};
      if (coreId === "gbc" && coreName.includes("gambatte")) {
        overrides.gambatte_gb_hwmode = "GBC";
      }
      if (sekaiemu.core_options && typeof sekaiemu.core_options === "object") {
        for (const [rawKey, rawValue] of Object.entries(sekaiemu.core_options)) {
          const key = String(rawKey || "").trim();
          const value = String(rawValue ?? "").replace(/[\r\n]/g, " ").trim();
          if (/^[A-Za-z0-9._-]+$/.test(key) && value) {
            overrides[key] = value;
          }
        }
      }
      const entries = Object.entries(overrides);
      if (!entries.length) return { ok: true, skipped: true };

      const configDir = path.join(saveDir, "core-config");
      ensureDir(configDir);
      const coreKey = normalizeConfigKey(path.basename(corePath, path.extname(corePath)));
      const gameKey = normalizeConfigKey(path.basename(romPath, path.extname(romPath)));
      const merged = {};
      const gameConfigPath = path.join(configDir, `${coreKey}__${gameKey}.cfg`);
      if (fs.existsSync(gameConfigPath)) {
        for (const rawLine of fs.readFileSync(gameConfigPath, "utf8").split(/\r?\n/)) {
          const line = rawLine.trim();
          if (!line || line.startsWith("#")) continue;
          const equals = line.indexOf("=");
          if (equals <= 0) continue;
          const key = line.slice(0, equals).trim();
          const value = line.slice(equals + 1).trim();
          if (/^[A-Za-z0-9._-]+$/.test(key) && value) merged[key] = value;
        }
      }
      for (const [key, value] of entries) merged[key] = value;
      const lines = [
        "# Sekaiemu core settings generated by SekaiLink Client Core.",
        "# Game-specific overrides required for stable module launch.",
        ...Object.entries(merged).sort(([a], [b]) => a.localeCompare(b)).map(([key, value]) => `${key}=${value}`),
        "",
      ];
      fs.writeFileSync(gameConfigPath, lines.join("\n"), "utf8");
      writeLogLine("info", "sekaiemu", `runtime core option overrides written: ${gameConfigPath}`);
      return { ok: true, path: gameConfigPath, applied: entries.length };
    } catch (err) {
      const detail = String(err || "");
      writeLogLine("warn", "sekaiemu", `runtime core option override write failed: ${detail}`);
      return { ok: false, error: "sekaiemu_core_option_write_failed", detail };
    }
  };
  
  const writeSekaiemuInputConfig = (saveDir, corePath, manifest, settings) => {
    try {
      if (!saveDir || !corePath) return { ok: true, skipped: true };
      const input = settings.input && typeof settings.input === "object" ? settings.input : {};
      const allMappings = input.core_mappings && typeof input.core_mappings === "object" ? input.core_mappings : {};
      const profileId = controlCoreIdForSekaiemuCore(corePath, manifest);
      const fallbackProfileId = String(input.default_core_id || "").trim();
      const selectedMappings = {
        ...(fallbackProfileId && allMappings[fallbackProfileId] && typeof allMappings[fallbackProfileId] === "object" ? allMappings[fallbackProfileId] : {}),
        ...(allMappings[profileId] && typeof allMappings[profileId] === "object" ? allMappings[profileId] : {}),
      };
      const bindings = JSON.parse(JSON.stringify(sekaiemuInputDefaults));
      let applied = 0;
      for (const [clientLabel, binding] of Object.entries(selectedMappings)) {
        const key = controlLabelToSekaiemuKey[clientLabel];
        if (!key || !bindings[key]) continue;
        const token = clientBindingToSekaiemuControllerToken(binding);
        if (!token) continue;
        bindings[key].controller = token;
        applied += 1;
      }
      const coreKey = normalizeConfigKey(path.basename(corePath, path.extname(corePath)));
      const inputDir = path.join(saveDir, "input-config");
      ensureDir(inputDir);
      const lines = [
        "# Sekaiemu core input configuration",
        "# Generated by SekaiLink Client Core from Settings > Control Settings.",
        "selected_controller_guid=",
      ];
      for (const [key, binding] of Object.entries(bindings)) {
        lines.push(`${key}.keyboard=${binding.keyboard}`);
        lines.push(`${key}.controller=${binding.controller}`);
      }
      lines.push("");
      const configPath = path.join(inputDir, `${coreKey}.cfg`);
      fs.writeFileSync(configPath, lines.join("\n"), "utf8");
      writeLogLine("info", "sekaiemu", `runtime input config written: ${configPath} profile=${profileId} applied=${applied}`);
      return { ok: true, path: configPath, profileId, applied };
    } catch (err) {
      const detail = String(err || "");
      writeLogLine("warn", "sekaiemu", `runtime input config write failed: ${detail}`);
      return { ok: false, error: "sekaiemu_input_config_write_failed", detail };
    }
  };
  
  const fileExists = (filePath) => {
    const safe = String(filePath || "").trim();
    if (!safe) return false;
    try {
      return fs.existsSync(safe) && fs.statSync(safe).isFile();
    } catch (_err) {
      return false;
    }
  };
  
  const dirExists = (dirPath) => {
    const safe = String(dirPath || "").trim();
    if (!safe) return false;
    try {
      return fs.existsSync(safe) && fs.statSync(safe).isDirectory();
    } catch (_err) {
      return false;
    }
  };
  
  const pathExists = (targetPath) => {
    const safe = String(targetPath || "").trim();
    if (!safe) return false;
    try {
      return fs.existsSync(safe);
    } catch (_err) {
      return false;
    }
  };
  
  const findExecutableByNamesInDir = (rootDir, names = [], maxDepth = 6) => {
    const root = String(rootDir || "").trim();
    if (!root) return "";
    try {
      if (!fs.existsSync(root) || !fs.statSync(root).isDirectory()) return "";
    } catch (_err) {
      return "";
    }
    const wanted = new Set(names.map((name) => String(name || "").toLowerCase()).filter(Boolean));
    const queue = [{ dir: root, depth: 0 }];
    while (queue.length) {
      const cur = queue.shift();
      if (!cur || cur.depth > maxDepth) continue;
      let entries = [];
      try {
        entries = fs.readdirSync(cur.dir, { withFileTypes: true });
      } catch (_err) {
        continue;
      }
      for (const entry of entries) {
        const full = path.join(cur.dir, entry.name);
        if (entry.isDirectory()) {
          queue.push({ dir: full, depth: cur.depth + 1 });
        } else if (entry.isFile() && wanted.has(entry.name.toLowerCase())) {
          return full;
        }
      }
    }
    return "";
  };
  
  const resolveSekaiemuExecutable = () => {
    const settings = getSekaiemuSettings();
    const exeNames = process.platform === "win32"
      ? ["sekaiemu_libretro_spike.exe", "sekaiemu.exe"]
      : ["sekaiemu_libretro_spike", "sekaiemu"];
    const home = app.getPath("home");
    const runtimeBinCandidates = exeNames.flatMap((exeName) => [
      getRuntimePlatformPath("bin", exeName),
      path.join(getBundledRuntimeDir(), "bin", exeName),
      path.join(getBundledRuntimeDir(), "sekaiemu", exeName),
    ]);
    const devCandidates = app.isPackaged ? [] : [
      path.join("/tmp", "sekaiemu-libretro-spike-beta3-build", exeNames[0]),
      path.join(home, "Projects", "Sekaiemu-Libretro-Spike-Codex", "workspace", "sekaiemu-libretro-spike", "build", exeNames[0]),
      path.join(home, "Sekaiemu-Libretro-Spike-Codex", "workspace", "sekaiemu-libretro-spike", "build", exeNames[0]),
    ];
    const candidates = [
      settings.exe_path,
      ...runtimeBinCandidates,
      ...devCandidates,
    ];
    for (const candidate of candidates) {
      if (fileExists(candidate)) return candidate;
    }
    const roots = [
      settings.root_dir,
      getRuntimePlatformPath("sekaiemu"),
      path.join(getBundledRuntimeDir(), "sekaiemu"),
      ...(app.isPackaged ? [] : [
        path.join(home, "Projects", "Sekaiemu-Libretro-Spike-Codex", "workspace", "sekaiemu-libretro-spike"),
      ]),
    ].filter(Boolean);
    for (const root of roots) {
      const found = findExecutableByNamesInDir(root, exeNames, 5);
      if (found) return found;
    }
    return "";
  };
  
  const resolvePathFromRoots = (candidate, roots = [], existsPredicate = fileExists) => {
    const safe = String(candidate || "").trim();
    if (!safe) return "";
    if (path.isAbsolute(safe) && existsPredicate(safe)) return safe;
    for (const root of roots) {
      const base = String(root || "").trim();
      if (!base) continue;
      const resolved = path.join(base, safe);
      if (existsPredicate(resolved)) return resolved;
    }
    return "";
  };

  const sha256File = (filePath) => {
    const hash = crypto.createHash("sha256");
    const data = fs.readFileSync(filePath);
    hash.update(data);
    return hash.digest("hex");
  };

  const normalizeSystemFileEntries = (manifest = {}) => {
    const entries = Array.isArray(manifest.system_files) ? manifest.system_files : [];
    return entries
      .map((entry) => {
        if (typeof entry === "string") return { name: entry, target: entry };
        if (!entry || typeof entry !== "object") return null;
        const name = String(entry.name || entry.file || entry.path || "").trim();
        const target = String(entry.target || entry.name || "").trim();
        if (!name || !target) return null;
        return {
          name,
          target,
          size: Number(entry.size || 0) || 0,
          sha256: String(entry.sha256 || "").trim().toLowerCase(),
          description: String(entry.description || "").trim(),
        };
      })
      .filter(Boolean);
  };

  const stageSekaiemuSystemFiles = (manifest = {}, systemDir = "") => {
    const required = normalizeSystemFileEntries(manifest);
    if (!required.length || !systemDir) return { ok: true, staged: [] };
    const searchRoots = [
      path.join(app.getPath("userData"), "firmware"),
      path.join(getBundledRuntimeDir(), "firmware"),
      path.join(getBundledRuntimeDir(), "system"),
      getRuntimePlatformPath("firmware"),
      getRuntimePlatformPath("system"),
    ].filter(Boolean);
    const staged = [];
    for (const entry of required) {
      const targetPath = path.join(systemDir, entry.target);
      const validTarget = (() => {
        if (!fileExists(targetPath)) return false;
        try {
          if (entry.size && fs.statSync(targetPath).size !== entry.size) return false;
          if (entry.sha256 && sha256File(targetPath) !== entry.sha256) return false;
          return true;
        } catch (_err) {
          return false;
        }
      })();
      if (validTarget) {
        staged.push({ name: entry.name, target: targetPath, reused: true });
        continue;
      }

      const candidates = [entry.name, entry.target].filter(Boolean);
      let sourcePath = "";
      for (const root of searchRoots) {
        for (const candidate of candidates) {
          const resolved = path.join(root, candidate);
          if (!fileExists(resolved)) continue;
          try {
            if (entry.size && fs.statSync(resolved).size !== entry.size) continue;
            if (entry.sha256 && sha256File(resolved) !== entry.sha256) continue;
            sourcePath = resolved;
            break;
          } catch (_err) {
            // Keep scanning.
          }
        }
        if (sourcePath) break;
      }
      if (!sourcePath) {
        return {
          ok: false,
          error: "sekaiemu_system_file_missing",
          file: entry.name,
          target: entry.target,
          detail: entry.description || "Required firmware/system file was not found.",
        };
      }
      ensureDir(path.dirname(targetPath));
      fs.copyFileSync(sourcePath, targetPath);
      staged.push({ name: entry.name, source: sourcePath, target: targetPath, reused: false });
      writeLogLine("info", "sekaiemu", `staged system file ${entry.name} -> ${targetPath}`);
    }
    return { ok: true, staged };
  };
  
  const resolveSekaiemuCorePath = (manifest = {}) => {
    const settings = getSekaiemuSettings();
    const sekaiemu = manifest.sekaiemu && typeof manifest.sekaiemu === "object" ? manifest.sekaiemu : {};
    const corePath = String(sekaiemu.core_path || manifest.libretro_core_path || "").trim();
    const coreId = String(sekaiemu.core_id || manifest.libretro_core_id || manifest?.driver?.core_id || "").trim();
    const home = app.getPath("home");
    const roots = [
      ...settings.core_dirs,
      getRuntimePlatformPath("cores"),
      path.join(getBundledRuntimeDir(), "cores"),
      path.join(getBundledRuntimeDir(), "libretro"),
      ...(app.isPackaged ? [] : [
        path.join(home, "Projects", "Sekaiemu-Libretro-Spike-Codex", "workspace", "src"),
      ]),
    ];
    const direct = resolvePathFromRoots(corePath, roots);
    if (direct) return direct;
  
    const ext = process.platform === "win32" ? ".dll" : ".so";
    const names = (...baseNames) => baseNames.map((name) => `${name}${ext}`);
    const aliases = {
      bsnes: names("bsnes_mercury_performance_libretro", "bsnes_mercury_balanced_libretro", "bsnes_libretro", "snes9x_libretro"),
      snes: names("bsnes_mercury_performance_libretro", "bsnes_mercury_balanced_libretro", "snes9x_libretro"),
      snes9x: names("snes9x_libretro"),
      gba: names("mgba_libretro"),
      mgba: names("mgba_libretro"),
      gb: names("gambatte_libretro"),
      gbc: names("gambatte_libretro"),
      gambatte: names("gambatte_libretro"),
      nes: names("nestopia_libretro", "fceumm_libretro"),
      nestopia: names("nestopia_libretro"),
      fceumm: names("fceumm_libretro"),
      n64: names("mupen64plus_next_libretro", "parallel_n64_libretro"),
      mupen64plus: names("mupen64plus_next_libretro"),
      parallel_n64: names("parallel_n64_libretro"),
    };
    const wanted = aliases[coreId.toLowerCase()] || (coreId ? [coreId] : []);
    if (!wanted.length) return "";
    for (const root of roots) {
      const found = findExecutableByNamesInDir(root, wanted, 5);
      if (found) return found;
    }
    return "";
  };
  
  const parseArchipelagoAddress = (address) => {
    const raw = String(address || "").trim();
    if (!raw) return null;
    try {
      const url = new URL(raw.includes("://") ? raw : `ws://${raw}`);
      const port = Number(url.port || (url.protocol === "wss:" ? 443 : 0));
      return {
        host: url.hostname,
        port: Number.isFinite(port) ? port : 0,
        path: url.pathname && url.pathname !== "" ? url.pathname : "/",
      };
    } catch (_err) {
      const [host, portText] = raw.split(":");
      const port = Number(portText || 0);
      if (!host || !Number.isFinite(port)) return null;
      return { host, port, path: "/" };
    }
  };
  
  const resolveSklmiRuntimeForSekaiemu = () => {
    const runtimeDir = getBundledRuntimeDir();
    const exeName = process.platform === "win32" ? "sekailink_sklmi_runtime.exe" : "sekailink_sklmi_runtime";
    const sourceRepoRoot = path.resolve(__dirname, "../../../..");
    const devCandidates = app.isPackaged ? [] : [
      path.join(sourceRepoRoot, "services", "sklmi", "build", exeName),
      path.join(sourceRepoRoot, "runtime", "bin", exeName),
    ];
    const candidates = [
      process.env.SEKAILINK_SKLMI_RUNTIME,
      getRuntimePlatformPath("bin", exeName),
      path.join(runtimeDir, "bin", exeName),
      path.join(runtimeDir, "sklmi", exeName),
      path.join(runtimeDir, exeName),
      ...devCandidates,
    ];
    for (const candidate of candidates) {
      if (fileExists(candidate)) return candidate;
    }
    return "";
  };
  
  const resolveSklmiManifestDirForSekaiemu = () => {
    const runtimeDir = getBundledRuntimeDir();
    const sourceRepoRoot = path.resolve(__dirname, "../../../..");
    const devCandidates = app.isPackaged ? [] : [
      path.join(sourceRepoRoot, "services", "sklmi", "manifests"),
      path.join(sourceRepoRoot, "runtime", "sklmi", "manifests"),
    ];
    const candidates = [
      process.env.SEKAILINK_SKLMI_MANIFEST_DIR,
      getRuntimePlatformPath("sklmi", "manifests"),
      path.join(runtimeDir, "sklmi", "manifests"),
      path.join(runtimeDir, "manifests", "sklmi"),
      path.join(runtimeDir, "manifests"),
      ...devCandidates,
    ];
    for (const candidate of candidates) {
      try {
        if (candidate && fs.existsSync(candidate) && fs.statSync(candidate).isDirectory()) return candidate;
      } catch (_err) {
        // keep scanning
      }
    }
    return "";
  };
  
  const resolveSekaiemuTrackerPackPath = (manifest = {}, roots = [], options = {}) => {
    const gameId = String(manifest.game_id || manifest.gameId || "").trim();
    const sekaiemu = manifest.sekaiemu && typeof manifest.sekaiemu === "object" ? manifest.sekaiemu : {};
    const allowInstalledFallback = options.allowInstalledFallback !== false;
    const adaptedCandidates = [
      sekaiemu.sklmi_tracker_pack,
      sekaiemu.sklmi_tracker_pack_path,
      manifest.sklmi_tracker_pack_path,
      manifest.sklmi_tracker_pack,
      gameId ? path.join("tracker-bundles", `${gameId}-linkedworld-default.zip`) : "",
      gameId ? path.join("tracker-bundles", `${gameId}-linkedworld-default`) : "",
    ].filter(Boolean);
    for (const candidate of adaptedCandidates) {
      const resolved = resolvePathFromRoots(String(candidate || "").trim(), roots, pathExists) || getRuntimeFilePath(String(candidate || "").trim());
      if (resolved && pathExists(resolved)) return resolved;
    }
    if (allowInstalledFallback && gameId) {
      const installed = getInstalledTrackerPack(gameId);
      if (installed?.path && pathExists(installed.path)) return String(installed.path);
    }
    const declared = String(
      sekaiemu.tracker_pack ||
        sekaiemu.tracker_pack_path ||
        manifest.tracker_pack_path ||
        ""
    ).trim();
    if (declared && !path.isAbsolute(declared)) {
      const runtimePath = getRuntimeFilePath(declared);
      if (pathExists(runtimePath)) return runtimePath;
      if (declared.toLowerCase().endsWith(".zip")) {
        const unpackedRuntimePath = getRuntimeFilePath(declared.slice(0, -4));
        if (pathExists(unpackedRuntimePath)) return unpackedRuntimePath;
      }
    }
    const resolved = resolvePathFromRoots(declared, roots, pathExists);
    if (resolved) return resolved;
    if (declared.toLowerCase().endsWith(".zip")) {
      return resolvePathFromRoots(declared.slice(0, -4), roots, pathExists);
    }
    return "";
  };
  
  
  const tryLaunchSekaiemu = async (options = {}) => {
    const moduleId = String(options.moduleId || "").trim();
    const manifest = options.manifest || getModuleManifest(moduleId) || {};
    const romPath = String(options.romPath || "").trim();
    const layoutPreview = Boolean(options.layoutPreview);
    const settings = getSekaiemuSettings();
    const sekaiemu = manifest.sekaiemu && typeof manifest.sekaiemu === "object" ? manifest.sekaiemu : {};
    const exePath = resolveSekaiemuExecutable();
    if (!exePath) return { ok: false, error: "sekaiemu_not_found" };
    const requestedCorePath = String(options.corePath || "").trim();
    const corePath = layoutPreview
      ? ""
      : (requestedCorePath && fileExists(requestedCorePath)
        ? requestedCorePath
        : resolveSekaiemuCorePath(manifest));
    if (!layoutPreview && !corePath) return { ok: false, error: "sekaiemu_core_missing" };
    if (!layoutPreview && (!romPath || !fileExists(romPath))) return { ok: false, error: "rom_missing", detail: "patched_rom_not_found" };
  
    const safeModuleId = moduleId || String(manifest.game_id || "game").trim() || "game";
    const runtimeRoot = path.join(app.getPath("userData"), "sekaiemu", safeModuleId);
    const systemDir = settings.system_dir || path.join(runtimeRoot, "system");
    const romSaveKey = !layoutPreview && romPath
      ? `${path.basename(romPath, path.extname(romPath)).replace(/[^a-z0-9_.-]+/gi, "_").slice(0, 72)}-${crypto
          .createHash("sha1")
          .update(path.resolve(romPath))
          .digest("hex")
          .slice(0, 10)}`
      : "";
    const saveDir = settings.save_dir || path.join(runtimeRoot, "saves", romSaveKey || "default");
    const logDir = settings.log_dir || path.join(app.getPath("userData"), "logs", "sekaiemu");
    ensureDir(systemDir);
    ensureDir(saveDir);
    ensureDir(logDir);
    const systemFilesRes = stageSekaiemuSystemFiles(manifest, systemDir);
    if (!systemFilesRes.ok) {
      return {
        ok: false,
        error: systemFilesRes.error || "sekaiemu_system_file_missing",
        detail: systemFilesRes.detail || systemFilesRes.file || "",
        file: systemFilesRes.file || "",
      };
    }
    writeSekaiemuRuntimeConfig(saveDir, settings);
    if (!layoutPreview) {
      const coreOptionRes = writeSekaiemuCoreOptionOverrides(saveDir, corePath, romPath, manifest);
      if (!coreOptionRes.ok) {
        return {
          ok: false,
          error: coreOptionRes.error || "sekaiemu_core_option_write_failed",
          detail: coreOptionRes.detail || "",
        };
      }
      const inputConfigRes = writeSekaiemuInputConfig(saveDir, corePath, manifest, settings);
      if (!inputConfigRes.ok) {
        return {
          ok: false,
          error: inputConfigRes.error || "sekaiemu_input_config_write_failed",
          detail: inputConfigRes.detail || "",
        };
      }
    }
  
    const requestedMemorySocket = String(options.memorySocketPath || "").trim();
    const needsBizHawkProtocolPort = !layoutPreview && !requestedMemorySocket && moduleUsesBizHawkProtocolClient(manifest);
    const bizHawkProtocolPort = needsBizHawkProtocolPort
      ? await findFreeLocalPortInRange(43055, 43059)
      : 0;
    if (needsBizHawkProtocolPort && !bizHawkProtocolPort) {
      return { ok: false, error: "bizhawk_protocol_port_unavailable", detail: "Ports 43055-43059 are already in use." };
    }
    const memorySocketPath = layoutPreview
      ? ""
      : (requestedMemorySocket || (
        bizHawkProtocolPort
          ? `tcp://127.0.0.1:${bizHawkProtocolPort}`
          : process.platform === "win32"
          ? `tcp://127.0.0.1:${await findFreeLocalPort(43080)}`
          : path.join(os.tmpdir(), `sekailink-sekaiemu-${Date.now()}-${crypto.randomBytes(4).toString("hex")}.sock`)
      ));
    const args = layoutPreview
      ? ["--layout-preview", "--save-dir", saveDir, "--log-dir", logDir, options.startFullscreen ? "--fullscreen" : "--windowed"]
      : ["--core", corePath, "--game", romPath, "--system-dir", systemDir, "--save-dir", saveDir, "--log-dir", logDir, "--memory-socket", memorySocketPath];
    const hudRoot = layoutPreview ? "" : path.join(app.getPath("userData"), "runtime", "sekaiemu-hud", safeModuleId, `${Date.now()}-${crypto.randomBytes(4).toString("hex")}`);
    const hudStatePath = hudRoot ? path.join(hudRoot, "hud-state.json") : "";
    const hudEventsPath = hudRoot ? path.join(hudRoot, "hud-events.jsonl") : "";
    if (!layoutPreview) {
      ensureDir(hudRoot);
      fs.writeFileSync(hudStatePath, JSON.stringify({ chatUnread: 0, activityUnread: 0, buttonsVisible: true, toasts: [] }, null, 2), "utf8");
      fs.writeFileSync(hudEventsPath, "", "utf8");
      args.push("--client-core-hud-state", hudStatePath, "--client-core-hud-events", hudEventsPath);
    }
    const runtimeDir = getBundledRuntimeDir();
    const overlayRuntimeDir = getOverlayRuntimeDir();
    const roots = [
      overlayRuntimeDir,
      runtimeDir,
      path.dirname(exePath),
      path.join(path.dirname(exePath), ".."),
      path.join(app.getPath("home"), "Projects", "Sekaiemu-Libretro-Spike-Codex", "workspace", "sekaiemu-libretro-spike"),
    ];
    const profilePath = resolvePathFromRoots(String(sekaiemu.profile_path || sekaiemu.profile || "").trim(), roots);
    if (!layoutPreview && profilePath) args.push("--profile", profilePath);
    const sklmiRuntime = layoutPreview ? "" : resolveSklmiRuntimeForSekaiemu();
    const sklmiManifestDir = layoutPreview ? "" : resolveSklmiManifestDirForSekaiemu();
    if (!layoutPreview && sklmiRuntime) args.push("--sklmi-runtime", sklmiRuntime);
    if (!layoutPreview && sklmiManifestDir) args.push("--sklmi-manifest-dir", sklmiManifestDir);
  
    const apAddress = parseArchipelagoAddress(options.serverAddress);
    const apGame = String(sekaiemu.ap_game || manifest.ap_game || manifest.display_name || "").trim();
    const apSlot = String(options.slot || "").trim();
    const sklmiBridgeId = !layoutPreview ? sklmiBridgeIdForGame(apGame) : "";
    const sklmiTraceLogPath = sklmiBridgeId ? path.join(saveDir, "sklmi", sklmiBridgeId, "trace.jsonl") : "";
    const playerAlias = String(options.playerAlias || "").trim();
    const playerAliasMap = isPlainObject(options.playerAliasMap) ? options.playerAliasMap : {};
    const playerAliasMapJson = JSON.stringify(Object.fromEntries(
      Object.entries(playerAliasMap)
        .map(([slotName, alias]) => [String(slotName || "").trim(), String(alias || "").trim()])
        .filter(([slotName, alias]) => slotName && alias)
    ));
    const cleanupModuleId = safeModuleId;
    const cleanupSlot = apSlot;
    if (!layoutPreview && apAddress?.host && apAddress.port && apGame && apSlot) {
      args.push(
        "--ap-host", apAddress.host,
        "--ap-port", String(apAddress.port),
        "--ap-path", apAddress.path || "/",
        "--ap-game", apGame,
        "--ap-slot-name", apSlot,
        "--ap-uuid", `sekailink-${safeModuleId}`
      );
      if (options.password) args.push("--ap-password", String(options.password));
      if (playerAlias) args.push("--player-alias", playerAlias);
      if (playerAliasMapJson !== "{}") args.push("--player-alias-map", playerAliasMapJson);
      args.push("--ap-tags", "AP,SekaiLink,SKLMI");
    }
    writeLogLine("info", "sekaiemu", `internal tracker disabled for BETA-3 runtime: module=${safeModuleId}`);
    if (!layoutPreview) {
      const trackerPack = resolveSekaiemuTrackerPackPath(manifest, [
        overlayRuntimeDir,
        runtimeDir,
        path.join(runtimeDir, "tracker-bundles"),
        path.dirname(exePath),
        path.join(path.dirname(exePath), ".."),
      ], { allowInstalledFallback: false });
      if (trackerPack) {
        args.push("--tracker-pack", trackerPack);
        const trackerVariant = String(options.trackerVariant || options.packVariant || getTrackerVariant(String(manifest.game_id || safeModuleId))).trim();
        if (trackerVariant && trackerVariant !== "default") args.push("--tracker-variant", trackerVariant);
        writeLogLine("info", "sekaiemu", `tracker pack provided to SKLMI companion: module=${safeModuleId} pack=${trackerPack} variant=${trackerVariant || "default"}`);
      }
    }
    if (!layoutPreview && Array.isArray(sekaiemu.args)) {
      for (const entry of sekaiemu.args) {
        const value = String(entry || "").trim();
        if (value) args.push(value);
      }
    }
    const chatBridge = layoutPreview ? null : startSekaiemuChatBridge({
      moduleId: safeModuleId,
      ...(isPlainObject(options.chatBridge) ? options.chatBridge : {}),
      apiBaseUrl: options.apiBaseUrl,
      authToken: options.authToken,
      deviceId: options.deviceId,
    });
    if (chatBridge) {
      args.push("--chat-inbox", chatBridge.inboxPath, "--chat-outbox", chatBridge.outboxPath);
    }
    if (!layoutPreview) args.push(...settings.args);
  
    try {
      fs.chmodSync(exePath, 0o755);
    } catch (_err) {
      // ignore chmod failures
    }
  
    try {
      const spawnEnv = { ...process.env };
      if (process.platform === "win32") {
        const runtimeBinDir = path.dirname(exePath);
        const priorPath = spawnEnv.PATH || spawnEnv.Path || "";
        spawnEnv.PATH = priorPath ? `${runtimeBinDir}${path.delimiter}${priorPath}` : runtimeBinDir;
        spawnEnv.Path = spawnEnv.PATH;
      }
      writeLogLine?.("info", "sekaiemu", `launching runtime exe=${exePath} core=${corePath} rom=${romPath} system=${systemDir} save=${saveDir} log=${logDir}`);
      const wrapped = spawnMaybeGamescope(exePath, args, { stdio: ["ignore", "pipe", "pipe"], env: spawnEnv });
      if (!wrapped.ok) {
        chatBridge?.stop?.();
        return { ok: false, error: wrapped.error || "sekaiemu_launch_failed", detail: wrapped.detail || "" };
      }
      const immediateSpawnError = await waitForSpawnFailure(wrapped.proc);
      if (immediateSpawnError) {
        chatBridge?.stop?.();
        const detail = formatLaunchSpawnError(immediateSpawnError, { exePath, corePath, romPath, systemDir, saveDir, logDir, args });
        writeLogLine?.("warn", "sekaiemu", `runtime spawn failed: ${detail}`);
        return { ok: false, error: "sekaiemu_launch_failed", detail };
      }
      const stdoutTail = { value: "" };
      const stderrTail = { value: "" };
      logProcessStream(wrapped.proc.stdout, "info", "sekaiemu", "stdout", stdoutTail);
      logProcessStream(wrapped.proc.stderr, "warn", "sekaiemu", "stderr", stderrTail);
      wrapped.proc.once("error", (err) => {
        const detail = formatLaunchSpawnError(err, { exePath, corePath, romPath, systemDir, saveDir, logDir, args });
        writeLogLine?.("warn", "sekaiemu", `runtime process error pid=${wrapped.proc.pid || ""}: ${detail}`);
        emitSessionEvent({
          event: "error",
          step: "emu",
          error: "sekaiemu_launch_failed",
          detail,
          moduleId: safeModuleId,
        });
      });
      nativeGameProcs.set(wrapped.proc.pid, wrapped.proc);
      rememberSekaiemuSession(wrapped.proc.pid, {
        exePath,
        corePath,
        romPath,
        memorySocketPath,
        moduleId: safeModuleId,
        lobbyId: String(options?.chatBridge?.lobbyId || "").trim(),
        lobbyTitle: String(options?.chatBridge?.lobbyTitle || options?.lobbyTitle || "").trim(),
        channelId: String(options?.chatBridge?.channelId || (options?.chatBridge?.lobbyId ? `lobby:${options.chatBridge.lobbyId}` : "")).trim(),
        slot: apSlot,
        game: apGame,
        apiBaseUrl: options.apiBaseUrl,
        authToken: options.authToken,
        deviceId: options.deviceId,
        playerAlias,
        playerAliasMap,
        hudStatePath,
        hudEventsPath,
        traceLogPath: sklmiTraceLogPath,
        layoutPreview,
        launchedAt: nowIso(),
      });
      if (chatBridge) sekaiemuChatBridges.set(wrapped.proc.pid, chatBridge);
      wrapped.proc.on("exit", (code, signal) => {
        const exitLevel = code === 0 ? "info" : "warn";
        writeLogLine?.(exitLevel, "sekaiemu", `runtime exited pid=${wrapped.proc.pid} code=${code} signal=${signal || ""} stdout_tail=${oneLine(stdoutTail.value)} stderr_tail=${oneLine(stderrTail.value)}`);
        nativeGameProcs.delete(wrapped.proc.pid);
        forgetSekaiemuSession(wrapped.proc.pid);
        emitSessionEvent({
          event: "exited",
          type: "exited",
          source: "sekaiemu",
          pid: wrapped.proc.pid,
          moduleId: cleanupModuleId,
          slot: cleanupSlot,
          code,
          signal,
          at: nowIso(),
        });
        if (!layoutPreview && cleanupSlot) {
          stopArchipelagoClientsForSession(cleanupModuleId, cleanupSlot, `sekaiemu_exit:${wrapped.proc.pid}`).catch((err) => {
            writeLogLine("warn", "archipelagoclient", `session cleanup after sekaiemu exit failed: ${String(err?.message || err || "")}`);
          });
        }
        if (!layoutPreview) {
          cleanupSklmiCompanionForSocket(memorySocketPath, `sekaiemu_exit:${wrapped.proc.pid}`);
          triggerCoupledRuntimeTeardown("sekaiemu", wrapped.proc.pid, { code, signal });
        }
        const activeBridge = sekaiemuChatBridges.get(wrapped.proc.pid);
        activeBridge?.stop?.();
        sekaiemuChatBridges.delete(wrapped.proc.pid);
      });
      return { ok: true, pid: wrapped.proc.pid, method: "libretro-spike", exePath, corePath, romPath, memorySocketPath, layoutPreview, chatBridge };
    } catch (err) {
      chatBridge?.stop?.();
      const detail = formatLaunchSpawnError(err, { exePath, corePath, romPath, systemDir, saveDir, logDir, args });
      writeLogLine?.("warn", "sekaiemu", `runtime launch failed: ${detail}`);
      return { ok: false, error: "sekaiemu_launch_failed", detail };
    }
  };

  const tryCaptureSekaiemuInput = async (options = {}) => {
    const profile = String(options.profile || options.coreId || "snes").trim() || "snes";
    const exePath = resolveSekaiemuExecutable();
    if (!exePath) return { ok: false, error: "sekaiemu_not_found" };
    const root = path.join(app.getPath("userData"), "runtime", "sekaiemu-input-capture");
    ensureDir(root);
    const outputPath = path.join(root, `${Date.now()}-${crypto.randomBytes(4).toString("hex")}.cfg`);
    const logDir = path.join(app.getPath("userData"), "logs", "sekaiemu");
    ensureDir(logDir);
    const args = ["--input-capture", outputPath, "--input-capture-profile", profile, "--save-dir", root, "--log-dir", logDir];
    try {
      const spawnEnv = { ...process.env };
      if (process.platform === "win32") {
        const runtimeBinDir = path.dirname(exePath);
        const priorPath = spawnEnv.PATH || spawnEnv.Path || "";
        spawnEnv.PATH = priorPath ? `${runtimeBinDir}${path.delimiter}${priorPath}` : runtimeBinDir;
        spawnEnv.Path = spawnEnv.PATH;
      }
      writeLogLine?.("info", "sekaiemu", `launching input capture exe=${exePath} profile=${profile} output=${outputPath}`);
      const wrapped = spawnMaybeGamescope(exePath, args, { stdio: ["ignore", "pipe", "pipe"], env: spawnEnv });
      if (!wrapped.ok) return { ok: false, error: wrapped.error || "sekaiemu_input_capture_failed", detail: wrapped.detail || "" };
      const immediateSpawnError = await waitForSpawnFailure(wrapped.proc);
      if (immediateSpawnError) {
        return { ok: false, error: "sekaiemu_input_capture_failed", detail: formatLaunchSpawnError(immediateSpawnError, { exePath, args }) };
      }
      const stdoutTail = { value: "" };
      const stderrTail = { value: "" };
      logProcessStream(wrapped.proc.stdout, "info", "sekaiemu", "input-capture stdout", stdoutTail);
      logProcessStream(wrapped.proc.stderr, "warn", "sekaiemu", "input-capture stderr", stderrTail);
      const exitCode = await new Promise((resolve) => wrapped.proc.once("exit", (code) => resolve(Number(code || 0))));
      if (exitCode !== 0) {
        return {
          ok: false,
          error: "sekaiemu_input_capture_cancelled",
          detail: oneLine(stderrTail.value || stdoutTail.value || `exit=${exitCode}`),
        };
      }
      if (!fileExists(outputPath)) return { ok: false, error: "sekaiemu_input_capture_missing_output" };
      const mappings = parseSekaiemuInputConfigMappings(outputPath);
      return { ok: true, profile, outputPath, mappings };
    } catch (err) {
      return { ok: false, error: "sekaiemu_input_capture_failed", detail: String(err?.message || err || "") };
    }
  };

  return {
    getSekaiemuSettings,
    writeSekaiemuRuntimeConfig,
    fileExists,
    dirExists,
    pathExists,
    findExecutableByNamesInDir,
    resolveSekaiemuExecutable,
    resolveSekaiemuCorePath,
    parseArchipelagoAddress,
    resolveSklmiRuntimeForSekaiemu,
    resolveSklmiManifestDirForSekaiemu,
    resolveSekaiemuTrackerPackPath,
    tryLaunchSekaiemu,
    tryCaptureSekaiemuInput,
  };
};

module.exports = { createSekaiemuRuntime };
