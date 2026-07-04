import { FormEvent, useEffect, useMemo, useState } from "react";
import { MessageCircle, Radio, Search, Send, Sparkles } from "lucide-react";
import { runtime } from "../../services/runtime";
import TitleBar from "./TitleBar";

type Surface = "chat" | "activity" | "hint";

type RuntimeState = {
  title?: string;
  lobbyId?: string;
  moduleId?: string;
  slot?: string;
  game?: string;
  hintPoints?: string | number;
  hintItems?: string[];
  activity?: Array<any>;
  messages?: Array<any>;
};

const surfaceFromLocation = (): Surface => {
  const text = `${window.location.hash || ""} ${window.location.pathname || ""}`;
  if (text.includes("activity")) return "activity";
  if (text.includes("hint")) return "hint";
  return "chat";
};

const displayName = (message: any) =>
  String(message?.display_name || message?.global_name || message?.username || message?.author_name || message?.author || message?.name || "Room");

const messageText = (message: any) =>
  String(message?.content || message?.text || message?.body || "");

const messageTime = (message: any) => {
  const raw = String(message?.created_at || message?.timestamp || "");
  const date = raw ? new Date(raw) : null;
  if (!date || Number.isNaN(date.getTime())) return "";
  return date.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
};

const iconFor = (surface: Surface) => {
  if (surface === "activity") return <Radio className="h-4 w-4" />;
  if (surface === "hint") return <Sparkles className="h-4 w-4" />;
  return <MessageCircle className="h-4 w-4" />;
};

