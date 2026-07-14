import { FormEvent, useCallback, useEffect, useRef, useState } from 'react';
import { RefreshCw, Send, Server, TerminalSquare } from 'lucide-react';
import { apiFetch } from '../../services/api';
import TitleBar from './TitleBar';

type ConsoleState = {
  generation_status?: string;
  room_status?: string;
  room_runtime_alive?: boolean;
  generation_log?: string;
  room_server_log?: string;
  updated_at?: number;
};

const lobbyIdFromHash = () => decodeURIComponent(window.location.hash.split('/').slice(2).join('/') || '');

export default function HostConsoleWindow() {
  const [lobbyId] = useState(lobbyIdFromHash);
  const [tab, setTab] = useState<'generation' | 'room'>('generation');
  const [state, setState] = useState<ConsoleState>({});
  const [command, setCommand] = useState('');
  const [history, setHistory] = useState<string[]>([]);
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);
  const [sending, setSending] = useState(false);
  const logRef = useRef<HTMLPreElement | null>(null);

  const refresh = useCallback(async () => {
    if (!lobbyId) return;
    setLoading(true);
    try {
      const response = await apiFetch(`/api/lobbies/${encodeURIComponent(lobbyId)}/host-console`);
      const data = await response.json().catch(() => ({}));
      if (!response.ok || data?.ok === false) throw new Error(String(data?.message || data?.error || 'Unable to load Host Console.'));
      setState(data);
      setError('');
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unable to load Host Console.');
    } finally {
      setLoading(false);
    }
  }, [lobbyId]);

  useEffect(() => {
    void refresh();
    const timer = window.setInterval(() => void refresh(), 3000);
    return () => window.clearInterval(timer);
  }, [refresh]);

  useEffect(() => {
    if (logRef.current) logRef.current.scrollTop = logRef.current.scrollHeight;
  }, [state.generation_log, state.room_server_log, tab]);

  const submit = async (event: FormEvent) => {
    event.preventDefault();
    const clean = command.trim();
    if (!clean || sending) return;
    setSending(true);
    try {
      const response = await apiFetch(`/api/lobbies/${encodeURIComponent(lobbyId)}/host-console`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ command: clean }),
      });
      const data = await response.json().catch(() => ({}));
      if (!response.ok || data?.ok === false) throw new Error(String(data?.detail || data?.error || 'Admin command failed.'));
      setHistory((items) => [...items.slice(-39), `${data?.command || clean}  [sent]`]);
      setCommand('');
      setError('');
      window.setTimeout(() => void refresh(), 400);
    } catch (err) {
      const message = err instanceof Error ? err.message : 'Admin command failed.';
      setHistory((items) => [...items.slice(-39), `${clean}  [failed: ${message}]`]);
      setError(message);
    } finally {
      setSending(false);
    }
  };

  const log = tab === 'generation' ? state.generation_log : state.room_server_log;

  return (
    <div className="flex h-screen flex-col bg-[#05070a] text-[#f5f7fb]">
      <TitleBar />
      <header className="flex items-center justify-between border-b border-[#252a33] bg-[#10151c] px-4 py-3">
        <div className="flex min-w-0 items-center gap-3">
          <span className="grid h-9 w-9 place-items-center rounded-md bg-[#1a232d] text-[#8fe8de]"><TerminalSquare className="h-5 w-5" /></span>
          <div className="min-w-0">
            <h1 className="font-semibold">Host Console</h1>
            <p className="truncate text-xs text-[#9aa6b2]">{lobbyId}</p>
          </div>
        </div>
        <div className="flex items-center gap-3 text-xs text-[#9aa6b2]">
          <span>Generation: <b className="text-[#f5f7fb]">{state.generation_status || 'unknown'}</b></span>
          <span className={state.room_runtime_alive ? 'text-[#8fe8de]' : 'text-[#ff8b82]'}>{state.room_runtime_alive ? 'Room Online' : 'Room Offline'}</span>
          <button onClick={() => void refresh()} disabled={loading} title="Refresh logs" className="grid h-8 w-8 place-items-center rounded-md border border-[#303844] hover:border-[#8fe8de] disabled:opacity-50">
            <RefreshCw className={`h-4 w-4 ${loading ? 'animate-spin' : ''}`} />
          </button>
        </div>
      </header>

      <div className="flex border-b border-[#252a33] bg-[#0b0f14] px-3 pt-2">
        <button onClick={() => setTab('generation')} className={`flex items-center gap-2 border-b-2 px-4 py-2 text-sm ${tab === 'generation' ? 'border-[#8fe8de] text-[#f5f7fb]' : 'border-transparent text-[#7f8a97]'}`}><TerminalSquare className="h-4 w-4" />Generation Log</button>
        <button onClick={() => setTab('room')} className={`flex items-center gap-2 border-b-2 px-4 py-2 text-sm ${tab === 'room' ? 'border-[#8fe8de] text-[#f5f7fb]' : 'border-transparent text-[#7f8a97]'}`}><Server className="h-4 w-4" />Room Server Log</button>
      </div>

      {error && <div className="mx-4 mt-3 rounded-md border border-[#f85149]/50 bg-[#24120d] px-3 py-2 text-sm text-[#ffd5c4]">{error}</div>}

      <main className="flex min-h-0 flex-1 gap-3 p-3">
        <pre ref={logRef} className="min-h-0 min-w-0 flex-1 overflow-auto rounded-md border border-[#252a33] bg-[#020407] p-4 font-mono text-xs leading-5 text-[#c7d2df] whitespace-pre-wrap break-words">{log || `No ${tab === 'generation' ? 'generation' : 'room server'} log available.`}</pre>
        <aside className="flex w-72 min-w-60 flex-col rounded-md border border-[#252a33] bg-[#0b0f14]">
          <div className="border-b border-[#252a33] px-3 py-2 text-xs font-semibold text-[#9aa6b2]">COMMAND HISTORY</div>
          <div className="min-h-0 flex-1 space-y-2 overflow-y-auto p-3 font-mono text-xs text-[#aab6c3]">
            {history.map((entry, index) => <div key={`${index}:${entry}`} className="break-words">{entry}</div>)}
            {!history.length && <div className="font-sans text-[#66717e]">Commands sent during this console session appear here.</div>}
          </div>
        </aside>
      </main>

      <form onSubmit={submit} className="flex gap-2 border-t border-[#252a33] bg-[#10151c] p-3">
        <span className="self-center font-mono text-sm text-[#8fe8de]">!admin</span>
        <input value={command} onChange={(event) => setCommand(event.target.value)} placeholder="/status" maxLength={400} className="min-w-0 flex-1 rounded-md border border-[#303844] bg-[#05070a] px-3 py-2 font-mono text-sm outline-none focus:border-[#8fe8de]" />
        <button disabled={sending || !command.trim() || !state.room_runtime_alive} title="Send admin command" className="flex h-10 items-center gap-2 rounded-md bg-[#ff6b35] px-4 text-sm font-semibold text-white disabled:opacity-40"><Send className="h-4 w-4" />Send</button>
      </form>
    </div>
  );
}
