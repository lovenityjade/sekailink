export const publicAssetUrl = (assetPath: string): string => {
  const cleanPath = String(assetPath || "").replace(/^\/+/, "");
  const base = String(import.meta.env.BASE_URL || "./").replace(/\/?$/, "/");
  const relativePath = `${base}${cleanPath}`;
  if (typeof window !== "undefined" && window.location?.href) {
    return new URL(relativePath, window.location.href).toString();
  }
  return relativePath;
};
