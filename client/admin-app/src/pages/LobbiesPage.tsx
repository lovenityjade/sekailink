import React, { useEffect, useState } from "react";
import { adminApi } from "@/services/adminApi";

type LobbySummary = {
  id: string;
  name: string;
  owner: string;
  member_count: number;
  last_activity: string;
  room_id: string | null;
};

const LobbiesPage: React.FC = () => {
  const [lobbies, setLobbies] = useState<LobbySummary[]>([]);
  const [detail, setDetail] = useState<Record<string, unknown> | null>(null);
  const [error, setError] = useState("");

  const load = async () => {
    try {
      const data = await adminApi.getLobbies();
      setLobbies((data.lobbies || []) as LobbySummary[]);
      setError("");
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err || "lobbies_failed"));
    }
  };

  useEffect(() => {
    void load();
  }, []);

  return (
    <section className="page-section" aria-label="Lobby administration">
      <header className="panel-header">
        <h2>Lobbies</h2>
        <div className="actions-row">
          <button className="btn ghost" type="button" onClick={() => void load()}>Refresh</button>
        </div>
      </header>
      {error && <p className="error-text">{error}</p>}
      <div className="table-wrap">
        <table className="table">
          <thead>
            <tr>
              <th>Lobby</th>
              <th>Owner</th>
              <th>Members</th>
              <th>Last activity</th>
              <th>Room</th>
              <th>Action</th>
            </tr>
          </thead>
          <tbody>
            {lobbies.map((l) => (
              <tr key={l.id}>
                <td>{l.name}</td>
                <td>{l.owner}</td>
                <td>{l.member_count}</td>
                <td>{new Date(l.last_activity).toLocaleString()}</td>
                <td>{l.room_id || "-"}</td>
                <td>
                  <div className="actions-row">
                    <button className="btn ghost" type="button" onClick={async () => setDetail(await adminApi.getLobby(l.id))}>Inspect</button>
                    <button className="btn danger" type="button" onClick={async () => {
                      if (!window.confirm(`Close lobby ${l.name}?`)) return;
                      await adminApi.closeLobby(l.id);
                      await load();
                    }}>Close</button>
                  </div>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      {detail && (
        <div className="panel-block">
          <h3>Lobby detail</h3>
          <pre className="json-viewer">{JSON.stringify(detail, null, 2)}</pre>
        </div>
      )}
    </section>
  );
};

export default LobbiesPage;
