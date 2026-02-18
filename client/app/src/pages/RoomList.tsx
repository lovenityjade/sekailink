import React, { useCallback, useMemo, useState } from "react";
import { useNavigate } from "react-router-dom";
import { apiFetch, apiJson, apiUrl } from "../services/api";
import { useInterval } from "../hooks/useInterval";
import { useSfx } from "../hooks/useSfx";
import { formatLocalDateTime } from "../utils/time";
import { useI18n } from "../i18n";

interface LobbySummary {
  id: string;
  name: string;
  description?: string;
  owner?: string;
  is_private?: boolean;
  release_mode?: string;
  collect_mode?: string;
  remaining_mode?: string;
  countdown_mode?: string;
  item_cheat?: boolean;
  spoiler?: string;
  hint_cost?: number;
  plando_items?: boolean;
  plando_bosses?: boolean;
  plando_connections?: boolean;
  plando_texts?: boolean;
  allow_custom_yamls?: boolean;
  is_locked?: boolean;
  last_activity?: string;
  member_count?: number;
  message_count?: number;
  url?: string;
}

interface LobbyListResponse {
  lobbies: LobbySummary[];
}

const formatTimestamp = (value?: string) => {
  return formatLocalDateTime(value);
};

const formatMode = (mode?: string) => {
  if (!mode) return "Standard";
  if (mode === "auto") return "Auto";
  if (mode === "enabled") return "Enabled";
  if (mode === "disabled") return "Disabled";
  return mode.replace(/-/g, " ");
};

const HINT_COST_OPTIONS = [5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75];

