import React from "react";
import ReactDOM from "react-dom/client";
import { HashRouter } from "react-router-dom";
import PrototypeApp from "./ui-prototype/prototypeApp";
import { I18nProvider } from "./i18n";
import { runtime } from "./services/runtime";

import "./styles/globalStyles.css";
import "./styles/markdown.css";
import "./styles/tooltip.css";
import "./styles/appShell.css";
import "./styles/termsModal.css";
import "./styles/roomList.css";
import "./styles/lobby.css";
import "./styles/gameManager.css";
import "./styles/account.css";
import "./styles/yamlDashboard.css";
import "./styles/social.css";
import "./styles/settings.css";
import "./styles/boot.css";
import "./styles/playerOptions.css";
import "./styles/trackerPanel.css";
import "./ui-prototype/styles/tokens.css";
import "./ui-prototype/styles/base.css";
import "./ui-prototype/styles/overrides.css";
import "./ui-prototype/styles/dashboard.css";
import "./ui-prototype/styles/lobby.css";

const CRASH_REPORTING_OPT_IN_KEY = "skl_crash_reporting_opt_in";

try {
  const raw = window.localStorage.getItem(CRASH_REPORTING_OPT_IN_KEY);
  const enabled = raw === "1";
  void runtime.setCrashReportingOptIn?.(enabled);
} catch {
  // ignore
}

// Forward renderer crashes/errors to the Electron main process so they get written to log files.
// This is best-effort; logging must never break the UI.
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
    type: "window.error",
    message: String(event.message || ""),
    filename: String((event as any).filename || ""),
    lineno: Number((event as any).lineno || 0),
    colno: Number((event as any).colno || 0),
    stack: (event.error && event.error.stack) ? String(event.error.stack) : ""
  });
});

window.addEventListener("unhandledrejection", (event) => {
  const reason: any = (event as any).reason;
  sendRendererLog({
    type: "window.unhandledrejection",
    reason: reason && reason.stack ? String(reason.stack) : String(reason || ""),
  });
});

ReactDOM.createRoot(document.getElementById("root")!).render(
  <React.StrictMode>
    <HashRouter>
      <I18nProvider>
        <PrototypeApp />
      </I18nProvider>
    </HashRouter>
  </React.StrictMode>
);
