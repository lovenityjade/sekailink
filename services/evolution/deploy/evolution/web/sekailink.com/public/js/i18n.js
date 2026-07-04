/**
 * SekaiLink i18n — détection (navigateur), localStorage, ?lang=, sélecteur
 */
(function (global) {
  const STORAGE_KEY = "sekailink-lang";
  const LANG_CODES = ["fr", "en", "es", "ja", "zh-Hans", "zh-Hant", "ko", "de", "pt", "it"];

  function normalizeBrowserTag(tag) {
    if (!tag || typeof tag !== "string") return null;
    const t = tag.toLowerCase().replace(/_/g, "-");
    if (t.startsWith("zh")) {
      if (t === "zh-tw" || t === "zh-hk" || t === "zh-mo" || t.includes("hant")) return "zh-Hant";
      return "zh-Hans";
    }
    const base = t.split("-")[0];
    const map = { en: "en", es: "es", ja: "ja", ko: "ko", de: "de", fr: "fr", it: "it", pt: "pt" };
    return map[base] || null;
  }

  function readStoredLang() {
    try {
      const v = localStorage.getItem(STORAGE_KEY);
      if (v && LANG_CODES.includes(v)) return v;
    } catch (e) {}
    return null;
  }

  function detectLang() {
    const stored = readStoredLang();
    if (stored) return stored;
    const list =
      typeof navigator !== "undefined" && navigator.languages && navigator.languages.length
        ? navigator.languages
        : [navigator.language || "fr"];
    for (let i = 0; i < list.length; i++) {
      const n = normalizeBrowserTag(list[i]);
      if (n && LANG_CODES.includes(n)) return n;
    }
    return "fr";
  }

  function pack(locale) {
    if (!locale) return { s: {}, features: [] };
    if (locale.s && Array.isArray(locale.features)) return locale;
    if (locale.s) return { s: locale.s, features: locale.features || [] };
    return { s: locale, features: locale.features || [] };
  }

  let current = "fr";

  function localeData(code) {
    const LO = global.SEKAILINK_LOCALES;
    if (!LO) return { s: {}, features: [] };
    return pack(LO[code] || LO.fr);
  }

  function t(key) {
    const cur = localeData(current);
    if (cur.s[key] != null) return cur.s[key];
    const fr = localeData("fr");
    if (fr.s[key] != null) return fr.s[key];
    return key;
  }

  function featuresList() {
    const cur = localeData(current);
    if (cur.features && cur.features.length) return cur.features;
    return localeData("fr").features || [];
  }

  function htmlLang(code) {
    if (code === "zh-Hans") return "zh-Hans";
    if (code === "zh-Hant") return "zh-Hant";
    return code;
  }

  function setLang(code) {
    if (!LANG_CODES.includes(code)) return;
    current = code;
    try {
      localStorage.setItem(STORAGE_KEY, code);
    } catch (e) {}
    document.documentElement.setAttribute("lang", htmlLang(code));
    document.documentElement.setAttribute("data-lang", code);
    applyTranslations();
    const sel = document.getElementById("lang-select");
    if (sel) sel.value = code;
    global.dispatchEvent(new CustomEvent("sekailink:languagechange", { detail: { lang: code } }));
  }

  function applyTranslations() {
    document.querySelectorAll("[data-i18n]").forEach((el) => {
      const key = el.getAttribute("data-i18n");
      if (key) el.textContent = t(key);
    });
    document.querySelectorAll("[data-i18n-html]").forEach((el) => {
      const key = el.getAttribute("data-i18n-html");
      if (key) el.innerHTML = t(key);
    });

    const metaD = document.querySelector('meta[name="description"]');
    if (metaD) metaD.setAttribute("content", t("meta.desc"));

    document.title = t("meta.title");

    const nav = document.getElementById("site-nav");
    if (nav) nav.setAttribute("aria-label", t("nav_aria"));

    const langSel = document.getElementById("lang-select");
    if (langSel) langSel.setAttribute("aria-label", t("lang_aria"));

    const y = new Date().getFullYear();
    const foot = document.getElementById("foot-line");
    if (foot) foot.innerHTML = t("footer.html").replace(/\{\{year\}\}/g, String(y));

    const tierList = document.querySelector(".tier-switch");
    if (tierList) tierList.setAttribute("aria-label", t("patreon.tier_aria"));

    document.querySelectorAll("[data-i18n-aria]").forEach((el) => {
      const key = el.getAttribute("data-i18n-aria");
      if (key) el.setAttribute("aria-label", t(key));
    });
  }

  function init() {
    const param = new URLSearchParams(global.location.search).get("lang");
    if (param && LANG_CODES.includes(param)) {
      try {
        localStorage.setItem(STORAGE_KEY, param);
      } catch (e) {}
      current = param;
    } else {
      current = detectLang();
    }

    document.documentElement.setAttribute("lang", htmlLang(current));
    document.documentElement.setAttribute("data-lang", current);

    const sel = document.getElementById("lang-select");
    if (sel) {
      sel.value = current;
      sel.addEventListener("change", function () {
        setLang(sel.value);
      });
    }

    applyTranslations();
  }

  global.SekaiLinkI18n = {
    init,
    t,
    featuresList,
    getLang: function () {
      return current;
    },
    setLang,
    LANG_CODES,
    STORAGE_KEY,
  };
})(typeof window !== "undefined" ? window : this);
