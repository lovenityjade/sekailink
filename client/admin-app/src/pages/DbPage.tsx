import React, { useEffect, useMemo, useState } from "react";
import { adminApi } from "@/services/adminApi";

type EntityKey = "users" | "lobbies" | "rooms" | "generations" | "tickets";

const DbPage: React.FC = () => {
  const [entity, setEntity] = useState<EntityKey>("users");
  const [limit, setLimit] = useState(50);
  const [offset, setOffset] = useState(0);
  const [total, setTotal] = useState(0);
  const [rows, setRows] = useState<Array<Record<string, unknown>>>([]);
  const [error, setError] = useState("");

  const load = async (nextEntity = entity, nextLimit = limit, nextOffset = offset) => {
    try {
      const res = await adminApi.getDbEntity(nextEntity, nextLimit, nextOffset);
      setRows(Array.isArray(res.rows) ? res.rows : []);
      setTotal(Number(res.total || 0));
      setError("");
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err || "db_load_failed"));
    }
  };

  useEffect(() => {
    void load();
  }, []);

  const columns = useMemo(() => {
    const keys = new Set<string>();
    for (const row of rows) {
      for (const k of Object.keys(row || {})) keys.add(k);
    }
    return Array.from(keys);
  }, [rows]);

  return (
    <section className="page-section" aria-label="Database explorer">
      <header className="panel-header">
        <h2>DB Explorer (read-only)</h2>
        <div className="actions-row">
          <select className="input" value={entity} onChange={(e) => setEntity(e.target.value as EntityKey)} aria-label="Entity">
            <option value="users">users</option>
            <option value="lobbies">lobbies</option>
            <option value="rooms">rooms</option>
            <option value="generations">generations</option>
            <option value="tickets">tickets</option>
          </select>
          <input
            className="input"
            type="number"
            min={1}
            max={500}
            value={limit}
            onChange={(e) => setLimit(Math.max(1, Math.min(500, Number(e.target.value || 50))))}
            aria-label="Limit"
          />
          <input
            className="input"
            type="number"
            min={0}
            value={offset}
            onChange={(e) => setOffset(Math.max(0, Number(e.target.value || 0)))}
            aria-label="Offset"
          />
          <button className="btn ghost" type="button" onClick={() => void load(entity, limit, offset)}>Load</button>
        </div>
      </header>

      {error && <p className="error-text">{error}</p>}
      <p className="sidebar-meta">Total: {total}</p>

      <div className="table-wrap">
        <table className="table">
          <thead>
            <tr>
              {columns.map((c) => (
                <th key={c}>{c}</th>
              ))}
            </tr>
          </thead>
          <tbody>
            {rows.map((row, idx) => (
              <tr key={idx}>
                {columns.map((c) => (
                  <td key={c}>{typeof row[c] === "object" ? JSON.stringify(row[c]) : String(row[c] ?? "")}</td>
                ))}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </section>
  );
};

export default DbPage;