export default function RuntimeSocialWindow() {
  const [surface] = useState<Surface>(() => surfaceFromLocation());
  const [state, setState] = useState<RuntimeState | null>(null);
  const [error, setError] = useState("");
  const [text, setText] = useState("");
  const [search, setSearch] = useState("");
  const [sending, setSending] = useState(false);

  const refresh = async () => {
    try {
      const res = await runtime.runtimeSocialGetState?.();
      if (!res?.ok) {
        setError(res?.error || "No active Sekaiemu session.");
        setState(null);
        return;
      }
      setError("");
      setState(res.state || {});
    } catch (err) {
      setError(err instanceof Error ? err.message : "Runtime social window failed.");
    }
  };

  useEffect(() => {
    void refresh();
    const timer = window.setInterval(() => void refresh(), surface === "chat" ? 1800 : 3000);
    return () => window.clearInterval(timer);
  }, [surface]);

  const filteredHints = useMemo(() => {
    const items = Array.isArray(state?.hintItems) ? state?.hintItems || [] : [];
    const needle = search.trim().toLowerCase();
    return items
      .filter((item) => !needle || item.toLowerCase().includes(needle))
      .slice(0, 80);
  }, [state?.hintItems, search]);

  const submitChat = async (event: FormEvent) => {
    event.preventDefault();
    const clean = text.trim();
    if (!clean || sending) return;
    setSending(true);
    try {
      const res = await runtime.runtimeSocialSendChat?.(clean);
      if (!res?.ok) throw new Error(res?.error || "Unable to send chat message.");
      setText("");
      await refresh();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Unable to send chat message.");
    } finally {
      setSending(false);
    }
  };

  const requestHint = async (item: string) => {
    setSending(true);
    try {
      const res = await runtime.runtimeSocialRequestHint?.(item);
      if (!res?.ok) throw new Error(res?.error || "Unable to request hint.");
      setSearch("");
      await refresh();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Unable to request hint.");
    } finally {
      setSending(false);
    }
  };

  const title = surface === "activity" ? "Runtime Activity" : surface === "hint" ? "Runtime Hint" : "Runtime Chat";

  return (
    <div className="flex h-screen flex-col bg-[#05070a] text-[#f5f7fb]">
      <TitleBar />
      <div className="border-b border-[#252a33] bg-[#10151c] px-4 py-3">
        <div className="flex items-center gap-2 text-sm font-semibold">
          <span className="grid h-8 w-8 place-items-center rounded-md bg-[#1a232d] text-[#8fe8de]">{iconFor(surface)}</span>
          <div className="min-w-0">
            <div>{title}</div>
            <div className="truncate text-xs font-normal text-[#9aa6b2]">{state?.title || "Sekaiemu session"}</div>
          </div>
        </div>
      </div>

      {error && <div className="mx-4 mt-3 rounded-md border border-[#ff6b35]/40 bg-[#24120d] px-3 py-2 text-sm text-[#ffd5c4]">{error}</div>}

      {surface === "chat" && (
        <main className="flex min-h-0 flex-1 flex-col">
          <div className="flex-1 space-y-3 overflow-y-auto px-4 py-4">
            {(state?.messages || []).map((message, index) => (
              <div key={String(message?.id || index)} className="rounded-md border border-[#252a33] bg-[#10151c] px-3 py-2">
                <div className="flex items-center justify-between gap-3 text-xs text-[#9aa6b2]">
                  <span className="font-semibold text-[#8fe8de]">{displayName(message)}</span>
                  <span>{messageTime(message)}</span>
                </div>
                <p className="mt-1 break-words text-sm leading-5">{messageText(message)}</p>
              </div>
            ))}
            {!(state?.messages || []).length && <div className="text-sm text-[#9aa6b2]">No chat messages yet.</div>}
          </div>
          <form onSubmit={submitChat} className="flex gap-2 border-t border-[#252a33] bg-[#10151c] p-3">
            <input
              value={text}
              onChange={(event) => setText(event.target.value)}
              className="min-w-0 flex-1 rounded-md border border-[#303844] bg-[#080c12] px-3 py-2 text-sm outline-none focus:border-[#8fe8de]"
              placeholder="Message"
            />
            <button disabled={sending || !text.trim()} className="grid h-10 w-10 place-items-center rounded-md bg-[#ff6b35] text-white disabled:opacity-50">
              <Send className="h-4 w-4" />
            </button>
          </form>
        </main>
      )}

      {surface === "activity" && (
        <main className="min-h-0 flex-1 space-y-3 overflow-y-auto px-4 py-4">
          {(state?.activity || []).map((entry, index) => (
            <div key={String(entry?.id || index)} className="rounded-md border border-[#252a33] bg-[#10151c] px-3 py-3 text-sm">
              <div className="break-words font-semibold text-[#f5f7fb]">{String(entry?.title || entry?.kind || "Activity")}</div>
              <div className="mt-1 whitespace-pre-wrap break-words leading-5 text-[#b8c3cf]">{String(entry?.detail || entry?.text || entry?.message || "")}</div>
            </div>
          ))}
          {!(state?.activity || []).length && <div className="text-sm text-[#9aa6b2]">No runtime activity yet.</div>}
        </main>
      )}

      {surface === "hint" && (
        <main className="min-h-0 flex-1 space-y-3 overflow-hidden px-4 py-4">
          <div className="rounded-md border border-[#252a33] bg-[#10151c] px-3 py-3 text-sm text-[#b8c3cf]">
            Hint points: <span className="text-[#f5f7fb]">{String(state?.hintPoints || "unknown")}</span>
          </div>
          <div className="relative">
            <Search className="absolute left-3 top-2.5 h-4 w-4 text-[#73808f]" />
            <input
              value={search}
              onChange={(event) => setSearch(event.target.value)}
              className="w-full rounded-md border border-[#303844] bg-[#080c12] py-2 pl-9 pr-3 text-sm outline-none focus:border-[#8fe8de]"
              placeholder="Search hintable item"
            />
          </div>
          <div className="max-h-[calc(100vh-184px)] space-y-2 overflow-y-auto">
            {filteredHints.map((item) => (
              <button
                key={item}
                disabled={sending}
                onClick={() => void requestHint(item)}
                className="flex w-full items-center justify-between rounded-md border border-[#252a33] bg-[#10151c] px-3 py-2 text-left text-sm hover:border-[#8fe8de] disabled:opacity-50"
              >
                <span>{item}</span>
                <Sparkles className="h-4 w-4 text-[#ff6b35]" />
              </button>
            ))}
            {!filteredHints.length && <div className="text-sm text-[#9aa6b2]">Hintable items will appear when the room exposes them.</div>}
          </div>
        </main>
      )}
    </div>
  );
}
