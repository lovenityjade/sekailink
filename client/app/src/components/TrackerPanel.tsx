import React, { useCallback, useEffect, useMemo, useState } from "react";
import { apiJson, apiUrl } from "../services/api";
import { useInterval } from "../hooks/useInterval";

type TrackerMode = "player" | "multi" | "sphere";

interface StaticTrackerData {
  datapackage: Record<string, { checksum: string }>;
  player_game: Array<{ team: number; player: number; game: string }>;
}

interface TrackerViewData {
  room_players: Record<string, number[]>;
  player_names: Array<{ team: number; player: number; name: string }>;
  player_games: Array<{ team: number; player: number; game: string }>;
  player_status: Array<{ team: number; player: number; status: number }>;
  player_checks_done: Array<{ team: number; player: number; checks_done: number }>;
  player_locations_total: Array<{ team: number; player: number; total_locations: number }>;
  total_team_locations: Record<string, number>;
  total_team_locations_complete: Record<string, number>;
  completed_worlds: Record<string, number>;
  activity_timers: Array<{ team: number; player: number; seconds: number | null }>;
  hints: Array<{ team: number; hints: TrackerHint[] }>;
  enabled_trackers: string[];
  videos: Record<string, Array<[string, string]>> | Record<string, any>;
}

interface PlayerTrackerData {
  player_name: string;
  game: string;
  inventory: Record<string, number>;
  locations: number[];
  checked_locations: number[];
  received_items: Record<string, number>;
  hints: TrackerHint[];
  player_names: Array<{ team: number; player: number; name: string }>;
}

interface TrackerHint {
  receiving_player: number;
  finding_player: number;
  location: number;
  item: number;
  found: boolean;
  entrance?: string;
  item_flags?: number;
  status?: number;
}

interface SphereTrackerTeam {
  team: number;
  spheres: Array<{
    sphere: number;
    finders: Array<{
      finder_slot: number;
      receivers: Array<{
        receiver_slot: number;
        pairs: Array<[number, number, number]>;
      }>;
    }>;
  }>;
}

interface NameMap {
  itemIdToName: Record<string, string>;
  locationIdToName: Record<string, string>;
}

interface TrackerPanelProps {
  trackerId?: string;
  defaultTeam?: number;
  playerSlot?: number | null;
  onPlayerSelect?: (player: number) => void;
}

const statusLabel = (status: number) => {
  const map: Record<number, string> = {
    0: "Disconnected",
    5: "Connected",
    10: "Ready",
    20: "Playing",
    30: "Goal Completed",
  };
  return map[status] || "Unknown";
};

const secondsToHours = (seconds: number | null) => {
  if (!seconds && seconds !== 0) return "None";
  const hours = Math.floor(seconds / 3600);
  const minutes = Math.floor((seconds - hours * 3600) / 60)
    .toString()
    .padStart(2, "0");
  return `${hours}:${minutes}`;
};

const buildNameMap = (gamePackage: any): NameMap => {
  const itemIdToName: Record<string, string> = {};
  const locationIdToName: Record<string, string> = {};
  const itemNameToId = gamePackage?.item_name_to_id || {};
  const locationNameToId = gamePackage?.location_name_to_id || {};
  Object.entries(itemNameToId).forEach(([name, id]) => {
    itemIdToName[String(id)] = name;
  });
  Object.entries(locationNameToId).forEach(([name, id]) => {
    locationIdToName[String(id)] = name;
  });
  return { itemIdToName, locationIdToName };
};

