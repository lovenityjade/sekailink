import { runtime } from './runtime';
import { trace, traceError } from './trace';

export type ClientTheme = 'default' | 'light' | 'deep-dark';

const STORAGE_KEY = 'skl_ui_theme';
const THEME_CLASSES = ['skl-theme-default', 'skl-theme-light', 'skl-theme-deep-dark'];

export const normalizeClientTheme = (value?: string | null): ClientTheme => {
  const raw = String(value || '').trim().toLowerCase();
  if (raw === 'light') return 'light';
  if (raw === 'deep-dark' || raw === 'dark') return 'deep-dark';
  return 'default';
};

export const applyClientTheme = (theme: ClientTheme) => {
  const root = document.documentElement;
  root.classList.remove(...THEME_CLASSES);
  root.classList.add(`skl-theme-${theme}`);
  root.dataset.sklTheme = theme;
  try {
    window.localStorage.setItem(STORAGE_KEY, theme);
  } catch {
    // ignore
  }
};

export const getStoredClientTheme = (): ClientTheme => {
  try {
    return normalizeClientTheme(window.localStorage.getItem(STORAGE_KEY));
  } catch {
    return 'default';
  }
};

export const hydrateClientTheme = async () => {
  applyClientTheme(getStoredClientTheme());
  try {
    const config = await runtime.configGet?.();
    const root = config && typeof config === 'object' ? config as any : {};
    const layout = root.layout && typeof root.layout === 'object' ? root.layout : {};
    const ui = layout.ui && typeof layout.ui === 'object' ? layout.ui : {};
    const theme = normalizeClientTheme(ui.theme);
    applyClientTheme(theme);
    trace('theme', 'hydrated', { theme });
  } catch (error) {
    traceError('theme', 'hydrate_failed', error);
  }
};
