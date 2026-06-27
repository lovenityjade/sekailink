export type AppToastKind = "info" | "success" | "error";

export type AppToastPayload = {
  message: string;
  kind?: AppToastKind;
  durationMs?: number;
  sticky?: boolean;
  action?: {
    type: "open_dm";
    userId: string;
    name?: string;
  };
};

export const APP_TOAST_EVENT = "sekailink:toast";
export const SOCIAL_OPEN_DM_EVENT = "sekailink:social-open-dm";
export const UPDATE_AVAILABLE_EVENT = "sekailink:update-available";

export const emitToast = (payload: AppToastPayload) => {
  if (typeof window === "undefined") return;
  window.dispatchEvent(new CustomEvent(APP_TOAST_EVENT, { detail: payload }));
};

export const openSocialDm = (userId: string, name?: string) => {
  if (typeof window === "undefined") return;
  const cleanUserId = String(userId || "").trim();
  if (!cleanUserId) return;
  window.dispatchEvent(
    new CustomEvent(SOCIAL_OPEN_DM_EVENT, {
      detail: {
        userId: cleanUserId,
        name: String(name || "").trim(),
      },
    })
  );
};

export const emitUpdateAvailable = (payload: { version?: string; message?: string } = {}) => {
  if (typeof window === "undefined") return;
  window.dispatchEvent(new CustomEvent(UPDATE_AVAILABLE_EVENT, { detail: payload }));
};
