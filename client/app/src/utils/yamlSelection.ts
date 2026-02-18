const ACTIVE_YAML_STORAGE_KEY = "skl.activeYamlId";

export const getPreferredYamlId = (): string => {
  try {
    return String(window.localStorage.getItem(ACTIVE_YAML_STORAGE_KEY) || "").trim();
  } catch (_err) {
    return "";
  }
};

export const setPreferredYamlId = (yamlId: string) => {
  try {
    const value = String(yamlId || "").trim();
    if (!value) {
      window.localStorage.removeItem(ACTIVE_YAML_STORAGE_KEY);
      return;
    }
    window.localStorage.setItem(ACTIVE_YAML_STORAGE_KEY, value);
  } catch (_err) {
    // ignore storage errors
  }
};

