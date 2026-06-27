import { useCallback, useEffect, useMemo, useState } from 'react';
import {
  Cpu,
  Database,
  FileArchive,
  FolderOpen,
  Gamepad2,
  HardDriveDownload,
  Loader2,
  LogOut,
  MemoryStick,
  Play,
  RefreshCw,
  Square,
} from 'lucide-react';
import { runtime } from '../../services/runtime';
import { clearRuntimeLabSession } from '../../services/runtimeLab';

type LabCore = {
  id: string;
  name: string;
  path: string;
  system: string;
};

type MemoryDomain = {
  name: string;
  size?: number;
  writable?: boolean;
};

type LaunchResult = {
  ok?: boolean;
  pid?: number;
  error?: string;
  detail?: string;
  exePath?: string;
  corePath?: string;
  romPath?: string;
  memorySocketPath?: string;
  layoutPreview?: boolean;
};

const parseAddress = (value: string) => {
  const trimmed = value.trim();
  if (!trimmed) return 0;
  if (/^0x/i.test(trimmed)) return Number.parseInt(trimmed.slice(2), 16);
  return Number.parseInt(trimmed, 10);
};

const formatDomainSize = (size?: number) => {
  if (!size || !Number.isFinite(size)) return 'size unknown';
  if (size >= 1024 * 1024) return `${(size / (1024 * 1024)).toFixed(2)} MiB`;
  if (size >= 1024) return `${(size / 1024).toFixed(1)} KiB`;
  return `${size} bytes`;
};

