import React, { useMemo } from "react";
import { formatLocalTime } from "@/utils/time";
import { useI18n } from "@/i18n";

export type RoomListRoom = {
  id: string;
  name: string;
  description?: string;
  owner?: string;
  is_private?: boolean;
  member_count?: number;
  max_players?: number;
  last_activity?: string;
};

const formatTime = (iso?: string) => {
  return formatLocalTime(iso) || "—";
};

type FilterId = "all" | "open" | "fn";

type Props = {
  rooms: RoomListRoom[];
  search: string;
  setSearch: (value: string) => void;
  filter: FilterId;
  setFilter: (value: FilterId) => void;
  onCreateRoom: () => void;
  onJoinRoom: (room: RoomListRoom) => void;
  selectedRoomId: string;
  setSelectedRoomId: (id: string) => void;
};

const RoomListPanel: React.FC<Props> = ({
  rooms,
  search,
  setSearch,
  filter,
  setFilter,
  onCreateRoom,
  onJoinRoom,
  selectedRoomId,
  setSelectedRoomId,
}) => {
  const { t } = useI18n();
  const filteredRooms = useMemo(() => {
    const q = search.trim().toLowerCase();
    return rooms.filter((r) => {
      if (filter === "open" && r.is_private) return false;
      if (filter === "fn") {
        // “FN” is ambiguous in the mockup. Treat as “flagged” rooms.
        const name = String(r.name || "").toLowerCase();
        return name.includes("fn") || Boolean(r.is_private);
      }
      if (!q) return true;
      const name = String(r.name || "").toLowerCase();
      const owner = String(r.owner || "").toLowerCase();
      return name.includes(q) || owner.includes(q);
    });
  }, [rooms, search, filter]);

  return (
    <section className="sklp-panel sklp-roomlist" data-tutorial="room-list-panel">
      <header className="sklp-panel-head">
        <div className="sklp-title">{t("proto.room_list.title")}</div>
        <div className="sklp-roomlist-filters">
          {([
            { id: "all", label: t("roomlist.filter.all").toUpperCase() },
            { id: "open", label: t("roomlist.filter.open").toUpperCase() },
            { id: "fn", label: "FN" },
          ] as const).map((c) => (
            <button
              key={c.id}
              type="button"
              className={`sklp-chip${filter === c.id ? " active" : ""}`}
              onClick={() => setFilter(c.id)}
            >
              {c.label}
            </button>
          ))}
          <button
            type="button"
            className="sklp-chip sklp-chip-create"
            onClick={() => onCreateRoom()}
          >
            {t("roomlist.create_room").toUpperCase()}
          </button>
        </div>
      </header>

      <div className="sklp-roomlist-bar">
        <div className="sklp-search">
          <span className="sklp-search-ico" aria-hidden="true">⌕</span>
          <input
            className="sklp-input"
            type="search"
            placeholder={t("roomlist.search")}
            value={search}
            onChange={(e) => setSearch(e.target.value)}
          />
        </div>
      </div>

      <div className="sklp-table-head">
        <div>{t("roomlist.col.room").toUpperCase()}</div>
        <div>{t("roomlist.col.players").toUpperCase()}</div>
        <div>{t("roomlist.col.access").toUpperCase()}</div>
        <div>{t("roomlist.col.mode").toUpperCase()}</div>
        <div>{t("roomlist.col.progress").toUpperCase()}</div>
        <div />
      </div>

      <div className="sklp-roomlist-scroll">
        {filteredRooms.map((room) => {
          const active = room.id === selectedRoomId;
          const access = room.is_private ? t("roomlist.private") : t("roomlist.public");
          return (
            <button
              key={room.id}
              type="button"
              className={`sklp-roomrow${active ? " active" : ""}`}
              onClick={() => setSelectedRoomId(room.id)}
            >
              <div className="sklp-roomcell room">
                <div className="sklp-roombadge">{String(room.name || "S").slice(0, 1).toUpperCase()}</div>
                <div className="sklp-roommeta">
                  <div className="sklp-roomname">{room.name}</div>
                  <div className="sklp-roomsub">
                    {t("roomlist.owner")}: {room.owner || "—"} · {t("roomlist.last_active")}: {formatTime(room.last_activity)}
                  </div>
                </div>
              </div>
              <div className="sklp-roomcell">{`${room.member_count || 0}/${room.max_players || 50}`}</div>
              <div className="sklp-roomcell dim">{access}</div>
              <div className="sklp-roomcell dim">Auto</div>
              <div className="sklp-roomcell dim">—</div>
              <div className="sklp-roomcell action">
                <button
                  type="button"
                  className="sklp-join-pill"
                  data-tutorial="room-join"
                  onClick={(e) => {
                    e.preventDefault();
                    e.stopPropagation();
                    onJoinRoom(room);
                  }}
                >
                  {t("proto.join")}
                </button>
              </div>
            </button>
          );
        })}
      </div>
    </section>
  );
};

export default RoomListPanel;
