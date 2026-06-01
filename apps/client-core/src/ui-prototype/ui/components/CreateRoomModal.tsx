import React, { useState } from "react";
import { useNavigate } from "react-router-dom";
import { useI18n } from "@/i18n";
import { useSfx } from "@/hooks/useSfx";
import { apiFetch } from "@/services/api";
import { emitToast } from "@/services/toast";

type Props = {
  open: boolean;
  onClose: () => void;
  onCreated: () => void;
};

const SPOILER_OPTIONS = [
  { value: 0, label: "Off" },
  { value: 1, label: "Basic" },
  { value: 2, label: "Detailed" },
  { value: 3, label: "Full" },
] as const;

const CreateRoomModal: React.FC<Props> = ({ open, onClose, onCreated }) => {
  const { t } = useI18n();
  const sfx = useSfx();
  const navigate = useNavigate();
  const [name, setName] = useState("");
  const [description, setDescription] = useState("");
  const [password, setPassword] = useState("");
  const [itemCheat, setItemCheat] = useState(false);
  const [spoiler, setSpoiler] = useState<number>(0);
  const [maxPlayers, setMaxPlayers] = useState<number>(50);
  const [submitting, setSubmitting] = useState(false);
  const [error, setError] = useState("");

  const submit = async () => {
    if (!name.trim()) { setError("Room name is required."); return; }
    setSubmitting(true);
    setError("");
    try {
      const res = await apiFetch("/api/lobbies", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          name: name.trim(),
          description: description.trim(),
          server_password: password.trim() || "",
          max_players: Math.min(50, Math.max(1, Number(maxPlayers) || 50)),
          is_solo: false,
          countdown_mode: "auto",
          item_cheat: itemCheat,
          spoiler,
        }),
      });
      if (!res.ok) {
        const data: any = await res.json().catch(() => ({}));
        if (res.status === 401) {
          throw new Error("Your session is no longer valid. Log in again, then retry room creation.");
        }
        if (res.status === 403) {
          throw new Error(data?.error || "The server refused this desktop action. Retry after the client reconnects.");
        }
        throw new Error(data?.error || "Failed to create room.");
      }
      const data: any = await res.json().catch(() => ({}));
      sfx.play("success", 0.2);
      emitToast({ message: `Room "${name.trim()}" created.`, kind: "success" });
      onCreated();
      onClose();
      setName("");
      setDescription("");
      setPassword("");
      setItemCheat(false);
      setSpoiler(0);
      setMaxPlayers(50);
      if (data?.url) {
        try {
          const path = new URL(String(data.url), window.location.origin).pathname;
          navigate(path);
          return;
        } catch {
          // Ignore malformed room path and stay on dashboard.
        }
      }
    } catch (err) {
      sfx.play("error", 0.2);
      setError(err instanceof Error ? err.message : "Failed to create room.");
    } finally {
      setSubmitting(false);
    }
  };

  if (!open) return null;

  return (
    <div className="fixed inset-0 z-[55] flex items-center justify-center" role="dialog" aria-modal="true">
      <div className="absolute inset-0 bg-black/70 backdrop-blur-sm" onClick={onClose} />
      <div className="relative w-[480px] max-h-[80vh] overflow-y-auto panel-chamfer bg-gunmetal border border-teal/20 p-6" style={{ boxShadow: "0 0 50px rgba(0,255,200,0.08)" }}>
        {/* Header */}
        <div className="flex items-center justify-between mb-5">
          <h2 className="font-header text-sm text-teal tracking-[0.2em] text-glow">CREATE SYNC</h2>
          <button type="button" className="text-phosphor/25 hover:text-teal transition-colors text-xs font-header tracking-wider" onClick={onClose}>ESC</button>
        </div>

        {/* Form */}
        <div className="space-y-3">
          <div>
            <label className="block text-[10px] font-header text-phosphor/25 tracking-widest mb-1">{t("roomlist.col.room").toUpperCase()} NAME</label>
            <input type="text" className="w-full h-9 bg-void/60 border border-teal/10 panel-chamfer-sm px-3 text-xs text-phosphor font-code placeholder:text-phosphor/15 focus:outline-none focus:border-teal/25 transition-colors" value={name} onChange={(e) => setName(e.target.value)} placeholder="Enter room name..." autoFocus />
          </div>

          <div>
            <label className="block text-[10px] font-header text-phosphor/25 tracking-widest mb-1">DESCRIPTION</label>
            <input type="text" className="w-full h-9 bg-void/60 border border-teal/10 panel-chamfer-sm px-3 text-xs text-phosphor font-code placeholder:text-phosphor/15 focus:outline-none focus:border-teal/25 transition-colors" value={description} onChange={(e) => setDescription(e.target.value)} placeholder="Short room description..." />
          </div>

          <div>
            <label className="block text-[10px] font-header text-phosphor/25 tracking-widest mb-1">PASSWORD (OPTIONAL)</label>
            <input type="password" className="w-full h-9 bg-void/60 border border-teal/10 panel-chamfer-sm px-3 text-xs text-phosphor font-code placeholder:text-phosphor/15 focus:outline-none focus:border-teal/25 transition-colors" value={password} onChange={(e) => setPassword(e.target.value)} placeholder="Leave blank for public" />
          </div>

          <div className="panel-chamfer-sm border border-teal/10 bg-black/20 px-3 py-2">
            <div className="text-[10px] font-header text-phosphor/25 tracking-widest mb-1">ROOM-LOCAL CONFIG</div>
            <div className="text-[11px] font-code text-phosphor/55">
              Games and configs are selected after entering the room.
              <div className="text-phosphor/25 mt-1">
                Players can add or remove selections until Ready -&gt; Generation locks the room snapshot.
              </div>
            </div>
          </div>

          <div>
            <span className="block text-[10px] font-header text-phosphor/25 tracking-widest mb-2">SYNC SETTINGS</span>
            <div className="grid grid-cols-2 gap-2">
              <label className="block">
                <span className="block text-[10px] font-header text-phosphor/25 tracking-widest mb-1">MAX PLAYERS</span>
                <input type="number" min={1} max={50} className="w-full h-9 bg-void/60 border border-teal/10 panel-chamfer-sm px-3 text-xs text-phosphor font-code focus:outline-none focus:border-teal/25 disabled:opacity-40" value={maxPlayers} onChange={(e) => setMaxPlayers(Number(e.target.value) || 50)} />
              </label>
              <label className="block">
                <span className="block text-[10px] font-header text-phosphor/25 tracking-widest mb-1">SPOILER</span>
                <select className="w-full h-9 bg-void/60 border border-teal/10 panel-chamfer-sm px-2 text-xs text-phosphor font-code focus:outline-none focus:border-teal/25" value={String(spoiler)} onChange={(e) => setSpoiler(Number(e.target.value) || 0)}>
                  {SPOILER_OPTIONS.map((option) => <option key={option.value} value={option.value}>{option.label}</option>)}
                </select>
              </label>
            </div>
          </div>

          <div>
            <span className="block text-[10px] font-header text-phosphor/25 tracking-widest mb-2">HOST TOOLS</span>
            <div className="grid grid-cols-1 gap-1.5">
              {[
                { label: "Item Cheat", value: itemCheat, setValue: setItemCheat, note: "Testing helper for the host during showcase validation." },
              ].map((item) => (
                <button
                  key={item.label}
                  type="button"
                  onClick={() => item.setValue((prev: boolean) => !prev)}
                  className={`text-left px-3 py-2 panel-chamfer-sm border transition-all ${item.value ? "bg-teal/10 border-teal/20 text-phosphor/60" : "bg-void/30 border-teal/5 text-phosphor/25 hover:border-teal/10"}`}
                >
                  <div className="text-[10px] font-header tracking-wider">{item.label}</div>
                  <div className="text-[9px] font-code text-phosphor/15 mt-0.5">{item.value ? "Enabled" : "Disabled"} · {item.note}</div>
                </button>
              ))}
            </div>
          </div>

          {error && <div className="text-xs text-magenta font-code">{error}</div>}

          {/* Actions */}
          <div className="flex justify-end gap-2 pt-2">
            <button type="button" className="h-9 px-4 panel-chamfer-sm bg-void/30 border border-teal/10 text-phosphor/30 text-[10px] font-header tracking-widest hover:text-phosphor/50 transition-colors" onClick={onClose}>
              {t("modal.cancel").toUpperCase()}
            </button>
            <button type="button" disabled={submitting} className="h-9 px-5 panel-chamfer-sm bg-teal/10 border border-teal/25 text-teal text-[10px] font-header tracking-widest hover:bg-teal/20 transition-all btn-charge disabled:opacity-30" onClick={() => void submit()}>
              {submitting ? "CREATING..." : "CREATE SYNC"}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};

export default CreateRoomModal;
