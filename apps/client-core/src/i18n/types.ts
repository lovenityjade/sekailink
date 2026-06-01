export const SUPPORTED_LOCALES = ["en", "fr", "es", "ja", "zh-CN", "zh-TW"] as const;
export type LocaleCode = (typeof SUPPORTED_LOCALES)[number];

export type Messages = Record<string, string>;
