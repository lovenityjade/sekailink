export type AppToastKind = "info" | "success" | "error";

export type AppToastPayload = {
  message: string;
  kind?: AppToastKind;
  durationMs?: number;
  sticky?: boolean;
};

export const APP_TOAST_EVENT = "sekailink:toast";

export const emitToast = (payload: AppToastPayload) => {
  if (typeof window === "undefined") return;
  window.dispatchEvent(new CustomEvent(APP_TOAST_EVENT, { detail: payload }));
};

