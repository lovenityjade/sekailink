import React, { useEffect, useMemo, useState } from "react";
import { APP_TOAST_EVENT, type AppToastKind, type AppToastPayload } from "@/services/toast";

type ToastItem = {
  id: number;
  message: string;
  kind: AppToastKind;
  sticky: boolean;
};

const ToastHost: React.FC = () => {
  const [toasts, setToasts] = useState<ToastItem[]>([]);

  useEffect(() => {
    const onToast = (event: Event) => {
      const custom = event as CustomEvent<AppToastPayload>;
      const detail = custom.detail || { message: "" };
      const text = String(detail.message || "").trim();
      if (!text) return;
      const id = Date.now() + Math.floor(Math.random() * 999);
      const item: ToastItem = {
        id,
        message: text,
        kind: (detail.kind || "info") as AppToastKind,
        sticky: Boolean(detail.sticky),
      };
      setToasts((prev) => [...prev.slice(-5), item]);
      if (!item.sticky) {
        const duration = Math.max(2500, Number(detail.durationMs || 9000));
        window.setTimeout(() => {
          setToasts((prev) => prev.filter((t) => t.id !== id));
        }, duration);
      }
    };

    window.addEventListener(APP_TOAST_EVENT, onToast as EventListener);
    return () => window.removeEventListener(APP_TOAST_EVENT, onToast as EventListener);
  }, []);

  const visible = useMemo(() => toasts.slice(-6), [toasts]);
  if (!visible.length) return null;

  return (
    <div className="skl-toast-stack sklp-toast-stack" aria-live="polite" aria-atomic="false">
      {visible.map((toast) => (
        <div key={toast.id} className={`skl-toast ${toast.kind === "error" ? "error" : toast.kind === "success" ? "success" : ""}`}>
          <div className="sklp-toast-row">
            <span className="sklp-toast-text">{toast.message}</span>
            <button
              type="button"
              className="sklp-toast-close"
              aria-label="Dismiss notification"
              onClick={() => setToasts((prev) => prev.filter((t) => t.id !== toast.id))}
            >
              ×
            </button>
          </div>
        </div>
      ))}
    </div>
  );
};

export default ToastHost;

