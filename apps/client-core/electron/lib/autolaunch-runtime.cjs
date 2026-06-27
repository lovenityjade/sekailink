"use strict";

const createAutoLaunchRuntime = (deps = {}) => {
  const {
    path,
    isPlainObject,
    normalizeIpcPath,
    normalizeIpcString,
    downloadToDirWithProgress,
    getRuntimeDownloadsDir,
    emitSessionEvent,
    writeLogLine,
    writeLogJson,
    findModuleByApGameName,
    validateSetupForModule,
    getModuleManifest,
    resolveModuleForDownload,
    tryHandleDownloadedArtifact,
    moduleHasExternalTracker,
    launchBizHawk,
    tryLaunchSoh,
    tryLaunchSekaiemu,
    startCommonClient,
    sendCommonClientCommand,
    startBizHawkClient,
    sendBizHawkClientCommand,
    stopSniBridge,
    startSniBridge,
    waitForAnyTcpPort,
    startArchipelagoClient,
    startWebApClient,
    stopArchipelagoClientsForSession,
    launchPopTracker,
    stopPopTracker = async () => ({ ok: true }),
    stopGameProcess = async () => ({ ok: true }),
    requestSaveState = async () => ({ ok: false, error: "save_state_not_available" }),
    applyLayoutBestEffort,
    ensureRuntimePacksForModule,
    getDefaultPatcherConfigPath,
    patchedRomCache,
    runPatcher,
  } = deps;

  const isPidAlive = (pid) => {
    const safePid = Number(pid);
    if (!Number.isFinite(safePid) || safePid <= 0) return false;
    try {
      process.kill(safePid, 0);
      return true;
    } catch (_err) {
      return false;
    }
  };

  const resolveWebSocketImpl = () => {
    if (typeof WebSocket === "function") return WebSocket;
    try {
      return require("ws");
    } catch (_err) {
      return null;
    }
  };

  const normalizeApGameName = (value) =>
    String(value || "")
      .trim()
      .toLowerCase()
      .replace(/[^a-z0-9]+/g, "");

  const apRoomInfoProbe = async (serverAddress, timeoutMs = 5000) => {
    const address = String(serverAddress || "").trim();
    if (!address) return { ok: false, error: "missing_server_address" };
    const WebSocketCtor = resolveWebSocketImpl();
    if (!WebSocketCtor) return { ok: false, error: "websocket_unavailable" };
    const uri = address.startsWith("ws://") || address.startsWith("wss://") ? address : `ws://${address}`;
    return await new Promise((resolve) => {
      let settled = false;
      const finish = (payload) => {
        if (settled) return;
        settled = true;
        clearTimeout(timer);
        try {
          socket.close();
        } catch (_err) {
          // ignore close failures
        }
        resolve(payload);
      };
      const timer = setTimeout(() => finish({ ok: false, error: "roominfo_timeout" }), timeoutMs);
      let socket;
      try {
        socket = new WebSocketCtor(uri);
      } catch (err) {
        finish({ ok: false, error: "roominfo_connect_failed", detail: String(err?.message || err || "") });
        return;
      }
      const bind = (event, handler) => {
        if (typeof socket.addEventListener === "function") {
          socket.addEventListener(event, handler);
        } else if (typeof socket.on === "function") {
          socket.on(event, handler);
        } else {
          socket[`on${event}`] = handler;
        }
      };
      bind("open", () => {});
      bind("error", (err) => {
        finish({ ok: false, error: "roominfo_socket_error", detail: String(err?.message || err || "") });
      });
      bind("message", (event) => {
        try {
          const data = event?.data !== undefined ? event.data : event;
          const raw = typeof data === "string" ? data : String(data || "");
          const packets = JSON.parse(raw);
          const roomInfo = Array.isArray(packets)
            ? packets.find((packet) => packet && packet.cmd === "RoomInfo")
            : null;
          if (!roomInfo) {
            finish({ ok: false, error: "roominfo_missing", packets });
            return;
          }
          finish({
            ok: true,
            uri,
            seedName: roomInfo.seed_name || "",
            games: Array.isArray(roomInfo.games) ? roomInfo.games : [],
          });
        } catch (err) {
          finish({ ok: false, error: "roominfo_bad_json", detail: String(err?.message || err || "") });
        }
      });
    });
  };

  const validateRoomEndpointForLaunch = async ({ serverAddress, apGameName, slot, moduleId = "" }) => {
    const expectedGame = String(apGameName || "").trim();
    if (!serverAddress || !expectedGame) return { ok: true, skipped: true };
    const probe = await apRoomInfoProbe(serverAddress);
    if (!probe.ok) {
      writeLogLine("warn", "autolaunch", `room endpoint probe failed: ${probe.error || "unknown"} ${probe.detail || ""}`);
      return { ok: true, skipped: true, warning: probe.error || "roominfo_probe_failed" };
    }
    const expected = normalizeApGameName(expectedGame);
    const actualGames = Array.isArray(probe.games) ? probe.games : [];
    const matched = actualGames.some((game) => normalizeApGameName(game) === expected);
    if (!matched) {
      writeLogJson("room-endpoint-mismatch", {
        serverAddress,
        moduleId,
        slot,
        expectedGame,
        actualGames,
        seedName: probe.seedName || "",
      });
      return {
        ok: false,
        error: "room_endpoint_mismatch",
        detail: `Room endpoint ${serverAddress} is not serving ${expectedGame}. It serves: ${actualGames.join(", ") || "unknown"}.`,
        actualGames,
        expectedGame,
      };
    }
    return { ok: true, seedName: probe.seedName || "" };
  };

  const multiGameSession = {
    entries: [],
    activeEntryId: "",
    current: {
      moduleId: "",
      emuPid: 0,
      trackerPid: 0,
      slot: "",
    },
    base: {
      serverAddress: "",
      password: "",
      chatBridge: null,
      apiBaseUrl: "",
      authToken: null,
      deviceId: "",
      forceTrackerVariantPrompt: false,
    },
  };

  const multiGameSaveStatePrompt = {
    type: "save_state_before_switch",
    title: "Do you wish to make a save state?",
    message: "Do you wish to make a save state?",
    choices: [
      { id: "yes", label: "Yes" },
      { id: "no", label: "No" },
      { id: "cancel", label: "Cancel" },
    ],
  };

  const makeEntryId = (entry = {}, index = 0) => {
    const raw = String(entry.id || `${entry.apGameName || "game"}-${entry.slot || "slot"}-${index}`).trim();
    return raw.replace(/[^a-z0-9_.:-]+/gi, "-").slice(0, 96) || `game-${index}`;
  };

  const resolveMultiGameEntry = async (entry = {}, index = 0) => {
    const downloadUrl = String(entry.downloadUrl || "").trim();
    const apGameName = String(entry.apGameName || "").trim();
    const plan = await planSessionAutoLaunch({ downloadUrl, apGameName }).catch((err) => ({
      ok: false,
      error: String(err?.message || err || "plan_failed"),
    }));
    if (!plan?.ok || !plan.moduleId) return null;
    const manifest = getModuleManifest(plan.moduleId) || {};
    const emu = String(manifest.emu || "").trim().toLowerCase();
    if (emu !== "sekaiemu" && emu !== "sekaiemu-libretro") return null;
    return {
      id: makeEntryId(entry, index),
      label: String(entry.label || apGameName || manifest.display_name || manifest.game_id || plan.moduleId).trim(),
      configName: String(entry.configName || "").trim(),
      moduleId: String(plan.moduleId || "").trim(),
      gameId: String(plan.gameId || manifest.game_id || "").trim(),
      emu,
      downloadUrl,
      apGameName,
      slot: String(entry.slot || "").trim(),
      playerAlias: String(entry.playerAlias || "").trim(),
      trackerVariant: String(entry.trackerVariant || "").trim(),
      hasTracker: moduleHasExternalTracker(manifest),
    };
  };

  const rememberMultiGameSession = async (options = {}, launchResult = {}) => {
    const rawEntries = Array.isArray(options.multiGameEntries) ? options.multiGameEntries : [];
    const entries = [];
    for (let i = 0; i < rawEntries.length; i += 1) {
      const resolved = await resolveMultiGameEntry(rawEntries[i], i);
      if (resolved) entries.push(resolved);
    }
    if (!entries.length) {
      multiGameSession.entries = [];
      multiGameSession.activeEntryId = "";
      return;
    }
    const active = entries.find((entry) =>
      entry.downloadUrl && entry.downloadUrl === String(options.downloadUrl || "").trim()
    ) || entries.find((entry) => entry.apGameName && entry.apGameName === String(options.apGameName || "").trim()) || entries[0];
    multiGameSession.entries = entries;
    multiGameSession.activeEntryId = active.id;
    multiGameSession.current = {
      moduleId: String(launchResult.moduleId || active.moduleId || "").trim(),
      emuPid: Number(launchResult.emuPid || launchResult.gamePid || 0) || 0,
      trackerPid: Number(launchResult.trackerPid || 0) || 0,
      slot: String(options.slot || active.slot || "").trim(),
    };
    multiGameSession.base = {
      serverAddress: String(options.serverAddress || "").trim(),
      password: options.password,
      chatBridge: isPlainObject(options.chatBridge) ? options.chatBridge : null,
      apiBaseUrl: options.apiBaseUrl,
      authToken: options.authToken,
      deviceId: options.deviceId,
      forceTrackerVariantPrompt: options.forceTrackerVariantPrompt === true,
    };
    emitSessionEvent({
      event: "multi-game-ready",
      activeEntryId: multiGameSession.activeEntryId,
      entries: entries.map((entry) => ({
        id: entry.id,
        label: entry.label,
        moduleId: entry.moduleId,
        apGameName: entry.apGameName,
        slot: entry.slot,
        hasTracker: entry.hasTracker,
      })),
    });
  };

  const listMultiGameSession = () => ({
    ok: true,
    activeEntryId: multiGameSession.activeEntryId,
    current: { ...multiGameSession.current },
    entries: multiGameSession.entries.map((entry) => ({ ...entry })),
    saveStatePrompt: multiGameSaveStatePrompt,
  });

  const stopCurrentMultiGameProcesses = async () => {
    const trackerPid = Number(multiGameSession.current.trackerPid || 0) || 0;
    const emuPid = Number(multiGameSession.current.emuPid || 0) || 0;
    if (trackerPid > 0) {
      await stopPopTracker(trackerPid).catch((err) => {
        writeLogLine("warn", "multigame", `tracker stop failed pid=${trackerPid}: ${String(err?.message || err || "")}`);
      });
    }
    if (emuPid > 0) {
      await stopGameProcess(emuPid, "multi_game_switch").catch((err) => {
        writeLogLine("warn", "multigame", `game stop failed pid=${emuPid}: ${String(err?.message || err || "")}`);
      });
    }
  };

  const launchGameRuntimeForModule = async (options = {}) => {
    const moduleId = String(options.moduleId || "").trim();
    const manifest = options.manifest || getModuleManifest(moduleId) || {};
    const emu = String(manifest.emu || "bizhawk").trim().toLowerCase();
    const romPath = String(options.romPath || "").trim();
  
    emitSessionEvent({ event: "status", status: "Launching game runtime...", moduleId });
    writeLogLine(
      "info",
      "autolaunch",
      `launching runtime: emu=${emu} romPath=${romPath} moduleId=${moduleId} reused=${options.reusedPatchedRom ? "true" : "false"}`
    );
  
    let launchRes = null;
    if (emu === "bizhawk") {
      launchRes = await launchBizHawk({ romPath, moduleId });
    } else if (emu === "soh") {
      launchRes = await tryLaunchSoh();
    } else if (emu === "sekaiemu" || emu === "sekaiemu-libretro") {
      launchRes = await tryLaunchSekaiemu({
        romPath,
        moduleId,
        manifest,
        serverAddress: options.serverAddress,
        slot: options.slot,
        playerAlias: options.playerAlias,
        password: options.password,
        chatBridge: options.chatBridge,
        apiBaseUrl: options.apiBaseUrl,
        authToken: options.authToken,
        deviceId: options.deviceId,
      });
    } else {
      launchRes = { ok: false, error: "unsupported_emulator", emu };
    }
  
    writeLogLine(
      "info",
      "autolaunch",
      `runtime result: ok=${launchRes?.ok} pid=${launchRes?.pid || ""} error=${launchRes?.error || ""} memory=${launchRes?.memorySocketPath || ""}`
    );
  
    if (!launchRes?.ok) {
      emitSessionEvent({ event: "error", step: "emu", error: launchRes?.error || "emu_failed", moduleId, emu });
      return { ok: false, error: launchRes?.error || "emu_failed", detail: launchRes?.detail || "", emu };
    }
  
  	  return {
  	    ok: true,
  	    emu,
  	    pid: launchRes.pid,
  	    detail: launchRes?.detail || "",
  	    memorySocketPath: launchRes?.memorySocketPath || "",
  	    chatBridge: launchRes?.chatBridge || null,
  	  };
  	};
  
  const connectRuntimeBridgeForModule = async (options = {}) => {
    const moduleId = String(options.moduleId || "").trim();
    const manifest = options.manifest || getModuleManifest(moduleId) || {};
    const emu = String(manifest.emu || "bizhawk").trim().toLowerCase();
    const serverAddress = String(options.serverAddress || "").trim();
    const slot = String(options.slot || "").trim();
    const password = options.password;
  	  const patchPath = normalizeIpcPath(options.patchPath || options.patch || "");
  	  const romPath = normalizeIpcPath(options.romPath || options.rom || "");
  	  const memorySocketPath = normalizeIpcString(options.memorySocketPath || options.memorySocket || "", 500);
  	  const chatBridge = isPlainObject(options.chatBridge) ? options.chatBridge : null;
    const clientMode = String(manifest.client_mode || ((emu === "sekaiemu" || emu === "sekaiemu-libretro") ? "embedded" : "")).trim().toLowerCase();
  
    emitSessionEvent({ event: "status", status: "Connecting to server...", moduleId });
    writeLogLine("info", "autolaunch", `starting bridge: emu=${emu} serverAddress=${serverAddress} slot=${slot}`);
  
    if (emu === "bizhawk") {
      const apClient = String(manifest.ap_client || "").trim().toLowerCase();
      if (apClient === "sni") {
        emitSessionEvent({ event: "status", status: "Preparing SNES connection...", moduleId });
        const luaPort = await waitForAnyTcpPort("127.0.0.1", [43055, 43056, 43057, 43058, 43059, 43060], 9000);
        writeLogLine("info", "autolaunch", `lua connector port detected: ${luaPort || "none"}`);
        await stopSniBridge();
        const bridgeRes = await startSniBridge({
          host: "127.0.0.1",
          port: 23074,
          luaHost: "127.0.0.1",
          luaPortStart: 43055,
          luaPortEnd: 43060,
        });
        if (!bridgeRes?.ok) {
          emitSessionEvent({ event: "warning", step: "sni-bridge", error: bridgeRes?.error || "sni_bridge_failed", moduleId });
        }
        const clientRes = await startBizHawkClient({ clientKind: "sni" });
        writeLogLine("info", "autolaunch", `sniclient started: ok=${clientRes?.ok} alreadyRunning=${clientRes?.alreadyRunning || false}`);
        await sendBizHawkClientCommand({ cmd: "connect", address: serverAddress, slot, password });
        return { ok: true, mode: "bizhawk-sni" };
      }
      const clientRes = await startBizHawkClient({ address: serverAddress, slot, clientKind: "bizhawk" });
      writeLogLine("info", "autolaunch", `bizhawkclient started: ok=${clientRes?.ok} alreadyRunning=${clientRes?.alreadyRunning || false}`);
      await sendBizHawkClientCommand({ cmd: "connect", address: serverAddress, slot, password });
      return { ok: true, mode: "bizhawk" };
    }
  
    if ((emu === "sekaiemu" || emu === "sekaiemu-libretro") && clientMode === "sklmi") {
      return { ok: false, error: "sklmi_bridge_pending" };
    }

    if ((emu === "sekaiemu" || emu === "sekaiemu-libretro") && clientMode === "mmbn3") {
      const clientSpec = manifest.archipelago_client && typeof manifest.archipelago_client === "object"
        ? manifest.archipelago_client
        : {};
      const clientRes = await startArchipelagoClient({
        gameKey: clientSpec.game_key || manifest.game_id || moduleId,
        world: clientSpec.world || manifest.ap_world || "",
        wrapper: "mmbn3",
        moduleId,
        address: serverAddress,
        password,
        patchPath,
        romPath,
        memorySocketPath,
        chatBridge,
      });
      writeLogLine(
        "info",
        "autolaunch",
        `MMBN3 client started: ok=${clientRes?.ok} id=${clientRes?.clientId || ""} error=${clientRes?.error || ""}`
      );
      if (!clientRes?.ok) {
        emitSessionEvent({
          event: "warning",
          step: "mmbn3-client",
          error: clientRes?.error || "mmbn3_client_failed",
          detail: clientRes?.detail || "",
          moduleId,
        });
        return { ok: false, error: clientRes?.error || "mmbn3_client_failed", detail: clientRes?.detail || "" };
      }
      return { ok: true, mode: "mmbn3", clientId: clientRes.clientId };
    }

    if (["web_ap_client", "web-ap-client", "webclient", "browser_ap_client"].includes(clientMode)) {
      const webSpec = manifest.web_ap_client && typeof manifest.web_ap_client === "object"
        ? manifest.web_ap_client
        : {};
      emitSessionEvent({ event: "status", status: "Preparing web AP client...", moduleId });
      await stopSniBridge();
      const bridgeRes = await startSniBridge({
        host: "127.0.0.1",
        port: 23074,
        memorySocketPath,
        memorySocket: memorySocketPath,
      });
      if (!bridgeRes?.ok) {
        emitSessionEvent({ event: "warning", step: "sni-bridge", error: bridgeRes?.error || "sni_bridge_failed", moduleId });
      }
      const clientRes = await startWebApClient({
        gameKey: webSpec.game_key || manifest.game_id || moduleId,
        moduleId,
        url: webSpec.url || webSpec.client_url || "",
        address: serverAddress,
        serverAddress,
        slot,
        password,
        memorySocketPath,
        chatBridge,
        show: webSpec.show_window === true,
      });
      writeLogLine(
        "info",
        "autolaunch",
        `web AP client started: ok=${clientRes?.ok} id=${clientRes?.clientId || ""} error=${clientRes?.error || ""}`
      );
      if (!clientRes?.ok) {
        emitSessionEvent({
          event: "warning",
          step: "web-ap-client",
          error: clientRes?.error || "web_ap_client_failed",
          detail: clientRes?.detail || "",
          moduleId,
        });
        return { ok: false, error: clientRes?.error || "web_ap_client_failed", detail: clientRes?.detail || "" };
      }
      return { ok: true, mode: "web-ap-client", clientId: clientRes.clientId };
    }
  
    if (["archipelago", "archipelago-client", "apclient", "wrapper"].includes(clientMode)) {
      const sekaiemu = manifest.sekaiemu && typeof manifest.sekaiemu === "object" ? manifest.sekaiemu : {};
      const clientSpec = manifest.archipelago_client && typeof manifest.archipelago_client === "object"
        ? manifest.archipelago_client
        : {};
      const clientRes = await startArchipelagoClient({
        gameKey: clientSpec.game_key || manifest.game_id || moduleId,
        world: clientSpec.world || manifest.ap_world || manifest.world || "",
        game: sekaiemu.ap_game || manifest.ap_game || manifest.display_name || "",
        wrapper: clientSpec.wrapper || manifest.ap_client || "",
        module: clientSpec.module || manifest.ap_client_module || "",
        moduleId,
        address: serverAddress,
        slot,
        password,
        sniAddress: "ws://127.0.0.1:23074",
  	      patchPath,
  	      romPath,
  	      memorySocketPath,
  	      memoryBridge: clientSpec.memory_bridge || clientSpec.memoryBridge || "",
  	      chatBridge,
  	    });
      writeLogLine(
        "info",
        "autolaunch",
        `archipelago client started: ok=${clientRes?.ok} id=${clientRes?.clientId || ""} error=${clientRes?.error || ""}`
      );
      if (!clientRes?.ok) {
        emitSessionEvent({
          event: "warning",
          step: "archipelago-client",
          error: clientRes?.error || "archipelago_client_failed",
          detail: clientRes?.detail || "",
          moduleId,
        });
        return { ok: false, error: clientRes?.error || "archipelago_client_failed", detail: clientRes?.detail || "" };
      }
      return { ok: true, mode: "archipelago-client", clientId: clientRes.clientId };
    }
  
    if (clientMode !== "embedded") {
      const clientRes = await startCommonClient({ address: serverAddress, slot, password });
      writeLogLine("info", "autolaunch", `commonclient started: ok=${clientRes?.ok} alreadyRunning=${clientRes?.alreadyRunning || false}`);
      await sendCommonClientCommand({ cmd: "connect", address: serverAddress, slot, password });
      return { ok: true, mode: "commonclient" };
    }
  
    return { ok: true, mode: "embedded" };
  };
  
  const launchTrackerForModuleSession = async (options = {}) => {
    const moduleId = String(options.moduleId || "").trim();
    const manifest = options.manifest || getModuleManifest(moduleId) || {};
    const serverAddress = String(options.serverAddress || "").trim();
    const slot = String(options.slot || "").trim();
    const password = options.password;
    const forceTrackerVariantPrompt = options.forceTrackerVariantPrompt === true;
    const trackerVariant = String(options.trackerVariant || options.packVariant || options.tracker_variant || "").trim();
  
    if (!moduleHasExternalTracker(manifest)) {
      return { ok: true, skipped: true, pid: null };
    }
  
    emitSessionEvent({ event: "status", status: "Launching tracker...", moduleId });
    writeLogLine("info", "autolaunch", `launching tracker: moduleId=${moduleId} serverAddress=${serverAddress} slot=${slot}`);
    const trackerRes = await launchPopTracker({
      moduleId,
      apHost: serverAddress,
      apSlot: slot,
      apPass: password,
      apAutoconnect: true,
      packVariant: trackerVariant,
      trackerVariant,
      forceTrackerVariantPrompt,
    });
    writeLogLine("info", "autolaunch", `tracker result: ok=${trackerRes?.ok} pid=${trackerRes?.pid || ""} error=${trackerRes?.error || ""}`);
  
    if (trackerRes?.error === "tracker_variant_cancelled") {
      emitSessionEvent({ event: "error", step: "tracker", error: "tracker_variant_cancelled", moduleId });
      return { ok: false, error: "tracker_variant_cancelled", pid: null };
    }
  
    if (!trackerRes?.ok) {
      emitSessionEvent({ event: "warning", step: "tracker", error: trackerRes?.error || "tracker_failed", moduleId });
      return { ok: true, warning: trackerRes?.error || "tracker_failed", pid: null };
    }
  
    return { ok: true, pid: trackerRes?.pid || null };
  };
  
  const applyRuntimeLayoutForSession = async (options = {}) => {
    const moduleId = String(options.moduleId || "").trim();
    const gamePid = Number(options.gamePid || 0);
    const trackerPid = Number(options.trackerPid || 0) || null;
  
    emitSessionEvent({ event: "status", status: "Applying window layout...", moduleId });
    writeLogLine("info", "autolaunch", `applying layout: gamePid=${gamePid} trackerPid=${trackerPid || ""}`);
    const layoutRes = await applyLayoutBestEffort({ gamePid, trackerPid });
    if (!layoutRes?.ok) {
      emitSessionEvent({ event: "warning", step: "layout", error: layoutRes?.error || "layout_failed", moduleId });
    }
    return layoutRes;
  };

  const planSessionAutoLaunch = async (options = {}) => {
    const downloadUrl = String(options.downloadUrl || "").trim();
    const apGameName = String(options.apGameName || "").trim();
  
    if (!downloadUrl) {
      const moduleId = findModuleByApGameName(apGameName);
      if (!moduleId) return { ok: false, error: "missing_patch_url" };
      const setup = await validateSetupForModule(moduleId);
      if (!setup.ok) return { ...setup, moduleId };
      const manifest = setup.manifest || getModuleManifest(moduleId) || {};
      if (String(manifest.patcher || "").toLowerCase() !== "none") {
        return { ok: false, error: "missing_patch_url", moduleId };
      }
      return {
        ok: true,
        moduleId,
        gameId: String(manifest.game_id || "").trim(),
        emu: String(manifest.emu || "").trim().toLowerCase(),
        launchKind: "native_patchless",
        canAutoPatch: false,
      };
    }
  
    let pathname = "";
    try {
      pathname = new URL(downloadUrl).pathname || "";
    } catch (_err) {
      pathname = String(downloadUrl || "");
    }
    const ext = path.extname(pathname).toLowerCase();
    const extResolved = ext ? resolveModuleForDownload(downloadUrl) : { ok: false, error: "unknown_extension" };
    const byName = !extResolved.ok ? findModuleByApGameName(apGameName) : null;
    const resolved = extResolved.ok
      ? extResolved
      : byName
        ? { ok: true, moduleId: byName, gameId: "", ext }
        : extResolved;
  
    if (!resolved.ok || !resolved.moduleId) {
      return {
        ok: true,
        ext,
        launchKind: "manual_unknown_artifact",
        canAutoPatch: false,
      };
    }
  
    const moduleId = String(resolved.moduleId || "").trim();
    const canAutoPatch = Boolean(extResolved.ok);
    if (!canAutoPatch) {
      return {
        ok: true,
        moduleId,
        gameId: String(resolved.gameId || "").trim(),
        ext,
        launchKind: "manual_known_artifact",
        canAutoPatch: false,
      };
    }
  
    const setup = await validateSetupForModule(moduleId);
    if (!setup.ok) {
      return {
        ...setup,
        moduleId,
        ext,
        launchKind: "auto_patch",
        canAutoPatch: true,
      };
    }
  
    const manifest = setup.manifest || getModuleManifest(moduleId) || {};
    return {
      ok: true,
      moduleId,
      gameId: String(resolved.gameId || manifest.game_id || "").trim(),
      ext,
      emu: String(manifest.emu || "").trim().toLowerCase(),
      launchKind: "auto_patch",
      canAutoPatch: true,
    };
  };
  
  const autoLaunchFromPatchUrl = async (options = {}) => {
    const downloadUrl = String(options.downloadUrl || "").trim();
    const serverAddress = options.serverAddress;
    const slot = options.slot;
    const playerAlias = options.playerAlias;
    const password = options.password;
    const apGameName = options.apGameName;
    const forceTrackerVariantPrompt = options.forceTrackerVariantPrompt === true;
    const trackerVariant = String(options.trackerVariant || options.packVariant || options.tracker_variant || "").trim();
    const chatBridge = isPlainObject(options.chatBridge) ? options.chatBridge : null;
    const apiBaseUrl = options.apiBaseUrl;
    const authToken = options.authToken;
    const deviceId = options.deviceId;
  
    if (!serverAddress) return { ok: false, error: "missing_server_address" };
    if (!slot) return { ok: false, error: "missing_slot" };

    emitSessionEvent({ event: "status", status: "Validating room endpoint...", apGameName, slot });
    const endpointCheck = await validateRoomEndpointForLaunch({
      serverAddress,
      apGameName,
      slot,
    });
    if (!endpointCheck.ok) {
      emitSessionEvent({
        event: "error",
        step: "room-endpoint",
        error: endpointCheck.error || "room_endpoint_mismatch",
        detail: endpointCheck.detail || "",
        apGameName,
        slot,
      });
      return endpointCheck;
    }
  
    // Patchless flow for native AP-integrated games (e.g. Ship of Harkinian).
    if (!downloadUrl) {
      const moduleId = findModuleByApGameName(apGameName);
      if (!moduleId) return { ok: false, error: "missing_patch_url" };
  
      emitSessionEvent({ event: "status", status: "Validating setup...", moduleId });
      const setup = await validateSetupForModule(moduleId);
      if (!setup.ok) {
        emitSessionEvent({
          event: "error",
          step: "validate",
          error: setup.error,
          detail: setup.detail,
          setupArea: setup.setupArea,
          moduleId,
          gameId: setup.gameId,
        });
        return setup;
      }
  
      const manifest = setup.manifest || getModuleManifest(moduleId) || {};
      if (String(manifest.patcher || "").toLowerCase() !== "none") {
        return { ok: false, error: "missing_patch_url" };
      }
  
      const runtimeRes = await launchGameRuntimeForModule({
        moduleId,
        manifest,
        serverAddress,
        slot,
        playerAlias,
        password,
        chatBridge,
        apiBaseUrl,
        authToken,
        deviceId,
      });
      if (!runtimeRes?.ok) {
        return { ok: false, error: runtimeRes?.error || "emu_failed", detail: runtimeRes?.detail || "" };
      }
  
      await applyRuntimeLayoutForSession({ moduleId, gamePid: runtimeRes.pid, trackerPid: null });
  
      const note =
        "Ship of Harkinian started. Connect from in-game menu: ESC > Network > Archipelago.";
      emitSessionEvent({ event: "status", status: note, moduleId });
      emitSessionEvent({
        event: "ready",
        moduleId,
        emuPid: runtimeRes.pid,
        trackerPid: null,
        note,
      });
      const result = {
        ok: true,
        moduleId,
        emuPid: runtimeRes.pid,
        trackerPid: null,
        note,
        noPatch: true,
      };
      if (!options.skipMultiGameRegister) await rememberMultiGameSession(options, result);
      return result;
    }
  
    emitSessionEvent({ event: "status", status: "Downloading...", downloadUrl });
    let dl = null;
    try {
      const downloadHeaders = {
        "Accept": "application/octet-stream,*/*",
      };
      if (authToken) downloadHeaders.Authorization = `Bearer ${authToken}`;
      if (deviceId) downloadHeaders["X-SekaiLink-Device-Id"] = String(deviceId);
      dl = await downloadToDirWithProgress(downloadUrl, getRuntimeDownloadsDir(), {
        defaultBasename: "ap-download",
        headers: downloadHeaders,
        onProgress: (p) => {
          // Drive a real progress bar in the renderer (Lobby launch modal).
          // Keep the status message stable to avoid log/DOM spam.
          emitSessionEvent({
            event: "download-progress",
            downloadUrl,
            receivedBytes: p?.receivedBytes ?? 0,
            totalBytes: p?.totalBytes ?? 0,
            percent: p?.percent ?? null,
          });
        },
      });
    } catch (err) {
      const detail = String(err?.message || err || "download_failed");
      emitSessionEvent({ event: "error", step: "download", error: "download_failed", detail, downloadUrl });
      return { ok: false, error: "download_failed", detail, downloadUrl };
    }
    const downloadedPath = dl?.path;
    const ext = downloadedPath ? path.extname(downloadedPath).toLowerCase() : "";
    emitSessionEvent({ event: "status", status: "Resolving game type...", downloadUrl, ext });
  
    // Before falling back to manual, try dedicated handlers for non-AP patch artifacts (slot files).
    const artifact = await tryHandleDownloadedArtifact({ downloadedPath, ext, apGameName });
    if (artifact?.ok && artifact?.handled) {
      const artifactModuleId =
        (ext === ".apsm64ex" ? "sm64ex" : "") ||
        findModuleByApGameName(apGameName) ||
        "";
  
      emitSessionEvent({
        event: "status",
        status: artifact.gamePid ? "Game package prepared. Connecting..." : "Game package installed. Manual action may be required.",
        downloadedPath,
        ext,
        apGameName,
        installedPath: artifact.installedPath,
        handlerError: artifact.error,
        note: artifact.note,
      });
  
      // Still connect CommonClient for chat/commands.
      emitSessionEvent({ event: "status", status: "Connecting to server...", moduleId: artifactModuleId });
      await startCommonClient({ address: serverAddress, slot, password });
      await sendCommonClientCommand({ cmd: "connect", address: serverAddress, slot, password });
  
      let trackerPid = null;
      const artifactManifest = artifactModuleId ? (getModuleManifest(artifactModuleId) || {}) : {};
      if (artifactModuleId && moduleHasExternalTracker(artifactManifest)) {
        emitSessionEvent({ event: "status", status: "Launching tracker...", moduleId: artifactModuleId });
        const trackerRes = await launchPopTracker({
          moduleId: artifactModuleId,
          apHost: serverAddress,
          apSlot: slot,
          apPass: password,
          apAutoconnect: true,
          packVariant: trackerVariant,
          trackerVariant,
          forceTrackerVariantPrompt,
        });
        if (trackerRes?.error === "tracker_variant_cancelled") {
          emitSessionEvent({ event: "error", step: "tracker", error: "tracker_variant_cancelled", moduleId: artifactModuleId });
          return { ok: false, error: "tracker_variant_cancelled" };
        }
        if (!trackerRes?.ok) {
          emitSessionEvent({ event: "warning", step: "tracker", error: trackerRes?.error || "tracker_failed", moduleId: artifactModuleId });
        } else if (trackerRes?.pid) {
          trackerPid = trackerRes.pid;
        }
      }
  
      if (artifact.gamePid) {
        emitSessionEvent({ event: "status", status: "Applying window layout...", moduleId: artifactModuleId });
        const layoutRes = await applyLayoutBestEffort({ gamePid: artifact.gamePid, trackerPid });
        if (!layoutRes?.ok) {
          emitSessionEvent({ event: "warning", step: "layout", error: layoutRes?.error || "layout_failed", moduleId: artifactModuleId });
        }
        emitSessionEvent({
          event: "ready",
          moduleId: artifactModuleId,
          downloadedPath,
          emuPid: artifact.gamePid,
          trackerPid,
          installedPath: artifact.installedPath,
        });
        return {
          ok: true,
          handled: true,
          moduleId: artifactModuleId,
          downloadedPath,
          ext,
          gamePid: artifact.gamePid,
          trackerPid,
          installedPath: artifact.installedPath,
        };
      }
  
      emitSessionEvent({
        event: "manual",
        downloadedPath,
        ext,
        apGameName,
        moduleId: artifactModuleId,
        trackerPid,
        installedPath: artifact.installedPath,
        note: artifact.note || "Installed successfully. Launch the game to continue.",
      });
      return {
        ok: true,
        manual: true,
        moduleId: artifactModuleId,
        downloadedPath,
        ext,
        trackerPid,
        installedPath: artifact.installedPath,
      };
    }
  
    // Prefer extension-based routing when available (Archipelago patch files).
    const extResolved = ext ? resolveModuleForDownload(downloadedPath) : { ok: false, error: "unknown_extension" };
    const byName = !extResolved.ok ? findModuleByApGameName(apGameName) : null;
    const resolved = extResolved.ok ? extResolved : byName ? { ok: true, moduleId: byName, gameId: "", ext } : extResolved;
    const canAutoPatch = Boolean(extResolved.ok);
  
    const moduleId = resolved.moduleId;
    if (!resolved.ok || !moduleId) {
      // Unknown file type: keep it simple for now. Download succeeded, user may need manual steps.
      emitSessionEvent({
        event: "status",
        status: "Download complete. Manual action may be required for this game.",
        downloadedPath,
        ext,
        apGameName
      });
      // Still connect CommonClient for chat/commands if possible.
      emitSessionEvent({ event: "status", status: "Connecting to server...", moduleId: "" });
      await startCommonClient({ address: serverAddress, slot, password });
      await sendCommonClientCommand({ cmd: "connect", address: serverAddress, slot, password });
      emitSessionEvent({
        event: "status",
        status: "Manual action required: file downloaded and client connected.",
        downloadedPath,
        ext
      });
      emitSessionEvent({ event: "manual", downloadedPath, ext, apGameName });
      return { ok: true, manual: true, downloadedPath, ext };
    }
  
    const packCheck = await ensureRuntimePacksForModule({
      moduleId,
      gameId: String(resolved.gameId || "").trim(),
    });
    if (!packCheck?.ok) {
      emitSessionEvent({
        event: "error",
        step: "packs",
        error: packCheck?.error || "runtime_pack_sync_failed",
        moduleId,
        packId: packCheck?.packId || "",
      });
      return {
        ok: false,
        error: packCheck?.error || "runtime_pack_sync_failed",
        moduleId,
        packId: packCheck?.packId || "",
        detail: packCheck?.detail || "",
      };
    }
  
    if (!canAutoPatch) {
      // We recognized the game by name, but not the file type. Keep the download and connect/tracker best-effort.
      emitSessionEvent({
        event: "status",
        status: "Download complete. Automatic game preparation is not supported for this file type yet.",
        downloadedPath,
        ext,
        moduleId
      });
      emitSessionEvent({ event: "status", status: "Connecting to server...", moduleId });
      await startCommonClient({ address: serverAddress, slot, password });
      await sendCommonClientCommand({ cmd: "connect", address: serverAddress, slot, password });
      let trackerRes = null;
      const manifest = getModuleManifest(moduleId) || {};
      if (moduleHasExternalTracker(manifest)) {
        emitSessionEvent({ event: "status", status: "Launching tracker...", moduleId });
        trackerRes = await launchPopTracker({
          moduleId,
          apHost: serverAddress,
          apSlot: slot,
          apPass: password,
          apAutoconnect: true,
          packVariant: trackerVariant,
          trackerVariant,
          forceTrackerVariantPrompt,
        });
        if (trackerRes?.error === "tracker_variant_cancelled") {
          emitSessionEvent({ event: "error", step: "tracker", error: "tracker_variant_cancelled", moduleId });
          return { ok: false, error: "tracker_variant_cancelled" };
        }
        if (!trackerRes?.ok) {
          emitSessionEvent({ event: "warning", step: "tracker", error: trackerRes?.error || "tracker_failed", moduleId });
        }
      }
      emitSessionEvent({
        event: "status",
        status: "Manual action required: file downloaded and client connected.",
        downloadedPath,
        ext,
        moduleId
      });
      emitSessionEvent({ event: "manual", downloadedPath, ext, moduleId, apGameName, trackerPid: trackerRes?.pid });
      return { ok: true, manual: true, moduleId, downloadedPath, ext, trackerPid: trackerRes?.pid };
    }
  
    emitSessionEvent({ event: "status", status: "Validating setup...", moduleId });
    const setup = await validateSetupForModule(moduleId);
    if (!setup.ok) {
      emitSessionEvent({
        event: "error",
        step: "validate",
        error: setup.error,
        detail: setup.detail,
        setupArea: setup.setupArea,
        moduleId,
        gameId: setup.gameId,
      });
      return setup;
    }
  
    const configPath = getDefaultPatcherConfigPath();
    let patchOutput = "";
    let patchCacheKey = "";
    let patchHash = "";
    let reusedPatchedRom = false;
  
    try {
      const cached = await patchedRomCache.resolveByPatch({ moduleId, patchPath: downloadedPath });
      patchCacheKey = cached.key || "";
      patchHash = cached.patchHash || "";
      if (cached.ok && cached.outputPath) {
        patchOutput = cached.outputPath;
        reusedPatchedRom = true;
        emitSessionEvent({ event: "status", status: "Reusing prepared game image...", moduleId });
        writeLogLine("info", "autolaunch", `reusing patched rom: moduleId=${moduleId} patchHash=${patchHash} output=${patchOutput}`);
        writeLogJson("patch-decision", {
          moduleId,
          action: "reuse",
          patchPath: downloadedPath,
          patchHash,
          outputPath: patchOutput,
          cacheHit: cached.cacheHit || "exact",
        });
      }
      // Do not reuse by URL only. URL reuse can occur with updated patch content and
      // cause slot/ROM auth mismatches ("Invalid ROM detected"). Hash-based reuse
      // above remains enabled.
    } catch (err) {
      writeLogLine("warn", "autolaunch", `cache lookup failed; patching normally: ${String(err)}`);
    }
  
    if (!patchOutput) {
      emitSessionEvent({ event: "status", status: "Preparing game image...", moduleId });
      writeLogLine("info", "autolaunch", `patching: patchPath=${downloadedPath} moduleId=${moduleId}`);
      writeLogJson("patch-decision", {
        moduleId,
        action: "patch",
        patchPath: downloadedPath,
        patchHash,
        reason: "cache_miss",
      });
      const manifest = setup.manifest || getModuleManifest(moduleId) || {};
      const patchRes = await runPatcher({ patchPath: downloadedPath, patchUrl: downloadUrl, configPath, moduleId, manifest });
      writeLogLine("info", "autolaunch", `patch result: ok=${patchRes?.ok} output=${patchRes?.output || ""} error=${patchRes?.error || ""}`);
      if (!patchRes?.ok || !patchRes.output) {
        emitSessionEvent({ event: "error", step: "patch", error: patchRes?.error || "patch_failed", moduleId });
        return { ok: false, error: patchRes?.error || "patch_failed" };
      }
      patchOutput = patchRes.output;
      patchedRomCache.remember({
        key: patchCacheKey,
        moduleId,
        patchPath: downloadedPath,
        patchHash,
        outputPath: patchOutput,
        downloadUrl,
      });
      writeLogJson("patch-result", {
        moduleId,
        ok: true,
        action: "patch",
        patchPath: downloadedPath,
        patchHash,
        outputPath: patchOutput,
        remembered: true,
      });
    }
  
    const manifest = setup.manifest || getModuleManifest(moduleId) || {};
    await stopArchipelagoClientsForSession(moduleId, slot, "pre_autolaunch").catch((err) => {
      writeLogLine("warn", "archipelagoclient", `pre-launch session cleanup failed: ${String(err?.message || err || "")}`);
    });
    const runtimeRes = await launchGameRuntimeForModule({
      moduleId,
      manifest,
      romPath: patchOutput,
      reusedPatchedRom,
      serverAddress,
      slot,
      playerAlias,
      password,
      chatBridge,
      apiBaseUrl,
      authToken,
      deviceId,
    });
    if (!runtimeRes?.ok) {
      return { ok: false, error: runtimeRes?.error || "emu_failed", detail: runtimeRes?.detail || "" };
    }
  
  	    await connectRuntimeBridgeForModule({
  	      moduleId,
  	      manifest,
  	      serverAddress,
  	      slot,
  	      playerAlias: options.playerAlias,
  	      password,
  	      patchPath: downloadedPath,
  	      romPath: patchOutput,
  	      memorySocketPath: runtimeRes.memorySocketPath,
  	      chatBridge: runtimeRes.chatBridge,
  	    });
  
    const trackerRes = await launchTrackerForModuleSession({
      moduleId,
      manifest,
      serverAddress,
      slot,
      password,
      trackerVariant,
      forceTrackerVariantPrompt,
    });
    if (!trackerRes?.ok && trackerRes?.error === "tracker_variant_cancelled") {
      return { ok: false, error: "tracker_variant_cancelled" };
    }
  
    await applyRuntimeLayoutForSession({
      moduleId,
      gamePid: runtimeRes.pid,
      trackerPid: trackerRes?.pid || null,
    });

    if (runtimeRes.pid && !isPidAlive(runtimeRes.pid)) {
      const detail = `Game runtime exited before launch completed (pid=${runtimeRes.pid}).`;
      emitSessionEvent({ event: "error", step: "emu", error: "emu_exited_before_ready", detail, moduleId });
      return { ok: false, error: "emu_exited_before_ready", detail, moduleId };
    }
  
    emitSessionEvent({
      event: "ready",
      moduleId,
      patchOutput,
      patchReused: reusedPatchedRom,
      emuPid: runtimeRes.pid,
      trackerPid: trackerRes?.pid,
      downloadedPath,
    });
  
    const result = {
      ok: true,
      moduleId,
      patchOutput,
      patchReused: reusedPatchedRom,
      emuPid: runtimeRes.pid,
      trackerPid: trackerRes?.pid,
      downloadedPath,
    };
    if (!options.skipMultiGameRegister) await rememberMultiGameSession(options, result);
    return result;
  };

  const switchMultiGame = async (entryId, options = {}) => {
    const safeEntryId = String(entryId || "").trim();
    if (!safeEntryId) return { ok: false, error: "missing_entry_id" };
    const entry = multiGameSession.entries.find((candidate) => candidate.id === safeEntryId);
    if (!entry) return { ok: false, error: "multi_game_entry_not_found" };
    if (!multiGameSession.base.serverAddress) return { ok: false, error: "multi_game_session_not_ready" };

    const saveStateDecision = String(
      options.saveStateDecision || options.save_state_decision || options.save_state || ""
    ).trim().toLowerCase();
    if (!["yes", "no", "cancel"].includes(saveStateDecision)) {
      return {
        ok: false,
        error: "save_state_decision_required",
        entryId: entry.id,
        moduleId: entry.moduleId,
        prompt: {
          ...multiGameSaveStatePrompt,
          entryId: entry.id,
          moduleId: entry.moduleId,
          apGameName: entry.apGameName,
          slot: entry.slot || multiGameSession.current.slot,
        },
      };
    }

    if (saveStateDecision === "cancel") {
      emitSessionEvent({
        event: "multi-game-switch-cancelled",
        entryId: entry.id,
        moduleId: entry.moduleId,
        apGameName: entry.apGameName,
      });
      writeLogLine("info", "multigame", `switch cancelled from save-state prompt: entry=${entry.id}`);
      return { ok: true, cancelled: true, entryId: entry.id };
    }

    if (saveStateDecision === "yes") {
      emitSessionEvent({
        event: "multi-game-save-state-start",
        entryId: entry.id,
        moduleId: multiGameSession.current.moduleId,
        emuPid: multiGameSession.current.emuPid,
      });
      const saveRes = await requestSaveState({
        reason: "multi_game_switch",
        entryId: multiGameSession.activeEntryId,
        nextEntryId: entry.id,
        moduleId: multiGameSession.current.moduleId,
        pid: multiGameSession.current.emuPid,
      });
      if (!saveRes?.ok) {
        emitSessionEvent({
          event: "multi-game-save-state-failed",
          entryId: entry.id,
          moduleId: multiGameSession.current.moduleId,
          error: saveRes?.error || "save_state_failed",
          detail: saveRes?.detail || "",
        });
        return {
          ok: false,
          error: saveRes?.error || "save_state_failed",
          detail: saveRes?.detail || "",
          entryId: entry.id,
        };
      }
      emitSessionEvent({
        event: "multi-game-save-state-ready",
        entryId: entry.id,
        moduleId: multiGameSession.current.moduleId,
        path: saveRes.path || "",
      });
    }

    emitSessionEvent({
      event: "multi-game-switch-start",
      entryId: entry.id,
      moduleId: entry.moduleId,
      apGameName: entry.apGameName,
      slot: entry.slot || multiGameSession.current.slot,
    });
    writeLogLine("info", "multigame", `switch requested: entry=${entry.id} module=${entry.moduleId} game=${entry.apGameName}`);

    await stopCurrentMultiGameProcesses();

    const launchRes = await autoLaunchFromPatchUrl({
      downloadUrl: entry.downloadUrl,
      serverAddress: multiGameSession.base.serverAddress,
      slot: entry.slot || multiGameSession.current.slot,
      playerAlias: entry.playerAlias,
      password: multiGameSession.base.password,
      apGameName: entry.apGameName,
      trackerVariant: String(options.trackerVariant || entry.trackerVariant || "").trim(),
      packVariant: String(options.trackerVariant || entry.trackerVariant || "").trim(),
      forceTrackerVariantPrompt:
        options.forceTrackerVariantPrompt === true || multiGameSession.base.forceTrackerVariantPrompt,
      chatBridge: multiGameSession.base.chatBridge,
      apiBaseUrl: multiGameSession.base.apiBaseUrl,
      authToken: multiGameSession.base.authToken,
      deviceId: multiGameSession.base.deviceId,
      multiGameEntries: multiGameSession.entries,
      skipMultiGameRegister: true,
    });

    if (!launchRes?.ok) {
      emitSessionEvent({
        event: "multi-game-switch-failed",
        entryId: entry.id,
        moduleId: entry.moduleId,
        error: launchRes?.error || "multi_game_switch_failed",
        detail: launchRes?.detail || "",
      });
      return launchRes;
    }

    multiGameSession.activeEntryId = entry.id;
    multiGameSession.current = {
      moduleId: String(launchRes.moduleId || entry.moduleId || "").trim(),
      emuPid: Number(launchRes.emuPid || launchRes.gamePid || 0) || 0,
      trackerPid: Number(launchRes.trackerPid || 0) || 0,
      slot: entry.slot || multiGameSession.current.slot,
    };
    emitSessionEvent({
      event: "multi-game-switch-ready",
      entryId: entry.id,
      moduleId: multiGameSession.current.moduleId,
      emuPid: multiGameSession.current.emuPid,
      trackerPid: multiGameSession.current.trackerPid,
    });
    return { ...launchRes, multiGameEntryId: entry.id };
  };

  return {
    planSessionAutoLaunch,
    autoLaunchFromPatchUrl,
    listMultiGameSession,
    switchMultiGame,
  };
};

module.exports = { createAutoLaunchRuntime };