const TrackerPanel: React.FC<TrackerPanelProps> = ({
  trackerId,
  defaultTeam = 0,
  playerSlot,
  onPlayerSelect,
}) => {
  const [mode, setMode] = useState<TrackerMode>("player");
  const [search, setSearch] = useState("");
  const [hideChecked, setHideChecked] = useState(false);
  const [staticData, setStaticData] = useState<StaticTrackerData | null>(null);
  const [viewData, setViewData] = useState<TrackerViewData | null>(null);
  const [playerData, setPlayerData] = useState<PlayerTrackerData | null>(null);
  const [sphereData, setSphereData] = useState<SphereTrackerTeam[] | null>(null);
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(true);
  const [gameFilter, setGameFilter] = useState("Generic");
  const [nameMaps, setNameMaps] = useState<Record<string, NameMap>>({});

  const normalizedSearch = search.trim().toLowerCase();

  const playerGames = useMemo(() => {
    const map = new Map<string, string>();
    staticData?.player_game.forEach((entry) => {
      map.set(`${entry.team}:${entry.player}`, entry.game);
    });
    return map;
  }, [staticData]);

  const playerNames = useMemo(() => {
    const map = new Map<string, string>();
    viewData?.player_names.forEach((entry) => {
      map.set(`${entry.team}:${entry.player}`, entry.name);
    });
    return map;
  }, [viewData]);

  const activePlayerName = playerSlot ? playerNames.get(`${defaultTeam}:${playerSlot}`) : "";

  const loadStatic = useCallback(async () => {
    if (!trackerId) return;
    try {
      const data = await apiJson<StaticTrackerData>(`/api/static_tracker/${trackerId}`);
      setStaticData(data);
      setError("");
    } catch (err) {
      setError(err instanceof Error ? err.message : "Unable to load tracker data.");
    }
  }, [trackerId]);

  const loadView = useCallback(async () => {
    if (!trackerId) return;
    try {
      const data = await apiJson<TrackerViewData>(`/api/tracker_view/${trackerId}`);
      setViewData(data);
      setError("");
    } catch (err) {
      setError(err instanceof Error ? err.message : "Unable to load tracker data.");
    }
  }, [trackerId]);

  const loadPlayer = useCallback(async () => {
    if (!trackerId || !playerSlot) return;
    try {
      const data = await apiJson<PlayerTrackerData>(`/api/tracker_player/${trackerId}/${defaultTeam}/${playerSlot}`);
      setPlayerData(data);
      setError("");
    } catch (err) {
      setError(err instanceof Error ? err.message : "Unable to load player tracker.");
    }
  }, [trackerId, defaultTeam, playerSlot]);

  const loadSphere = useCallback(async () => {
    if (!trackerId) return;
    try {
      const data = await apiJson<SphereTrackerTeam[]>(`/api/sphere_tracker/${trackerId}`);
      setSphereData(data);
      setError("");
    } catch (err) {
      setError(err instanceof Error ? err.message : "Unable to load sphere tracker.");
    }
  }, [trackerId]);

  useEffect(() => {
    setLoading(true);
    loadStatic().finally(() => setLoading(false));
    loadView();
    loadPlayer();
    loadSphere();
  }, [loadStatic, loadView, loadPlayer, loadSphere]);

  useInterval(() => {
    loadView();
    loadPlayer();
    if (mode === "sphere") {
      loadSphere();
    }
  }, 30000);

  useEffect(() => {
    const fetchMaps = async () => {
      if (!staticData) return;
      const next: Record<string, NameMap> = { ...nameMaps };
      for (const [game, meta] of Object.entries(staticData.datapackage || {})) {
        if (!meta?.checksum || next[game]) continue;
        try {
          const pkg = await apiJson<any>(`/api/datapackage/${meta.checksum}`);
          next[game] = buildNameMap(pkg);
        } catch {
          next[game] = { itemIdToName: {}, locationIdToName: {} };
        }
      }
      setNameMaps(next);
    };
    fetchMaps();
  }, [staticData]);

  const getItemName = (game: string | undefined, id: number) => {
    if (!game) return `Item ${id}`;
    return nameMaps[game]?.itemIdToName?.[String(id)] || `Item ${id}`;
  };

  const getLocationName = (game: string | undefined, id: number) => {
    if (!game) return `Location ${id}`;
    return nameMaps[game]?.locationIdToName?.[String(id)] || `Location ${id}`;
  };

  const matchesSearch = (value: string) => {
    if (!normalizedSearch) return true;
    return value.toLowerCase().includes(normalizedSearch);
  };

  const multiRows = useMemo(() => {
    if (!viewData) return [];
    const statusMap = new Map(viewData.player_status.map((entry) => [`${entry.team}:${entry.player}`, entry.status]));
    const checksMap = new Map(viewData.player_checks_done.map((entry) => [`${entry.team}:${entry.player}`, entry.checks_done]));
    const totalMap = new Map(viewData.player_locations_total.map((entry) => [`${entry.team}:${entry.player}`, entry.total_locations]));
    const activityMap = new Map(viewData.activity_timers.map((entry) => [`${entry.team}:${entry.player}`, entry.seconds]));
    const gameMap = new Map(viewData.player_games.map((entry) => [`${entry.team}:${entry.player}`, entry.game]));

    const rows: Array<{
      team: number;
      player: number;
      name: string;
      game: string;
      status: number;
      checksDone: number;
      total: number;
      activity: number | null;
    }> = [];

    Object.entries(viewData.room_players || {}).forEach(([teamKey, players]) => {
      const team = Number(teamKey);
      players.forEach((player) => {
        const key = `${team}:${player}`;
        const name = playerNames.get(key) || `Player ${player}`;
        const game = gameMap.get(key) || "Unknown";
        if (gameFilter !== "Generic" && game !== gameFilter) return;
        rows.push({
          team,
          player,
          name,
          game,
          status: statusMap.get(key) ?? 0,
          checksDone: checksMap.get(key) ?? 0,
          total: totalMap.get(key) ?? 0,
          activity: activityMap.get(key) ?? null,
        });
      });
    });

    return rows.filter((row) => matchesSearch(`${row.player} ${row.name} ${row.game}`));
  }, [viewData, playerNames, gameFilter, normalizedSearch]);

  const playerRows = useMemo(() => {
    if (!playerData) return [];
    const game = playerData.game;
    const checked = new Set(playerData.checked_locations || []);
    return playerData.locations
      .filter((locId) => !hideChecked || !checked.has(locId))
      .map((locId) => ({
        id: locId,
        name: getLocationName(game, locId),
        checked: checked.has(locId),
      }))
      .filter((row) => matchesSearch(row.name));
  }, [playerData, hideChecked, nameMaps, normalizedSearch]);

  const inventoryRows = useMemo(() => {
    if (!playerData) return [];
    const game = playerData.game;
    return Object.entries(playerData.inventory || {})
      .map(([itemId, count]) => ({
        id: Number(itemId),
        name: getItemName(game, Number(itemId)),
        count,
        order: playerData.received_items?.[itemId] ?? 0,
      }))
      .filter((row) => row.count > 0)
      .sort((a, b) => a.order - b.order)
      .filter((row) => matchesSearch(row.name));
  }, [playerData, nameMaps, normalizedSearch]);

  const hintRows = useMemo(() => {
    const hints =
      mode === "player" ? playerData?.hints || [] :
      viewData?.hints?.find((entry) => entry.team === defaultTeam)?.hints || [];

    const nameMap = playerData?.player_names || viewData?.player_names || [];
    const nameLookup = new Map(nameMap.map((entry) => [`${defaultTeam}:${entry.player}`, entry.name]));

    return hints
      .map((hint) => {
        const finderGame = playerGames.get(`${defaultTeam}:${hint.finding_player}`);
        const receiverGame = playerGames.get(`${defaultTeam}:${hint.receiving_player}`);
        const finderName = nameLookup.get(`${defaultTeam}:${hint.finding_player}`) || `Player ${hint.finding_player}`;
        const receiverName = nameLookup.get(`${defaultTeam}:${hint.receiving_player}`) || `Player ${hint.receiving_player}`;
        return {
          finderName,
          receiverName,
          item: getItemName(receiverGame, hint.item),
          location: getLocationName(finderGame, hint.location),
          game: finderGame || receiverGame || "Unknown",
          entrance: hint.entrance || "Vanilla",
          found: hint.found,
        };
      })
      .filter((row) =>
        matchesSearch(`${row.finderName} ${row.receiverName} ${row.item} ${row.location} ${row.game} ${row.entrance}`)
      );
  }, [mode, playerData, viewData, playerGames, nameMaps, normalizedSearch]);

  const sphereRows = useMemo(() => {
    if (!sphereData) return [];
    const rows: Array<{
      sphere: number;
      finder: string;
      receiver: string;
      item: string;
      location: string;
      game: string;
    }> = [];
    const nameLookup = new Map(viewData?.player_names?.map((entry) => [`${entry.team}:${entry.player}`, entry.name]) || []);
    sphereData.forEach((teamEntry) => {
      if (teamEntry.team !== defaultTeam) return;
      teamEntry.spheres.forEach((sphere) => {
        sphere.finders.forEach((finder) => {
          const finderGame = playerGames.get(`${defaultTeam}:${finder.finder_slot}`);
          const finderName = nameLookup.get(`${defaultTeam}:${finder.finder_slot}`) || `Player ${finder.finder_slot}`;
          finder.receivers.forEach((receiver) => {
            const receiverGame = playerGames.get(`${defaultTeam}:${receiver.receiver_slot}`);
            const receiverName = nameLookup.get(`${defaultTeam}:${receiver.receiver_slot}`) || `Player ${receiver.receiver_slot}`;
            receiver.pairs.forEach(([itemId, locId]) => {
              rows.push({
                sphere: sphere.sphere,
                finder: finderName,
                receiver: receiverName,
                item: getItemName(receiverGame, itemId),
                location: getLocationName(finderGame, locId),
                game: finderGame || "Unknown",
              });
            });
          });
        });
      });
    });
    return rows.filter((row) =>
      matchesSearch(`${row.sphere} ${row.finder} ${row.receiver} ${row.item} ${row.location} ${row.game}`)
    );
  }, [sphereData, viewData, playerGames, nameMaps, normalizedSearch]);

  if (!trackerId) {
    return <div className="skl-tracker-empty">Tracker is available after generation.</div>;
  }

  if (loading && !viewData) {
    return <div className="skl-tracker-empty">Loading tracker…</div>;
  }

  if (error) {
    return <div className="skl-tracker-empty">{error}</div>;
  }

  return (
    <div className="skl-tracker-view">
      <div className="skl-tracker-toolbar">
        <div className="skl-tracker-tabs">
          <button className={mode === "player" ? "active" : ""} onClick={() => setMode("player")}>My Tracker</button>
          <button className={mode === "multi" ? "active" : ""} onClick={() => setMode("multi")}>All Players</button>
          <button className={mode === "sphere" ? "active" : ""} onClick={() => setMode("sphere")}>Spheres</button>
        </div>
        <div className="skl-tracker-actions">
          <input
            id="tracker-search"
            placeholder="Search…"
            value={search}
            onChange={(event) => setSearch(event.target.value)}
          />
        </div>
      </div>

      {mode === "multi" && (
        <div className="skl-tracker-filter">
          <label>
            Game:
            <select value={gameFilter} onChange={(event) => setGameFilter(event.target.value)}>
              <option value="Generic">All Games</option>
              {(viewData?.enabled_trackers || []).map((game) => (
                <option key={game} value={game}>{game}</option>
              ))}
            </select>
          </label>
        </div>
      )}

      {mode === "player" && (
        <div className="skl-tracker-filter">
          <label>
            Player:
            <select
              value={playerSlot ?? ""}
              onChange={(event) => onPlayerSelect?.(Number(event.target.value))}
            >
              {(viewData?.room_players?.[String(defaultTeam)] || []).map((player) => (
                <option key={player} value={player}>
                  {playerNames.get(`${defaultTeam}:${player}`) || `Player ${player}`}
                </option>
              ))}
            </select>
          </label>
        </div>
      )}

      {mode === "player" && playerData && (
        <div className="skl-tracker-tables">
          <div className="skl-tracker-table">
            <h4>Inventory</h4>
            <table>
              <thead>
                <tr>
                  <th>Item</th>
                  <th className="center">Amount</th>
                  <th className="center">Last Order</th>
                </tr>
              </thead>
              <tbody>
                {inventoryRows.map((row) => (
                  <tr key={row.id}>
                    <td>{row.name}</td>
                    <td className="center">{row.count}</td>
                    <td className="center">{row.order}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>

          <div className="skl-tracker-table">
            <div className="skl-tracker-table-header">
              <h4>Locations</h4>
              <label className="skl-tracker-checkbox">
                <input type="checkbox" checked={hideChecked} onChange={(event) => setHideChecked(event.target.checked)} />
                Hide checked
              </label>
            </div>
            <table>
              <thead>
                <tr>
                  <th>Location</th>
                  <th className="center">Checked</th>
                </tr>
              </thead>
              <tbody>
                {playerRows.map((row) => (
                  <tr key={row.id} data-checked={row.checked}>
                    <td>{row.name}</td>
                    <td className="center">{row.checked ? "✔" : ""}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>

          <div className="skl-tracker-table">
            <h4>Hints</h4>
            <table>
              <thead>
                <tr>
                  <th>Finder</th>
                  <th>Receiver</th>
                  <th>Item</th>
                  <th>Location</th>
                  <th>Game</th>
                  <th>Entrance</th>
                  <th className="center">Found</th>
                </tr>
              </thead>
              <tbody>
                {hintRows.map((row, index) => (
                  <tr key={index}>
                    <td>{row.finderName}</td>
                    <td>{row.receiverName}</td>
                    <td>{row.item}</td>
                    <td>{row.location}</td>
                    <td>{row.game}</td>
                    <td>{row.entrance}</td>
                    <td className="center">{row.found ? "✔" : ""}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}

      {mode === "multi" && viewData && (
        <div className="skl-tracker-tables">
          <div className="skl-tracker-table">
            <h4>Multiworld Tracker</h4>
            <table>
              <thead>
                <tr>
                  <th>#</th>
                  <th>Name</th>
                  <th>Game</th>
                  <th>Status</th>
                  <th className="center">Checks</th>
                  <th className="center">%</th>
                  <th className="center">Last Activity</th>
                </tr>
              </thead>
              <tbody>
                {multiRows.map((row) => (
                  <tr key={`${row.team}-${row.player}`}>
                    <td>{row.player}</td>
                    <td>{row.name}</td>
                    <td>{row.game}</td>
                    <td>{statusLabel(row.status)}</td>
                    <td className="center">{row.checksDone}/{row.total}</td>
                    <td className="center">{row.total ? ((row.checksDone / row.total) * 100).toFixed(2) : "100.00"}</td>
                    <td className="center">{secondsToHours(row.activity)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>

          <div className="skl-tracker-table">
            <h4>Hints</h4>
            <table>
              <thead>
                <tr>
                  <th>Finder</th>
                  <th>Receiver</th>
                  <th>Item</th>
                  <th>Location</th>
                  <th>Game</th>
                  <th>Entrance</th>
                  <th className="center">Found</th>
                </tr>
              </thead>
              <tbody>
                {hintRows.map((row, index) => (
                  <tr key={index}>
                    <td>{row.finderName}</td>
                    <td>{row.receiverName}</td>
                    <td>{row.item}</td>
                    <td>{row.location}</td>
                    <td>{row.game}</td>
                    <td>{row.entrance}</td>
                    <td className="center">{row.found ? "✔" : ""}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}

      {mode === "sphere" && (
        <div className="skl-tracker-tables">
          <div className="skl-tracker-table">
            <h4>Sphere Tracker</h4>
            <table>
              <thead>
                <tr>
                  <th>Sphere</th>
                  <th>Finder</th>
                  <th>Receiver</th>
                  <th>Item</th>
                  <th>Location</th>
                  <th>Game</th>
                </tr>
              </thead>
              <tbody>
                {sphereRows.map((row, index) => (
                  <tr key={index}>
                    <td>{row.sphere}</td>
                    <td>{row.finder}</td>
                    <td>{row.receiver}</td>
                    <td>{row.item}</td>
                    <td>{row.location}</td>
                    <td>{row.game}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}

      <div className="skl-tracker-footer">
        {activePlayerName ? <span>Viewing: {activePlayerName}</span> : <span>Tracker</span>}
        {trackerId && (
          <a
            href={apiUrl(`/tracker/${trackerId}`)}
            target="_blank"
            rel="noopener"
          >
            Open web tracker
          </a>
        )}
      </div>
    </div>
  );
};

export default TrackerPanel;
