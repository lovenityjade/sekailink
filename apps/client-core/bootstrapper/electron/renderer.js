const { ipcRenderer } = require("electron");

const fields = {
  statusText: document.getElementById("statusText"),
  channel: document.getElementById("channel"),
  build: document.getElementById("build"),
  platform: document.getElementById("platform"),
  installDir: document.getElementById("installDir"),
  apiBaseUrl: document.getElementById("apiBaseUrl"),
  autoLaunch: document.getElementById("autoLaunch"),
  force: document.getElementById("force"),
  phase: document.getElementById("phase"),
  percent: document.getElementById("percent"),
  bar: document.getElementById("bar"),
  changelog: document.getElementById("changelog"),
  version: document.getElementById("version"),
  errorPanel: document.getElementById("errorPanel"),
  errorText: document.getElementById("errorText"),
  logPath: document.getElementById("logPath"),
  startBtn: document.getElementById("startBtn"),
  launchBtn: document.getElementById("launchBtn"),
  folderBtn: document.getElementById("folderBtn"),
  logBtn: document.getElementById("logBtn"),
};

let lastInstallDir = "";

const options = () => ({
  channel: fields.channel.value.trim(),
  build: fields.build.value.trim(),
  platform: fields.platform.value.trim(),
  installDir: fields.installDir.value.trim(),
  apiBaseUrl: fields.apiBaseUrl.value.trim(),
  autoLaunch: fields.autoLaunch.checked,
  force: fields.force.checked,
});

const setBusy = (busy) => {
  fields.startBtn.disabled = busy;
  fields.channel.disabled = busy;
  fields.build.disabled = busy;
  fields.platform.disabled = busy;
  fields.installDir.disabled = busy;
  fields.apiBaseUrl.disabled = busy;
  fields.autoLaunch.disabled = busy;
  fields.force.disabled = busy;
};

const setProgress = (phase, percent) => {
  const clean = Math.max(0, Math.min(100, Number(percent) || 0));
  fields.phase.textContent = phase || "Working";
  fields.percent.textContent = `${clean}%`;
  fields.bar.style.width = `${clean}%`;
};

ipcRenderer.on("bootloader:event", (_event, payload) => {
  if (payload.event === "status") {
    fields.statusText.textContent = payload.text || "Working...";
  } else if (payload.event === "progress") {
    setProgress(payload.phase, payload.percent);
  } else if (payload.event === "release") {
    fields.version.textContent = payload.version || "";
    fields.changelog.textContent = payload.changelog || "No changelog was provided by the release server.";
  } else if (payload.event === "done") {
    lastInstallDir = payload.installDir || lastInstallDir;
    fields.statusText.textContent = "SekaiLink is up to date.";
    setProgress("Ready", 100);
    fields.launchBtn.disabled = !payload.executable;
  } else if (payload.event === "error") {
    fields.errorPanel.hidden = false;
    fields.errorText.textContent = payload.message || "Unknown bootloader error.";
    fields.logPath.textContent = payload.logFilePath || "";
    fields.statusText.textContent = "Update failed. Error trace was captured.";
  }
  if (payload.logFilePath) fields.logPath.textContent = payload.logFilePath;
});

fields.startBtn.addEventListener("click", async () => {
  setBusy(true);
  fields.launchBtn.disabled = true;
  fields.statusText.textContent = "Starting update...";
  setProgress("Starting", 0);
  try {
    const result = await ipcRenderer.invoke("bootloader:start", options());
    lastInstallDir = result.installDir || fields.installDir.value.trim();
    fields.launchBtn.disabled = !result.executable;
  } catch (err) {
    fields.errorPanel.hidden = false;
    fields.errorText.textContent = err instanceof Error ? err.stack || err.message : String(err || "Update failed.");
    fields.statusText.textContent = err instanceof Error ? err.message : String(err || "Update failed.");
    setProgress("Error", 0);
  } finally {
    setBusy(false);
  }
});

fields.launchBtn.addEventListener("click", async () => {
  fields.statusText.textContent = "Launching SekaiLink...";
  try {
    await ipcRenderer.invoke("bootloader:launch");
  } catch (err) {
    fields.errorPanel.hidden = false;
    fields.errorText.textContent = err instanceof Error ? err.stack || err.message : String(err || "Launch failed.");
    fields.statusText.textContent = err instanceof Error ? err.message : String(err || "Launch failed.");
  }
});

fields.folderBtn.addEventListener("click", () => {
  ipcRenderer.invoke("bootloader:open-folder", lastInstallDir || fields.installDir.value.trim());
});

fields.logBtn.addEventListener("click", () => {
  ipcRenderer.invoke("bootloader:open-log");
});

ipcRenderer.invoke("bootloader:defaults").then((defaults) => {
  fields.channel.value = defaults.channel || "test";
  fields.build.value = defaults.build || "release";
  fields.platform.value = defaults.platform || "";
  fields.installDir.value = defaults.installDir || "";
  fields.apiBaseUrl.value = defaults.apiBaseUrl || "https://sekailink.com";
  fields.logPath.textContent = defaults.logFilePath || "";
  lastInstallDir = fields.installDir.value;
});
