"use strict";

const { hardenBrowserWindow } = require("./browser-window-hardening.cjs");

const createSekaiemuRuntimeSocial = (deps = {}) => {
  const {
    app,
    BrowserWindow,
    fs,
    path,
    processRef = globalThis.process,
    ensureDir,
    writeLogLine,
    nowIso,
    isDev,
    devServerUrl,
    dirname,
    secureIpcHandle,
    readConfig,
    emitSessionEvent = () => {},
  } = deps;
  const process = processRef;
  const sessions = new Map();
  const windows = new Map();
  const eventOffsets = new Map();
  const traceOffsets = new Map();
  const activityHistoryByKey = new Map();
  const itemNotificationsSeen = new Set();
  const itemNotificationsSeenOrder = [];
  const toastLifetimeMs = 4200;
  const maxActivityEntries = 1000;
  let lastTracePollAt = 0;

  const normalizeApiBase = (value) => {
    const raw = String(value || process.env.SEKAILINK_API_BASE_URL || process.env.VITE_API_BASE_URL || "https://sekailink.com").trim();
    if (!raw || raw === "same") return "https://sekailink.com";
    return raw.replace(/\/+$/, "");
  };

  const writeJsonAtomic = (filePath, payload) => {
    if (!filePath) return;
    try {
      ensureDir(path.dirname(filePath));
      const temp = `${filePath}.tmp`;
      fs.writeFileSync(temp, JSON.stringify(payload, null, 2), "utf8");
      fs.renameSync(temp, filePath);
    } catch (err) {
      writeLogLine("warn", "runtime-social", `hud-state write failed: ${String(err?.message || err || "")}`);
    }
  };

  const sessionTitle = (session) =>
    session?.lobbyTitle || session?.lobbyId || [session?.moduleId, session?.game].filter(Boolean).join(" - ") || "Runtime";

  const normalizeText = (value) => String(value || "").trim();

  const isNumericText = (value) => /^\d+$/.test(normalizeText(value));

  const normalizeItemDisplayName = (value) => {
    const clean = normalizeText(value);
    const knownShortNames = {
      book: "Book of Mudora",
      fire: "Fire Rod",
      ice: "Ice Rod",
      small: "Small Key",
    };
    const mapped = knownShortNames[clean.toLowerCase()];
    if (mapped) return mapped;
    if (/^small$/i.test(clean)) return "Small Key";
    if (clean && clean === clean.toLowerCase() && /[a-z]/.test(clean)) {
      return clean.replace(/\b([a-z])([a-z0-9']*)/g, (_match, first, rest) => `${first.toUpperCase()}${rest}`);
    }
    return clean;
  };

  const isGenericItemDisplayName = (value) => /^(small|small key|key|big key|map|compass)$/i.test(normalizeText(value));

  const displayItemName = (...values) => {
    const candidates = values
      .map((value) => normalizeItemDisplayName(value))
      .filter((clean) => clean && clean !== "-" && !isNumericText(clean));
    const specific = candidates.find((clean) => !isGenericItemDisplayName(clean));
    if (specific) return specific;
    if (candidates.length > 0) return candidates[0];
    for (const value of values) {
      const clean = normalizeItemDisplayName(value);
      if (clean && clean !== "-") return clean;
    }
    return "Item";
  };

  const firstAlias = (session) => {
    const local = normalizeText(session?.playerAlias);
    if (local) return local;
    const aliases = session?.playerAliasMap && typeof session.playerAliasMap === "object" ? Object.values(session.playerAliasMap) : [];
    return aliases.map(normalizeText).find(Boolean) || "";
  };

  const humanSlotName = (session, ...values) => {
    const aliases = session?.playerAliasMap && typeof session.playerAliasMap === "object" ? session.playerAliasMap : {};
    for (const value of values) {
      const clean = normalizeText(value);
      if (!clean) continue;
      const alias = normalizeText(aliases[clean]);
      if (alias) return alias;
    }
    return values
      .map(normalizeText)
      .find((value) => value && !isNumericText(value) && !/^Player\s+\d+$/i.test(value)) ||
      values.map(normalizeText).find(Boolean) ||
      "";
  };

  const humanSenderName = (session, item = {}) => {
    const explicit = normalizeText(item.senderName || item.sender || item.playerName || item.playerDisplayName || item.apPlayerName);
    const senderSlot = normalizeText(item.senderSlot || item.senderSlotName || item.slotName);
    const slotName = humanSlotName(session, senderSlot, explicit);
    if (slotName) return slotName;
    if (explicit && !/^Player\s+\d+$/i.test(explicit)) return explicit;
    return firstAlias(session) || explicit || "";
  };

  const itemActivityDetail = (session, item = {}, itemName = "Item", senderName = "") => {
    const direction = normalizeText(item.direction).toLowerCase();
    const recipientSlot = normalizeText(item.recipientSlot || item.slot);
    const recipientDisplayName = normalizeText(item.recipientName);
    const recipientName = humanSlotName(session, recipientSlot, recipientDisplayName) || recipientDisplayName;
    const recipientGame = normalizeText(item.recipientGame || item.game);
    const localSlot = normalizeText(session?.slot);
    const isLocalRecipient = !recipientSlot || (localSlot && recipientSlot === localSlot);
    if (direction === "outgoing") {
      const target = recipientName ? `${recipientName}${recipientGame ? `'s ${recipientGame}` : ""}` : (recipientGame || recipientSlot || "another player");
      return `You sent ${itemName} to ${target}`;
    }
    if (senderName) {
      return isLocalRecipient
        ? `${senderName} sent you ${itemName}`
        : `${senderName} sent ${itemName} to ${recipientName || recipientSlot}`;
    }
    return `You received ${itemName}`;
  };

  const rememberItemNotification = (key) => {
    const clean = normalizeText(key);
    if (!clean) return false;
    if (itemNotificationsSeen.has(clean)) return false;
    itemNotificationsSeen.add(clean);
    itemNotificationsSeenOrder.push(clean);
    while (itemNotificationsSeenOrder.length > 512) {
      itemNotificationsSeen.delete(itemNotificationsSeenOrder.shift());
    }
    return true;
  };

  const parseDetailFields = (detail) => {
    const fields = {};
    const text = String(detail || "");
    const matches = Array.from(text.matchAll(/(?:^|\s)([A-Za-z0-9_]+)=/g));
    for (let index = 0; index < matches.length; index += 1) {
      const match = matches[index];
      const key = String(match?.[1] || "");
      if (!key) continue;
      const valueStart = Number(match.index || 0) + String(match[0] || "").length;
      const valueEnd = index + 1 < matches.length ? Number(matches[index + 1].index || text.length) : text.length;
      let raw = text.slice(valueStart, valueEnd).trim().replace(/[,;]+$/g, "").trim();
      if (raw.startsWith("\"") && raw.endsWith("\"")) {
        raw = raw.slice(1, -1).replace(/\\"/g, "\"");
      }
      fields[key] = raw;
    }
    return fields;
  };

  const readHudButtonsVisible = () => {
    try {
      const config = readConfig?.() || {};
      const frontend = config?.games?.sekaiemu?.frontend || {};
      if (typeof frontend.client_core_hud_buttons_visible === "boolean") return frontend.client_core_hud_buttons_visible;
      if (typeof frontend.hud_buttons_visible === "boolean") return frontend.hud_buttons_visible;
    } catch (_err) {
      // Keep default.
    }
    return true;
  };

  const toastCreatedMs = (toast) => {
    const explicit = Number(toast?.createdAtMs);
    if (Number.isFinite(explicit) && explicit > 0) return explicit;
    const seconds = Number(toast?.createdAt);
    if (Number.isFinite(seconds) && seconds > 0) return seconds * 1000;
    return Date.now();
  };

  const pruneToasts = (session) => {
    if (!session) return [];
    const now = Date.now();
    session.toasts = (Array.isArray(session.toasts) ? session.toasts : [])
      .filter((toast) => now - toastCreatedMs(toast) <= toastLifetimeMs)
      .slice(-4);
    return session.toasts;
  };

  const publishState = (session) => {
    if (!session?.hudStatePath) return;
    const liveToasts = pruneToasts(session).map((toast) => {
      const createdAtMs = toastCreatedMs(toast);
      return {
        id: normalizeText(toast.id) || `toast:${createdAtMs}`,
        text: String(toast.text || "").slice(0, 160),
        createdAt: createdAtMs / 1000,
        createdAtMs,
      };
    });
    writeJsonAtomic(session.hudStatePath, {
      chatUnread: Math.max(0, Number(session.chatUnread || 0) || 0),
      activityUnread: Math.max(0, Number(session.activityUnread || 0) || 0),
      buttonsVisible: readHudButtonsVisible(),
      toasts: liveToasts,
      updatedAt: nowIso(),
    });
  };

  const activeSession = () => {
    const live = Array.from(sessions.values()).filter((session) => session && !session.layoutPreview);
    live.sort((a, b) => String(b.launchedAt || "").localeCompare(String(a.launchedAt || "")));
    return live[0] || null;
  };

  const activityHistoryKey = (session = {}) => [
    normalizeText(session.lobbyId) || normalizeText(session.roomId) || normalizeText(session.moduleId) || "runtime",
    normalizeText(session.slot) || "slot",
    normalizeText(session.game) || normalizeText(session.moduleId) || "game",
  ].join(":");

  const rememberActivityHistory = (session) => {
    const key = activityHistoryKey(session);
    if (!key) return;
    activityHistoryByKey.set(key, (Array.isArray(session.activity) ? session.activity : []).slice(0, maxActivityEntries));
    while (activityHistoryByKey.size > 24) {
      const first = activityHistoryByKey.keys().next().value;
      if (!first) break;
      activityHistoryByKey.delete(first);
    }
  };

  const closeAllSurfaces = () => {
    for (const [surface, win] of Array.from(windows.entries())) {
      try {
        if (win && !win.isDestroyed()) win.close();
      } catch (err) {
        writeLogLine("warn", "runtime-social", `window close failed surface=${surface}: ${String(err?.message || err || "")}`);
      }
      windows.delete(surface);
    }
  };

  const rememberSession = (pid, rawSession = {}) => {
    const safePid = Number(pid);
    if (!Number.isFinite(safePid) || safePid <= 0 || rawSession.layoutPreview) return;
    const session = {
      ...rawSession,
      pid: safePid,
      chatUnread: 0,
      activityUnread: 0,
      activity: activityHistoryByKey.get(activityHistoryKey(rawSession)) || [],
      toasts: [],
      apiBaseUrl: normalizeApiBase(rawSession.apiBaseUrl),
      authToken: String(rawSession.authToken || "").trim(),
      deviceId: String(rawSession.deviceId || "").trim(),
    };
    sessions.set(safePid, session);
    publishState(session);
  };

  const forgetSession = (pid) => {
    const safePid = Number(pid);
    if (!Number.isFinite(safePid) || safePid <= 0) return;
    sessions.delete(safePid);
    eventOffsets.delete(safePid);
    traceOffsets.delete(safePid);
    if (sessions.size === 0) closeAllSurfaces();
  };

  const appendToastForSession = (pid, text, key = "") => {
    const session = sessions.get(Number(pid));
    if (!session) return false;
    const clean = String(text || "").trim();
    if (!clean) return false;
    const id = normalizeText(key) || `toast:${Date.now()}:${Math.random().toString(16).slice(2)}`;
    pruneToasts(session);
    const nowMs = Date.now();
    const existing = (session.toasts || []).find((toast) => toast.id === id);
    if (existing) {
      existing.text = clean.slice(0, 160);
      existing.createdAt = nowMs / 1000;
      existing.createdAtMs = nowMs;
      publishState(session);
      return true;
    }
    session.toasts.push({
      id,
      text: clean.slice(0, 160),
      createdAt: nowMs / 1000,
      createdAtMs: nowMs,
    });
    session.toasts = session.toasts.slice(-4);
    publishState(session);
    return true;
  };

  const appendActivityForSession = (session, entry = {}) => {
    if (!session) return false;
    const title = normalizeText(entry.title || entry.kind || "Activity") || "Activity";
    const detail = normalizeText(entry.detail || entry.text || entry.message);
    if (!detail && !title) return false;
    const id = normalizeText(entry.id) || `activity:${Date.now()}:${Math.random().toString(16).slice(2)}`;
    session.activity = Array.isArray(session.activity) ? session.activity : [];
    session.activity.unshift({
      id,
      kind: normalizeText(entry.kind) || "activity",
      title,
      detail,
      at: normalizeText(entry.at) || nowIso(),
    });
    session.activity = session.activity.slice(0, maxActivityEntries);
    rememberActivityHistory(session);
    session.activityUnread = Math.max(0, Number(session.activityUnread || 0) || 0) + 1;
    publishState(session);
    return true;
  };

  const readNewLines = (filePath, offsets, key, maxBytes = 1024 * 1024) => {
    if (!filePath) return [];
    try {
      if (!fs.existsSync(filePath)) return [];
      const stat = fs.statSync(filePath);
      const current = offsets.get(key) || 0;
      const offset = stat.size < current ? 0 : current;
      if (stat.size === offset) return [];
      const fd = fs.openSync(filePath, "r");
      try {
        const buffer = Buffer.alloc(Math.min(stat.size - offset, maxBytes));
        const bytes = fs.readSync(fd, buffer, 0, buffer.length, offset);
        offsets.set(key, offset + bytes);
        return buffer.toString("utf8", 0, bytes).split(/\r?\n/).map((line) => line.trim()).filter(Boolean);
      } finally {
        fs.closeSync(fd);
      }
    } catch (err) {
      writeLogLine("warn", "runtime-social", `jsonl read failed: ${String(err?.message || err || "")}`);
      return [];
    }
  };

  const readNewEventLines = (session) => readNewLines(String(session?.hudEventsPath || ""), eventOffsets, session.pid);

  const readNewTraceLines = (session) => readNewLines(String(session?.traceLogPath || ""), traceOffsets, session.pid, 256 * 1024);

  const loadRuntimeWindow = (win, surface) => {
    const hash = `#/runtime-social/${encodeURIComponent(surface)}`;
    if (isDev) {
      win.loadURL(`${devServerUrl}${hash}`);
    } else {
      win.loadFile(path.join(dirname, "../dist/index.html"), { hash });
    }
  };

  const openSurface = (surface) => {
    const session = activeSession();
    if (!session) return { ok: false, error: "no_active_runtime_session" };
    if (surface === "chat") session.chatUnread = 0;
    if (surface === "activity") session.activityUnread = 0;
    publishState(session);

    const existing = windows.get(surface);
    if (existing && !existing.isDestroyed()) {
      existing.show();
      existing.focus();
      return { ok: true, focused: true };
    }
    const win = new BrowserWindow({
      width: surface === "hint" ? 520 : 460,
      height: surface === "activity" ? 640 : 560,
      minWidth: 380,
      minHeight: 420,
      title: `Runtime ${surface[0].toUpperCase()}${surface.slice(1)}`,
      frame: false,
      titleBarStyle: "hidden",
      autoHideMenuBar: true,
      backgroundColor: "#05070A",
      show: false,
      webPreferences: {
        preload: path.join(dirname, "preload.cjs"),
        contextIsolation: true,
        nodeIntegration: false,
        sandbox: true,
        devTools: false,
      },
    });
    hardenBrowserWindow(win);
    win.once("ready-to-show", () => {
      win.show();
      win.focus();
    });
    win.on("closed", () => windows.delete(surface));
    windows.set(surface, win);
    loadRuntimeWindow(win, surface);
    return { ok: true, opened: true };
  };

  const sessionMatchesItem = (session, item) => {
    if (!session) return false;
    const itemLobby = normalizeText(item.lobbyId);
    const itemSlot = normalizeText(item.recipientSlot || item.slot);
    const direction = normalizeText(item.direction).toLowerCase();
    const itemGame = normalizeText(item.game || (direction === "outgoing" ? "" : item.recipientGame));
    const itemModule = normalizeText(item.moduleId);
    if (itemLobby && normalizeText(session.lobbyId) && itemLobby !== normalizeText(session.lobbyId)) return false;
    if (!item.allowAnySlot && itemSlot && normalizeText(session.slot) && itemSlot !== normalizeText(session.slot)) return false;
    if (itemGame && normalizeText(session.game) && itemGame !== normalizeText(session.game)) return false;
    if (itemModule && normalizeText(session.moduleId) && itemModule !== normalizeText(session.moduleId)) return false;
    return true;
  };

  const notifyRuntimeItem = (item = {}) => {
    const recipientSlot = normalizeText(item.recipientSlot || item.slot);
    const targetSession = Array.from(sessions.values()).find((session) => sessionMatchesItem(session, item)) ||
      (item.pid ? sessions.get(Number(item.pid)) : null);
    const itemName = displayItemName(item.itemName, item.item, item.mappedValue, item.eventKey, item.itemId);
    const senderName = humanSenderName(targetSession, item);
    const dedupeKey = normalizeText(item.dedupeKey) || [
      normalizeText(item.lobbyId) || normalizeText(targetSession?.lobbyId),
      normalizeText(item.roomId) || normalizeText(targetSession?.roomId),
      recipientSlot || normalizeText(targetSession?.slot),
      normalizeText(item.receivedIndex || item.deliveryId || item.itemId || item.traceId),
      itemName,
    ].join(":");
    if (!rememberItemNotification(`runtime-item:${dedupeKey}`)) return { ok: true, deduped: true };

    if (targetSession) {
      const direction = normalizeText(item.direction).toLowerCase();
      const recipientDisplayName = normalizeText(item.recipientName);
      const recipientName = humanSlotName(targetSession, recipientSlot, recipientDisplayName) || recipientDisplayName;
      const recipientGame = normalizeText(item.recipientGame || item.game);
      const toastText = direction === "outgoing"
        ? `You -> ${recipientName || recipientSlot || "Player"}: ${itemName}${recipientGame ? ` (${recipientGame})` : ""}`
        : (senderName ? `${senderName} -> ${itemName}` : itemName);
      appendActivityForSession(targetSession, {
        id: `item:${dedupeKey}`,
        kind: "item",
        title: direction === "outgoing" ? "Item sent" : "Item received",
        detail: itemActivityDetail(targetSession, item, itemName, senderName),
      });
      appendToastForSession(targetSession.pid, toastText, `item:${dedupeKey}`);
      return { ok: true, target: "sekaiemu", pid: targetSession.pid };
    }

    const lobbyTitle = normalizeText(item.lobbyTitle || item.title) || "Runtime";
    const message = senderName
      ? `${senderName} sent you ${itemName} in ${lobbyTitle}`
      : `You received ${itemName} in ${lobbyTitle}`;
    emitSessionEvent({
      event: "runtime:item_received",
      type: "runtime:item_received",
      source: "runtime-social",
      title: "Item received",
      message,
      itemName,
      senderName,
      recipientSlot,
      lobbyId: normalizeText(item.lobbyId),
      roomId: normalizeText(item.roomId),
      moduleId: normalizeText(item.moduleId),
      game: normalizeText(item.game || item.recipientGame),
      at: nowIso(),
    });
    return { ok: true, target: "client-core" };
  };

  const notifyRuntimeActivity = (activity = {}) => {
    const dedupeKey = normalizeText(activity.dedupeKey || activity.id) || [
      normalizeText(activity.lobbyId),
      normalizeText(activity.moduleId),
      normalizeText(activity.kind),
      normalizeText(activity.detail || activity.text || activity.message),
    ].join(":");
    if (!rememberItemNotification(`runtime-activity:${dedupeKey}`)) return { ok: true, deduped: true };
    const targetSession = Array.from(sessions.values()).find((session) => {
      const itemLobby = normalizeText(activity.lobbyId);
      const itemGame = normalizeText(activity.game || activity.recipientGame);
      const itemModule = normalizeText(activity.moduleId);
      if (itemLobby && normalizeText(session.lobbyId) && itemLobby !== normalizeText(session.lobbyId)) return false;
      if (itemGame && normalizeText(session.game) && itemGame !== normalizeText(session.game)) return false;
      if (itemModule && normalizeText(session.moduleId) && itemModule !== normalizeText(session.moduleId)) return false;
      return true;
    }) || (activity.pid ? sessions.get(Number(activity.pid)) : null);
    if (!targetSession) return { ok: false, error: "no_matching_runtime_session" };
    appendActivityForSession(targetSession, {
      id: `activity:${dedupeKey}`,
      kind: activity.kind,
      title: activity.title,
      detail: activity.detail || activity.text || activity.message,
      at: activity.at,
    });
    return { ok: true, target: "sekaiemu", pid: targetSession.pid };
  };

  const handleTraceRecord = (session, record) => {
    if (!session || !record || record.record_type !== "trace" || record.event !== "room_item_pending") return;
    const fields = parseDetailFields(record.detail);
    const itemName = displayItemName(fields.item_name, fields.mapped_value, fields.event_key, fields.ap_item_id);
    notifyRuntimeItem({
      pid: session.pid,
      lobbyId: session.lobbyId,
      lobbyTitle: sessionTitle(session),
      moduleId: session.moduleId,
      game: session.game,
      recipientSlot: session.slot,
      senderName: normalizeText(fields.ap_player_name),
      apPlayerId: fields.ap_player_id,
      itemName,
      itemId: fields.ap_item_id,
      mappedValue: fields.mapped_value,
      deliveryId: fields.delivery_id,
      receivedIndex: fields.delivery_id,
      dedupeKey: [
        normalizeText(session.lobbyId) || normalizeText(session.moduleId),
        normalizeText(session.slot),
        normalizeText(fields.delivery_id),
      ].join(":"),
    });
  };

  const pollEvents = () => {
    for (const session of sessions.values()) {
      for (const line of readNewEventLines(session)) {
        try {
          const event = JSON.parse(line);
          if (event?.type === "open_chat") openSurface("chat");
          if (event?.type === "open_activity") openSurface("activity");
          if (event?.type === "open_hint") openSurface("hint");
        } catch (_err) {
          // Ignore partial or stale event lines.
        }
      }
    }
    const now = Date.now();
    if (now - lastTracePollAt < 1500) return;
    lastTracePollAt = now;
    for (const session of sessions.values()) {
      if (!session?.traceLogPath) continue;
      for (const line of readNewTraceLines(session)) {
        try {
          handleTraceRecord(session, JSON.parse(line));
        } catch (_err) {
          // Ignore partial trace lines.
        }
      }
    }
  };

  const fetchJson = async (session, apiPath, init = {}) => {
    const headers = { ...(init.headers || {}), "X-SekaiLink-Client": "desktop" };
    if (session.authToken && !headers.Authorization) headers.Authorization = `Bearer ${session.authToken}`;
    if (session.deviceId && !headers["X-SekaiLink-Device-Id"]) headers["X-SekaiLink-Device-Id"] = session.deviceId;
    if (init.body && !headers["Content-Type"]) headers["Content-Type"] = "application/json";
    const response = await fetch(`${normalizeApiBase(session.apiBaseUrl)}${apiPath}`, { ...init, headers, credentials: "omit" });
    const data = await response.json().catch(() => ({}));
    return { ok: response.ok, status: response.status, data };
  };

  const channelId = (session) => String(session?.channelId || (session?.lobbyId ? `lobby:${session.lobbyId}` : "")).trim();

  const getState = async () => {
    const session = activeSession();
    if (!session) return { ok: false, error: "no_active_runtime_session" };
    const state = {
      pid: session.pid,
      lobbyId: session.lobbyId || "",
      title: sessionTitle(session),
      moduleId: session.moduleId || "",
      slot: session.slot || "",
      game: session.game || "",
      chatUnread: session.chatUnread || 0,
      activityUnread: session.activityUnread || 0,
      hintPoints: session.hintPoints || "unknown",
      hintItems: Array.isArray(session.hintItems) ? session.hintItems : [],
      activity: session.activity || [],
      messages: [],
    };
    const channel = channelId(session);
    if (channel && typeof fetch === "function") {
      try {
        const res = await fetchJson(session, `/api/chat/channels/${encodeURIComponent(channel)}/messages`);
        state.messages = Array.isArray(res.data?.messages) ? res.data.messages.slice(-80) : [];
      } catch (err) {
        writeLogLine("warn", "runtime-social", `chat list failed: ${String(err?.message || err || "")}`);
      }
    }
    return { ok: true, state };
  };

  const sendChat = async (text) => {
    const session = activeSession();
    if (!session) return { ok: false, error: "no_active_runtime_session" };
    const channel = channelId(session);
    if (!channel) return { ok: false, error: "runtime_channel_missing" };
    const body = JSON.stringify({ content: String(text || "").trim() });
    if (!body || body === "{\"content\":\"\"}") return { ok: false, error: "empty_message" };
    const res = await fetchJson(session, `/api/chat/channels/${encodeURIComponent(channel)}/messages`, { method: "POST", body });
    return res.ok ? { ok: true } : { ok: false, error: "chat_send_failed", status: res.status };
  };

  const requestHint = async (item) => {
    const clean = String(item || "").trim();
    if (!clean) return { ok: false, error: "empty_hint" };
    const res = await sendChat(`/hint ${clean}`);
    return res.ok ? { ok: true, message: `Your ${clean} hint was requested.` } : res;
  };

  const registerIpcHandlers = () => {
    secureIpcHandle("runtimeSocial:getState", async () => getState());
    secureIpcHandle("runtimeSocial:open", async (_event, surface) => openSurface(String(surface || "")));
    secureIpcHandle("runtimeSocial:sendChat", async (_event, text) => sendChat(text));
    secureIpcHandle("runtimeSocial:requestHint", async (_event, item) => requestHint(item));
  };

  const timer = setInterval(pollEvents, 250);
  if (timer.unref) timer.unref();

  return {
    rememberSession,
    forgetSession,
    appendToastForSession,
    notifyRuntimeItem,
    notifyRuntimeActivity,
    openSurface,
    registerIpcHandlers,
    closeAllSurfaces,
    getLogPaths: () => Array.from(sessions.values()).flatMap((session) => [session?.traceLogPath].filter(Boolean)),
    stop: () => {
      clearInterval(timer);
      closeAllSurfaces();
    },
  };
};

module.exports = { createSekaiemuRuntimeSocial };
