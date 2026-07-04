import React, { useRef, useEffect, useState } from "react";
import { motion } from "framer-motion";
import { useI18n } from "@/i18n";
import { useSfx } from "@/hooks/useSfx";
import { chatChannelId, chatService, type ChatChannelRef, type SekaiChatMessage } from "@/services/chatService";
import { isUsableAvatarUrl } from "@/services/api";
import { emitToast } from "@/services/toast";
import ChamferedPanel from "@/components/ChamferedPanel";

export type ChatMessage = SekaiChatMessage;

type Props = {
  messages: ChatMessage[]; setMessages: React.Dispatch<React.SetStateAction<ChatMessage[]>>;
  currentUserName?: string; currentUserAvatarUrl?: string;
  userAvatarByName?: Record<string, string>;
  lobbyId?: string;
  channel?: ChatChannelRef;
  title?: string;
  presenceCountOverride?: number | null;
};

function UserAvatar({ name, url }: { name: string; url?: string }) {
  const [failed, setFailed] = useState(false);
  const initials = name.slice(0, 2).toUpperCase();
  useEffect(() => setFailed(false), [url]);
  return url && !failed
    ? <img src={url} alt="" className="w-7 h-7 rounded-full ring-1 ring-teal/20 object-cover" onError={() => setFailed(true)} />
    : <div className="w-7 h-7 rounded-full bg-gradient-to-br from-teal/45 to-cyan-500/50 flex items-center justify-center text-[9px] font-display text-white ring-1 ring-teal/20">{initials}</div>;
}

