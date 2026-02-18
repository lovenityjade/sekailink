const path = require("node:path");
const { app, BrowserWindow, ipcMain, shell } = require("electron");

const isDev = !app.isPackaged;
const devServerUrl = process.env.VITE_DEV_SERVER_URL || "http://localhost:5174";

let mainWindow = null;
let pendingAuthUrl = null;

const createWindow = () => {
  if (mainWindow && !mainWindow.isDestroyed()) return mainWindow;

  mainWindow = new BrowserWindow({
    width: 1380,
    height: 900,
    minWidth: 1120,
    minHeight: 700,
    backgroundColor: "#071117",
    webPreferences: {
      preload: path.join(__dirname, "preload.cjs"),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true
    }
  });

  mainWindow.on("closed", () => {
    mainWindow = null;
  });

  mainWindow.webContents.setWindowOpenHandler(({ url }) => {
    shell.openExternal(url);
    return { action: "deny" };
  });

  mainWindow.once("ready-to-show", () => {
    mainWindow.show();
    if (pendingAuthUrl) {
      mainWindow.webContents.send("auth:callback", pendingAuthUrl);
      pendingAuthUrl = null;
    }
  });

  if (isDev) {
    mainWindow.loadURL(devServerUrl);
    mainWindow.webContents.openDevTools({ mode: "detach" });
  } else {
    mainWindow.loadFile(path.join(__dirname, "../dist/index.html"));
  }

  return mainWindow;
};

const emitAuthCallback = (url) => {
  if (!url || typeof url !== "string") return;
  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.webContents.send("auth:callback", url);
  } else {
    pendingAuthUrl = url;
  }
};

const gotLock = app.requestSingleInstanceLock();
if (!gotLock) {
  app.quit();
} else {
  app.on("second-instance", (_event, argv) => {
    const urlArg = argv.find((arg) => typeof arg === "string" && arg.startsWith("sekailink-admin://"));
    if (urlArg) emitAuthCallback(urlArg);
    if (mainWindow) {
      if (mainWindow.isMinimized()) mainWindow.restore();
      mainWindow.focus();
    }
  });
}

app.whenReady().then(() => {
  app.setAsDefaultProtocolClient("sekailink-admin");
  createWindow();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on("open-url", (event, url) => {
  event.preventDefault();
  emitAuthCallback(url);
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") app.quit();
});

ipcMain.handle("app:openExternal", async (_event, url) => {
  const target = String(url || "").trim();
  if (!target) return { ok: false, error: "missing_url" };
  await shell.openExternal(target);
  return { ok: true };
});

ipcMain.handle("app:showItemInFolder", async (_event, targetPath) => {
  const p = String(targetPath || "").trim();
  if (!p) return { ok: false, error: "missing_path" };
  shell.showItemInFolder(p);
  return { ok: true };
});

ipcMain.handle("app:getVersion", async () => ({ ok: true, version: app.getVersion() }));
