const RUNTIME_LAB_SESSION_KEY = "sekailink.runtimeLab.session";

export const RUNTIME_LAB_USERNAME = "sekaiemu";
export const RUNTIME_LAB_PASSWORD = "runtime";

export const isRuntimeLabCredentials = (identity: string, password: string) =>
  identity.trim().toLowerCase() === RUNTIME_LAB_USERNAME && password === RUNTIME_LAB_PASSWORD;

export const setRuntimeLabSession = (enabled: boolean) => {
  try {
    if (enabled) {
      window.sessionStorage.setItem(RUNTIME_LAB_SESSION_KEY, "1");
    } else {
      window.sessionStorage.removeItem(RUNTIME_LAB_SESSION_KEY);
    }
  } catch {
    // Session storage can be unavailable in restricted contexts.
  }
};

export const isRuntimeLabSession = () => {
  try {
    return window.sessionStorage.getItem(RUNTIME_LAB_SESSION_KEY) === "1";
  } catch {
    return false;
  }
};

export const clearRuntimeLabSession = () => setRuntimeLabSession(false);
