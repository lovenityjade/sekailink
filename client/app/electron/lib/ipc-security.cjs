const path = require("path");

function isSafeExternalUrl(value) {
  try {
    const u = new URL(String(value || "").trim());
    const protocol = String(u.protocol || "").toLowerCase();
    return protocol === "https:" || protocol === "http:" || protocol === "mailto:" || protocol === "discord:";
  } catch (_err) {
    return false;
  }
}

function isPlainObject(value) {
  return Boolean(value) && typeof value === "object" && !Array.isArray(value);
}

function normalizeIpcString(value, maxLength = 4096) {
  if (typeof value !== "string") return "";
  const normalized = value.trim();
  if (!normalized) return "";
  if (normalized.length > maxLength) return "";
  return normalized;
}

function normalizeIpcPath(value, maxLength = 4096) {
  const p = normalizeIpcString(value, maxLength);
  if (!p) return "";
  if (!path.isAbsolute(p)) return "";
  return p;
}

function normalizeGameId(value) {
  const id = normalizeIpcString(value, 128);
  if (!id) return "";
  return /^[a-z0-9_.-]+$/i.test(id) ? id : "";
}

function createIpcSecurity({
  ipcMain,
  shell,
  isDev,
  devServerUrl,
  writeLogLine,
}) {
  const isTrustedIpcSender = (event) => {
    const senderUrl = String(event?.senderFrame?.url || event?.sender?.getURL?.() || "").trim();
    if (!senderUrl) return false;
    if (isDev) {
      const devOrigin = (() => {
        try {
          return new URL(devServerUrl).origin;
        } catch (_err) {
          return "";
        }
      })();
      if (devOrigin && senderUrl.startsWith(devOrigin)) return true;
    }
    if (senderUrl.startsWith("file://")) return true;
    return false;
  };

  const secureIpcHandle = (channel, handler) => {
    ipcMain.handle(channel, async (event, ...args) => {
      if (!isTrustedIpcSender(event)) {
        writeLogLine("warn", "ipc", `blocked sender for ${channel}`);
        return { ok: false, error: "untrusted_sender" };
      }
      try {
        return await handler(event, ...args);
      } catch (err) {
        writeLogLine("error", "ipc", `${channel} failed: ${String(err || "")}`);
        return { ok: false, error: "ipc_handler_failed" };
      }
    });
  };

  const openExternalSafely = async (value, scope = "external") => {
    const target = normalizeIpcString(value, 2048);
    if (!target) return { ok: false, error: "invalid_url" };
    if (!isSafeExternalUrl(target)) {
      writeLogLine("warn", scope, `blocked external url: ${target}`);
      return { ok: false, error: "unsafe_url" };
    }
    await shell.openExternal(target);
    return { ok: true };
  };

  return {
    secureIpcHandle,
    openExternalSafely,
  };
}

module.exports = {
  isSafeExternalUrl,
  isPlainObject,
  normalizeIpcString,
  normalizeIpcPath,
  normalizeGameId,
  createIpcSecurity,
};
