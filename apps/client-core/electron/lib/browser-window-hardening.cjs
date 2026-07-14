"use strict";

const isCtrlOrMeta = (input = {}) => Boolean(input.control || input.meta);

const shouldBlockBrowserAccelerator = (input = {}) => {
  const key = String(input.key || "").toLowerCase();
  const code = String(input.code || "").toLowerCase();
  const type = String(input.type || "").toLowerCase();
  if (type !== "keyDown" && type !== "rawKeyDown") return false;

  if (key === "f5" || code === "f5") return true;
  if (key === "f12" || code === "f12") return true;
  if (key === "browserback" || key === "browserforward") return true;

  if (input.alt && (key === "arrowleft" || key === "arrowright" || key === "left" || key === "right")) {
    return true;
  }

  if (isCtrlOrMeta(input)) {
    if (key === "r") return true;
    if (key === "[" || key === "]") return true;
    if (input.shift && (key === "i" || key === "j" || key === "c")) return true;
  }

  return false;
};

const hardenBrowserWindow = (win) => {
  if (!win || win.isDestroyed?.() || !win.webContents) return win;
  const contents = win.webContents;

  try {
    contents.on("before-input-event", (event, input) => {
      if (shouldBlockBrowserAccelerator(input)) event.preventDefault();
    });
  } catch (_err) {
    // best-effort shell hardening
  }

  try {
    contents.on("devtools-opened", () => {
      try {
        contents.closeDevTools();
      } catch (_err) {
        // ignore
      }
    });
  } catch (_err) {
    // best-effort shell hardening
  }

  return win;
};

module.exports = {
  hardenBrowserWindow,
  shouldBlockBrowserAccelerator,
};
