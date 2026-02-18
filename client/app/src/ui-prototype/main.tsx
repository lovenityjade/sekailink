import React from "react";
import ReactDOM from "react-dom/client";
import { HashRouter } from "react-router-dom";
import PrototypeApp from "./prototypeApp";

// Baseline client styles (to keep existing pages functional/usable),
// then prototype overrides to match the mockup theme.
import "@/styles/globalStyles.css";
import "@/styles/markdown.css";
import "@/styles/tooltip.css";
import "@/styles/appShell.css";
import "@/styles/termsModal.css";
import "@/styles/roomList.css";
import "@/styles/lobby.css";
import "@/styles/gameManager.css";
import "@/styles/account.css";
import "@/styles/yamlDashboard.css";
import "@/styles/social.css";
import "@/styles/boot.css";
import "@/styles/playerOptions.css";
import "@/styles/trackerPanel.css";

import "./styles/tokens.css";
import "./styles/base.css";
import "./styles/overrides.css";
import "./styles/dashboard.css";
import "./styles/lobby.css";

// Best-effort renderer error forwarding (same contract as main app).
const sendRendererLog = (payload: Record<string, unknown>) => {
  try {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    (window as any)?.sekailink?.logToMain?.(payload);
  } catch (_err) {
    // ignore
  }
};

window.addEventListener("error", (event) => {
  sendRendererLog({
    type: "ui-prototype.window.error",
    message: String(event.message || ""),
    filename: String((event as any).filename || ""),
    lineno: Number((event as any).lineno || 0),
    colno: Number((event as any).colno || 0),
    stack: (event as any).error?.stack ? String((event as any).error.stack) : "",
  });
});

window.addEventListener("unhandledrejection", (event) => {
  const reason: any = (event as any).reason;
  sendRendererLog({
    type: "ui-prototype.window.unhandledrejection",
    reason: reason && reason.stack ? String(reason.stack) : String(reason || ""),
  });
});

ReactDOM.createRoot(document.getElementById("root")!).render(
  <React.StrictMode>
    <HashRouter>
      <PrototypeApp />
    </HashRouter>
  </React.StrictMode>
);
