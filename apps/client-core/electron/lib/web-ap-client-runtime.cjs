"use strict";

const { hardenBrowserWindow } = require("./browser-window-hardening.cjs");

const DEFAULT_WEB_AP_CLIENT_URL = "http://evermizer.com/apclient/";

const createWebApClientRuntime = (deps = {}) => {
  const {
    BrowserWindow,
    crypto,
    writeLogLine = () => {},
    writeLogJson = () => {},
    nowIso = () => new Date().toISOString(),
    normalizeIpcString = (value, max = 500) => String(value || "").slice(0, max),
    appendSekaiemuSystemChat = () => {},
    notifyRuntimeItem = () => {},
    notifyRuntimeActivity = () => {},
  } = deps;

  const clients = new Map();

  const redact = (value) =>
    String(value || "")
      .replace(/\b(password|pass|token|secret|auth)\s*=\s*([^\s,;]+)/gi, "$1=[REDACTED]")
      .replace(/(\/connect\s+\S+\s+)(\S+)/gi, "$1[REDACTED]");

  const normalizeApWebSocketAddress = (value) => {
    const clean = normalizeIpcString(value || "", 240).trim();
    if (!clean) return "";
    if (/^wss?:\/\//i.test(clean)) return clean;
    return `ws://${clean}`;
  };

  const buildClientUrl = (baseUrl, serverAddress, options = {}) => {
    const rawBase = normalizeIpcString(baseUrl || DEFAULT_WEB_AP_CLIENT_URL, 800) || DEFAULT_WEB_AP_CLIENT_URL;
    const url = new URL(rawBase);
    const server = normalizeIpcString(serverAddress || "", 240);
    if (server && options.hashAutoConnect === true) url.hash = `server=${server}`;
    return url.toString();
  };

  const normalizeSlotText = (value) => String(value || "").trim();

  const aliasForParticipant = (session, ...values) => {
    const aliases = session?.playerAliasMap && typeof session.playerAliasMap === "object" ? session.playerAliasMap : {};
    for (const value of values) {
      const clean = normalizeSlotText(value);
      if (!clean) continue;
      const alias = normalizeSlotText(aliases[clean]);
      if (alias) return alias;
    }
    return "";
  };

  const humanNameForSlot = (session, slotName) => {
    const clean = normalizeSlotText(slotName);
    if (!clean) return "";
    return aliasForParticipant(session, clean) || clean;
  };

  const gameNameForSlot = (session, slotName) => {
    const clean = normalizeSlotText(slotName);
    if (!clean) return "";
    return normalizeSlotText(session?.gameNameBySlot?.[clean]);
  };

  const isOwnedRecipientSlot = (session, slotName) => {
    const clean = normalizeSlotText(slotName);
    if (!clean) return false;
    if (clean === normalizeSlotText(session?.slot)) return true;
    return Boolean(session?.gameNameBySlot && Object.prototype.hasOwnProperty.call(session.gameNameBySlot, clean));
  };

  const rememberActivity = (session, signature) => {
    if (!session || !signature) return false;
    if (session.activitySeen.has(signature)) return false;
    session.activitySeen.add(signature);
    session.activitySeenOrder.push(signature);
    while (session.activitySeenOrder.length > 256) {
      session.activitySeen.delete(session.activitySeenOrder.shift());
    }
    return true;
  };

  const mirrorActivityToSekaiemu = (session, kind, text, signature) => {
    // Runtime-social UI now belongs to Client Core. Keep web AP client activity
    // out of Sekaiemu so emulator frame pacing is not affected by notification bursts.
    const isNew = rememberActivity(session, signature);
    if (isNew) {
      notifyRuntimeActivity({
        moduleId: session.moduleId,
        kind,
        title: kind === "item" ? "Runtime item" : "Runtime activity",
        detail: text,
        dedupeKey: `webap:${session.clientId}:${signature}`,
      });
    }
    return isNew;
  };

  const parseAndMirrorActivityLine = (session, line) => {
    const text = normalizeSlotText(line);
    if (!text) return;

    const sent = text.match(/^(.+?)\s+sent\s+(.+?)\s+to\s+(.+?)(?:\s+\((.+)\))?\.?$/i);
    if (sent) {
      const senderSlot = normalizeSlotText(sent[1]);
      const item = normalizeSlotText(sent[2]);
      const recipientSlot = normalizeSlotText(sent[3]);
      const location = normalizeSlotText(sent[4]);
      if (!senderSlot || !item || !recipientSlot) return;
      const sender = humanNameForSlot(session, senderSlot);
      const recipient = humanNameForSlot(session, recipientSlot);
      const recipientGame = gameNameForSlot(session, recipientSlot) || "their game";
      let activityText = `${sender} sent ${item} to ${recipient}'s ${recipientGame}`;
      if (location) activityText += ` (${location})`;
      const signature = `sent:${senderSlot}:${recipientSlot}:${item}:${location}`;
      const ownedRecipient = isOwnedRecipientSlot(session, recipientSlot);
      const ownedSender = isOwnedRecipientSlot(session, senderSlot);
      mirrorActivityToSekaiemu(session, "item", activityText, signature);
      if (!ownedRecipient) {
        if (ownedSender) {
          notifyRuntimeItem({
            moduleId: session.moduleId,
            direction: "outgoing",
            senderName: sender,
            senderSlot,
            recipientName: recipient,
            recipientSlot,
            recipientGame,
            itemName: item,
            location,
            lobbyTitle: session.gameKey,
            allowAnySlot: true,
            dedupeKey: `webap:${session.clientId}:out:${senderSlot}:${recipientSlot}:${item}:${location}`,
          });
        }
        return;
      }
      notifyRuntimeItem({
        moduleId: session.moduleId,
        direction: ownedSender ? "outgoing" : "incoming",
        senderName: sender,
        senderSlot,
        recipientName: recipient,
        recipientSlot,
        recipientGame,
        itemName: item,
        location,
        lobbyTitle: session.gameKey,
        allowAnySlot: true,
        dedupeKey: `webap:${session.clientId}:sent:${senderSlot}:${recipientSlot}:${item}:${location}`,
      });
      return;
    }

    const found = text.match(/^(.+?)\s+found\s+their\s+(.+?)(?:\s+(?:in|at)\s+(.+?))?\.?$/i);
    if (found) {
      const playerSlot = normalizeSlotText(found[1]);
      const item = normalizeSlotText(found[2]);
      const location = normalizeSlotText(found[3]);
      if (!playerSlot || !item) return;
      const player = humanNameForSlot(session, playerSlot);
      let activityText = `${player} found their ${item}`;
      if (location) activityText += ` in ${location}`;
      mirrorActivityToSekaiemu(session, "item", activityText, `found:${playerSlot}:${item}:${location}`);
    }
  };

  const writeClientLog = (level, clientId, message, extra = {}) => {
    const msg = redact(message).trim();
    if (!msg) return;
    writeLogLine(level, "web-ap-client", `[${clientId}] ${msg}`);
    writeLogJson("web-ap-client", {
      at: nowIso(),
      clientId,
      level,
      message: msg,
      ...extra,
    });
    const session = clients.get(clientId);
    if (session) parseAndMirrorActivityLine(session, msg);
  };

  const mirrorToSekaiemuChat = (session, text, kind = "web-ap-client") => {
    // Web AP clients are runtime plumbing, not player chat. Keep their output
    // in logs/debug views so the in-game Sekaiemu chat stays human-readable.
    void session;
    void text;
    void kind;
  };

  const runCommand = async (clientId, command) => {
    const session = clients.get(clientId);
    if (!session?.window || session.window.isDestroyed()) return { ok: false, error: "not_running" };
    const cmd = normalizeIpcString(command || "", 1000).trim();
    if (!cmd) return { ok: false, error: "empty_command" };
    try {
      await session.window.webContents.executeJavaScript(
        `if (window.Module && typeof window.Module.on_command === "function") { window.Module.on_command(${JSON.stringify(cmd)}); true; } else { false; }`,
        true
      );
      writeClientLog("info", clientId, `command: ${cmd}`);
      mirrorToSekaiemuChat(session, `command: ${cmd}`, "web-ap-client-command");
      return { ok: true };
    } catch (err) {
      return { ok: false, error: String(err?.message || err || "command_failed") };
    }
  };

  const pollOutput = async (clientId) => {
    const session = clients.get(clientId);
    if (!session?.window || session.window.isDestroyed()) return;
    try {
      const text = await session.window.webContents.executeJavaScript(
        `(() => {
          const output = document.getElementById("output");
          return output ? output.innerText || "" : "";
        })()`,
        true
      );
      const normalized = String(text || "").replace(/\r\n/g, "\n").trim();
      if (!normalized || normalized === session.lastOutput) return;
      const previous = session.lastOutput || "";
      session.lastOutput = normalized;
      const delta = previous && normalized.startsWith(previous)
        ? normalized.slice(previous.length).trim()
        : normalized;
      for (const line of delta.split(/\n+/).map((entry) => entry.trim()).filter(Boolean).slice(-40)) {
        writeClientLog("info", clientId, line, { source: "dom-output" });
        mirrorToSekaiemuChat(session, line);
      }
    } catch (err) {
      writeClientLog("warn", clientId, `output poll failed: ${String(err?.message || err || "")}`);
    }
  };

  const startWebApClient = async (options = {}) => {
    if (!BrowserWindow) return { ok: false, error: "browser_window_unavailable" };
    const gameKey = normalizeIpcString(options.gameKey || options.game_key || options.moduleId || "web-ap-client", 120);
    const moduleId = normalizeIpcString(options.moduleId || gameKey, 120);
    const slot = normalizeIpcString(options.slot || "", 160);
    const serverAddress = normalizeIpcString(options.address || options.serverAddress || "", 240);
    const connectAddress = normalizeApWebSocketAddress(serverAddress);
    const clientUrl = buildClientUrl(options.url || options.clientUrl, connectAddress, {
      hashAutoConnect: options.hashAutoConnect === true || options.hash_autoconnect === true,
    });
    const clientId = `${gameKey || "web"}-${Date.now()}-${crypto.randomBytes(3).toString("hex")}`;
    const showWindow = options.show === true;
    const chatBridge = options.chatBridge && typeof options.chatBridge === "object" ? options.chatBridge : null;
    const playerAliasMap = options.playerAliasMap && typeof options.playerAliasMap === "object" ? options.playerAliasMap : {};
    const gameNameBySlot = {};
    const entries = Array.isArray(options.multiGameEntries) ? options.multiGameEntries : [];
    for (const entry of entries) {
      const entrySlot = normalizeSlotText(entry?.slot);
      const entryGame = normalizeSlotText(entry?.apGameName || entry?.label || entry?.moduleId);
      if (entrySlot && entryGame) gameNameBySlot[entrySlot] = entryGame;
    }

    const win = new BrowserWindow({
      width: 960,
      height: 720,
      show: showWindow,
      title: `SekaiLink Web AP Client - ${gameKey}`,
      autoHideMenuBar: true,
      backgroundColor: "#101419",
      webPreferences: {
        nodeIntegration: false,
        contextIsolation: true,
        sandbox: true,
        backgroundThrottling: false,
        devTools: false,
      },
    });
    hardenBrowserWindow(win);

    const session = {
      clientId,
      gameKey,
      moduleId,
      slot,
      serverAddress,
      connectAddress,
      url: clientUrl,
      window: win,
      chatBridge,
      playerAliasMap,
      gameNameBySlot,
      activitySeen: new Set(),
      activitySeenOrder: [],
      startedAt: nowIso(),
      lastOutput: "",
      pollTimer: null,
    };
    clients.set(clientId, session);

    win.webContents.on("console-message", (_event, level, message, line, sourceId) => {
      const lvl = level >= 2 ? "warn" : "info";
      writeClientLog(lvl, clientId, message, { source: "console", line, sourceId });
      mirrorToSekaiemuChat(session, message);
    });
    win.webContents.on("did-fail-load", (_event, errorCode, errorDescription, validatedUrl) => {
      writeClientLog("warn", clientId, `load failed ${errorCode}: ${errorDescription} (${validatedUrl})`);
      mirrorToSekaiemuChat(session, `load failed: ${errorDescription}`, "web-ap-client-error");
    });
    win.webContents.on("did-finish-load", async () => {
      writeClientLog("info", clientId, `loaded ${clientUrl}`);
      mirrorToSekaiemuChat(session, `loaded web AP client for ${gameKey}`);
      if (connectAddress) {
        setTimeout(() => {
          runCommand(clientId, `/connect ${connectAddress}`).catch((err) => {
            writeClientLog("warn", clientId, `auto-connect command failed: ${String(err?.message || err || "")}`);
          });
        }, 900);
      }
    });
    win.on("closed", () => {
      if (session.pollTimer) clearInterval(session.pollTimer);
      clients.delete(clientId);
      writeClientLog("info", clientId, "closed");
    });

    writeClientLog("info", clientId, `starting url=${clientUrl} show=${showWindow ? "true" : "false"}`);
    mirrorToSekaiemuChat(session, `starting web AP client for ${gameKey}`);
    await win.loadURL(clientUrl);
    session.pollTimer = setInterval(() => {
      pollOutput(clientId).catch(() => {});
    }, 1200);
    return { ok: true, clientId, url: clientUrl, mode: "web-ap-client" };
  };

  const stopWebApClient = async (clientId) => {
    const id = normalizeIpcString(clientId || "", 160);
    const session = clients.get(id);
    if (!session) return { ok: false, error: "not_running" };
    if (session.pollTimer) clearInterval(session.pollTimer);
    if (session.window && !session.window.isDestroyed()) session.window.close();
    clients.delete(id);
    return { ok: true };
  };

  const stopAllWebApClients = async () => {
    for (const clientId of Array.from(clients.keys())) {
      await stopWebApClient(clientId).catch((_err) => {});
    }
    return { ok: true };
  };

  const getWebApClientStats = () => ({
    processes: clients.size,
    clients: Array.from(clients.values()).map((session) => ({
      clientId: session.clientId,
      gameKey: session.gameKey,
      moduleId: session.moduleId,
      serverAddress: session.serverAddress,
      connectAddress: session.connectAddress,
      url: session.url,
      startedAt: session.startedAt,
    })),
  });

  return {
    startWebApClient,
    stopWebApClient,
    stopAllWebApClients,
    sendWebApClientCommand: runCommand,
    getWebApClientStats,
  };
};

module.exports = { createWebApClientRuntime, DEFAULT_WEB_AP_CLIENT_URL };
