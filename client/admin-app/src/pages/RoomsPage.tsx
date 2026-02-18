import React, { useEffect, useState } from "react";
import { adminApi } from "@/services/adminApi";

type RoomSummary = {
  id: string;
  owner: string;
  last_port: number;
  last_activity: string;
  seed_id: string;
  status: string;
  log_file: string;
};

const RoomsPage: React.FC = () => {
  const [rooms, setRooms] = useState<RoomSummary[]>([]);
  const [logText, setLogText] = useState("");
  const [error, setError] = useState("");

  const load = async () => {
    try {
      const data = await adminApi.getRooms();
      setRooms((data.rooms || []) as RoomSummary[]);
      setError("");
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err || "rooms_failed"));
    }
  };

  useEffect(() => {
    void load();
  }, []);

  return (
    <section className="page-section" aria-label="Room server administration">
      <header className="panel-header">
        <h2>Room Servers</h2>
        <div className="actions-row">
          <button className="btn ghost" type="button" onClick={() => void load()}>Refresh</button>
          <button className="btn danger" type="button" onClick={async () => {
            if (!window.confirm("Close all active rooms?")) return;
            await adminApi.purgeRooms();
            await load();
          }}>Purge all</button>
          <button className="btn ghost" type="button" onClick={async () => {
            const older = Number(window.prompt("Close rooms older than how many minutes?", "180") || 180);
            await adminApi.purgeRoomsFiltered({ older_than_minutes: older, status: ["Idle", "Error"] });
            await load();
          }}>Purge filtered</button>
        </div>
      </header>
      {error && <p className="error-text">{error}</p>}
      <div className="table-wrap">
        <table className="table">
          <thead>
            <tr>
              <th>Room</th>
              <th>Owner</th>
              <th>Port</th>
              <th>Last activity</th>
              <th>Status</th>
              <th>Action</th>
            </tr>
          </thead>
          <tbody>
            {rooms.map((r) => (
              <tr key={r.id}>
                <td><code>{r.id}</code></td>
                <td>{r.owner}</td>
                <td>{r.last_port || "-"}</td>
                <td>{new Date(r.last_activity).toLocaleString()}</td>
                <td>{r.status}</td>
                <td>
                  <div className="actions-row">
                    <button className="btn ghost" type="button" onClick={async () => {
                      const data = await adminApi.getLogContent(r.log_file, 260);
                      setLogText(data.content || "");
                    }}>Log</button>
                    <button className="btn danger" type="button" onClick={async () => {
                      if (!window.confirm(`Close room ${r.id}?`)) return;
                      await adminApi.closeRoom(r.id);
                      await load();
                    }}>Close</button>
                  </div>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      <div className="panel-block">
        <h3>Room Log</h3>
        <pre className="log-viewer">{logText || "Select a room log."}</pre>
      </div>
    </section>
  );
};

export default RoomsPage;
