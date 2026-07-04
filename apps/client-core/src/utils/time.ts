export const parseServerDate = (value?: string | null): Date | null => {
  if (!value) return null;
  const raw = String(value).trim();
  if (!raw) return null;
  const hasZone = /(?:Z|[+-]\d{2}:?\d{2})$/i.test(raw);
  const normalized = hasZone ? raw : `${raw}Z`;
  const date = new Date(normalized);
  if (Number.isNaN(date.getTime())) return null;
  return date;
};

export const formatLocalTime = (value?: string | null) => {
  const d = parseServerDate(value);
  if (!d) return "";
  return d.toLocaleTimeString(undefined, { hour: "2-digit", minute: "2-digit" });
};

export const formatLocalDateTime = (value?: string | null) => {
  const d = parseServerDate(value);
  if (!d) return "";
  return d.toLocaleString(undefined, { dateStyle: "medium", timeStyle: "short" });
};
