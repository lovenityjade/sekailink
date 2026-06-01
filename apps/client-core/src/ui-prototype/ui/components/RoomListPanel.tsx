import React, { useMemo } from "react";
import { motion } from "framer-motion";
import { formatLocalTime } from "@/utils/time";
import { useI18n } from "@/i18n";
import ChamferedPanel from "@/components/ChamferedPanel";

export type RoomListRoom = {
  id: string; name: string; description?: string; owner?: string;
  is_private?: boolean; member_count?: number; max_players?: number; last_activity?: string;
};

type FilterId = "all" | "open";
type Props = {
  rooms: RoomListRoom[]; search: string; setSearch: (value: string) => void;
  filter: FilterId; setFilter: (value: FilterId) => void;
  onCreateRoom: () => void; joinPendingRoomId?: string;
  onJoinRoom: (room: RoomListRoom) => void;
  selectedRoomId: string; setSelectedRoomId: (id: string) => void;
};

function PulsingOrb({ color = "#00ffc8", size = 5 }: { color?: string; size?: number }) {
  return (
    <span className="relative inline-flex shrink-0">
      <span className="inline-block rounded-full" style={{ width: size, height: size, backgroundColor: color, boxShadow: `0 0 ${size}px ${color}` }} />
      <span className="absolute inset-0 rounded-full animate-ping" style={{ backgroundColor: color, opacity: 0.3, animationDuration: "1.5s" }} />
    </span>
  );
}

function formatRelative(iso?: string) {
  if (!iso) return "—";
  const diff = Date.now() - new Date(iso).getTime();
  if (diff < 60000) return "just now";
  if (diff < 3600000) return `${Math.floor(diff / 60000)}m ago`;
  return `${Math.floor(diff / 3600000)}h ago`;
}

