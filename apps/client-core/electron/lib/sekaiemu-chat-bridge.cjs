"use strict";

const createSekaiemuChatBridge = (deps = {}) => {
  const {
    app,
    fs,
    path,
    crypto,
    processRef = process,
    getClientServerBaseUrl,
    ensureDir,
    writeLogLine,
    nowIso,
  } = deps;
  const process = processRef;

  const normalizeChatBridgeApiBase = (value) => {
    const raw = String(
      value ||
        process.env.SEKAILINK_API_BASE_URL ||
        process.env.VITE_API_BASE_URL ||
        getClientServerBaseUrl()
    ).trim();
    if (!raw || raw === "same") return "";
    return raw.replace(/\/+$/, "");
  };
  
  const chatBridgeApiUrl = (apiBaseUrl, apiPath) => {
    const cleanPath = String(apiPath || "").startsWith("/") ? String(apiPath || "") : `/${apiPath || ""}`;
    return `${apiBaseUrl}${cleanPath}`;
  };
  
  const appendChatBridgeLine = (filePath, payload) => {
    ensureDir(path.dirname(filePath));
    fs.appendFileSync(filePath, `${JSON.stringify(payload)}\n`, "utf8");
  };
  
  const appendSekaiemuSystemChat = (bridge, payload) => {
    if (!bridge?.inboxPath) return;
    const text = String(payload?.text || "").trim();
    if (!text) return;
    appendChatBridgeLine(bridge.inboxPath, {
      id: payload.id || `sekaiemu-system:${Date.now()}:${crypto.randomBytes(4).toString("hex")}`,
      author: String(payload.author || "SYSTEM").slice(0, 32),
      text: text.slice(0, 500),
      created_at: nowIso(),
      kind: String(payload.kind || "system"),
    });
  };
  
  const readNewChatBridgeLines = (filePath, state) => {
    try {
      if (!filePath || !fs.existsSync(filePath)) return [];
      const stat = fs.statSync(filePath);
      if (stat.size < state.offset) state.offset = 0;
      if (stat.size === state.offset) return [];
      const fd = fs.openSync(filePath, "r");
      try {
        const length = stat.size - state.offset;
        const buffer = Buffer.alloc(Math.min(length, 1024 * 1024));
        const bytesRead = fs.readSync(fd, buffer, 0, buffer.length, state.offset);
        state.offset += bytesRead;
        return buffer.toString("utf8", 0, bytesRead).split(/\r?\n/).filter((line) => line.trim());
      } finally {
        fs.closeSync(fd);
      }
    } catch (err) {
      writeLogLine("warn", "sekaiemu-chat", `read failed: ${String(err || "")}`);
      return [];
    }
  };
  
  const normalizeChatBridgeMessage = (message, fallbackChannel) => {
    const author = String(
      message?.display_name ||
        message?.global_name ||
        message?.username ||
        message?.author_name ||
        message?.author ||
        message?.name ||
        "Room"
    );
    const text = String(message?.content || message?.text || message?.body || "");
    const createdAt = String(message?.created_at || message?.timestamp || nowIso());
    const id = String(
      message?.id ||
        message?.message_id ||
        `${fallbackChannel}:${createdAt}:${author}:${text}`
    );
    return {
      id,
      author,
      text,
      created_at: createdAt,
      kind: String(message?.kind || message?.type || "user"),
    };
  };

  const isHumanChatBridgeMessage = (message) => {
    const kind = String(message?.kind || message?.type || "user").trim().toLowerCase();
    if (["system", "connection", "archipelago", "web-ap-client", "web-ap-client-command", "web-ap-client-error"].includes(kind)) {
      return false;
    }
    const author = String(
      message?.display_name ||
        message?.global_name ||
        message?.username ||
        message?.author_name ||
        message?.author ||
        message?.name ||
        ""
    ).trim();
    if (!author || /^(system|room|ap client|web ap client|sekaiemu)$/i.test(author)) {
      return false;
    }
    return true;
  };
  
  const rememberChatBridgeMessageId = (bridge, id) => {
    if (!id) return true;
    if (bridge.seenServerIds.has(id)) return false;
    bridge.seenServerIds.add(id);
    bridge.seenServerOrder.push(id);
    while (bridge.seenServerOrder.length > 512) {
      bridge.seenServerIds.delete(bridge.seenServerOrder.shift());
    }
    return true;
  };
  
  const fetchChatBridgeJson = async (bridge, apiPath, init = {}) => {
    const headers = {
      ...(init.headers || {}),
      "X-SekaiLink-Client": "desktop",
    };
    if (bridge.authToken && !headers.Authorization) {
      headers.Authorization = `Bearer ${bridge.authToken}`;
    }
    if (bridge.deviceId && !headers["X-SekaiLink-Device-Id"]) {
      headers["X-SekaiLink-Device-Id"] = bridge.deviceId;
    }
    if (init.body && !headers["Content-Type"]) {
      headers["Content-Type"] = "application/json";
    }
    const response = await fetch(chatBridgeApiUrl(bridge.apiBaseUrl, apiPath), {
      ...init,
      headers,
      credentials: "omit",
    });
    const text = await response.text().catch(() => "");
    let data = {};
    if (text) {
      try {
        data = JSON.parse(text);
      } catch (_err) {
        data = { raw: text };
      }
    }
    return { ok: response.ok, status: response.status, data };
  };
  
  const listChatBridgeMessages = async (bridge) => {
    const channelPath = `/api/chat/channels/${encodeURIComponent(bridge.channelId)}/messages`;
    const primary = await fetchChatBridgeJson(bridge, channelPath);
    if (primary.ok) {
      return Array.isArray(primary.data?.messages) ? primary.data.messages : [];
    }
    if (!bridge.lobbyId) {
      throw new Error(`chat_list_failed:${primary.status}`);
    }
    const fallback = await fetchChatBridgeJson(
      bridge,
      `/api/lobbies/${encodeURIComponent(bridge.lobbyId)}/messages`
    );
    if (!fallback.ok) {
      throw new Error(`chat_list_failed:${fallback.status}`);
    }
    return Array.isArray(fallback.data?.messages) ? fallback.data.messages : [];
  };
  
  const sendChatBridgeMessage = async (bridge, text) => {
    const body = JSON.stringify({ content: String(text || "").trim() });
    const channelPath = `/api/chat/channels/${encodeURIComponent(bridge.channelId)}/messages`;
    const primary = await fetchChatBridgeJson(bridge, channelPath, { method: "POST", body });
    if (primary.ok) return true;
    if (!bridge.lobbyId) {
      throw new Error(`chat_send_failed:${primary.status}`);
    }
    const fallback = await fetchChatBridgeJson(
      bridge,
      `/api/lobbies/${encodeURIComponent(bridge.lobbyId)}/messages`,
      { method: "POST", body }
    );
    if (!fallback.ok) {
      throw new Error(`chat_send_failed:${fallback.status}`);
    }
    return true;
  };
  
  const logChatBridgeErrorOnce = (bridge, step, err) => {
    const key = `${step}:${String(err?.message || err || "")}`;
    if (bridge.lastErrorKey === key) return;
    bridge.lastErrorKey = key;
    writeLogLine("warn", "sekaiemu-chat", key);
  };
  
  const startSekaiemuChatBridge = (options = {}) => {
    const lobbyId = String(options.lobbyId || "").trim();
    const channelId = String(options.channelId || (lobbyId ? `lobby:${lobbyId}` : "")).trim();
    const apiBaseUrl = normalizeChatBridgeApiBase(options.apiBaseUrl);
    if (!channelId || !apiBaseUrl || typeof fetch !== "function") {
      return null;
    }
    const moduleId = String(options.moduleId || "game").replace(/[^a-z0-9_.-]+/gi, "_") || "game";
    const bridgeId = `${Date.now()}-${crypto.randomBytes(4).toString("hex")}`;
    const root = path.join(app.getPath("userData"), "runtime", "sekaiemu-chat", moduleId, bridgeId);
    ensureDir(root);
    const bridge = {
      id: bridgeId,
      channelId,
      lobbyId,
      apiBaseUrl,
      authToken: String(options.authToken || "").trim(),
      deviceId: String(options.deviceId || "").trim(),
      inboxPath: path.join(root, "from-core.jsonl"),
      outboxPath: path.join(root, "to-core.jsonl"),
      outboxRead: { offset: 0 },
      seenServerIds: new Set(),
      seenServerOrder: [],
      lastErrorKey: "",
      stopped: false,
      timers: [],
    };
    fs.writeFileSync(bridge.inboxPath, "", "utf8");
    fs.writeFileSync(bridge.outboxPath, "", "utf8");
  
    const pollOutbox = async () => {
      if (bridge.stopped) return;
      const lines = readNewChatBridgeLines(bridge.outboxPath, bridge.outboxRead);
      for (const line of lines) {
        try {
          const parsed = JSON.parse(line);
          const text = String(parsed?.text || parsed?.content || "").trim();
          if (text) await sendChatBridgeMessage(bridge, text);
        } catch (err) {
          logChatBridgeErrorOnce(bridge, "send", err);
        }
      }
    };
  
    const pollServer = async () => {
      if (bridge.stopped) return;
      try {
        const messages = await listChatBridgeMessages(bridge);
        for (const rawMessage of messages) {
          if (!isHumanChatBridgeMessage(rawMessage)) continue;
          const message = normalizeChatBridgeMessage(rawMessage, bridge.channelId);
          if (!message.text || !rememberChatBridgeMessageId(bridge, message.id)) continue;
          appendChatBridgeLine(bridge.inboxPath, message);
        }
      } catch (err) {
        logChatBridgeErrorOnce(bridge, "list", err);
      }
    };
  
    bridge.timers.push(setInterval(() => { void pollOutbox(); }, 250));
    bridge.timers.push(setInterval(() => { void pollServer(); }, 1500));
    setTimeout(() => { void pollServer(); }, 100);
    return {
      id: bridge.id,
      channelId: bridge.channelId,
      moduleId,
      inboxPath: bridge.inboxPath,
      outboxPath: bridge.outboxPath,
      stop: () => {
        bridge.stopped = true;
        for (const timer of bridge.timers) clearInterval(timer);
      },
    };
  };

  return {
    appendSekaiemuSystemChat,
    startSekaiemuChatBridge,
  };
};

module.exports = { createSekaiemuChatBridge };
