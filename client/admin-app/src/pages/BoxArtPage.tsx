import React, { useEffect, useMemo, useState } from "react";
import { adminApi, type GameBoxArtMapping, type SteamGridAsset } from "@/services/adminApi";

type GameEntry = { game_id: string; display_name: string };

const BoxArtPage: React.FC = () => {
  const [games, setGames] = useState<GameEntry[]>([]);
  const [mappings, setMappings] = useState<GameBoxArtMapping[]>([]);
  const [results, setResults] = useState<SteamGridAsset[]>([]);
  const [selectedGameId, setSelectedGameId] = useState("");
  const [query, setQuery] = useState("");
  const [kind, setKind] = useState<"grid" | "hero" | "logo">("grid");
  const [status, setStatus] = useState("");
  const [error, setError] = useState("");

  const selectedGame = useMemo(
    () => games.find((g) => g.game_id === selectedGameId) || null,
    [games, selectedGameId]
  );

  const loadBaseData = async () => {
    try {
      const [g, m] = await Promise.all([adminApi.getClientGames(), adminApi.getGameBoxArtMappings()]);
      const gameList = Array.isArray(g?.games) ? g.games : [];
      setGames(gameList);
      setMappings(Array.isArray(m?.items) ? m.items : []);
      if (!selectedGameId && gameList[0]?.game_id) {
        setSelectedGameId(gameList[0].game_id);
      }
      setError("");
    } catch (err) {
      setError(err instanceof Error ? err.message : "boxart_load_failed");
    }
  };

  useEffect(() => {
    void loadBaseData();
  }, []);

  const runSearch = async () => {
    const q = query.trim() || selectedGame?.display_name || "";
    if (!q) {
      setError("Pick a game or enter a search query first.");
      return;
    }
    setStatus("Searching SteamGridDB...");
    try {
      const data = await adminApi.searchSteamGridDB(q, kind, 24);
      setResults(Array.isArray(data?.items) ? data.items : []);
      setStatus(`Found ${Array.isArray(data?.items) ? data.items.length : 0} result(s).`);
      setError("");
    } catch (err) {
      setError(err instanceof Error ? err.message : "steamgriddb_search_failed");
      setStatus("");
    }
  };

  const saveSelection = async (asset: SteamGridAsset) => {
    if (!selectedGameId) {
      setError("Select a game first.");
      return;
    }
    setStatus("Saving selection...");
    try {
      await adminApi.setGameBoxArtMapping(selectedGameId, {
        provider: "steamgriddb",
        provider_asset_id: String(asset.id),
        image_url: asset.url,
        thumb_url: asset.thumb || asset.url,
        kind,
      });
      await loadBaseData();
      setStatus("Saved.");
      setError("");
    } catch (err) {
      setError(err instanceof Error ? err.message : "boxart_save_failed");
      setStatus("");
    }
  };

  const clearSelection = async (gameId: string) => {
    setStatus("Clearing selection...");
    try {
      await adminApi.clearGameBoxArtMapping(gameId);
      await loadBaseData();
      setStatus("Cleared.");
      setError("");
    } catch (err) {
      setError(err instanceof Error ? err.message : "boxart_clear_failed");
      setStatus("");
    }
  };

  return (
    <section className="page-section" aria-label="Game box art">
      <header className="panel-header">
        <h2>Game Box Art (SteamGridDB)</h2>
        <div className="actions-row">
          <button className="btn ghost" type="button" onClick={() => void loadBaseData()}>Refresh</button>
        </div>
      </header>

      <div className="panel-block">
        <div className="actions-row">
          <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
            Game
            <select className="input" value={selectedGameId} onChange={(e) => setSelectedGameId(e.target.value)}>
              {games.map((game) => (
                <option key={game.game_id} value={game.game_id}>
                  {game.display_name}
                </option>
              ))}
            </select>
          </label>
          <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
            Type
            <select className="input" value={kind} onChange={(e) => setKind((e.target.value as any) || "grid")}>
              <option value="grid">Grid</option>
              <option value="hero">Hero</option>
              <option value="logo">Logo</option>
            </select>
          </label>
          <input
            className="input"
            style={{ minWidth: 320 }}
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            placeholder="Search query (leave empty to use selected game name)"
          />
          <button className="btn" type="button" onClick={() => void runSearch()}>
            Search SGDB
          </button>
        </div>
      </div>

      {status && <p className="status-text">{status}</p>}
      {error && <p className="error-text">{error}</p>}

      <div className="table-wrap">
        <table className="table">
          <thead>
            <tr>
              <th>Game</th>
              <th>Provider</th>
              <th>Preview</th>
              <th>Updated</th>
              <th>Action</th>
            </tr>
          </thead>
          <tbody>
            {mappings.map((m) => (
              <tr key={m.game_id}>
                <td>{m.display_name || m.game_id}</td>
                <td>{m.provider || "-"}</td>
                <td>
                  {m.image_url ? (
                    <img src={m.thumb_url || m.image_url} alt="" style={{ width: 120, height: 56, objectFit: "cover", borderRadius: 6, border: "1px solid var(--line)" }} />
                  ) : "-"}
                </td>
                <td>{m.updated_at || "-"}</td>
                <td>
                  <button className="btn ghost" type="button" onClick={() => void clearSelection(m.game_id)}>
                    Clear
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {!!results.length && (
        <div className="panel-block">
          <h3 style={{ marginTop: 0 }}>Search Results</h3>
          <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(220px, 1fr))", gap: 10 }}>
            {results.map((asset) => (
              <div key={asset.id} style={{ border: "1px solid var(--line)", borderRadius: 8, padding: 8, background: "var(--panel-alt)" }}>
                <img src={asset.thumb || asset.url} alt="" style={{ width: "100%", height: 110, objectFit: "cover", borderRadius: 6 }} />
                <div style={{ marginTop: 8, fontSize: 12, color: "var(--muted)" }}>
                  #{asset.id} {asset.width && asset.height ? `• ${asset.width}x${asset.height}` : ""}
                </div>
                <div style={{ marginTop: 8 }}>
                  <button className="btn" type="button" onClick={() => void saveSelection(asset)}>
                    Use this art
                  </button>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}
    </section>
  );
};

export default BoxArtPage;

