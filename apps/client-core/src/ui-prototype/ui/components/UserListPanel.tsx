import React, { useState } from "react";
import { motion } from "framer-motion";
import { useI18n } from "@/i18n";
import { useSfx } from "@/hooks/useSfx";
import { apiFetch, isUsableAvatarUrl } from "@/services/api";
import { emitToast } from "@/services/toast";
import ChamferedPanel from "@/components/ChamferedPanel";

type User = { user_id: string; name: string; avatar_url?: string; status?: string; last_seen?: string; ready?: boolean };
type Props = { users: User[]; onUserBlocked?: (userId: string) => void };

function PulsingOrb({ color = "#22c55e", size = 5 }: { color?: string; size?: number }) {
  return (
    <span className="relative inline-flex shrink-0">
      <span className="inline-block rounded-full" style={{ width: size, height: size, backgroundColor: color, boxShadow: `0 0 ${size}px ${color}` }} />
      <span className="absolute inset-0 rounded-full animate-ping" style={{ backgroundColor: color, opacity: 0.3, animationDuration: "1.5s" }} />
    </span>
  );
}

function UserAvatar({ user }: { user: User }) {
  const [failed, setFailed] = useState(false);
  const initials = (user.name || "?").slice(0, 2).toUpperCase();
  React.useEffect(() => setFailed(false), [user.avatar_url]);
  if (user.avatar_url && isUsableAvatarUrl(user.avatar_url) && !failed) {
    return <img src={user.avatar_url} alt="" className="w-7 h-7 rounded-full object-cover ring-1 ring-teal/20" onError={() => setFailed(true)} />;
  }
  return (
    <div className="w-7 h-7 rounded-full bg-gradient-to-br from-teal/35 to-cyan-500/45 flex items-center justify-center text-[9px] font-header text-phosphor/80 ring-1 ring-teal/20">
      {initials}
    </div>
  );
}

function formatLastSeen(value?: string) {
  if (!value) return "active now";
  const timestamp = Date.parse(value);
  if (!Number.isFinite(timestamp)) return "active now";
  const diff = Math.max(0, Date.now() - timestamp);
  if (diff < 30_000) return "active now";
  if (diff < 90_000) return "seen 1m ago";
  return `seen ${Math.max(2, Math.round(diff / 60_000))}m ago`;
}

const UserListPanel: React.FC<Props> = ({ users, onUserBlocked }) => {
  const { t } = useI18n();
  const sfx = useSfx();
  const [contextMenu, setContextMenu] = useState<{ x: number; y: number; user: User } | null>(null);

  const blockUser = async (user: User) => {
    try {
      const res = await apiFetch("/api/social/blocks", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ user_id: user.user_id }),
      });
      if (!res.ok) {
        const data: any = await res.json().catch(() => ({}));
        throw new Error(data?.error || "Unable to block user.");
      }
      onUserBlocked?.(user.user_id);
      sfx.play("success", 0.2);
      emitToast({ message: `${user.name} blocked.`, kind: "success" });
    } catch {
      sfx.play("error", 0.2);
      emitToast({ message: `Unable to block ${user.name}.`, kind: "error" });
    }
  };

  const sendFriendRequest = async (user: User) => {
    try {
      const res = await apiFetch("/api/social/friends/request", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ user_id: user.user_id }),
      });
      if (!res.ok) {
        const data: any = await res.json().catch(() => ({}));
        throw new Error(data?.error || "Unable to send friend request.");
      }
      sfx.play("success", 0.2);
      emitToast({ message: `Friend request sent to ${user.name}`, kind: "success" });
    } catch {
      sfx.play("error", 0.2);
      emitToast({ message: `Unable to send friend request to ${user.name}.`, kind: "error" });
    }
  };

  return (
    <>
      <ChamferedPanel title="GLOBAL CHAT ONLINE" delay={0.4} className="flex-1 min-h-0 overflow-hidden flex flex-col"
        titleRight={<span className="text-[10px] text-teal/40 font-code flex items-center gap-1.5"><PulsingOrb color="#22c55e" size={4} />{users.length}</span>}
      >
        <div className="flex-1 overflow-y-auto space-y-0.5 pr-1">
          {users.map((user, i) => {
            const isOnline = (user.status || "").toLowerCase() === "online";
            return (
              <motion.button
                key={user.user_id}
                initial={{ opacity: 0, x: 8 }}
                animate={{ opacity: 1, x: 0 }}
                transition={{ delay: 0.4 + i * 0.04 }}
                className="w-full text-left flex items-center gap-2.5 px-3 py-1.5 hover:bg-teal/5 transition-colors group"
                onContextMenu={(e) => { e.preventDefault(); setContextMenu({ x: e.clientX, y: e.clientY, user }); }}
              >
                <PulsingOrb color={isOnline ? "#22c55e" : "#3a5a50"} size={5} />
                <UserAvatar user={user} />
                <div className="min-w-0 flex-1">
                  <div className={`text-xs font-code truncate ${isOnline ? "text-phosphor/60" : "text-phosphor/25"}`}>{user.name}</div>
                  <div className="text-[9px] font-code text-phosphor/18">{formatLastSeen(user.last_seen)}</div>
                </div>
                {user.ready && <span className="text-[9px] font-header text-teal tracking-wider text-glow">READY</span>}
              </motion.button>
            );
          })}
          {users.length === 0 && (
            <div className="text-center py-4 text-[11px] font-code text-phosphor/15">
              {t("userlist.empty")}
            </div>
          )}
        </div>
      </ChamferedPanel>

      {/* Context menu */}
      {contextMenu && (
        <div className="fixed inset-0 z-50" onClick={() => setContextMenu(null)}>
          <div className="absolute bg-gunmetal border border-teal/20 panel-chamfer-sm p-1 min-w-[130px]" style={{ left: contextMenu.x, top: contextMenu.y }} onClick={(e) => e.stopPropagation()}>
            <button className="w-full text-left px-3 py-1.5 text-[11px] font-code text-phosphor/50 hover:bg-teal/10 hover:text-phosphor/80 transition-colors"
              onClick={() => { setContextMenu(null); void sendFriendRequest(contextMenu.user); }}>
              {t("social.add_friend")}
            </button>
            <button className="w-full text-left px-3 py-1.5 text-[11px] font-code text-magenta/50 hover:bg-magenta/10 hover:text-magenta transition-colors"
              onClick={() => { const u = contextMenu.user; setContextMenu(null); void blockUser(u); }}>
              {t("social.block")}
            </button>
          </div>
        </div>
      )}
    </>
  );
};

export default UserListPanel;