const RoomListPanel: React.FC<Props> = ({
  rooms, search, setSearch, filter, setFilter,
  onCreateRoom, joinPendingRoomId, onJoinRoom, selectedRoomId, setSelectedRoomId,
}) => {
  const { t } = useI18n();
  const filteredRooms = useMemo(() => {
    const q = search.trim().toLowerCase();
    return rooms.filter((r) => {
      if (filter === "open" && r.is_private) return false;
      if (!q) return true;
      return (r.name || "").toLowerCase().includes(q) || (r.owner || "").toLowerCase().includes(q);
    });
  }, [rooms, search, filter]);

  return (
    <ChamferedPanel title="SYNC LIST" delay={0.1} className="flex-[1.2] overflow-hidden flex flex-col"
      titleRight={<span className="text-[10px] text-teal/50 font-code flex items-center gap-1.5"><PulsingOrb />{filteredRooms.length} syncs</span>}
    >
      {/* Search + filters */}
      <div className="skl-room-toolbar flex items-center gap-2 mb-3">
        <div className="flex-1 relative flex items-center h-8"
          style={{ background: "rgba(0,0,0,0.20)", border: "1px solid rgba(0,255,200,0.08)" }}>
          <svg className="ml-2.5 shrink-0" width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" style={{ color: "rgba(200,255,224,0.20)" }}>
            <circle cx="11" cy="11" r="8" /><path d="m21 21-4.35-4.35" />
          </svg>
          <input
            className="w-full h-full pl-2 pr-3 text-xs text-phosphor font-code placeholder:text-phosphor/15 focus:outline-none"
            style={{ border: "none", boxShadow: "none", background: "transparent" }}
            placeholder={t("roomlist.search")} value={search} onChange={(e) => setSearch(e.target.value)}
          />
        </div>
        {([{ id: "all" as const, label: t("roomlist.filter.all").toUpperCase() }, { id: "open" as const, label: t("roomlist.filter.open").toUpperCase() }]).map((f) => (
          <button key={f.id} onClick={() => setFilter(f.id)}
            style={{
              border: filter === f.id ? "1px solid rgba(0,255,200,0.20)" : "1px solid rgba(0,255,200,0.08)",
              background: filter === f.id ? "rgba(0,255,200,0.06)" : "rgba(0,0,0,0.15)",
              boxShadow: "none",
            }}
            className={`h-8 px-3 text-[10px] font-header tracking-widest leading-none transition-all flex items-center ${filter === f.id ? "text-teal" : "text-phosphor/25 hover:text-phosphor/40"}`}>
            {f.label}
          </button>
        ))}
        <button onClick={onCreateRoom}
          style={{ border: "1px solid rgba(0,255,200,0.12)", background: "rgba(0,255,200,0.04)", boxShadow: "none" }}
          className="h-8 px-3 text-[10px] font-header tracking-widest text-teal/60 hover:text-teal hover:bg-teal/10 transition-all flex items-center gap-1.5 leading-none">
          <svg width="10" height="10" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round">
            <path d="M12 5v14M5 12h14" />
          </svg>
          CREATE SYNC
        </button>
      </div>

      {/* Table header */}
      <div className="flex items-center text-[9px] font-header text-phosphor/20 tracking-widest px-3 py-1.5 border-b border-teal/8">
        <div className="flex-[2]">SYNC</div>
        <div className="w-20 text-center">{t("roomlist.col.players").toUpperCase()}</div>
        <div className="w-16 text-center">{t("roomlist.col.access").toUpperCase()}</div>
        <div className="w-20 text-center">ACTIVITY</div>
        <div className="w-16" />
      </div>

      {/* Rows */}
      <div className="flex-1 overflow-y-auto space-y-0.5 pr-1">
        {filteredRooms.map((room, i) => {
          const active = room.id === selectedRoomId;
          return (
            <motion.div
              key={room.id}
              role="button"
              tabIndex={0}
              initial={{ opacity: 0, x: -15 }}
              animate={{ opacity: 1, x: 0 }}
              transition={{ delay: 0.1 + i * 0.04 }}
              onClick={() => setSelectedRoomId(room.id)}
              onKeyDown={(event) => {
                if (event.key === "Enter" || event.key === " ") {
                  event.preventDefault();
                  setSelectedRoomId(room.id);
                }
              }}
              className={`relative w-full text-left flex items-center px-3 py-2.5 scan-hover overflow-hidden group transition-all ${active ? "bg-teal/8 border-l-2 border-teal" : "border-l-2 border-transparent hover:bg-gunmetal-light/40"}`}
            >
              <div className="flex-[2] min-w-0">
                <div className="text-sm font-header text-phosphor/80 tracking-wide truncate group-hover:text-teal transition-colors">{room.name}</div>
                <div className="text-[10px] font-code text-phosphor/20 mt-0.5">Owner: {room.owner || "—"}</div>
              </div>
              <div className="w-20 text-center text-xs font-code text-phosphor/40">{room.member_count || 0}/{room.max_players || 50}</div>
              <div className="w-16 text-center">
                {room.is_private
                  ? <span className="text-amber/60 text-[10px] font-header">🔒</span>
                  : <span className="text-teal/40 text-[10px] font-header">🔓</span>
                }
              </div>
              <div className="w-20 text-center text-[10px] font-code text-phosphor/20">{formatRelative(room.last_activity)}</div>
              <div className="w-16 text-right">
                <button
                  type="button"
                  disabled={Boolean(joinPendingRoomId)}
                  className="opacity-0 group-hover:opacity-100 transition-opacity text-[10px] font-header text-teal tracking-wider px-2 py-1 bg-teal/10 border border-teal/20 hover:bg-teal/20 btn-charge"
                  onClick={(e) => { e.preventDefault(); e.stopPropagation(); onJoinRoom(room); }}
                >
                  JOIN
                </button>
              </div>
            </motion.div>
          );
        })}
      </div>
    </ChamferedPanel>
  );
};

export default RoomListPanel;
