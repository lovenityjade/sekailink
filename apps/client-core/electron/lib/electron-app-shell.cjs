"use strict";

const { hardenBrowserWindow } = require("./browser-window-hardening.cjs");

const createElectronAppShell = (deps = {}) => {
  const {
    app,
    BrowserWindow,
    Menu,
    dialog,
    screen,
    fs,
    path,
    processRef = process,
    isDev,
    devServerUrl,
    shouldOpenDevTools,
    resolveWindowIconPath,
    openExternalSafely,
    dirname,
    readJsonFileSafe,
    startFileLogging,
    getLatestLogFilePath,
    writeLogLine,
    writeLogJson,
    logger,
    validateBootstrapLaunchToken,
    shutdownRuntimeProcesses,
    trackerWebWindows,
    submitDiagnosticsReport,
    getCrashReportingOptIn,
  } = deps;
  const process = processRef;
  let mainWindow;
  let pendingAuthUrl = null;
  const dashboardWindows = new Set();

  const getActiveMainWindow = () => {
    try {
      return mainWindow && !mainWindow.isDestroyed() ? mainWindow : null;
    } catch (_err) {
      return null;
    }
  };

  const getWindowStatePath = () => path.join(app.getPath("userData"), "window-state.json");
  
  const isBoundsVisible = (bounds) => {
    if (!bounds || !Number.isFinite(bounds.x) || !Number.isFinite(bounds.y)) return false;
    const displays = screen.getAllDisplays();
    const center = {
      x: bounds.x + Math.max(1, Number(bounds.width || 1)) / 2,
      y: bounds.y + Math.max(1, Number(bounds.height || 1)) / 2,
    };
    return displays.some((display) => {
      const area = display.workArea;
      return center.x >= area.x && center.x <= area.x + area.width && center.y >= area.y && center.y <= area.y + area.height;
    });
  };
  
  const readMainWindowBounds = (fallbackWidth, fallbackHeight) => {
    const fallbackDisplay = screen.getDisplayNearestPoint(screen.getCursorScreenPoint());
    const fallbackArea = fallbackDisplay?.workArea || screen.getPrimaryDisplay().workArea;
    const fallback = {
      width: fallbackWidth,
      height: fallbackHeight,
      x: Math.round(fallbackArea.x + (fallbackArea.width - fallbackWidth) / 2),
      y: Math.round(fallbackArea.y + (fallbackArea.height - fallbackHeight) / 2),
    };
    const parsed = readJsonFileSafe(getWindowStatePath());
    const saved = parsed?.mainWindow;
    if (!saved || !isBoundsVisible(saved)) return fallback;
    return {
      width: Math.max(fallbackWidth, Math.round(Number(saved.width) || fallbackWidth)),
      height: Math.max(fallbackHeight, Math.round(Number(saved.height) || fallbackHeight)),
      x: Math.round(Number(saved.x)),
      y: Math.round(Number(saved.y)),
    };
  };
  
  const saveMainWindowBounds = () => {
    try {
      if (!mainWindow || mainWindow.isDestroyed()) return;
      if (mainWindow.isMinimized() || mainWindow.isFullScreen()) return;
      fs.mkdirSync(path.dirname(getWindowStatePath()), { recursive: true });
      fs.writeFileSync(getWindowStatePath(), JSON.stringify({ mainWindow: mainWindow.getBounds() }, null, 2));
    } catch (_err) {
      // best-effort window placement persistence
    }
  };
  
  const escapeHtml = (value) =>
    String(value || "").replace(/[<>&]/g, (ch) => ({
      "<": "&lt;",
      ">": "&gt;",
      "&": "&amp;",
    }[ch]));
  
  const showMainWindowLoadFailure = (message) => {
    try {
      if (!mainWindow || mainWindow.isDestroyed()) return;
      const logPath = escapeHtml(getLatestLogFilePath());
      const html = `<!doctype html>
  <html>
    <head>
      <meta charset="utf-8" />
      <title>SekaiLink Load Error</title>
      <style>
        body { margin: 0; min-height: 100vh; display: grid; place-items: center; background: #05070a; color: #f5f7fb; font-family: Inter, Segoe UI, sans-serif; }
        main { width: min(720px, calc(100vw - 48px)); border: 1px solid rgba(255,107,53,.45); border-radius: 14px; background: #14151a; padding: 28px; box-shadow: 0 20px 60px rgba(0,0,0,.4); }
        h1 { margin: 0 0 10px; color: #ff6b35; font-size: 24px; }
        p { color: #c9ced8; line-height: 1.5; }
        code { display: block; margin-top: 14px; padding: 12px; border-radius: 8px; background: #0b1018; color: #8fe8de; word-break: break-all; }
      </style>
    </head>
    <body>
      <main>
        <h1>SekaiLink could not finish loading</h1>
        <p>${escapeHtml(message || "The client UI failed to load.")}</p>
        ${logPath ? `<code>${logPath}</code>` : ""}
      </main>
    </body>
  </html>`;
      mainWindow.loadURL(`data:text/html;charset=utf-8,${encodeURIComponent(html)}`);
    } catch (_err) {
      // Last-resort display path; ignore.
    }
  };

  const createDashboardWindow = (targetUrl) => {
    const win = new BrowserWindow({
      width: 1200,
      height: 820,
      minWidth: 1000,
      minHeight: 700,
      backgroundColor: "#05070A",
      show: false,
      autoHideMenuBar: true,
      icon: resolveWindowIconPath(),
      webPreferences: {
        contextIsolation: true,
        nodeIntegration: false,
        sandbox: true,
        devTools: false
      }
    });
    hardenBrowserWindow(win);
  
    const loadingPath = path.join(dirname, "loading.html");
    let loaded = false;
  
    win.once("ready-to-show", () => {
      win.show();
    });
  
    win.on("closed", () => {
      dashboardWindows.delete(win);
    });
  
    win.webContents.setWindowOpenHandler(({ url }) => {
      void openExternalSafely(url, "dashboard-window-open");
      return { action: "deny" };
    });
  
    win.webContents.on("did-finish-load", () => {
      if (!loaded && targetUrl) {
        loaded = true;
        // Give the loading screen a moment to paint.
        setTimeout(() => {
          if (!win.isDestroyed()) win.loadURL(targetUrl);
        }, 150);
      }
    });
  
    win.loadFile(loadingPath);
    dashboardWindows.add(win);
    return win;
  };
  
  const createWindow = () => {
    const width = 1600;
    const height = 900;
    const bounds = readMainWindowBounds(width, height);
  
    mainWindow = new BrowserWindow({
      width: bounds.width,
      height: bounds.height,
      x: bounds.x,
      y: bounds.y,
      minWidth: 1280,
      minHeight: 720,
      frame: false,
      titleBarStyle: "hidden",
      autoHideMenuBar: true,
      backgroundColor: "#05070A",
      show: false,
      icon: resolveWindowIconPath(),
      webPreferences: {
        preload: path.join(dirname, "preload.cjs"),
        contextIsolation: true,
        nodeIntegration: false,
        sandbox: true,
        devTools: false
      }
    });
    hardenBrowserWindow(mainWindow);
  
    mainWindow.on("moved", saveMainWindowBounds);
    mainWindow.on("resized", saveMainWindowBounds);
    mainWindow.on("close", saveMainWindowBounds);
  
    mainWindow.once("ready-to-show", () => {
      mainWindow.show();
      if (!mainWindow.isMaximized()) mainWindow.maximize();
      mainWindow.focus();
    });
  
    mainWindow.webContents.setWindowOpenHandler(({ url }) => {
      void openExternalSafely(url, "main-window-open");
      return { action: "deny" };
    });
  
    // Capture renderer console output into the main log file (best-effort).
    mainWindow.webContents.on("console-message", (_event, level, message, line, sourceId) => {
      const lvl = level >= 2 ? "warn" : "info";
      writeLogLine(lvl, "renderer-console", `${message} (${sourceId || "unknown"}:${line || 0})`);
    });
  
    mainWindow.webContents.on("render-process-gone", (_event, details) => {
      writeLogJson("renderer-crash", details || {});
      showMainWindowLoadFailure(`Renderer process stopped: ${String(details?.reason || "unknown")}`);
    });
  
    mainWindow.webContents.on("did-fail-load", (_event, errorCode, errorDescription, validatedURL) => {
      writeLogLine("error", "main-window", `did-fail-load code=${errorCode} detail=${errorDescription} url=${validatedURL}`);
      showMainWindowLoadFailure(`Failed to load client UI: ${errorDescription || errorCode}`);
    });
  
    const loadingPath = path.join(dirname, "loading.html");
    let mainTargetLoadStarted = false;
    let mainTargetLoaded = false;
    const loadMainTarget = () => {
      if (mainWindow.isDestroyed()) return;
      writeLogLine("info", "main-window", "loading main target");
      if (isDev) {
        mainWindow.loadURL(devServerUrl);
      } else {
        mainWindow.loadFile(path.join(dirname, "../dist/index.html"));
      }
    };
    const deliverPendingAuthUrl = () => {
      if (!mainTargetLoaded || !pendingAuthUrl || mainWindow.isDestroyed()) return;
      mainWindow.webContents.send("auth:callback", pendingAuthUrl);
      pendingAuthUrl = null;
    };
  
    mainWindow.webContents.once("did-finish-load", () => {
      if (!mainTargetLoadStarted) {
        mainTargetLoadStarted = true;
        setTimeout(loadMainTarget, 80);
      }
    });
    mainWindow.webContents.on("did-finish-load", () => {
      if (!mainTargetLoadStarted) return;
      if (mainTargetLoaded) return;
      const currentUrl = mainWindow.webContents.getURL();
      if (currentUrl.startsWith("file:") && currentUrl.endsWith("loading.html")) return;
      mainTargetLoaded = true;
      deliverPendingAuthUrl();
    });
    setTimeout(() => {
      if (!mainWindow || mainWindow.isDestroyed()) return;
      if (mainTargetLoadStarted && !mainTargetLoaded) {
        writeLogLine("error", "main-window", "main target load timeout");
        showMainWindowLoadFailure("The client UI timed out while loading.");
      }
    }, 8000);
    mainWindow.loadFile(loadingPath);
  };
  
  const getMainWindow = () => mainWindow || null;

  const registerAppLifecycle = () => {
    app.whenReady().then(() => {
      try {
        Menu?.setApplicationMenu?.(null);
      } catch (_err) {
        // ignore menu cleanup failures
      }
      startFileLogging();
      writeLogJson("env", {
        app_version: app.getVersion ? app.getVersion() : "",
        platform: process.platform,
        arch: process.arch,
        electron: process.versions?.electron,
        chrome: process.versions?.chrome,
        node: process.versions?.node
      });

      const bootstrapCheck = validateBootstrapLaunchToken();
      if (!bootstrapCheck.ok) {
        writeLogLine("error", "bootstrap", `launch rejected: ${String(bootstrapCheck.error || "unknown")}`);
        dialog.showErrorBox(
          "SekaiLink launch blocked",
          "This client must be launched via the SekaiLink bootstrapper/updater."
        );
        app.exit(1);
        return;
      }

      app.setAsDefaultProtocolClient("sekailink");
      createWindow();

      app.on("activate", () => {
        const activeWindow = getActiveMainWindow();
        if (activeWindow) {
          if (activeWindow.isMinimized()) activeWindow.restore();
          activeWindow.show();
          activeWindow.focus();
          return;
        }
        createWindow();
      });
    });

    app.on("window-all-closed", () => {
      if (process.platform !== "darwin") app.quit();
    });

    let quitCleanupInProgress = false;
    app.on("before-quit", (event) => {
      if (quitCleanupInProgress) return;
      event.preventDefault();
      quitCleanupInProgress = true;
      void (async () => {
        await shutdownRuntimeProcesses("before-quit");
        for (const win of Array.from(trackerWebWindows)) {
          try {
            if (!win.isDestroyed()) win.close();
          } catch (_err) {
            // ignore
          }
        }
        trackerWebWindows.clear();
        logger.stop();
        app.exit(0);
      })();
    });

    app.on("will-quit", () => {
      void shutdownRuntimeProcesses("will-quit");
    });

    let inSignalShutdown = false;
    const onSignal = (signalName) => {
      if (inSignalShutdown) return;
      inSignalShutdown = true;
      void (async () => {
        await shutdownRuntimeProcesses(`signal:${signalName}`);
        logger.stop();
        process.exit(0);
      })();
    };
    process.on("SIGINT", () => onSignal("SIGINT"));
    process.on("SIGTERM", () => onSignal("SIGTERM"));

    app.on("quit", () => {
      for (const win of Array.from(trackerWebWindows)) {
        try {
          if (!win.isDestroyed()) win.close();
        } catch (_err) {
          // ignore
        }
      }
      trackerWebWindows.clear();
    });

    app.on("open-url", (event, url) => {
      event.preventDefault();
      const activeWindow = getActiveMainWindow();
      if (activeWindow) {
        activeWindow.webContents.send("auth:callback", url);
      } else {
        pendingAuthUrl = url;
      }
    });

    const gotLock = app.requestSingleInstanceLock();
    if (!gotLock) {
      try {
        app.exit(0);
      } catch (_err) {
        // ignore
      }
      setTimeout(() => process.exit(0), 25).unref();
    } else {
      app.on("second-instance", (_event, argv) => {
        const urlArg = argv.find((arg) => typeof arg === "string" && arg.startsWith("sekailink://"));
        const activeWindow = getActiveMainWindow();
        if (urlArg) {
          if (activeWindow) {
            activeWindow.webContents.send("auth:callback", urlArg);
          } else {
            pendingAuthUrl = urlArg;
          }
        }
        if (activeWindow) {
          if (activeWindow.isMinimized()) activeWindow.restore();
          activeWindow.show();
          activeWindow.focus();
        }
      });
    }

    process.on("uncaughtException", (err) => {
      const msg = err && err.stack ? err.stack : String(err);
      writeLogLine("error", "uncaughtException", msg);
      if (getCrashReportingOptIn?.()) {
        void submitDiagnosticsReport({
          uploadUrl: process.env.SEKAILINK_CRASH_REPORT_URL || "",
          meta: { event: "uncaughtException", message: msg },
        });
      }
    });

    process.on("unhandledRejection", (reason) => {
      const msg = reason && reason.stack ? reason.stack : logger.safeStringify(reason);
      writeLogLine("error", "unhandledRejection", msg);
      if (getCrashReportingOptIn?.()) {
        void submitDiagnosticsReport({
          uploadUrl: process.env.SEKAILINK_CRASH_REPORT_URL || "",
          meta: { event: "unhandledRejection", message: msg },
        });
      }
    });

    app.on("render-process-gone", (_event, webContents, details) => {
      const payload = {
        url: webContents?.getURL ? webContents.getURL() : "",
        ...details
      };
      writeLogJson("app.render-process-gone", payload);
      if (getCrashReportingOptIn?.()) {
        void submitDiagnosticsReport({
          uploadUrl: process.env.SEKAILINK_CRASH_REPORT_URL || "",
          meta: { event: "render-process-gone", details: payload },
        });
      }
    });

    app.on("child-process-gone", (_event, details) => {
      writeLogJson("app.child-process-gone", details || {});
      if (getCrashReportingOptIn?.()) {
        void submitDiagnosticsReport({
          uploadUrl: process.env.SEKAILINK_CRASH_REPORT_URL || "",
          meta: { event: "child-process-gone", details: details || {} },
        });
      }
    });
  };

  return {
    getMainWindow,
    createDashboardWindow,
    registerAppLifecycle,
  };
};

module.exports = { createElectronAppShell };
