import React from "react";
import en from "./locales/en.json";
import fr from "./locales/fr.json";
import es from "./locales/es.json";
import ja from "./locales/ja.json";
import zhCN from "./locales/zh-CN.json";
import zhTW from "./locales/zh-TW.json";
import { LocaleCode, Messages, SUPPORTED_LOCALES } from "./types";

const STORAGE_KEY = "skl_locale";

const bundles: Record<LocaleCode, Messages> = {
  en,
  fr,
  es,
  ja,
  "zh-CN": zhCN,
  "zh-TW": zhTW,
};

const allLocaleCodes = Object.keys(bundles) as LocaleCode[];

const findKeyByLiteral = (literal: string): string | null => {
  const source = String(literal || "").trim();
  if (!source) return null;
  for (const code of allLocaleCodes) {
    const entries = Object.entries(bundles[code]);
    for (const [k, v] of entries) {
      if (String(v || "").trim() === source) return k;
    }
  }
  return null;
};

const resolveLocalizedLiteral = (literal: string, locale: LocaleCode): string => {
  const key = findKeyByLiteral(literal);
  if (!key) return literal;
  return bundles[locale]?.[key] ?? bundles.en?.[key] ?? literal;
};

export const localeLabelKey: Record<LocaleCode, string> = {
  en: "lang.english",
  fr: "lang.french",
  es: "lang.spanish",
  ja: "lang.japanese",
  "zh-CN": "lang.chinese_simplified",
  "zh-TW": "lang.chinese_traditional",
};

const normalizeLocale = (value?: string | null): LocaleCode => {
  const v = String(value || "").trim();
  if ((SUPPORTED_LOCALES as readonly string[]).includes(v)) return v as LocaleCode;
  if (v.toLowerCase().startsWith("fr")) return "fr";
  if (v.toLowerCase().startsWith("es")) return "es";
  if (v.toLowerCase().startsWith("ja")) return "ja";
  if (v.toLowerCase().startsWith("zh-cn") || v.toLowerCase().startsWith("zh-hans")) return "zh-CN";
  if (v.toLowerCase().startsWith("zh-tw") || v.toLowerCase().startsWith("zh-hant")) return "zh-TW";
  return "en";
};

type I18nContextType = {
  locale: LocaleCode;
  setLocale: (next: LocaleCode) => void;
  t: (key: string, params?: Record<string, string | number>) => string;
  locales: readonly LocaleCode[];
};

const I18nContext = React.createContext<I18nContextType>({
  locale: "en",
  setLocale: () => undefined,
  t: (key) => key,
  locales: SUPPORTED_LOCALES,
});

const format = (template: string, params?: Record<string, string | number>) => {
  if (!params) return template;
  return template.replace(/\{(\w+)\}/g, (_, name) => String(params[name] ?? `{${name}}`));
};

export const I18nProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [locale, setLocaleState] = React.useState<LocaleCode>(() => {
    const stored = typeof window !== "undefined" ? window.localStorage.getItem(STORAGE_KEY) : null;
    if (stored) return normalizeLocale(stored);
    const lang = typeof navigator !== "undefined" ? navigator.language : "en";
    return normalizeLocale(lang);
  });

  const setLocale = React.useCallback((next: LocaleCode) => {
    setLocaleState(next);
    try {
      window.localStorage.setItem(STORAGE_KEY, next);
    } catch (_err) {
      // ignore
    }
  }, []);

  const t = React.useCallback((key: string, params?: Record<string, string | number>) => {
    const msg = bundles[locale]?.[key] ?? bundles.en?.[key] ?? key;
    return format(msg, params);
  }, [locale]);

  const value = React.useMemo<I18nContextType>(() => ({ locale, setLocale, t, locales: SUPPORTED_LOCALES }), [locale, setLocale, t]);

  React.useEffect(() => {
    const applyNode = (root: Node) => {
      if (!(root instanceof HTMLElement) && !(root instanceof Document)) return;
      const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
      const textNodes: Text[] = [];
      while (walker.nextNode()) {
        const current = walker.currentNode as Text;
        if (!current?.nodeValue) continue;
        textNodes.push(current);
      }
      for (const node of textNodes) {
        const raw = node.nodeValue || "";
        const trimmed = raw.trim();
        if (!trimmed) continue;
        const translated = resolveLocalizedLiteral(trimmed, locale);
        if (translated !== trimmed) {
          node.nodeValue = raw.replace(trimmed, translated);
        }
      }
      const elements = (root instanceof Document ? root.body : root).querySelectorAll("*");
      elements.forEach((el) => {
        const input = el as HTMLInputElement;
        if (input.placeholder) {
          input.placeholder = resolveLocalizedLiteral(input.placeholder, locale);
        }
        if (el.getAttribute("aria-label")) {
          const label = el.getAttribute("aria-label") || "";
          el.setAttribute("aria-label", resolveLocalizedLiteral(label, locale));
        }
        if (el.getAttribute("title")) {
          const title = el.getAttribute("title") || "";
          el.setAttribute("title", resolveLocalizedLiteral(title, locale));
        }
      });
    };

    applyNode(document);
    const observer = new MutationObserver((mutations) => {
      for (const m of mutations) {
        m.addedNodes.forEach((n) => applyNode(n));
      }
    });
    observer.observe(document.body, { childList: true, subtree: true });
    return () => observer.disconnect();
  }, [locale]);

  return <I18nContext.Provider value={value}>{children}</I18nContext.Provider>;
};

export const useI18n = () => React.useContext(I18nContext);