export default function SekaiemuRuntimeLabPage() {
  const [cores, setCores] = useState<LabCore[]>([]);
  const [corePath, setCorePath] = useState('');
  const [romPath, setRomPath] = useState('');
  const [trackerPackPath, setTrackerPackPath] = useState('');
  const [layoutPreview, setLayoutPreview] = useState(true);
  const [fullscreen, setFullscreen] = useState(false);
  const [trackerDisplayMode, setTrackerDisplayMode] = useState('split-screen');
  const [apHost, setApHost] = useState('');
  const [apSlot, setApSlot] = useState('');
  const [apPass, setApPass] = useState('');
  const [launching, setLaunching] = useState(false);
  const [busy, setBusy] = useState(false);
  const [status, setStatus] = useState('');
  const [error, setError] = useState('');
  const [session, setSession] = useState<LaunchResult | null>(null);
  const [domains, setDomains] = useState<MemoryDomain[]>([]);
  const [domain, setDomain] = useState('system_ram');
  const [address, setAddress] = useState('0x0000');
  const [readSize, setReadSize] = useState('256');
  const [hex, setHex] = useState('');
  const runningPid = session?.ok && session.pid ? session.pid : 0;

  const selectedCore = useMemo(
    () => cores.find((entry) => entry.path === corePath) || null,
    [cores, corePath]
  );

  const refreshCores = useCallback(async () => {
    setBusy(true);
    setError('');
    try {
      const result = await runtime.sekaiEmuLabListCores?.() as { ok?: boolean; cores?: LabCore[]; error?: string } | undefined;
      if (!result?.ok) throw new Error(result?.error || 'core_scan_failed');
      const nextCores = Array.isArray(result.cores) ? result.cores : [];
      setCores(nextCores);
      setCorePath((current) => current || nextCores[0]?.path || '');
      setStatus(`${nextCores.length} core(s) detected.`);
    } catch (err) {
      setError(String((err as Error)?.message || err || 'Unable to list cores.'));
    } finally {
      setBusy(false);
    }
  }, []);

  useEffect(() => {
    void refreshCores();
  }, [refreshCores]);

  const pickRom = async () => {
    const result = await runtime.pickFile?.({
      title: 'Select ROM',
      filters: [
        { name: 'ROM files', extensions: ['sfc', 'smc', 'nes', 'gb', 'gbc', 'gba', 'n64', 'z64', 'v64'] },
        { name: 'All files', extensions: ['*'] },
      ],
    });
    if (result && !result.canceled && result.path) setRomPath(result.path);
  };

  const pickTracker = async () => {
    const result = await runtime.pickFolder?.({ title: 'Select PopTracker pack folder' });
    if (result && !result.canceled && result.path) setTrackerPackPath(result.path);
  };

  const launch = async () => {
    setLaunching(true);
    setError('');
    setHex('');
    setDomains([]);
    try {
      if (!layoutPreview && !romPath.trim()) throw new Error('Select a ROM first.');
      if (!layoutPreview && !corePath.trim()) throw new Error('Select a libretro core first.');
      const result = await runtime.sekaiEmuLabLaunch?.({
        romPath: romPath.trim() || undefined,
        corePath: corePath.trim() || undefined,
        trackerPackPath: trackerPackPath.trim() || undefined,
        layoutPreview,
        fullscreen,
        trackerDisplayMode,
        apHost: apHost.trim() || undefined,
        apSlot: apSlot.trim() || undefined,
        apPass,
      }) as LaunchResult | undefined;
      if (!result?.ok) throw new Error(result?.error || result?.detail || 'sekaiemu_launch_failed');
      setSession(result);
      setStatus(`Sekaiemu launched with pid ${result.pid}.`);
    } catch (err) {
      setError(String((err as Error)?.message || err || 'Launch failed.'));
    } finally {
      setLaunching(false);
    }
  };

  const stop = async () => {
    if (!runningPid) return;
    setBusy(true);
    setError('');
    try {
      const result = await runtime.sekaiEmuStop?.(runningPid) as { ok?: boolean; error?: string } | undefined;
      if (!result?.ok) throw new Error(result?.error || 'stop_failed');
      setSession(null);
      setDomains([]);
      setHex('');
      setStatus('Sekaiemu stopped.');
    } catch (err) {
      setError(String((err as Error)?.message || err || 'Stop failed.'));
    } finally {
      setBusy(false);
    }
  };

  const loadDomains = async () => {
    if (!runningPid) return;
    setBusy(true);
    setError('');
    try {
      const result = await runtime.sekaiEmuMemoryDomains?.({ pid: runningPid }) as { ok?: boolean; domains?: MemoryDomain[]; error?: string } | undefined;
      if (!result?.ok) throw new Error(result?.error || 'domains_failed');
      const nextDomains = Array.isArray(result.domains) ? result.domains : [];
      setDomains(nextDomains);
      setDomain((current) => current || nextDomains[0]?.name || 'system_ram');
      setStatus(`${nextDomains.length} memory domain(s) loaded.`);
    } catch (err) {
      setError(String((err as Error)?.message || err || 'Unable to read memory domains.'));
    } finally {
      setBusy(false);
    }
  };

  const readMemory = async () => {
    if (!runningPid) return;
    setBusy(true);
    setError('');
    try {
      const parsedAddress = parseAddress(address);
      const parsedSize = Number.parseInt(readSize, 10);
      if (!Number.isFinite(parsedAddress) || parsedAddress < 0) throw new Error('Invalid address.');
      if (!Number.isFinite(parsedSize) || parsedSize <= 0) throw new Error('Invalid size.');
      const result = await runtime.sekaiEmuReadMemory?.({
        pid: runningPid,
        domain,
        address: parsedAddress,
        size: parsedSize,
      }) as { ok?: boolean; hex?: string; error?: string } | undefined;
      if (!result?.ok) throw new Error(result?.error || 'read_failed');
      setHex(result.hex || '');
      setStatus(`Read ${parsedSize} byte(s) from ${domain}.`);
    } catch (err) {
      setError(String((err as Error)?.message || err || 'Read failed.'));
    } finally {
      setBusy(false);
    }
  };

  const dumpMemory = async () => {
    if (!runningPid) return;
    setBusy(true);
    setError('');
    try {
      const result = await runtime.sekaiEmuDumpMemory?.({ pid: runningPid, domain }) as { ok?: boolean; path?: string; error?: string } | undefined;
      if (!result?.ok) throw new Error(result?.error || 'dump_failed');
      setStatus(`Dump saved: ${result.path || 'done'}`);
    } catch (err) {
      setError(String((err as Error)?.message || err || 'Dump failed.'));
    } finally {
      setBusy(false);
    }
  };

  const exitLab = async () => {
    if (runningPid) await stop();
    clearRuntimeLabSession();
    window.location.reload();
  };

  return (
    <div className="min-h-screen bg-[#090b0f] text-white">
      <div className="mx-auto flex min-h-screen w-full max-w-7xl flex-col gap-6 px-8 py-8">
        <header className="flex items-center justify-between border-b border-[#203038] pb-5">
          <div>
            <div className="flex items-center gap-3">
              <div className="flex h-11 w-11 items-center justify-center rounded-lg bg-gradient-to-br from-[#ff6b35] to-[#4ecdc4] shadow-lg">
                <Gamepad2 className="h-6 w-6 text-[#091012]" />
              </div>
              <div>
                <h1 className="text-2xl font-black tracking-wide">Sekaiemu Runtime Lab</h1>
                <p className="text-sm text-[#8e8f94]">Local debug mode. Nexus, social and lobbies stay offline here.</p>
              </div>
            </div>
          </div>
          <button
            onClick={exitLab}
            className="flex items-center gap-2 rounded-lg border border-[#2a3b42] px-4 py-2 text-sm font-bold text-[#cfd8dc] transition hover:border-[#ff6b35] hover:text-white"
          >
            <LogOut className="h-4 w-4" />
            Exit Lab
          </button>
        </header>

        <main className="grid flex-1 grid-cols-[minmax(0,1.1fr)_minmax(360px,0.9fr)] gap-6">
          <section className="space-y-5">
            <div className="rounded-xl border border-[#203038] bg-[#11161c] p-5 shadow-2xl shadow-black/30">
              <div className="mb-4 flex items-center justify-between gap-3">
                <div>
                  <h2 className="text-lg font-bold">Launch Target</h2>
                  <p className="text-xs text-[#8e8f94]">
                    Preview the Sekaiemu layout without a ROM, or launch a real ROM/core session.
                  </p>
                </div>
                <button
                  onClick={refreshCores}
                  disabled={busy}
                  className="flex items-center gap-2 rounded-lg bg-[#1d2830] px-3 py-2 text-xs font-bold text-[#95e1d3] transition hover:bg-[#26353f] disabled:opacity-50"
                >
                  <RefreshCw className={`h-4 w-4 ${busy ? 'animate-spin' : ''}`} />
                  Rescan cores
                </button>
              </div>

              <div className="space-y-4">
                <div className="grid gap-3 rounded-lg border border-[#203038] bg-[#0c1116] p-3 sm:grid-cols-3">
                  <label className="flex items-center gap-3 rounded-lg border border-[#2a3b42] px-3 py-2 text-sm font-bold text-[#cfd8dc]">
                    <input
                      type="checkbox"
                      checked={layoutPreview}
                      onChange={(event) => setLayoutPreview(event.target.checked)}
                      className="h-4 w-4 accent-[#4ecdc4]"
                    />
                    Layout preview
                  </label>
                  <label className="flex items-center gap-3 rounded-lg border border-[#2a3b42] px-3 py-2 text-sm font-bold text-[#cfd8dc]">
                    <input
                      type="checkbox"
                      checked={fullscreen}
                      onChange={(event) => setFullscreen(event.target.checked)}
                      className="h-4 w-4 accent-[#4ecdc4]"
                    />
                    Start fullscreen
                  </label>
                  <select
                    value={trackerDisplayMode}
                    onChange={(event) => setTrackerDisplayMode(event.target.value)}
                    className="rounded-lg border border-[#2a3b42] bg-[#090b0f] px-3 py-2 text-sm font-bold text-white outline-none focus:border-[#4ecdc4]"
                  >
                    <option value="split-screen">Tracker split</option>
                    <option value="toggle-screen">Tracker toggle</option>
                    <option value="pip-overlay">Tracker PiP</option>
                  </select>
                  {layoutPreview && (
                    <p className="sm:col-span-3 text-xs text-[#8e8f94]">
                      Layout preview opens Sekaiemu with a no-cartridge screen and offline tracker; ROM, core and Archipelago fields are optional.
                    </p>
                  )}
                </div>

                <label className="block">
                  <span className="mb-2 flex items-center gap-2 text-xs font-bold uppercase tracking-wide text-[#95e1d3]">
                    <FileArchive className="h-4 w-4" />
                    ROM
                  </span>
                  <div className="flex gap-2">
                    <input
                      value={romPath}
                      onChange={(event) => setRomPath(event.target.value)}
                      className="min-w-0 flex-1 rounded-lg border border-[#2a3b42] bg-[#090b0f] px-3 py-2 text-sm text-white outline-none focus:border-[#4ecdc4]"
                      placeholder="Path to .sfc, .nes, .gba, .gb, .gbc..."
                    />
                    <button onClick={pickRom} className="rounded-lg bg-[#26353f] px-3 text-[#cfd8dc] transition hover:bg-[#314652]">
                      <FolderOpen className="h-5 w-5" />
                    </button>
                  </div>
                </label>

                <label className="block">
                  <span className="mb-2 flex items-center gap-2 text-xs font-bold uppercase tracking-wide text-[#95e1d3]">
                    <Cpu className="h-4 w-4" />
                    Libretro Core
                  </span>
                  <select
                    value={corePath}
                    onChange={(event) => setCorePath(event.target.value)}
                    className="w-full rounded-lg border border-[#2a3b42] bg-[#090b0f] px-3 py-2 text-sm text-white outline-none focus:border-[#4ecdc4]"
                  >
                    {cores.length === 0 && <option value="">No cores detected</option>}
                    {cores.map((core) => (
                      <option key={core.path} value={core.path}>
                        {core.system} - {core.name}
                      </option>
                    ))}
                  </select>
                  {selectedCore && (
                    <div className="mt-2 break-all rounded-lg bg-[#0c1116] px-3 py-2 text-xs text-[#8e8f94]">
                      {selectedCore.path}
                    </div>
                  )}
                </label>

                <label className="block">
                  <span className="mb-2 flex items-center gap-2 text-xs font-bold uppercase tracking-wide text-[#95e1d3]">
                    <HardDriveDownload className="h-4 w-4" />
                    Tracker Pack Optional
                  </span>
                  <div className="flex gap-2">
                    <input
                      value={trackerPackPath}
                      onChange={(event) => setTrackerPackPath(event.target.value)}
                      className="min-w-0 flex-1 rounded-lg border border-[#2a3b42] bg-[#090b0f] px-3 py-2 text-sm text-white outline-none focus:border-[#4ecdc4]"
                      placeholder="Optional PopTracker pack folder"
                    />
                    <button onClick={pickTracker} className="rounded-lg bg-[#26353f] px-3 text-[#cfd8dc] transition hover:bg-[#314652]">
                      <FolderOpen className="h-5 w-5" />
                    </button>
                  </div>
                </label>
              </div>
            </div>

            <div className="rounded-xl border border-[#203038] bg-[#11161c] p-5">
              <h2 className="mb-4 text-lg font-bold">Archipelago Optional</h2>
              <div className="grid grid-cols-3 gap-3">
                <input
                  value={apHost}
                  onChange={(event) => setApHost(event.target.value)}
                  className="rounded-lg border border-[#2a3b42] bg-[#090b0f] px-3 py-2 text-sm outline-none focus:border-[#4ecdc4]"
                  placeholder="host:port"
                />
                <input
                  value={apSlot}
                  onChange={(event) => setApSlot(event.target.value)}
                  className="rounded-lg border border-[#2a3b42] bg-[#090b0f] px-3 py-2 text-sm outline-none focus:border-[#4ecdc4]"
                  placeholder="slot"
                />
                <input
                  value={apPass}
                  onChange={(event) => setApPass(event.target.value)}
                  className="rounded-lg border border-[#2a3b42] bg-[#090b0f] px-3 py-2 text-sm outline-none focus:border-[#4ecdc4]"
                  placeholder="password"
                  type="password"
                />
              </div>
            </div>

            <div className="flex gap-3">
              <button
                onClick={launch}
                disabled={launching || (!layoutPreview && (!romPath || !corePath))}
                className="flex items-center gap-2 rounded-lg bg-gradient-to-r from-[#ff6b35] to-[#f38181] px-5 py-3 text-sm font-black text-white shadow-lg transition hover:brightness-110 disabled:cursor-not-allowed disabled:opacity-50"
              >
                {launching ? <Loader2 className="h-5 w-5 animate-spin" /> : <Play className="h-5 w-5" />}
                Launch Sekaiemu
              </button>
              <button
                onClick={stop}
                disabled={!runningPid || busy}
                className="flex items-center gap-2 rounded-lg border border-[#f85149]/60 px-5 py-3 text-sm font-bold text-[#ff8a80] transition hover:bg-[#f85149]/10 disabled:cursor-not-allowed disabled:opacity-50"
              >
                <Square className="h-5 w-5" />
                Stop
              </button>
            </div>
          </section>

          <section className="space-y-5">
            <div className="rounded-xl border border-[#203038] bg-[#11161c] p-5">
              <div className="mb-4 flex items-center justify-between">
                <div>
                  <h2 className="text-lg font-bold">Session</h2>
                  <p className="text-xs text-[#8e8f94]">Runtime process and socket details.</p>
                </div>
                <div className={`rounded-full px-3 py-1 text-xs font-black ${runningPid ? 'bg-[#4ecdc4] text-[#091012]' : 'bg-[#2a3b42] text-[#8e8f94]'}`}>
                  {runningPid ? `PID ${runningPid}` : 'Stopped'}
                </div>
              </div>
              <div className="space-y-2 text-xs text-[#cfd8dc]">
                <div className="break-all"><span className="text-[#8e8f94]">Mode:</span> {(session ? session.layoutPreview : layoutPreview) ? 'Layout preview' : 'ROM/Core runtime'}</div>
                <div className="break-all"><span className="text-[#8e8f94]">ROM:</span> {session?.romPath || romPath || '-'}</div>
                <div className="break-all"><span className="text-[#8e8f94]">Core:</span> {session?.corePath || corePath || '-'}</div>
                <div className="break-all"><span className="text-[#8e8f94]">Memory:</span> {session?.memorySocketPath || '-'}</div>
              </div>
              {status && <div className="mt-4 rounded-lg border border-[#4ecdc4]/30 bg-[#4ecdc4]/10 px-3 py-2 text-xs text-[#95e1d3]">{status}</div>}
              {error && <div className="mt-4 rounded-lg border border-[#f85149]/40 bg-[#f85149]/10 px-3 py-2 text-xs text-[#ffb4ab]">{error}</div>}
            </div>

            <div className="rounded-xl border border-[#203038] bg-[#11161c] p-5">
              <div className="mb-4 flex items-center justify-between gap-3">
                <div>
                  <h2 className="flex items-center gap-2 text-lg font-bold">
                    <MemoryStick className="h-5 w-5 text-[#4ecdc4]" />
                    Memory Tools
                  </h2>
                  <p className="text-xs text-[#8e8f94]">Read small ranges or dump a full domain.</p>
                </div>
                <button
                  onClick={loadDomains}
                  disabled={!runningPid || busy}
                  className="flex items-center gap-2 rounded-lg bg-[#26353f] px-3 py-2 text-xs font-bold text-[#cfd8dc] transition hover:bg-[#314652] disabled:opacity-50"
                >
                  <Database className="h-4 w-4" />
                  Domains
                </button>
              </div>

              <div className="space-y-3">
                <select
                  value={domain}
                  onChange={(event) => setDomain(event.target.value)}
                  className="w-full rounded-lg border border-[#2a3b42] bg-[#090b0f] px-3 py-2 text-sm outline-none focus:border-[#4ecdc4]"
                >
                  {domains.length === 0 && <option value={domain}>{domain}</option>}
                  {domains.map((entry) => (
                    <option key={entry.name} value={entry.name}>
                      {entry.name} - {formatDomainSize(entry.size)}
                    </option>
                  ))}
                </select>
                <div className="grid grid-cols-2 gap-3">
                  <input
                    value={address}
                    onChange={(event) => setAddress(event.target.value)}
                    className="rounded-lg border border-[#2a3b42] bg-[#090b0f] px-3 py-2 text-sm outline-none focus:border-[#4ecdc4]"
                    placeholder="0x0000"
                  />
                  <input
                    value={readSize}
                    onChange={(event) => setReadSize(event.target.value)}
                    className="rounded-lg border border-[#2a3b42] bg-[#090b0f] px-3 py-2 text-sm outline-none focus:border-[#4ecdc4]"
                    placeholder="256"
                  />
                </div>
                <div className="flex gap-3">
                  <button
                    onClick={readMemory}
                    disabled={!runningPid || busy}
                    className="flex-1 rounded-lg bg-[#4ecdc4] px-4 py-2 text-sm font-black text-[#091012] transition hover:brightness-110 disabled:opacity-50"
                  >
                    Read Hex
                  </button>
                  <button
                    onClick={dumpMemory}
                    disabled={!runningPid || busy}
                    className="flex-1 rounded-lg bg-[#26353f] px-4 py-2 text-sm font-bold text-[#cfd8dc] transition hover:bg-[#314652] disabled:opacity-50"
                  >
                    Dump RAW
                  </button>
                </div>
                <pre className="min-h-[170px] max-h-[260px] overflow-auto rounded-lg border border-[#203038] bg-[#070a0d] p-3 font-mono text-xs leading-relaxed text-[#d8f3ef]">
                  {hex || 'No memory read yet.'}
                </pre>
              </div>
            </div>
          </section>
        </main>
      </div>
    </div>
  );
}
