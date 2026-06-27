import React, { useEffect, useMemo, useState } from "react";
import { APP_TOAST_EVENT, openSocialDm, type AppToastKind, type AppToastPayload } from "@/services/toast";

type NotificationItem = {
  id: number;
  message: string;
  kind: AppToastKind;
  read: boolean;
  createdAt: string;
  action?: AppToastPayload["action"];
};

const STORAGE_KEY = "sklp_notification_center_v1";
const MAX_ITEMS = 200;

const NotificationCenter: React.FC = () => {
  const [open, setOpen] = useState(false);
  const [items, setItems] = useState<NotificationItem[]>([]);
  const [selectedId, setSelectedId] = useState<number | null>(null);

  useEffect(() => {
    try {
      const raw = window.localStorage.getItem(STORAGE_KEY);
      if (!raw) return;
      const parsed = JSON.parse(raw);
      if (!Array.isArray(parsed)) return;
      const hydrated: NotificationItem[] = parsed
        .filter((entry) => entry && typeof entry === "object")
        .map((entry) => ({
          id: Number(entry.id) || Date.now(),
          message: String(entry.message || "").trim(),
          kind: (entry.kind === "success" || entry.kind === "error" ? entry.kind : "info") as AppToastKind,
          read: Boolean(entry.read),
          createdAt: String(entry.createdAt || new Date().toISOString()),
          action: entry.action && typeof entry.action === "object" ? entry.action : undefined,
        }))
        .filter((entry) => entry.message);
      setItems(hydrated.slice(0, MAX_ITEMS));
    } catch {
      // ignore storage parsing errors
    }
  }, []);

  useEffect(() => {
    try {
      window.localStorage.setItem(STORAGE_KEY, JSON.stringify(items.slice(0, MAX_ITEMS)));
    } catch {
      // ignore storage write errors
    }
  }, [items]);

  useEffect(() => {
    const onToast = (event: Event) => {
      const custom = event as CustomEvent<AppToastPayload>;
      const detail = custom.detail || { message: "" };
      const text = String(detail.message || "").trim();
      if (!text) return;
      const id = Date.now() + Math.floor(Math.random() * 1000);
      const next: NotificationItem = {
        id,
        message: text,
        kind: (detail.kind || "info") as AppToastKind,
        read: false,
        createdAt: new Date().toISOString(),
        action: detail.action,
      };
      setItems((prev) => [next, ...prev].slice(0, MAX_ITEMS));
      if (selectedId === null) setSelectedId(id);
    };
    window.addEventListener(APP_TOAST_EVENT, onToast as EventListener);
    return () => window.removeEventListener(APP_TOAST_EVENT, onToast as EventListener);
  }, [selectedId]);

  const unreadCount = useMemo(() => items.reduce((acc, item) => acc + (item.read ? 0 : 1), 0), [items]);
  const selected = useMemo(
    () => items.find((item) => item.id === selectedId) || items[0] || null,
    [items, selectedId]
  );

  const markAllRead = () => {
    setItems([]);
    setSelectedId(null);
  };

  const selectItem = (id: number) => {
    const item = items.find((entry) => entry.id === id);
    if (item?.action?.type === "open_dm") {
      openSocialDm(item.action.userId, item.action.name);
      setOpen(false);
    }
    setItems((prev) => prev.filter((entry) => entry.id !== id));
    setSelectedId((current) => (current === id ? null : current));
  };

  return (
    <div className="sklp-notify-center">
      <button
        type="button"
        className={`sklp-notify-fab${open ? " open" : ""}${unreadCount > 0 ? " has-unread" : ""}`}
        onClick={() => setOpen((prev) => !prev)}
        aria-expanded={open}
        aria-controls="sklp-notify-panel"
      >
        <span className="sklp-notify-fab-label">Notifications</span>
        <span className="sklp-notify-fab-count">{unreadCount}</span>
      </button>

      {open && (
        <section id="sklp-notify-panel" className="sklp-notify-panel" aria-label="Notifications">
          <header className="sklp-notify-head">
            <div className="sklp-notify-title">Notifications</div>
            <button type="button" className="sklp-notify-markall" onClick={markAllRead} disabled={!items.length}>
              Clear Read
            </button>
          </header>

          <div className="sklp-notify-list" role="list">
            {!items.length && <div className="sklp-notify-empty">No notifications yet.</div>}
            {items.map((item) => (
              <button
                key={item.id}
                type="button"
                role="listitem"
                className={`sklp-notify-item${item.read ? " read" : " unread"}${selected?.id === item.id ? " active" : ""}`}
                onClick={() => selectItem(item.id)}
              >
                <span className={`sklp-notify-dot ${item.read ? "read" : "unread"}`} />
                <span className="sklp-notify-text">{item.message}</span>
              </button>
            ))}
          </div>

          <div className="sklp-notify-detail">
            {selected ? (
              <>
                <div className="sklp-notify-detail-meta">
                  <span className={`sklp-notify-kind ${selected.kind}`}>{selected.kind.toUpperCase()}</span>
                  <span>{new Date(selected.createdAt).toLocaleString()}</span>
                </div>
                <div className="sklp-notify-detail-body">{selected.message}</div>
              </>
            ) : (
              <div className="sklp-notify-empty">Select a notification to see full details.</div>
            )}
          </div>
        </section>
      )}
    </div>
  );
};

export default NotificationCenter;
