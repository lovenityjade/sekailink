import { useCallback, useEffect, useState } from 'react';
import { Download, Gamepad2, HardDrive, RefreshCw, Trash2 } from 'lucide-react';
import { runtime, type PcPackageEntry } from '../../services/runtime';

const statusLabel = (entry: PcPackageEntry) => {
  if (entry.installed) return `Installed ${entry.installed.version}`;
  if (entry.status === 'building') return 'In development';
  if (entry.status === 'testing') return 'Ready for testing';
  if (entry.status === 'planned') return 'Coming later';
  return 'Unavailable';
};

export default function AppStorePage() {
  const [packages, setPackages] = useState<PcPackageEntry[]>([]);
  const [loading, setLoading] = useState(true);
  const [busyId, setBusyId] = useState('');
  const [error, setError] = useState('');

  const load = useCallback(async (refresh = false) => {
    setLoading(true);
    setError('');
    try {
      const catalog = await runtime.listPcPackages({ refresh });
      setPackages(Array.isArray(catalog?.packages) ? catalog.packages : []);
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Unable to load Games & Mods.');
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => { void load(false); }, [load]);

  const install = async (entry: PcPackageEntry) => {
    setBusyId(entry.id);
    setError('');
    try {
      await runtime.installPcPackage(entry.id);
      await load(false);
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Installation failed.');
    } finally {
      setBusyId('');
    }
  };

  const uninstall = async (entry: PcPackageEntry) => {
    setBusyId(entry.id);
    setError('');
    try {
      await runtime.uninstallPcPackage(entry.id);
      await load(false);
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : 'Uninstall failed.');
    } finally {
      setBusyId('');
    }
  };

  return (
    <main className="h-full overflow-y-auto px-8 py-7">
      <div className="mx-auto max-w-6xl">
        <header className="mb-6 flex items-center justify-between gap-4">
          <div>
            <h1 className="text-3xl font-bold">Games & Mods</h1>
            <p className="mt-1 text-sm text-[#8e8f94]">Optional native integrations, installed separately from Client Core.</p>
          </div>
          <button type="button" title="Refresh catalog" onClick={() => void load(true)} disabled={loading} className="flex h-10 w-10 items-center justify-center rounded-md border border-[#2a2b30] bg-[#1c1d22] hover:bg-[#2a2b30] disabled:opacity-50">
            <RefreshCw className={`h-4 w-4 ${loading ? 'animate-spin' : ''}`} />
          </button>
        </header>

        {error && <div className="mb-5 border border-[#ff6b6b] bg-[#2a171b] px-4 py-3 text-sm text-[#ffb3b3]">{error}</div>}
        <div className="grid grid-cols-1 gap-4 xl:grid-cols-2">
          {packages.map((entry) => {
            const releaseAvailable = Boolean(entry.availableRelease);
            const busy = busyId === entry.id;
            return (
              <article key={entry.id} className="border border-[#2a2b30] bg-[#17191f] p-5">
                <div className="flex items-start gap-4">
                  <div className="flex h-11 w-11 shrink-0 items-center justify-center rounded-md border border-[#2f6f6b] bg-[#102523] text-[#4ecdc4]"><Gamepad2 className="h-6 w-6" /></div>
                  <div className="min-w-0 flex-1">
                    <div className="flex flex-wrap items-center gap-2">
                      <h2 className="text-lg font-semibold">{entry.name}</h2>
                      <span className="rounded-sm bg-[#252830] px-2 py-1 text-xs text-[#b8bac2]">{statusLabel(entry)}</span>
                    </div>
                    <p className="mt-2 text-sm text-[#a7a9b0]">{entry.summary}</p>
                    <div className="mt-4 flex items-center gap-2 text-xs text-[#8e8f94]"><HardDrive className="h-3.5 w-3.5" /> Installed outside routine SekaiLink updates</div>
                  </div>
                </div>
                <div className="mt-5 flex justify-end border-t border-[#2a2b30] pt-4">
                  {entry.installed ? (
                    <button type="button" onClick={() => void uninstall(entry)} disabled={busy} className="flex items-center gap-2 rounded-md border border-[#5a3438] px-4 py-2 text-sm text-[#ff9ca5] hover:bg-[#2a171b] disabled:opacity-50"><Trash2 className="h-4 w-4" /> Uninstall</button>
                  ) : (
                    <button type="button" onClick={() => void install(entry)} disabled={busy || !releaseAvailable} className="flex items-center gap-2 rounded-md bg-[#4ecdc4] px-4 py-2 text-sm font-semibold text-[#071817] hover:bg-[#6edbd4] disabled:cursor-not-allowed disabled:bg-[#303238] disabled:text-[#777980]"><Download className="h-4 w-4" /> {releaseAvailable ? 'Install' : statusLabel(entry)}</button>
                  )}
                </div>
              </article>
            );
          })}
        </div>
        {!loading && !packages.length && <div className="border border-[#2a2b30] bg-[#17191f] p-8 text-center text-[#8e8f94]">No optional packages are available for this platform.</div>}
      </div>
    </main>
  );
}
