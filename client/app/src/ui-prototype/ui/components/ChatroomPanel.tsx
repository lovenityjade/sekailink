import React, { useEffect, useRef, useState } from "react";
import { formatLocalTime } from "@/utils/time";
import { useI18n } from "@/i18n";

export type ChatMessage = {
  id: string;
  author: string;
  content: string;
  created_at: string;
  kind?: "system" | "user";
  avatar_url?: string;
};

export type SelectedRoom = {
  id: string;
  name: string;
};

const formatTime = (iso: string) => {
  return formatLocalTime(iso);
};

const isNearBottom = (el: HTMLElement, thresholdPx = 80) => {
  const delta = el.scrollHeight - (el.scrollTop + el.clientHeight);
  return delta < thresholdPx;
};

const initialsFrom = (name: string) => {
  const parts = String(name || "")
    .trim()
    .split(/\s+/)
    .filter(Boolean);
  if (parts.length === 0) return "?";
  if (parts.length === 1) return parts[0].slice(0, 2).toUpperCase();
  return `${parts[0][0] || ""}${parts[1][0] || ""}`.toUpperCase();
};

type Props = {
  messages: ChatMessage[];
  setMessages: React.Dispatch<React.SetStateAction<ChatMessage[]>>;
  currentUserName?: string;
  currentUserAvatarUrl?: string;
  userAvatarByName?: Record<string, string>;
};

const ChatroomPanel: React.FC<Props> = ({
  messages,
  setMessages,
  currentUserName,
  currentUserAvatarUrl,
  userAvatarByName,
}) => {
  const { t } = useI18n();
  const [chatInput, setChatInput] = useState("");
  const [newMsgs, setNewMsgs] = useState(false);
  const [channel, setChannel] = useState<"global" | "en" | "fr" | "es" | "ja" | "zh">("global");
  const [channelMessages, setChannelMessages] = useState<Record<string, ChatMessage[]>>({
    en: [],
    fr: [],
    es: [],
    ja: [],
    zh: [],
    global: [],
  });
  const chatScrollRef = useRef<HTMLDivElement | null>(null);
  const visibleMessages = channel === "global" ? messages : (channelMessages[channel] || []);

  useEffect(() => {
    const el = chatScrollRef.current;
    if (!el) return;
    if (isNearBottom(el)) {
      el.scrollTop = el.scrollHeight;
      setNewMsgs(false);
    } else {
      setNewMsgs(true);
    }
  }, [visibleMessages]);

  const sendMessage = () => {
    const text = chatInput.trim();
    if (!text) return;
    const nextMsg: ChatMessage = {
      id: `me-${Date.now()}`,
      author: currentUserName || t("proto.you"),
      content: text,
      created_at: new Date().toISOString(),
      kind: "user",
      avatar_url: currentUserAvatarUrl || undefined,
    };
    if (channel === "global") {
      setMessages((prev) => [...prev, nextMsg]);
    } else {
      setChannelMessages((prev) => ({ ...prev, [channel]: [...(prev[channel] || []), nextMsg] }));
    }
    setChatInput("");
    const el = chatScrollRef.current;
    if (el) el.scrollTop = el.scrollHeight;
  };

  const resolveAvatar = (m: ChatMessage) => {
    if (m.avatar_url) return m.avatar_url;
    const direct = userAvatarByName?.[m.author];
    if (direct) return direct;
    const key = Object.keys(userAvatarByName || {}).find(
      (k) => k.toLowerCase() === String(m.author || "").toLowerCase()
    );
    return key ? userAvatarByName?.[key] : undefined;
  };

  return (
    <section className="sklp-panel sklp-chat">
      <header className="sklp-panel-head">
        <div className="sklp-title">{t("proto.chatroom.title")}</div>
      </header>

      <div className="sklp-chat-top">
        <div className="sklp-search">
          <span className="sklp-search-ico" aria-hidden="true">⌕</span>
          <input className="sklp-input" type="search" placeholder={t("proto.chatroom.search_global")} />
        </div>
        <div className="sklp-chat-roomtag channel-tabs">
          {([
            { id: "global", label: t("proto.chatroom.global") },
            { id: "en", label: "EN" },
            { id: "fr", label: "FR" },
            { id: "es", label: "ES" },
            { id: "ja", label: "JA" },
            { id: "zh", label: "ZH" },
          ] as const).map((c) => (
            <button
              key={c.id}
              type="button"
              className={`sklp-chip${channel === c.id ? " active" : ""}`}
              onClick={() => setChannel(c.id)}
            >
              {c.label}
            </button>
          ))}
        </div>
      </div>

      <div className="sklp-chat-scroll" ref={chatScrollRef}>
        {visibleMessages.map((m) => (
          <article key={m.id} className={`sklp-msg${m.kind === "system" ? " sys" : ""}`}>
            <div className="sklp-msg-avatar" aria-hidden="true">
              {resolveAvatar(m) ? (
                <img src={resolveAvatar(m)} alt="" />
              ) : (
                <span>{m.kind === "system" ? "SYS" : initialsFrom(m.author)}</span>
              )}
            </div>
            <div className="sklp-msg-card">
              <div className="sklp-msg-head">
                <span className="sklp-msg-author">{m.author}</span>
                <span className="sklp-msg-time">{formatTime(m.created_at)}</span>
              </div>
              <div className="sklp-msg-body">{m.content}</div>
            </div>
          </article>
        ))}
      </div>

      {newMsgs && (
        <button
          type="button"
          className="sklp-newmsgs"
          onClick={() => {
            const el = chatScrollRef.current;
            if (el) el.scrollTop = el.scrollHeight;
            setNewMsgs(false);
          }}
        >
          {t("proto.chatroom.new_messages")}
        </button>
      )}

      <div className="sklp-chat-compose">
        <input
          className="sklp-input"
          value={chatInput}
          placeholder={t("proto.chatroom.type_message")}
          onChange={(e) => setChatInput(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === "Enter") sendMessage();
          }}
        />
        <button type="button" className="sklp-send" onClick={sendMessage}>{t("proto.chatroom.send")}</button>
      </div>
    </section>
  );
};

export default ChatroomPanel;