const ChatroomPanel: React.FC<Props> = ({
  messages,
  setMessages,
  currentUserName = "You",
  currentUserAvatarUrl,
  userAvatarByName,
  lobbyId,
  channel,
  title = "ROOM CHAT",
  presenceCountOverride,
}) => {
  const { t } = useI18n();
  const sfx = useSfx();
  const scrollRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);
  const [draft, setDraft] = useState("");
  const [presenceCount, setPresenceCount] = useState<number | null>(null);
  const targetChannel = React.useMemo(
    () => channel || (lobbyId ? ({ kind: "lobby", lobbyId } as ChatChannelRef) : null),
    [channel, lobbyId],
  );
  const activeChannelLabel = targetChannel?.kind === "global" ? `Global ${(targetChannel.locale || "fr").toUpperCase()}` : "Room";
  const targetChannelId = targetChannel ? chatChannelId(targetChannel) : "";

  useEffect(() => {
    if (scrollRef.current) scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
  }, [messages.length]);

  useEffect(() => {
    if (presenceCountOverride !== undefined) {
      setPresenceCount(presenceCountOverride);
      return;
    }
    if (!targetChannel || targetChannel.kind !== "global") {
      setPresenceCount(null);
      return;
    }
    let cancelled = false;
    const refreshPresence = async () => {
      try {
        await chatService.touchPresence(targetChannel);
        const users = await chatService.listPresence(targetChannel);
        if (!cancelled) setPresenceCount(users.length);
      } catch {
        if (!cancelled) setPresenceCount(null);
      }
    };
    void refreshPresence();
    const timer = window.setInterval(refreshPresence, 30000);
    return () => {
      cancelled = true;
      window.clearInterval(timer);
    };
  }, [presenceCountOverride, targetChannel, targetChannelId]);

  const send = async () => {
    const text = draft.trim();
    if (!text) return;
    if (!targetChannel) {
      emitToast({ message: "Select a chat channel before sending a message.", kind: "error" });
      return;
    }
    sfx.play("chat", 0.3);
    const optimisticId = `local-${Date.now()}-${Math.random().toString(16).slice(2)}`;
    const optimisticMessage: ChatMessage = {
      id: optimisticId,
      author: currentUserName,
      content: text,
      created_at: new Date().toISOString(),
      kind: "user",
      channel: chatChannelId(targetChannel),
      avatar_url: currentUserAvatarUrl,
    };
    setDraft("");
    setMessages((prev) => [...prev, optimisticMessage]);
    try {
      const msg = await chatService.sendMessage(targetChannel, text);
      setMessages((prev) => prev.map((entry) => {
        if (entry.id !== optimisticId) return entry;
        return {
          ...msg,
          avatar_url: isUsableAvatarUrl(msg.avatar_url) ? msg.avatar_url : optimisticMessage.avatar_url,
        };
      }));
    } catch (err) {
      setMessages((prev) => prev.filter((entry) => entry.id !== optimisticId));
      setDraft(text);
      emitToast({ message: err instanceof Error ? err.message : "Unable to send message.", kind: "error" });
    }
  };

  const channelMessages = messages.filter((m) => {
    if (m.kind === "system") return true;
    if (!targetChannel) return true;
    return !m.channel || m.channel === chatChannelId(targetChannel) || m.channel === activeChannelLabel;
  });

  return (
    <ChamferedPanel title={title} delay={0.2} className="flex-1 overflow-hidden flex flex-col">
      {/* Channel tabs */}
      <div className="flex items-end shrink-0 relative" style={{ marginBottom: "-1px", zIndex: 1 }}>
        {[activeChannelLabel].map((ch) => {
          const isActive = true;
          return (
            <button
              key={ch}
              type="button"
              className={`px-4 py-2 text-[10px] font-header tracking-widest transition-all panel-chamfer-sm ${isActive ? "text-teal text-glow" : "text-phosphor/25 hover:text-phosphor/50"}`}
              style={isActive
                ? { background: "rgba(0,0,0,0.25)", borderTop: "1px solid rgba(0,255,200,0.15)", borderLeft: "1px solid rgba(0,255,200,0.15)", borderRight: "1px solid rgba(0,255,200,0.15)", borderBottom: "1px solid transparent" }
                : { background: "transparent", border: "1px solid transparent", borderBottom: "1px solid rgba(0,255,200,0.08)" }
              }
            >
              {ch.toUpperCase()}
              {presenceCount !== null && (
                <span className="ml-2 text-[8px] font-code text-phosphor/30">
                  {presenceCount} ONLINE
                </span>
              )}
            </button>
          );
        })}
        <div className="flex-1" style={{ borderBottom: "1px solid rgba(0,255,200,0.08)" }} />
      </div>

      {/* Messages */}
      <div ref={scrollRef} className="flex-1 overflow-y-auto space-y-1.5 p-3 min-h-0 bg-black/25" style={{ border: "1px solid rgba(0,255,200,0.08)", borderTop: "none" }}>
        {channelMessages.map((msg) => {
          const isSystem = msg.kind === "system" || msg.kind === "join" || msg.kind === "leave";
          const isMe = msg.author === currentUserName;
          const avatarUrl = isUsableAvatarUrl(msg.avatar_url)
            ? msg.avatar_url
            : userAvatarByName?.[msg.author];
          return (
            <motion.div key={msg.id} initial={{ opacity: 0, y: 6 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.25 }}>
              {isSystem ? (
                <div className="flex items-center gap-2 py-1">
                  <div className="flex-1 h-px bg-teal/8" />
                  <span className="text-[9px] font-code text-phosphor/15 italic shrink-0">{msg.content}</span>
                  <div className="flex-1 h-px bg-teal/8" />
                </div>
              ) : (
                <div className={`flex gap-2 ${isMe ? "flex-row-reverse" : ""}`}>
                  <UserAvatar name={msg.author} url={avatarUrl} />
                  <div className={`min-w-0 max-w-[70%] ${isMe ? "text-right" : ""}`}>
                    <div className="flex items-baseline gap-2 mb-0.5">
                      <span className="text-[10px] font-header text-teal/60 tracking-wider">{msg.author}</span>
                      <span className="text-[8px] font-code text-phosphor/15">{new Date(msg.created_at).toLocaleTimeString(undefined, { hour: "2-digit", minute: "2-digit" })}</span>
                    </div>
                    <div className={`panel-chamfer-sm px-3 py-2 text-xs font-code ${isMe ? "bg-teal/10 text-phosphor/70 border border-teal/15" : "bg-gunmetal-light/50 text-phosphor/50 border border-teal/8"}`}>
                      {msg.content}
                    </div>
                  </div>
                </div>
              )}
            </motion.div>
          );
        })}
      </div>

      {/* Compose */}
      <div className="shrink-0 mt-2 pt-2 border-t border-teal/8 flex gap-2">
        <input
          ref={inputRef}
          className="flex-1 h-9 bg-void/60 border border-teal/10 panel-chamfer-sm px-3 text-xs text-phosphor font-code placeholder:text-phosphor/15 focus:outline-none focus:border-teal/25 transition-colors"
          placeholder={targetChannel ? t("chat.input_placeholder") : "Select a chat channel"}
          disabled={!targetChannel}
          value={draft} onChange={(e) => setDraft(e.target.value)}
          onKeyDown={(e) => { if (e.key === "Enter") { e.preventDefault(); send(); } }}
        />
        <button
          className="h-9 px-4 panel-chamfer-sm bg-teal/10 border border-teal/20 text-teal text-[10px] font-header tracking-widest hover:bg-teal/20 transition-all btn-charge disabled:opacity-40"
          disabled={!targetChannel}
          onClick={() => { void send(); }}
        >
          {t("chat.send")}
        </button>
      </div>
    </ChamferedPanel>
  );
};

export default ChatroomPanel;
