import { useCallback, useEffect, useMemo, useState } from 'react';
import {
  Settings, Globe, FolderOpen, Monitor, Volume2, Gamepad2,
  Terminal, Users, ShieldOff, ChevronDown, ChevronRight,
  Play, Square, Check, AlertCircle, XCircle, FolderSearch,
  Trash2, Save, User
} from 'lucide-react';
import { gameSetupRegistry } from '../../data/gameSetup';
import { runtime } from '../../services/runtime';
import { trace, traceError } from '../../services/trace';
import { ErrorModal } from './FeedbackModal';

type SettingsSection = 'general' | 'rom-library' | 'video-layout' | 'audio' | 'games-automation' | 'linux-windowing' | 'social' | 'blocked-users';

interface ROM {
  id: string;
  gameId: string;
  displayName: string;
  path: string;
  configured: boolean;
}

interface BlockedUser {
  id: string;
  displayName: string;
  userId: string;
  avatar?: string;
}

export default function SettingsPage() {
  const [activeSection, setActiveSection] = useState<SettingsSection>('general');

  // General
  const [language, setLanguage] = useState('en');
  const [themeVariant, setThemeVariant] = useState('variant-a');
  const [crashReporting, setCrashReporting] = useState(true);
  const [diagnosticsStatus, setDiagnosticsStatus] = useState('');

  // ROM Library
  const [configuredRoms, setConfiguredRoms] = useState<Record<string, string>>({});
  const [romSearch, setRomSearch] = useState('');
  const [romStatus, setRomStatus] = useState('');
  const [romError, setRomError] = useState('');

  // Video/Layout
  const [layoutMode, setLayoutMode] = useState('auto');
  const [splitRatio, setSplitRatio] = useState(50);
  const [gameDisplay, setGameDisplay] = useState('primary');
  const [trackerDisplay, setTrackerDisplay] = useState('secondary');

  // Audio
  const [muteAll, setMuteAll] = useState(false);
  const [globalVolume, setGlobalVolume] = useState(80);
  const [bgMusicEnabled, setBgMusicEnabled] = useState(true);
  const [bgMusicTrack, setBgMusicTrack] = useState('track-1');
  const [bgMusicVolume, setBgMusicVolume] = useState(50);

  // Games/Automation
  const [expandedGame, setExpandedGame] = useState<string | null>(null);
  const [factorioModsPath, setFactorioModsPath] = useState('');
  const [factorioExePath, setFactorioExePath] = useState('');
  const [gzdoomExePath, setGzdoomExePath] = useState('');
  const [gzdoomIwadPath, setGzdoomIwadPath] = useState('');

  // Linux Windowing
  const [gamescopeEnabled, setGamescopeEnabled] = useState(false);
  const [gamescopeMode, setGamescopeMode] = useState('prefer-fallback');
  const [gamescopeFullscreen, setGamescopeFullscreen] = useState(false);
  const [gamescopeWidth, setGamescopeWidth] = useState(1920);
  const [gamescopeHeight, setGamescopeHeight] = useState(1080);

  // Social
  const [presenceStatus, setPresenceStatus] = useState('online');
  const [dmPolicy, setDmPolicy] = useState('anyone');

  // Blocked Users
  const [blockedUsers, setBlockedUsers] = useState<BlockedUser[]>([]);

  const knownRomGames = useMemo(
    () => gameSetupRegistry.filter((entry) => entry.romKey),
    []
  );
  const roms = useMemo<ROM[]>(() => (
    knownRomGames.map((entry) => {
      const key = entry.romKey || entry.gameId;
      const path = String(configuredRoms[key] || '');
      return {
        id: key,
        gameId: key,
        displayName: entry.displayName,
        path,
        configured: Boolean(path),
      };
    })
  ), [configuredRoms, knownRomGames]);
  const configuredRomCount = roms.filter((rom) => rom.configured).length;
  const filteredRoms = roms.filter(rom =>
    rom.gameId.toLowerCase().includes(romSearch.toLowerCase()) ||
    rom.displayName.toLowerCase().includes(romSearch.toLowerCase()) ||
    rom.path.toLowerCase().includes(romSearch.toLowerCase())
  );

  const navItems = [
    { id: 'general' as SettingsSection, label: 'General', icon: <Settings className="w-4 h-4" /> },
    { id: 'rom-library' as SettingsSection, label: 'ROM Library', icon: <FolderOpen className="w-4 h-4" /> },
    { id: 'video-layout' as SettingsSection, label: 'Video / Layout', icon: <Monitor className="w-4 h-4" /> },
    { id: 'audio' as SettingsSection, label: 'Audio', icon: <Volume2 className="w-4 h-4" /> },
    { id: 'games-automation' as SettingsSection, label: 'Games / Automation', icon: <Gamepad2 className="w-4 h-4" /> },
    { id: 'linux-windowing' as SettingsSection, label: 'Linux Windowing', icon: <Terminal className="w-4 h-4" /> },
    { id: 'social' as SettingsSection, label: 'Social', icon: <Users className="w-4 h-4" /> },
    { id: 'blocked-users' as SettingsSection, label: 'Blocked Users', icon: <ShieldOff className="w-4 h-4" /> },
  ];

  const refreshConfiguredRoms = useCallback(async () => {
    trace('settings', 'rom_config_load_start');
    try {
      const config = await runtime.configGet?.();
      const next = config && typeof config === 'object' && (config as any).roms && typeof (config as any).roms === 'object'
        ? (config as any).roms as Record<string, string>
        : {};
      setConfiguredRoms(next);
      trace('settings', 'rom_config_load_success', { count: Object.keys(next).length });
    } catch (error) {
      traceError('settings', 'rom_config_load_failed', error);
      setRomError('Could not load ROM configuration.');
    }
  }, []);

  useEffect(() => {
    void refreshConfiguredRoms();
  }, [refreshConfiguredRoms]);

  const pickRomFile = useCallback(async (title: string) => {
    return runtime.pickFile?.({
      title,
      filters: [
        { name: 'ROM files', extensions: ['sfc', 'smc', 'nes', 'gba', 'gb', 'gbc', 'z64', 'n64', 'v64', 'rom', 'bin'] },
        { name: 'All files', extensions: ['*'] },
      ],
    });
  }, []);

  const handleImportRom = useCallback(async () => {
    setRomStatus('');
    setRomError('');
    trace('settings', 'rom_import_start');
    try {
      const pick = await pickRomFile('Import ROM File');
      if (!pick || pick.canceled || !pick.path) {
        trace('settings', 'rom_import_cancelled');
        return;
      }
      const imported = await runtime.romsImport?.(pick.path);
      if (!imported || !(imported as any).ok) {
        const error = String((imported as any)?.error || 'import_failed');
        setRomError(
          error === 'hash_mismatch'
            ? 'ROM not recognized automatically. Use Set Base ROM on the matching game card below.'
            : `ROM import failed: ${error}`
        );
        trace('settings', 'rom_import_failed', { error });
        return;
      }
      await refreshConfiguredRoms();
      setRomStatus(`Imported ${(imported as any).gameId || 'ROM'} successfully.`);
      trace('settings', 'rom_import_success', { gameId: (imported as any).gameId });
    } catch (error) {
      traceError('settings', 'rom_import_error', error);
      setRomError('ROM import failed. See trace logs for details.');
    }
  }, [pickRomFile, refreshConfiguredRoms]);

  const handleSetBaseRom = useCallback(async (rom: ROM) => {
    setRomStatus('');
    setRomError('');
    trace('settings', 'rom_set_base_start', { gameId: rom.gameId });
    try {
      const pick = await pickRomFile(`Set Base ROM for ${rom.displayName}`);
      if (!pick || pick.canceled || !pick.path) {
        trace('settings', 'rom_set_base_cancelled', { gameId: rom.gameId });
        return;
      }
      const saved = await runtime.configSetRom?.(rom.gameId, pick.path);
      if (!saved || !(saved as any).ok) {
        const error = String((saved as any)?.error || 'config_failed');
        setRomError(`Could not save ${rom.displayName}: ${error}`);
        trace('settings', 'rom_set_base_failed', { gameId: rom.gameId, error });
        return;
      }
      await refreshConfiguredRoms();
      setRomStatus(`${rom.displayName} base ROM configured.`);
      trace('settings', 'rom_set_base_success', { gameId: rom.gameId });
    } catch (error) {
      traceError('settings', 'rom_set_base_error', error, { gameId: rom.gameId });
      setRomError(`Could not configure ${rom.displayName}.`);
    }
  }, [pickRomFile, refreshConfiguredRoms]);

  const handleDeleteRom = useCallback(async (rom: ROM) => {
    setRomStatus('');
    setRomError('');
    trace('settings', 'rom_delete_start', { gameId: rom.gameId });
    try {
      const deleted = await runtime.configDeleteRom?.(rom.gameId);
      if (!deleted || !(deleted as any).ok) {
        const error = String((deleted as any)?.error || 'delete_failed');
        setRomError(`Could not remove ${rom.displayName}: ${error}`);
        trace('settings', 'rom_delete_failed', { gameId: rom.gameId, error });
        return;
      }
      await refreshConfiguredRoms();
      setRomStatus(`${rom.displayName} base ROM removed.`);
      trace('settings', 'rom_delete_success', { gameId: rom.gameId });
    } catch (error) {
      traceError('settings', 'rom_delete_error', error, { gameId: rom.gameId });
      setRomError(`Could not remove ${rom.displayName}.`);
    }
  }, [refreshConfiguredRoms]);

  const handleShowRom = useCallback(async (rom: ROM) => {
    if (!rom.path) return;
    try {
      await runtime.showItemInFolder?.(rom.path);
    } catch (error) {
      traceError('settings', 'rom_show_failed', error, { gameId: rom.gameId });
      setRomError(`Could not open ${rom.displayName} in the file manager.`);
    }
  }, []);

  const buttonPrimary = 'px-4 py-2 bg-gradient-to-r from-[#ff6b35] to-[#f38181] rounded-lg text-sm font-medium shadow-lg hover:shadow-xl glow-hover transition-all disabled:opacity-50 disabled:cursor-not-allowed';
  const buttonSecondary = 'px-3 py-1.5 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-xs font-medium transition-colors disabled:opacity-50 disabled:cursor-not-allowed';
  const buttonDanger = 'w-8 h-8 bg-[#2a2b30] hover:bg-[#f85149] rounded-lg flex items-center justify-center transition-colors group disabled:opacity-50 disabled:cursor-not-allowed';
  const inputClass = 'w-full pl-10 pr-4 py-2.5 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4ecdc4] outline-none transition-colors';

  const StatusBadge = ({ status }: { status: 'configured' | 'missing' | 'optional' | 'error' }) => {
    const config = {
      configured: { icon: <Check className="w-3 h-3" />, label: 'Configured', color: 'text-[#4ecdc4]', bg: 'bg-[#4ecdc4]/20' },
      missing: { icon: <XCircle className="w-3 h-3" />, label: 'Missing', color: 'text-[#f85149]', bg: 'bg-[#f85149]/20' },
      optional: { icon: <AlertCircle className="w-3 h-3" />, label: 'Optional', color: 'text-[#f69d50]', bg: 'bg-[#f69d50]/20' },
      error: { icon: <XCircle className="w-3 h-3" />, label: 'Error', color: 'text-[#f85149]', bg: 'bg-[#f85149]/20' },
    };
    const { icon, label, color, bg } = config[status];
    return (
      <span className={`inline-flex items-center gap-1 px-2 py-0.5 rounded text-xs font-bold ${bg} ${color}`}>
        {icon}
        {label}
      </span>
    );
  };

  return (
    <div className="flex h-full">
      {/* Left Navigation */}
      <div className="w-64 bg-[#0e0f13]/95 backdrop-blur-sm border-r border-[#2a2b30] p-4">
        <div className="mb-6">
          <h2 className="text-lg font-bold mb-1">Settings</h2>
          <p className="text-xs text-[#8e8f94]">Configure SekaiLink</p>
        </div>
        <nav className="space-y-1">
          {navItems.map(item => (
            <button
              key={item.id}
              onClick={() => setActiveSection(item.id)}
              className={`w-full flex items-center gap-3 px-3 py-2.5 rounded-lg text-sm transition-all ${
                activeSection === item.id
                  ? 'bg-gradient-to-r from-[#4ecdc4]/20 to-transparent text-white border-l-2 border-[#4ecdc4]'
                  : 'text-[#8e8f94] hover:text-white hover:bg-[#1c1d22]'
              }`}
            >
              {item.icon}
              <span>{item.label}</span>
            </button>
          ))}
        </nav>
      </div>

      {/* Main Content */}
      <div className="flex-1 overflow-y-auto p-8">
        {/* General Section */}
        {activeSection === 'general' && (
          <div className="max-w-3xl space-y-6">
            <div>
              <h1 className="text-2xl font-bold mb-1">General</h1>
              <p className="text-sm text-[#8e8f94]">Basic application settings</p>
            </div>

            {/* Language */}
            <div className="bg-gradient-card border-2 border-[#2a2b30] rounded-lg p-5 space-y-4">
              <div className="flex items-center gap-2">
                <Globe className="w-5 h-5 text-[#4ecdc4]" />
                <h3 className="font-bold">Language</h3>
              </div>
              <div>
                <label className="block text-sm text-[#8e8f94] mb-2">Interface Language</label>
                <select
                  value={language}
                  onChange={(e) => setLanguage(e.target.value)}
                  className="w-full px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none"
                >
                  <option value="en">English</option>
                  <option value="fr">Français</option>
                  <option value="es">Español</option>
                  <option value="de">Deutsch</option>
                  <option value="ja">日本語</option>
                </select>
              </div>
            </div>

            {/* Theme */}
            <div className="bg-gradient-card border-2 border-[#2a2b30] rounded-lg p-5 space-y-4">
              <h3 className="font-bold">UI Theme</h3>
              <div>
                <label className="block text-sm text-[#8e8f94] mb-2">Theme Variant</label>
                <div className="grid grid-cols-3 gap-3">
                  {['variant-a', 'variant-b', 'variant-c'].map((variant, idx) => (
                    <button
                      key={variant}
                      onClick={() => setThemeVariant(variant)}
                      className={`p-3 rounded-lg border-2 transition-all ${
                        themeVariant === variant
                          ? 'border-[#4ecdc4] bg-[#4ecdc4]/10'
                          : 'border-[#2a2b30] bg-[#1c1d22] hover:border-[#3a3b40]'
                      }`}
                    >
                      <div className="text-sm font-bold mb-1">Variant {String.fromCharCode(65 + idx)}</div>
                      <div className="text-xs text-[#8e8f94]">Color scheme {idx + 1}</div>
                    </button>
                  ))}
                </div>
              </div>
            </div>

            {/* Diagnostics */}
            <div className="bg-gradient-card border-2 border-[#2a2b30] rounded-lg p-5 space-y-4">
              <h3 className="font-bold">Diagnostics & Crash Reporting</h3>
              <div className="flex items-center justify-between">
                <div>
                  <div className="text-sm font-medium">Enable Crash Reporting</div>
                  <div className="text-xs text-[#8e8f94] mt-0.5">Help improve SekaiLink by sending crash reports</div>
                </div>
                <button
                  onClick={() => setCrashReporting(!crashReporting)}
                  className={`relative w-12 h-6 rounded-full transition-colors ${
                    crashReporting ? 'bg-[#4ecdc4]' : 'bg-[#2a2b30]'
                  }`}
                >
                  <div className={`absolute top-1 left-1 w-4 h-4 bg-white rounded-full transition-transform ${
                    crashReporting ? 'translate-x-6' : ''
                  }`} />
                </button>
              </div>
              <button
                onClick={() => setDiagnosticsStatus('Diagnostics sent successfully!')}
                className="px-4 py-2 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm font-medium transition-colors"
              >
                Send Diagnostics Now
              </button>
              {diagnosticsStatus && (
                <div className="p-3 bg-[#4ecdc4]/10 border border-[#4ecdc4] rounded-lg text-sm text-[#4ecdc4]">
                  {diagnosticsStatus}
                </div>
              )}
            </div>
          </div>
        )}

        {/* ROM Library Section */}
        {activeSection === 'rom-library' && (
          <div className="max-w-4xl space-y-6">
            <div className="flex items-center justify-between">
              <div>
                <h1 className="text-2xl font-bold mb-1">ROM Library</h1>
                <p className="text-sm text-[#8e8f94]">{configuredRomCount}/{roms.length} base files configured</p>
              </div>
              <button
                onClick={() => void handleImportRom()}
                className={buttonPrimary}
              >
                Import ROM File
              </button>
            </div>

            {romStatus && (
              <div className="p-3 bg-[#4ecdc4]/10 border border-[#4ecdc4] rounded-lg text-sm text-[#4ecdc4]">
                {romStatus}
              </div>
            )}

            {/* Search */}
            <div className="relative">
              <FolderSearch className="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-[#8e8f94]" />
              <input
                type="text"
                value={romSearch}
                onChange={(e) => setRomSearch(e.target.value)}
                placeholder="Search by game ID or path..."
                className={inputClass}
              />
            </div>

            {/* ROM List */}
            {filteredRoms.length > 0 ? (
              <div className="space-y-3">
                {filteredRoms.map((rom) => (
                  <div key={rom.id} className="bg-gradient-card border-2 border-[#2a2b30] rounded-lg p-4 hover:border-[#4ecdc4] transition-all">
                    <div className="flex items-center justify-between gap-4">
                      <div className="flex-1 min-w-0">
                        <div className="flex flex-wrap items-center gap-2 mb-1">
                          <div className="font-bold">{rom.displayName}</div>
                          <StatusBadge status={rom.configured ? 'configured' : 'missing'} />
                        </div>
                        <div className="text-xs text-[#8e8f94] mb-1">Config key: {rom.gameId}</div>
                        <div className="text-sm text-[#8e8f94] font-mono truncate">
                          {rom.path || 'No base ROM selected'}
                        </div>
                      </div>
                      <div className="flex items-center justify-end gap-2 ml-4 shrink-0">
                        <button
                          onClick={() => void handleSetBaseRom(rom)}
                          className={buttonSecondary}
                          title={`Choose a base ROM file for ${rom.displayName}`}
                          aria-label={`Choose a base ROM file for ${rom.displayName}`}
                        >
                          Set Base ROM
                        </button>
                        <button
                          onClick={() => void handleShowRom(rom)}
                          disabled={!rom.path}
                          className={`${buttonSecondary} flex items-center gap-1.5`}
                          title={`Show ${rom.displayName} base ROM in folder`}
                          aria-label={`Show ${rom.displayName} base ROM in folder`}
                        >
                          <FolderOpen className="w-3 h-3" />
                          Show in Folder
                        </button>
                        <button
                          onClick={() => void handleDeleteRom(rom)}
                          disabled={!rom.path}
                          className={buttonDanger}
                          title={`Remove ${rom.displayName} base ROM`}
                          aria-label={`Remove ${rom.displayName} base ROM`}
                        >
                          <Trash2 className="w-3.5 h-3.5 text-[#8e8f94] group-hover:text-white" />
                        </button>
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            ) : (
              <div className="text-center py-20 bg-gradient-card border-2 border-[#2a2b30] rounded-lg">
                <FolderOpen className="w-16 h-16 text-[#8e8f94] mx-auto mb-4 opacity-50" />
                <p className="text-lg text-[#8e8f94] mb-2">No matching games</p>
                <p className="text-sm text-[#8e8f94]">Clear the search to see supported base ROM slots</p>
              </div>
            )}
          </div>
        )}

        {/* Video/Layout Section */}
        {activeSection === 'video-layout' && (
          <div className="max-w-3xl space-y-6">
            <div>
              <h1 className="text-2xl font-bold mb-1">Video / Layout</h1>
              <p className="text-sm text-[#8e8f94]">Configure window and display settings</p>
            </div>

            <div className="bg-gradient-card border-2 border-[#2a2b30] rounded-lg p-5 space-y-4">
              <div className="flex items-center gap-2 pb-3 border-b border-[#2a2b30]">
                <Monitor className="w-5 h-5 text-[#4ecdc4]" />
                <h3 className="font-bold">Display Configuration</h3>
              </div>

              <div className="p-3 bg-[#1c1d22] rounded-lg border border-[#2a2b30] flex items-center gap-2">
                <Check className="w-4 h-4 text-[#4ecdc4]" />
                <span className="text-sm">Window manager detected: X11</span>
              </div>

              <div>
                <label className="block text-sm font-medium mb-2">Layout Mode</label>
                <select
                  value={layoutMode}
                  onChange={(e) => setLayoutMode(e.target.value)}
                  className="w-full px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none"
                >
                  <option value="auto">Auto</option>
                  <option value="side-by-side">Side-by-side</option>
                  <option value="separate">Separate Displays</option>
                </select>
              </div>

              <div>
                <label className="block text-sm font-medium mb-2">Split Ratio: {splitRatio}%</label>
                <input
                  type="range"
                  min="0"
                  max="100"
                  value={splitRatio}
                  onChange={(e) => setSplitRatio(Number(e.target.value))}
                  className="w-full accent-[#4ecdc4]"
                />
              </div>

              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="block text-sm font-medium mb-2">Game Display</label>
                  <select
                    value={gameDisplay}
                    onChange={(e) => setGameDisplay(e.target.value)}
                    className="w-full px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none"
                  >
                    <option value="primary">Primary</option>
                    <option value="secondary">Secondary</option>
                  </select>
                </div>
                <div>
                  <label className="block text-sm font-medium mb-2">Tracker Display</label>
                  <select
                    value={trackerDisplay}
                    onChange={(e) => setTrackerDisplay(e.target.value)}
                    className="w-full px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none"
                  >
                    <option value="primary">Primary</option>
                    <option value="secondary">Secondary</option>
                  </select>
                </div>
              </div>

              <div className="flex flex-wrap gap-2 pt-3 border-t border-[#2a2b30]">
                <button className="px-3 py-1.5 bg-[#4ecdc4] hover:bg-[#3db8b0] text-[#14151a] rounded-lg text-sm font-medium transition-colors">
                  Save Layout
                </button>
                <button className="px-3 py-1.5 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm transition-colors">
                  Handheld Preset
                </button>
                <button className="px-3 py-1.5 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm transition-colors">
                  Desktop Side-by-side
                </button>
                <button className="px-3 py-1.5 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm transition-colors">
                  Dual Display
                </button>
                <button className="px-3 py-1.5 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm transition-colors">
                  Streamer Dual
                </button>
              </div>
            </div>
          </div>
        )}

        {/* Audio Section */}
        {activeSection === 'audio' && (
          <div className="max-w-3xl space-y-6">
            <div>
              <h1 className="text-2xl font-bold mb-1">Audio</h1>
              <p className="text-sm text-[#8e8f94]">Configure sound and music settings</p>
            </div>

            {/* Global Volume */}
            <div className="bg-gradient-card border-2 border-[#2a2b30] rounded-lg p-5 space-y-4">
              <div className="flex items-center gap-2 pb-3 border-b border-[#2a2b30]">
                <Volume2 className="w-5 h-5 text-[#4ecdc4]" />
                <h3 className="font-bold">Global Audio</h3>
              </div>

              <div className="flex items-center justify-between">
                <div>
                  <div className="text-sm font-medium">Mute All Sounds</div>
                  <div className="text-xs text-[#8e8f94] mt-0.5">Disable all audio output</div>
                </div>
                <button
                  onClick={() => setMuteAll(!muteAll)}
                  className={`relative w-12 h-6 rounded-full transition-colors ${
                    muteAll ? 'bg-[#f85149]' : 'bg-[#2a2b30]'
                  }`}
                >
                  <div className={`absolute top-1 left-1 w-4 h-4 bg-white rounded-full transition-transform ${
                    muteAll ? 'translate-x-6' : ''
                  }`} />
                </button>
              </div>

              <div>
                <label className="block text-sm font-medium mb-2">Global Volume: {globalVolume}%</label>
                <input
                  type="range"
                  min="0"
                  max="100"
                  value={globalVolume}
                  onChange={(e) => setGlobalVolume(Number(e.target.value))}
                  className="w-full accent-[#4ecdc4]"
                  disabled={muteAll}
                />
              </div>
            </div>

            {/* Background Music */}
            <div className="bg-gradient-card border-2 border-[#2a2b30] rounded-lg p-5 space-y-4">
              <h3 className="font-bold">Background Music</h3>

              <div className="flex items-center justify-between">
                <div className="text-sm font-medium">Enable Background Music</div>
                <button
                  onClick={() => setBgMusicEnabled(!bgMusicEnabled)}
                  className={`relative w-12 h-6 rounded-full transition-colors ${
                    bgMusicEnabled ? 'bg-[#4ecdc4]' : 'bg-[#2a2b30]'
                  }`}
                >
                  <div className={`absolute top-1 left-1 w-4 h-4 bg-white rounded-full transition-transform ${
                    bgMusicEnabled ? 'translate-x-6' : ''
                  }`} />
                </button>
              </div>

              {bgMusicEnabled && (
                <>
                  <div>
                    <label className="block text-sm font-medium mb-2">Music Track</label>
                    <select
                      value={bgMusicTrack}
                      onChange={(e) => setBgMusicTrack(e.target.value)}
                      className="w-full px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none"
                    >
                      <option value="track-1">Ambient Chill</option>
                      <option value="track-2">Synthwave Dreams</option>
                      <option value="track-3">Lo-Fi Beats</option>
                      <option value="track-4">Cyber Drift</option>
                    </select>
                  </div>

                  <div>
                    <label className="block text-sm font-medium mb-2">Music Volume: {bgMusicVolume}%</label>
                    <input
                      type="range"
                      min="0"
                      max="100"
                      value={bgMusicVolume}
                      onChange={(e) => setBgMusicVolume(Number(e.target.value))}
                      className="w-full accent-[#4ecdc4]"
                    />
                  </div>

                  <div className="flex gap-2">
                    <button className="px-4 py-2 bg-[#4ecdc4] hover:bg-[#3db8b0] text-[#14151a] rounded-lg text-sm font-medium transition-colors flex items-center gap-2">
                      <Play className="w-3.5 h-3.5" />
                      Play BGM
                    </button>
                    <button className="px-4 py-2 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm font-medium transition-colors flex items-center gap-2">
                      <Square className="w-3.5 h-3.5" />
                      Stop BGM
                    </button>
                  </div>
                </>
              )}
            </div>

            {/* UI Sounds */}
            <div className="bg-gradient-card border-2 border-[#2a2b30] rounded-lg p-5 space-y-4">
              <h3 className="font-bold">UI Sound Events</h3>
              <div className="space-y-3">
                {['Button Click', 'Notification', 'Message Received', 'Lobby Join'].map((event) => (
                  <div key={event} className="p-3 bg-[#1c1d22] rounded-lg flex items-center gap-4">
                    <div className="flex-1 text-sm font-medium">{event}</div>
                    <input
                      type="range"
                      min="0"
                      max="100"
                      defaultValue="70"
                      className="w-32 accent-[#4ecdc4]"
                    />
                    <button className="w-8 h-8 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg flex items-center justify-center transition-colors">
                      <Play className="w-3 h-3" />
                    </button>
                  </div>
                ))}
              </div>
            </div>
          </div>
        )}

        {/* Games/Automation Section */}
        {activeSection === 'games-automation' && (
          <div className="max-w-4xl space-y-6">
            <div>
              <h1 className="text-2xl font-bold mb-1">Games / Automation Paths</h1>
              <p className="text-sm text-[#8e8f94]">Configure external runtimes and launch helpers</p>
            </div>

            {/* Game Panels */}
            {[
              { id: 'factorio', name: 'Factorio', status: 'configured' as const },
              { id: 'gzdoom', name: 'gzDoom', status: 'missing' as const },
              { id: 'sekaiemu', name: 'Sekaiemu Libretro Runtime', status: 'configured' as const },
              { id: 'sm64ex', name: 'SM64EX', status: 'optional' as const },
              { id: 'soh', name: 'Ship of Harkinian', status: 'optional' as const },
            ].map((game) => (
              <div key={game.id} className="bg-gradient-card border-2 border-[#2a2b30] rounded-lg overflow-hidden">
                <button
                  onClick={() => setExpandedGame(expandedGame === game.id ? null : game.id)}
                  className="w-full p-4 flex items-center justify-between hover:bg-[#1c1d22] transition-colors"
                >
                  <div className="flex items-center gap-3">
                    {expandedGame === game.id ? <ChevronDown className="w-5 h-5" /> : <ChevronRight className="w-5 h-5" />}
                    <Gamepad2 className="w-5 h-5 text-[#4ecdc4]" />
                    <span className="font-bold">{game.name}</span>
                    <StatusBadge status={game.status} />
                  </div>
                </button>

                {expandedGame === game.id && (
                  <div className="p-4 pt-0 space-y-4 border-t border-[#2a2b30]">
                    {game.id === 'factorio' && (
                      <>
                        <div>
                          <label className="block text-sm font-medium mb-2">Mods Directory Path</label>
                          <div className="flex gap-2">
                            <input
                              type="text"
                              value={factorioModsPath}
                              onChange={(e) => setFactorioModsPath(e.target.value)}
                              placeholder="/path/to/factorio/mods"
                              className="flex-1 px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4ecdc4] outline-none font-mono text-sm"
                            />
                            <button className="px-4 py-2 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm font-medium transition-colors">
                              Browse
                            </button>
                          </div>
                        </div>
                        <div>
                          <label className="block text-sm font-medium mb-2">Executable Path</label>
                          <div className="flex gap-2">
                            <input
                              type="text"
                              value={factorioExePath}
                              onChange={(e) => setFactorioExePath(e.target.value)}
                              placeholder="/path/to/factorio"
                              className="flex-1 px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4ecdc4] outline-none font-mono text-sm"
                            />
                            <button className="px-4 py-2 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm font-medium transition-colors">
                              Browse
                            </button>
                          </div>
                        </div>
                        <button className="px-4 py-2 bg-[#4ecdc4] hover:bg-[#3db8b0] text-[#14151a] rounded-lg text-sm font-medium transition-colors flex items-center gap-2">
                          <Save className="w-3.5 h-3.5" />
                          Save Factorio
                        </button>
                      </>
                    )}

                    {game.id === 'gzdoom' && (
                      <>
                        <div>
                          <label className="block text-sm font-medium mb-2">Executable Path</label>
                          <div className="flex gap-2">
                            <input
                              type="text"
                              value={gzdoomExePath}
                              onChange={(e) => setGzdoomExePath(e.target.value)}
                              placeholder="/path/to/gzdoom"
                              className="flex-1 px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4ecdc4] outline-none font-mono text-sm"
                            />
                            <button className="px-4 py-2 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm font-medium transition-colors">
                              Browse
                            </button>
                          </div>
                        </div>
                        <div>
                          <label className="block text-sm font-medium mb-2">IWAD Path</label>
                          <div className="flex gap-2">
                            <input
                              type="text"
                              value={gzdoomIwadPath}
                              onChange={(e) => setGzdoomIwadPath(e.target.value)}
                              placeholder="/path/to/doom2.wad"
                              className="flex-1 px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4ecdc4] outline-none font-mono text-sm"
                            />
                            <button className="px-4 py-2 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm font-medium transition-colors">
                              Browse
                            </button>
                          </div>
                        </div>
                        <div>
                          <label className="block text-sm font-medium mb-2">gzArchipelago.pk3 Path</label>
                          <div className="flex gap-2">
                            <input
                              type="text"
                              placeholder="/path/to/gzArchipelago.pk3"
                              className="flex-1 px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4ecdc4] outline-none font-mono text-sm"
                            />
                            <button className="px-4 py-2 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm font-medium transition-colors">
                              Browse
                            </button>
                          </div>
                        </div>
                        <div>
                          <label className="block text-sm font-medium mb-2">Extra Arguments (one per line)</label>
                          <textarea
                            rows={3}
                            placeholder="-iwad doom2.wad&#10;-file gzArchipelago.pk3"
                            className="w-full px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4ecdc4] outline-none font-mono text-sm resize-none"
                          />
                        </div>
                        <button className="px-4 py-2 bg-[#4ecdc4] hover:bg-[#3db8b0] text-[#14151a] rounded-lg text-sm font-medium transition-colors flex items-center gap-2">
                          <Save className="w-3.5 h-3.5" />
                          Save gzDoom
                        </button>
                      </>
                    )}

                    {game.id === 'sekaiemu' && (
                      <>
                        <div>
                          <label className="block text-sm font-medium mb-2">Executable Path</label>
                          <div className="flex gap-2">
                            <input
                              type="text"
                              placeholder="/path/to/sekaiemu"
                              className="flex-1 px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4ecdc4] outline-none font-mono text-sm"
                            />
                            <button className="px-4 py-2 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm font-medium transition-colors">
                              Browse
                            </button>
                          </div>
                        </div>
                        <div>
                          <label className="block text-sm font-medium mb-2">Root Directory</label>
                          <div className="flex gap-2">
                            <input
                              type="text"
                              placeholder="/path/to/sekaiemu/root"
                              className="flex-1 px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4ecdc4] outline-none font-mono text-sm"
                            />
                            <button className="px-4 py-2 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm font-medium transition-colors">
                              Browse
                            </button>
                          </div>
                        </div>
                        <div>
                          <label className="block text-sm font-medium mb-2">Libretro Core Directories (one per line)</label>
                          <textarea
                            rows={3}
                            placeholder="/usr/lib/libretro&#10;/home/user/.config/retroarch/cores"
                            className="w-full px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4ecdc4] outline-none font-mono text-sm resize-none"
                          />
                        </div>
                        <button className="px-4 py-2 bg-[#4ecdc4] hover:bg-[#3db8b0] text-[#14151a] rounded-lg text-sm font-medium transition-colors flex items-center gap-2">
                          <Save className="w-3.5 h-3.5" />
                          Save Sekaiemu
                        </button>
                      </>
                    )}
                  </div>
                )}
              </div>
            ))}
          </div>
        )}

        {/* Linux Windowing Section */}
        {activeSection === 'linux-windowing' && (
          <div className="max-w-3xl space-y-6">
            <div>
              <h1 className="text-2xl font-bold mb-1">Linux Windowing</h1>
              <p className="text-sm text-[#8e8f94]">Advanced Linux-specific display settings</p>
            </div>

            <div className="bg-gradient-card border-2 border-[#2a2b30] rounded-lg p-5 space-y-4">
              <div className="flex items-center gap-2 pb-3 border-b border-[#2a2b30]">
                <Terminal className="w-5 h-5 text-[#4ecdc4]" />
                <h3 className="font-bold">Gamescope Configuration</h3>
              </div>

              <div className="p-3 bg-[#1c1d22] rounded-lg border border-[#2a2b30] flex items-center gap-2">
                <AlertCircle className="w-4 h-4 text-[#f69d50]" />
                <span className="text-sm">Gamescope not detected</span>
              </div>

              <div className="flex items-center justify-between">
                <div>
                  <div className="text-sm font-medium">Enable Gamescope Wrapper</div>
                  <div className="text-xs text-[#8e8f94] mt-0.5">Use Gamescope for game windows</div>
                </div>
                <button
                  onClick={() => setGamescopeEnabled(!gamescopeEnabled)}
                  className={`relative w-12 h-6 rounded-full transition-colors ${
                    gamescopeEnabled ? 'bg-[#4ecdc4]' : 'bg-[#2a2b30]'
                  }`}
                >
                  <div className={`absolute top-1 left-1 w-4 h-4 bg-white rounded-full transition-transform ${
                    gamescopeEnabled ? 'translate-x-6' : ''
                  }`} />
                </button>
              </div>

              {gamescopeEnabled && (
                <>
                  <div>
                    <label className="block text-sm font-medium mb-2">Mode</label>
                    <select
                      value={gamescopeMode}
                      onChange={(e) => setGamescopeMode(e.target.value)}
                      className="w-full px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none"
                    >
                      <option value="prefer-fallback">Prefer (fallback if missing)</option>
                      <option value="require">Require (fail if missing)</option>
                    </select>
                  </div>

                  <div className="flex items-center justify-between">
                    <div className="text-sm font-medium">Fullscreen</div>
                    <button
                      onClick={() => setGamescopeFullscreen(!gamescopeFullscreen)}
                      className={`relative w-12 h-6 rounded-full transition-colors ${
                        gamescopeFullscreen ? 'bg-[#4ecdc4]' : 'bg-[#2a2b30]'
                      }`}
                    >
                      <div className={`absolute top-1 left-1 w-4 h-4 bg-white rounded-full transition-transform ${
                        gamescopeFullscreen ? 'translate-x-6' : ''
                      }`} />
                    </button>
                  </div>

                  <div className="grid grid-cols-2 gap-4">
                    <div>
                      <label className="block text-sm font-medium mb-2">Width</label>
                      <input
                        type="number"
                        value={gamescopeWidth}
                        onChange={(e) => setGamescopeWidth(Number(e.target.value))}
                        className="w-full px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none"
                      />
                    </div>
                    <div>
                      <label className="block text-sm font-medium mb-2">Height</label>
                      <input
                        type="number"
                        value={gamescopeHeight}
                        onChange={(e) => setGamescopeHeight(Number(e.target.value))}
                        className="w-full px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none"
                      />
                    </div>
                  </div>

                  <div>
                    <label className="block text-sm font-medium mb-2">Extra Arguments (one per line)</label>
                    <textarea
                      rows={3}
                      placeholder="--adaptive-sync&#10;--rt"
                      className="w-full px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4ecdc4] outline-none font-mono text-sm resize-none"
                    />
                  </div>

                  <div className="flex flex-wrap gap-2 pt-3 border-t border-[#2a2b30]">
                    <button className="px-4 py-2 bg-[#4ecdc4] hover:bg-[#3db8b0] text-[#14151a] rounded-lg text-sm font-medium transition-colors">
                      Save Windowing
                    </button>
                    <button className="px-3 py-1.5 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm transition-colors">
                      Handheld Preset
                    </button>
                    <button className="px-3 py-1.5 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm transition-colors">
                      Streamer Preset
                    </button>
                    <button className="px-3 py-1.5 bg-[#f85149] hover:bg-[#d83f37] rounded-lg text-sm transition-colors">
                      Disable Gamescope
                    </button>
                  </div>
                </>
              )}
            </div>
          </div>
        )}

        {/* Social Section */}
        {activeSection === 'social' && (
          <div className="max-w-3xl space-y-6">
            <div>
              <h1 className="text-2xl font-bold mb-1">Social</h1>
              <p className="text-sm text-[#8e8f94]">Configure presence and messaging settings</p>
            </div>

            <div className="bg-gradient-card border-2 border-[#2a2b30] rounded-lg p-5 space-y-4">
              <div className="flex items-center gap-2 pb-3 border-b border-[#2a2b30]">
                <Users className="w-5 h-5 text-[#4ecdc4]" />
                <h3 className="font-bold">Presence & Messaging</h3>
              </div>

              <div>
                <label className="block text-sm font-medium mb-2">Default Status</label>
                <select
                  value={presenceStatus}
                  onChange={(e) => setPresenceStatus(e.target.value)}
                  className="w-full px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none"
                >
                  <option value="online">Online</option>
                  <option value="dnd">Do Not Disturb</option>
                  <option value="offline">Offline</option>
                </select>
              </div>

              <div>
                <label className="block text-sm font-medium mb-2">Direct Message Policy</label>
                <select
                  value={dmPolicy}
                  onChange={(e) => setDmPolicy(e.target.value)}
                  className="w-full px-3 py-2 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none"
                >
                  <option value="anyone">Anyone</option>
                  <option value="friends">Friends Only</option>
                  <option value="none">No One</option>
                </select>
              </div>

              <div className="p-3 bg-[#1c1d22] border border-[#2a2b30] rounded-lg text-sm text-[#8e8f94]">
                Blocked users are managed in the Blocked Users section
              </div>
            </div>
          </div>
        )}

        {/* Blocked Users Section */}
        {activeSection === 'blocked-users' && (
          <div className="max-w-3xl space-y-6">
            <div>
              <h1 className="text-2xl font-bold mb-1">Blocked Users</h1>
              <p className="text-sm text-[#8e8f94]">Manage your blocked users list</p>
            </div>

            {blockedUsers.length > 0 ? (
              <div className="space-y-3">
                {blockedUsers.map((user) => (
                  <div key={user.id} className="bg-gradient-card border-2 border-[#2a2b30] rounded-lg p-4 flex items-center justify-between">
                    <div className="flex items-center gap-3">
                      <div className="w-10 h-10 rounded-full bg-gradient-to-br from-[#f85149] to-[#f69d50] flex items-center justify-center">
                        <User className="w-5 h-5 text-white" />
                      </div>
                      <div>
                        <div className="font-bold">{user.displayName}</div>
                        <div className="text-sm text-[#8e8f94]">ID: {user.userId}</div>
                      </div>
                    </div>
                    <button
                      onClick={() => setBlockedUsers(blockedUsers.filter(u => u.id !== user.id))}
                      className="px-4 py-2 bg-[#4ecdc4] hover:bg-[#3db8b0] text-[#14151a] rounded-lg text-sm font-medium transition-colors"
                    >
                      Unblock
                    </button>
                  </div>
                ))}
              </div>
            ) : (
              <div className="text-center py-20 bg-gradient-card border-2 border-[#2a2b30] rounded-lg">
                <ShieldOff className="w-16 h-16 text-[#8e8f94] mx-auto mb-4 opacity-50" />
                <p className="text-lg text-[#8e8f94] mb-2">No blocked users</p>
                <p className="text-sm text-[#8e8f94]">You haven't blocked anyone yet</p>
              </div>
            )}
          </div>
        )}
      </div>
      {romError && (
        <ErrorModal
          title="ROM setup error"
          message={romError}
          code="ROM_SETUP_FAILED"
          onClose={() => setRomError('')}
        />
      )}
    </div>
  );
}
