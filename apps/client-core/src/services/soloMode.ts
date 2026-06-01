export const SOLO_MODE_KEY = "skl_solo_mode";
export const SOLO_MODE_SESSION_KEY = "skl_solo_mode_session";
export const SOLO_MODE_EVENT = "sekailink:solo-mode-change";

export const hasSoloModeUrlFlag = () => {
  try {
    const params = new URLSearchParams(window.location.search || "");
    const hashQuery = String(window.location.hash || "").split("?")[1] || "";
    const hashParams = new URLSearchParams(hashQuery);
    return params.has("solo") || hashParams.has("solo");
  } catch {
    return false;
  }
};

export const isSoloModeEnabled = () => {
  try {
    if (hasSoloModeUrlFlag()) return true;
    return window.sessionStorage.getItem(SOLO_MODE_SESSION_KEY) === "1";
  } catch {
    return hasSoloModeUrlFlag();
  }
};

export const setSoloModeEnabled = (enabled: boolean) => {
  try {
    if (enabled) {
      window.sessionStorage.setItem(SOLO_MODE_SESSION_KEY, "1");
    } else {
      window.sessionStorage.removeItem(SOLO_MODE_SESSION_KEY);
    }
    // Legacy migration: solo mode must not hijack future online launches.
    window.localStorage.removeItem(SOLO_MODE_KEY);
  } catch {
    // ignore storage errors
  }
  try {
    window.dispatchEvent(new Event(SOLO_MODE_EVENT));
  } catch {
    // ignore event failures
  }
};
