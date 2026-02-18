const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("sekailinkAdmin", {
  openExternal: (url) => ipcRenderer.invoke("app:openExternal", url),
  showItemInFolder: (targetPath) => ipcRenderer.invoke("app:showItemInFolder", targetPath),
  getVersion: () => ipcRenderer.invoke("app:getVersion"),
  onAuthCallback: (handler) => {
    if (typeof handler !== "function") return () => {};
    const listener = (_event, url) => handler(url);
    ipcRenderer.on("auth:callback", listener);
    return () => ipcRenderer.removeListener("auth:callback", listener);
  }
});
