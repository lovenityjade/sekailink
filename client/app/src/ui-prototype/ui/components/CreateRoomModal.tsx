import React, { useState } from "react";
import { useNavigate } from "react-router-dom";
import { apiFetch, apiUrl } from "@/services/api";
import { useSfx } from "@/hooks/useSfx";
import { emitToast } from "@/services/toast";
import { markSoloDescription } from "@/utils/soloLobby";

type Props = {
  open: boolean;
  onClose: () => void;
  onCreated?: () => void;
};

const CreateRoomModal: React.FC<Props> = ({ open, onClose, onCreated }) => {
  const sfx = useSfx();
  const navigate = useNavigate();
  const [error, setError] = useState("");
  const [soloMode, setSoloMode] = useState(false);
  const hintCostOptions = [5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75];

  const onCreateSubmit = async (event: React.FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    setError("");
    const formData = new FormData(event.currentTarget);
    const roomName = (formData.get("name") || "").toString().trim();
    const roomDescription = (formData.get("description") || "").toString().trim();
    const payload = {
      name: roomName || (soloMode ? "Solo Session" : ""),
      description: soloMode ? markSoloDescription(roomDescription) : roomDescription,
      release_mode: (formData.get("release_mode") || "goal").toString(),
      collect_mode: "goal",
      remaining_mode: "enabled",
      countdown_mode: "auto",
      slow_release_timeout: (formData.get("slow_release_timeout") || "1800").toString(),
      item_cheat: formData.get("item_cheat") === "1",
      spoiler: (formData.get("spoiler") || "0").toString(),
      hint_cost: (formData.get("hint_cost") || "5").toString(),
      max_players: soloMode ? "1" : (formData.get("max_players") || "50").toString(),
      server_password: (formData.get("server_password") || "").toString(),
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
          throw new Error("Login required to create a lobby.");
        }
        throw new Error(data.error || "Unable to create lobby.");
      }
      const data: any = await response.json().catch(() => ({}));
      sfx.play("confirm", 0.35);
      emitToast({ message: soloMode ? "Solo session created." : "Room created successfully.", kind: "success", durationMs: 10000 });
      onClose();
      if (typeof onCreated === "function") onCreated();
      if (data?.url) {
        const lobbyPath = new URL(String(data.url), apiUrl("/")).pathname;
        const lobbyId = lobbyPath.split("/").filter(Boolean).pop();
        if (lobbyId) {
          navigate(`/lobby/${lobbyId}`);
        } else {
          window.location.hash = `#${lobbyPath}`;
        }
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : "Unable to create lobby.";
      setError(msg);
      emitToast({ message: msg, kind: "error", sticky: true });
      sfx.play("error", 0.35);
    }
  };

  return (
    <div className={`skl-modal${open ? " open" : ""}`} id="lobby-create-modal" aria-hidden={!open}>
      <div
        className="skl-modal-backdrop"
        onClick={() => {
          sfx.play("confirm", 0.2);
          onClose();
        }}
      ></div>
      <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-create-title">
        <div className="skl-modal-header">
          <h3 id="lobby-create-title">Create Room</h3>
          <button className="skl-modal-close" type="button" onClick={() => { sfx.play("confirm", 0.2); onClose(); }}>
            Close
          </button>
        </div>
        <form id="lobby-create-form" className="skl-lobby-form" onSubmit={onCreateSubmit}>
          <div className="skl-lobby-form-row">
            <button
              className={`skl-btn ghost${soloMode ? " active" : ""}`}
              type="button"
              onClick={() => setSoloMode((prev) => !prev)}
              title="Create a private solo session (not listed in Room List)."
            >
              {soloMode ? "Solo Mode: ON" : "Solo Mode"}
            </button>
          </div>
          <div className="skl-lobby-form-row">
            <input type="text" name="name" placeholder="Room name" maxLength={60} className="input" />
            <input type="text" name="description" placeholder="Short description" maxLength={180} className="input" />
          </div>
          <div className="skl-lobby-form-rules">
            <div className="skl-lobby-form-title">Room rules</div>
            <div className="skl-lobby-grid-fields">
              <label>
                Release
                <select name="release_mode" defaultValue="goal">
                  <option value="disabled">Disabled</option>
                  <option value="enabled">Manual Release</option>
                  <option value="goal">Goal completion</option>
                  <option value="auto">Auto</option>
                  <option value="auto-enabled">Auto + Manual</option>
                </select>
              </label>
              <label title="Coming soon">
                Collect
                <select name="collect_mode" defaultValue="goal" disabled>
                  <option value="goal">Coming soon</option>
                </select>
              </label>
              <label title="Coming soon">
                Remaining
                <select name="remaining_mode" defaultValue="enabled" disabled>
                  <option value="enabled">Coming soon</option>
                </select>
              </label>
              <label>
                Automatic Slow Release Inactivity Time
                <select name="slow_release_timeout" defaultValue="1800">
                  <option value="900">15m</option>
                  <option value="1800">30m</option>
                  <option value="3600">1h</option>
                  <option value="5400">1.5h</option>
                  <option value="7200">2h</option>
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
                  {hintCostOptions.map((value) => (
                    <option key={value} value={value}>{value}%</option>
                  ))}
                </select>
              </label>
              <label>
                Max Players
                <input type="number" name="max_players" min={1} max={50} defaultValue={50} className="input" disabled={soloMode} />
              </label>
              <label>
                Room Password (Private)
                <input type="text" name="server_password" placeholder="Leave empty for public room" maxLength={64} className="input" />
              </label>
            </div>
          </div>
          {error && <div className="skl-lobby-error show">{error}</div>}
          <div className="skl-modal-actions">
            <button className="skl-btn primary" type="submit">Create Room</button>
          </div>
        </form>
      </div>
    </div>
  );
};

export default CreateRoomModal;