const RoomListPage: React.FC = () => {
  const { t } = useI18n();
  const [lobbies, setLobbies] = useState<LobbySummary[]>([]);
  const [search, setSearch] = useState("");
  const [filter, setFilter] = useState<"all" | "open" | "private">("all");
  const [error, setError] = useState<string>("");
  const [modalOpen, setModalOpen] = useState(false);
  const [infoOpen, setInfoOpen] = useState(false);
  const [infoLobby, setInfoLobby] = useState<LobbySummary | null>(null);
  const [toast, setToast] = useState<string>("");
  const sfx = useSfx();
  const navigate = useNavigate();

  const loadLobbies = useCallback(async () => {
    try {
      const data = await apiJson<LobbyListResponse>("/api/lobbies");
      setLobbies(data.lobbies || []);
      setError("");
    } catch (err) {
      setError(err instanceof Error ? err.message : t("roomlist.failed_load"));
    }
  }, []);

  useInterval(loadLobbies, 20000);

  React.useEffect(() => {
    loadLobbies();
  }, [loadLobbies]);

  const filtered = useMemo(() => {
    const term = search.toLowerCase().trim();
    return lobbies.filter((lobby) => {
      if (filter === "open" && lobby.is_private) return false;
      if (filter === "private" && !lobby.is_private) return false;
      if (!term) return true;
      const name = (lobby.name || "").toLowerCase();
      const owner = (lobby.owner || "").toLowerCase();
      return name.includes(term) || owner.includes(term);
    });
  }, [lobbies, search, filter]);

  const openInfo = (lobby: LobbySummary) => {
    sfx.play("confirm", 0.2);
    setInfoLobby(lobby);
    setInfoOpen(true);
  };

  const showToast = useCallback((message: string, kind: "success" | "error" = "error") => {
    if (!message) return;
    sfx.play(kind === "error" ? "error" : "success", kind === "error" ? 0.35 : 0.25);
    setToast(message);
    window.setTimeout(() => setToast(""), 2800);
  }, [sfx]);

  const joinLobby = useCallback(async (lobby: LobbySummary) => {
    const payload: Record<string, unknown> = {};
    if (lobby.is_private) {
      const password = window.prompt("Lobby password");
      if (password === null) return;
      payload.password = password;
    }
    const doJoin = async (forceAbandon: boolean) => {
      const response = await apiFetch(`/api/lobbies/${lobby.id}/join`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ ...payload, force_abandon: forceAbandon })
      });
      const data = await response.json().catch(() => ({}));
      if (!response.ok) {
        const error = String(data.error || "Unable to join lobby.");
        const reason = String(data.reason || "");
        const needsConfirm = data.requires_confirmation === true || reason === "active_game_conflict";
        return { ok: false as const, error, reason, needsConfirm };
      }
      return { ok: true as const };
    };

    const first = await doJoin(false);
    if (!first.ok && first.needsConfirm) {
      const confirmMsg = `${first.error}\n\nContinue and slow-release the previous room?`;
      if (!window.confirm(confirmMsg)) return;
      const second = await doJoin(true);
      if (!second.ok) throw new Error(second.error);
    } else if (!first.ok) {
      throw new Error(first.error);
    }

    sfx.play("confirm", 0.2);
    navigate(`/lobby/${lobby.id}`);
  }, [navigate, sfx, t]);

  const onCreateSubmit = async (event: React.FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    setError("");
    const formData = new FormData(event.currentTarget);
    const payload = {
      name: (formData.get("name") || "").toString().trim(),
      description: (formData.get("description") || "").toString().trim(),
      release_mode: (formData.get("release_mode") || "").toString(),
      collect_mode: (formData.get("collect_mode") || "").toString(),
      remaining_mode: (formData.get("remaining_mode") || "").toString(),
      countdown_mode: (formData.get("countdown_mode") || "").toString(),
      item_cheat: formData.get("item_cheat") === "1",
      spoiler: (formData.get("spoiler") || "0").toString(),
      hint_cost: (formData.get("hint_cost") || "5").toString(),
      max_players: (formData.get("max_players") || "50").toString(),
      server_password: (formData.get("server_password") || "").toString(),
      plando_items: formData.get("plando_items") === "on",
      plando_bosses: formData.get("plando_bosses") === "on",
      plando_connections: formData.get("plando_connections") === "on",
      plando_texts: formData.get("plando_texts") === "on",
      allow_custom_yamls: formData.get("allow_custom_yamls") !== "0"
    };

    try {
      const response = await apiFetch("/api/lobbies", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload)
      });
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        if (response.status === 401 || response.status === 403) {
          throw new Error(t("roomlist.login_required_create"));
        }
        throw new Error(data.error || t("roomlist.failed_create"));
      }
      const data = await response.json();
      if (data.url) {
        const lobbyPath = new URL(data.url, apiUrl("/")).pathname;
        const lobbyId = lobbyPath.split("/").filter(Boolean).pop();
        if (lobbyId) {
          navigate(`/lobby/${lobbyId}`);
          return;
        }
        window.location.hash = `#${lobbyPath}`;
        return;
      }
      sfx.play("confirm", 0.4);
      setModalOpen(false);
      await loadLobbies();
    } catch (err) {
      showToast(err instanceof Error ? err.message : t("roomlist.failed_create"));
    }
  };

  return (
    <div className="skl-roomlist-page">
      <div className="skl-app-header">
        <div>
          <p className="skl-eyebrow">{t("roomlist.eyebrow")}</p>
          <h1 className="skl-app-title">{t("roomlist.title")}</h1>
        </div>
        <button className="skl-btn primary" type="button" onClick={() => { sfx.play("confirm", 0.25); setModalOpen(true); }}>
          {t("roomlist.create_room")}
        </button>
      </div>

      <div className="skl-app-panel">
        <div className="skl-roomlist-bar">
          <div className="skl-roomlist-search">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
              <circle cx="11" cy="11" r="8" />
              <line x1="21" y1="21" x2="16.65" y2="16.65" />
            </svg>
            <input
              id="lobby-search"
              type="search"
              placeholder={t("roomlist.search")}
              className="input"
              value={search}
              onChange={(event) => setSearch(event.target.value)}
            />
          </div>
          <div className="skl-roomlist-filters">
            {([
              { id: "all", label: t("roomlist.filter.all") },
              { id: "open", label: t("roomlist.filter.open") },
              { id: "private", label: t("roomlist.filter.private") }
            ] as const).map((item) => (
              <span
                key={item.id}
                className={`pill skl-filter-chip${filter === item.id ? " ok active" : ""}`}
                onClick={() => setFilter(item.id)}
              >
                {item.label}
              </span>
            ))}
          </div>
        </div>

        <div className="skl-room-table-header">
          <div className="skl-room-col room">{t("roomlist.col.room")}</div>
          <div className="skl-room-col">{t("roomlist.col.players")}</div>
          <div className="skl-room-col">{t("roomlist.col.access")}</div>
          <div className="skl-room-col">{t("roomlist.col.mode")}</div>
          <div className="skl-room-col">{t("roomlist.col.progress")}</div>
          <div className="skl-room-col actions">{t("roomlist.col.actions")}</div>
        </div>

        <div id="lobby-list" className="skl-room-table">
          {filtered.map((lobby) => {
            return (
              <div className="skl-lobby-card" key={lobby.id}>
                <div className="skl-lobby-card-info">
                  <div className="skl-room-title">
                    <div className="skl-room-avatar">
                      {(lobby.name || "S").trim().slice(0, 1).toUpperCase()}
                    </div>
                    <h3>
                      {lobby.name}
                      {lobby.is_private && <span className="skl-lobby-private">{t("roomlist.private")}</span>}
                    </h3>
                  </div>
                  <p>{lobby.description || t("roomlist.no_description")}</p>
                  <p>
                    {t("roomlist.owner")}: {lobby.owner || t("roomlist.unknown")} · {t("roomlist.last_active")}: {formatTimestamp(lobby.last_activity) || "—"}
                  </p>
                </div>
                <div className="skl-room-col-value">{lobby.member_count || 0} {t("roomlist.players_suffix")}</div>
                <div className="skl-room-col-value dim">{lobby.is_private ? t("roomlist.private") : t("roomlist.public")}</div>
                <div className="skl-room-col-value dim">{formatMode(lobby.countdown_mode)}</div>
                <div className="skl-room-col-value dim">{lobby.message_count ? t("roomlist.in_progress") : t("roomlist.no_progress")}</div>
                <div className="skl-lobby-actions">
                  <button
                    className="skl-btn ghost"
                    type="button"
                    onClick={() => {
                      joinLobby(lobby).catch((err) => {
                        showToast(err instanceof Error ? err.message : "Unable to join lobby.");
                      });
                    }}
                  >
                    {t("roomlist.open_lobby")}
                  </button>
                  <button
                    className="skl-btn ghost"
                    type="button"
                    onClick={() => {
                      if (!navigator.clipboard) return;
                      const fullUrl = lobby.url ? new URL(lobby.url, apiUrl("/")).toString() : "";
                      navigator.clipboard.writeText(fullUrl);
                      sfx.play("confirm", 0.2);
                      showToast(t("roomlist.copied_link"), "success");
                    }}
                  >
                    {t("roomlist.copy_link")}
                  </button>
                  <button className="skl-btn ghost" type="button" onClick={() => openInfo(lobby)}>
                    {t("roomlist.room_info")}
                  </button>
                </div>
              </div>
            );
          })}
        </div>

        {!filtered.length && (
          <div id="lobby-empty" className="skl-room-empty">
            <div className="skl-room-empty-icon">
              <svg width="48" height="48" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1" strokeLinecap="round" strokeLinejoin="round">
                <rect x="2" y="3" width="20" height="14" rx="2" ry="2" />
                <line x1="8" y1="21" x2="16" y2="21" />
                <line x1="12" y1="17" x2="12" y2="21" />
              </svg>
            </div>
            <p className="skl-room-empty-title">{t("roomlist.no_rooms")}</p>
          </div>
        )}

        {error && <div id="lobby-error" className="skl-lobby-error show">{error}</div>}
      </div>

      {toast && (
        <div className="skl-toast-stack" aria-live="polite">
          <div className="skl-toast">{toast}</div>
        </div>
      )}

      <div className={`skl-modal${modalOpen ? " open" : ""}`} id="lobby-create-modal" aria-hidden={!modalOpen}>
        <div className="skl-modal-backdrop" onClick={() => { sfx.play("confirm", 0.2); setModalOpen(false); }}></div>
        <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-create-title">
          <div className="skl-modal-header">
            <h3 id="lobby-create-title">{t("roomlist.create_title")}</h3>
            <button className="skl-modal-close" type="button" onClick={() => { sfx.play("confirm", 0.2); setModalOpen(false); }}>{t("common.close")}</button>
          </div>
          <form id="lobby-create-form" className="skl-lobby-form" onSubmit={onCreateSubmit}>
            <div className="skl-lobby-form-row">
              <input type="text" name="name" placeholder={t("roomlist.room_name")} maxLength={60} className="input" />
              <input type="text" name="description" placeholder={t("roomlist.short_description")} maxLength={180} className="input" />
            </div>
            <div className="skl-lobby-form-rules">
              <div className="skl-lobby-form-title">{t("roomlist.room_rules")}</div>
              <div className="skl-lobby-grid-fields">
                <label>
                  Release Permission
                  <select name="release_mode">
                    <option value="disabled">Disabled</option>
                    <option value="enabled" defaultValue="enabled">Manual !release</option>
                    <option value="goal">After goal completion</option>
                    <option value="auto">Auto</option>
                    <option value="auto-enabled">Auto + Manual</option>
                  </select>
                </label>
                <label>
                  Collect Permission
                  <select name="collect_mode" defaultValue="goal">
                    <option value="disabled">Disabled</option>
                    <option value="enabled">Manual !collect</option>
                    <option value="goal">Allow after goal completion</option>
                    <option value="auto">Auto</option>
                    <option value="auto-enabled">Auto + Manual</option>
                  </select>
                </label>
                <label>
                  Remaining Permission
                  <select name="remaining_mode" defaultValue="enabled">
                    <option value="disabled">Disabled</option>
                    <option value="enabled">Manual !remaining</option>
                    <option value="goal">After goal completion</option>
                  </select>
                </label>
                <label>
                  Countdown Permission
                  <select name="countdown_mode" defaultValue="auto">
                    <option value="auto">Auto (under 30 slots)</option>
                    <option value="enabled">Enabled</option>
                    <option value="disabled">Disabled</option>
                  </select>
                </label>
                <label>
                  Item Cheat
                  <select name="item_cheat" defaultValue="0">
                    <option value="0">Disabled</option>
                    <option value="1">Enabled</option>
                  </select>
                </label>
                <label>
                  Spoiler Log
                  <select name="spoiler" defaultValue="0">
                    <option value="0">Disabled</option>
                    <option value="1">Basic</option>
                    <option value="2">Playthrough</option>
                    <option value="3">Full</option>
                  </select>
                </label>
                <label>
                  Hint Cost (%)
                  <select name="hint_cost" defaultValue="5">
                    {HINT_COST_OPTIONS.map((value) => (
                      <option key={value} value={value}>{value}%</option>
                    ))}
                  </select>
                </label>
                <label>
                  Max Players
                  <input type="number" name="max_players" min={1} max={50} defaultValue={50} className="input" />
                </label>
                <label>
                  Custom YAMLs
                  <select name="allow_custom_yamls" defaultValue="1">
                    <option value="1">Allowed</option>
                    <option value="0">Blocked</option>
                  </select>
                </label>
                <label>
                  Server Password (Private)
                  <input type="text" name="server_password" placeholder="Leave empty for public" maxLength={64} className="input" />
                </label>
              </div>
              <div className="skl-lobby-plando">
                <span>Plando Options</span>
                <label><input type="checkbox" name="plando_items" defaultChecked /> Items</label>
                <label><input type="checkbox" name="plando_bosses" defaultChecked /> Bosses</label>
                <label><input type="checkbox" name="plando_connections" defaultChecked /> Connections</label>
                <label><input type="checkbox" name="plando_texts" defaultChecked /> Text</label>
              </div>
            </div>
            <div className="skl-modal-actions">
              <button className="skl-btn primary" type="submit">Create Room</button>
            </div>
          </form>
        </div>
      </div>

      <div className={`skl-modal${infoOpen ? " open" : ""}`} id="lobby-info-modal" aria-hidden={!infoOpen}>
        <div className="skl-modal-backdrop" onClick={() => { sfx.play("confirm", 0.2); setInfoOpen(false); }}></div>
        <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-info-title">
          <div className="skl-modal-header">
            <h3 id="lobby-info-title">Room info</h3>
            <button className="skl-modal-close" type="button" onClick={() => { sfx.play("confirm", 0.2); setInfoOpen(false); }}>Close</button>
          </div>
          <div className="skl-roominfo-grid" id="lobby-info-content">
            {infoLobby && (
              <>
                <div className="skl-roominfo-row"><span>Name</span><span>{infoLobby.name}</span></div>
                <div className="skl-roominfo-row"><span>Owner</span><span>{infoLobby.owner || "Unknown"}</span></div>
                <div className="skl-roominfo-row"><span>Access</span><span>{infoLobby.is_private ? "Private" : "Public"}</span></div>
                <div className="skl-roominfo-row"><span>Release</span><span>{infoLobby.release_mode || "—"}</span></div>
                <div className="skl-roominfo-row"><span>Collect</span><span>{infoLobby.collect_mode || "—"}</span></div>
                <div className="skl-roominfo-row"><span>Remaining</span><span>{infoLobby.remaining_mode || "—"}</span></div>
                <div className="skl-roominfo-row"><span>Countdown</span><span>{infoLobby.countdown_mode || "—"}</span></div>
                <div className="skl-roominfo-row"><span>Item cheat</span><span>{infoLobby.item_cheat ? "Enabled" : "Disabled"}</span></div>
                <div className="skl-roominfo-row"><span>Spoiler log</span><span>{infoLobby.spoiler || "—"}</span></div>
                <div className="skl-roominfo-row"><span>Hint cost</span><span>{infoLobby.hint_cost ?? 0}%</span></div>
                <div className="skl-roominfo-row"><span>Plando items</span><span>{infoLobby.plando_items ? "On" : "Off"}</span></div>
                <div className="skl-roominfo-row"><span>Plando bosses</span><span>{infoLobby.plando_bosses ? "On" : "Off"}</span></div>
                <div className="skl-roominfo-row"><span>Plando connections</span><span>{infoLobby.plando_connections ? "On" : "Off"}</span></div>
                <div className="skl-roominfo-row"><span>Plando texts</span><span>{infoLobby.plando_texts ? "On" : "Off"}</span></div>
                <div className="skl-roominfo-row"><span>Custom YAMLs</span><span>{infoLobby.allow_custom_yamls ? "Allowed" : "Blocked"}</span></div>
              </>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

export default RoomListPage;
