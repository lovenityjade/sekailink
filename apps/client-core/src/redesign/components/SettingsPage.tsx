import { useEffect, useMemo, useState } from 'react';
import type React from 'react';
import {
  Bot,
  CheckCircle,
  CircleAlert,
  Cpu,
  Database,
  Gamepad2,
  Globe,
  HardDrive,
  Keyboard,
  Link,
  Monitor,
  Radio,
  Save,
  Server,
  Shield,
  SlidersHorizontal,
  Twitch,
  Users,
  Volume2,
} from 'lucide-react';
import { apiJson } from '../../services/api';
import { runtime } from '../../services/runtime';
import { trace, traceError } from '../../services/trace';
import { normalizeLocale, useI18n } from '../../i18n';
import type { LocaleCode } from '../../i18n/types';
import { applyClientTheme, normalizeClientTheme, type ClientTheme } from '../../services/theme';
import { SEKAILINK_GAME_CATALOG } from '../../data/sekailinkGameCatalog';

type SettingsSection =
  | 'general'
  | 'rom-library'
  | 'video-layout'
  | 'audio'
  | 'games-automation'
  | 'core-settings'
  | 'control-settings'
  | 'hotkeys'
  | 'sklmi'
  | 'social'
  | 'connections'
  | 'twitch'
  | 'server-status';

const sections: Array<{ id: SettingsSection; label: string; icon: React.ReactNode }> = [
  { id: 'general', label: 'General', icon: <Globe className="w-4 h-4" /> },
  { id: 'rom-library', label: 'ROM Library', icon: <HardDrive className="w-4 h-4" /> },
  { id: 'video-layout', label: 'Video / Layout', icon: <Monitor className="w-4 h-4" /> },
  { id: 'audio', label: 'Audio', icon: <Volume2 className="w-4 h-4" /> },
  { id: 'games-automation', label: 'Games / Automation', icon: <Gamepad2 className="w-4 h-4" /> },
  { id: 'core-settings', label: 'Core Settings', icon: <SlidersHorizontal className="w-4 h-4" /> },
  { id: 'control-settings', label: 'Control Settings', icon: <Gamepad2 className="w-4 h-4" /> },
  { id: 'hotkeys', label: 'Hotkeys', icon: <Keyboard className="w-4 h-4" /> },
  { id: 'sklmi', label: 'SKLMI', icon: <Radio className="w-4 h-4" /> },
  { id: 'social', label: 'Social', icon: <Users className="w-4 h-4" /> },
  { id: 'connections', label: 'Connections', icon: <Link className="w-4 h-4" /> },
  { id: 'twitch', label: 'Twitch Integration', icon: <Twitch className="w-4 h-4" /> },
  { id: 'server-status', label: 'Server Status', icon: <Server className="w-4 h-4" /> },
];

const serverNames = ['Nexus', 'Link', 'Worlds', 'Pulse', 'CDN'];

type ControlCoreId = 'snes' | 'nes' | 'gbc' | 'gba' | 'n64';
type CoreControlMappings = Record<string, string>;
type ControlMappingsByCore = Partial<Record<ControlCoreId, CoreControlMappings>>;
type HotkeyMappings = Record<string, string>;
type CoreSettingsMode = 'simple' | 'advanced';
type CoreSettingsValues = Record<string, Record<string, string>>;

type CoreSettingOption = {
  key: string;
  label: string;
  description: string;
  defaultValue: string;
  values: Array<[string, string]>;
  advanced?: boolean;
  requiresRestart?: boolean;
};

type CoreSettingsProfile = {
  id: string;
  label: string;
  system: string;
  description: string;
  options: CoreSettingOption[];
};

type RomImportCandidate = {
  gameId: string;
  moduleId?: string;
  displayName: string;
};

const controlProfiles: Record<ControlCoreId, {
  label: string;
  description: string;
  controller: 'snes' | 'nes' | 'gb' | 'n64';
  buttons: string[];
}> = {
  snes: {
    label: 'SNES / Snes9x',
    description: 'A, B, X, Y, shoulders, Start, Select, and D-Pad.',
    controller: 'snes',
    buttons: ['D-Pad Up', 'D-Pad Down', 'D-Pad Left', 'D-Pad Right', 'A', 'B', 'X', 'Y', 'L', 'R', 'Start', 'Select'],
  },
  nes: {
    label: 'NES / FCEUmm',
    description: 'A, B, Start, Select, and D-Pad.',
    controller: 'nes',
    buttons: ['D-Pad Up', 'D-Pad Down', 'D-Pad Left', 'D-Pad Right', 'A', 'B', 'Start', 'Select'],
  },
  gbc: {
    label: 'GB/GBC / Gambatte',
    description: 'A, B, Start, Select, and D-Pad.',
    controller: 'gb',
    buttons: ['D-Pad Up', 'D-Pad Down', 'D-Pad Left', 'D-Pad Right', 'A', 'B', 'Start', 'Select'],
  },
  gba: {
    label: 'GBA / mGBA',
    description: 'A, B, L, R, Start, Select, and D-Pad.',
    controller: 'snes',
    buttons: ['D-Pad Up', 'D-Pad Down', 'D-Pad Left', 'D-Pad Right', 'A', 'B', 'L', 'R', 'Start', 'Select'],
  },
  n64: {
    label: 'N64 / Mupen64Plus-Next',
    description: 'A, B, Z, L, R, Start, C buttons, D-Pad, and analog stick.',
    controller: 'n64',
    buttons: ['Stick Up', 'Stick Down', 'Stick Left', 'Stick Right', 'C Up', 'C Down', 'C Left', 'C Right', 'D-Pad Up', 'D-Pad Down', 'D-Pad Left', 'D-Pad Right', 'A', 'B', 'Z', 'L', 'R', 'Start'],
  },
};

const hotkeyActions: Array<{ id: string; label: string; description: string; defaultBinding: string }> = [
  { id: 'sekaiemu_menu', label: 'Sekaiemu function menu', description: 'Open or close the Sekaiemu runtime function menu.', defaultBinding: 'keyboard:F1' },
  { id: 'toggle_chat', label: 'Open chat', description: 'Show or focus the in-game chat overlay.', defaultBinding: 'keyboard:Enter' },
  { id: 'reset_emulation', label: 'Reset emulation', description: 'Soft reset the current emulation session.', defaultBinding: 'keyboard:F5' },
  { id: 'save_state', label: 'Save state', description: 'Save the current emulator state slot.', defaultBinding: 'keyboard:F6' },
  { id: 'load_state', label: 'Load state', description: 'Load the current emulator state slot.', defaultBinding: 'keyboard:F7' },
  { id: 'reload_connection', label: 'Reload connection', description: 'Reconnect the active AP/SKLMI runtime bridge.', defaultBinding: 'keyboard:F8' },
  { id: 'toggle_fullscreen', label: 'Toggle fullscreen', description: 'Switch Sekaiemu between windowed and fullscreen.', defaultBinding: 'keyboard:F11' },
  { id: 'toggle_tracker', label: 'Toggle tracker', description: 'Show or hide the tracker panel/window.', defaultBinding: 'keyboard:F9' },
  { id: 'pause_emulation', label: 'Pause emulation', description: 'Pause or resume the emulator without closing the room.', defaultBinding: 'keyboard:Pause' },
  { id: 'screenshot', label: 'Screenshot', description: 'Capture the current emulator output for diagnostics or sharing.', defaultBinding: 'keyboard:F12' },
];

const defaultHotkeys = (): HotkeyMappings =>
  Object.fromEntries(hotkeyActions.map((action) => [action.id, action.defaultBinding]));

const coreSettingsProfiles: Record<string, CoreSettingsProfile> = {
  bsnes_mercury_performance: {
    id: 'bsnes_mercury_performance',
    label: 'SNES / bsnes-mercury-performance',
    system: 'SNES',
    description: 'Accuracy-focused SNES core used for ALTTP and other SNES games.',
    options: [
      { key: 'bsnes_mercury_region', label: 'Region', description: 'Force NTSC/PAL behavior or let the core detect it.', defaultValue: 'Auto', values: [['Auto', 'Auto'], ['NTSC', 'NTSC'], ['PAL', 'PAL']], requiresRestart: true },
      { key: 'bsnes_mercury_crop_overscan', label: 'Crop overscan', description: 'Hide noisy overscan borders around SNES output.', defaultValue: 'enabled', values: [['enabled', 'Enabled'], ['disabled', 'Disabled']] },
      { key: 'bsnes_mercury_aspect', label: 'Aspect correction', description: 'Choose how SNES pixels are stretched on modern displays.', defaultValue: 'auto', values: [['auto', 'Auto'], ['uncorrected', 'Pixel perfect'], ['corrected', 'Corrected']] },
      { key: 'bsnes_mercury_gfx_hires', label: 'High-resolution mode', description: 'Controls hires rendering used by a few SNES effects.', defaultValue: 'enabled', values: [['enabled', 'Enabled'], ['disabled', 'Disabled']], advanced: true },
      { key: 'bsnes_mercury_blargg_ntsc_filter', label: 'NTSC filter', description: 'Optional analog-style video filtering.', defaultValue: 'disabled', values: [['disabled', 'Disabled'], ['monochrome', 'Monochrome'], ['rf', 'RF'], ['composite', 'Composite'], ['s-video', 'S-Video'], ['rgb', 'RGB']], advanced: true },
    ],
  },
  fceumm: {
    id: 'fceumm',
    label: 'NES / FCEUmm',
    system: 'NES',
    description: 'Fast NES core used for generic NES support.',
    options: [
      { key: 'fceumm_region', label: 'Region', description: 'Force NTSC/PAL/Dendy timing or auto-detect.', defaultValue: 'Auto', values: [['Auto', 'Auto'], ['NTSC', 'NTSC'], ['PAL', 'PAL'], ['Dendy', 'Dendy']], requiresRestart: true },
      { key: 'fceumm_aspect', label: 'Aspect ratio', description: 'Choose corrected or pixel-oriented NES output.', defaultValue: '8:7 PAR', values: [['8:7 PAR', '8:7 PAR'], ['4:3', '4:3'], ['PP', 'Pixel perfect']] },
      { key: 'fceumm_palette', label: 'Palette', description: 'Select the NES color palette.', defaultValue: 'default', values: [['default', 'Default'], ['asqrealc', 'ASQ RealC'], ['nintendo-vc', 'Nintendo VC'], ['rgb', 'RGB']], advanced: true },
      { key: 'fceumm_ntsc_filter', label: 'NTSC filter', description: 'Analog-style video filtering.', defaultValue: 'disabled', values: [['disabled', 'Disabled'], ['composite', 'Composite'], ['svideo', 'S-Video'], ['rgb', 'RGB']], advanced: true },
      { key: 'fceumm_overclocking', label: 'Overclocking', description: 'Reduce slowdown at the cost of authenticity.', defaultValue: 'disabled', values: [['disabled', 'Disabled'], ['2x-Postrender', '2x post-render'], ['2x-VBlank', '2x VBlank']], advanced: true, requiresRestart: true },
    ],
  },
  gambatte: {
    id: 'gambatte',
    label: 'GB/GBC / Gambatte',
    system: 'GB/GBC',
    description: 'Game Boy and Game Boy Color core.',
    options: [
      { key: 'gambatte_gb_colorization', label: 'GB colorization', description: 'Colorize original Game Boy games.', defaultValue: 'disabled', values: [['disabled', 'Disabled'], ['internal', 'Internal palette'], ['custom', 'Custom palette']] },
      { key: 'gambatte_gb_internal_palette', label: 'GB palette', description: 'Palette used when internal colorization is enabled.', defaultValue: 'GB - DMG', values: [['GB - DMG', 'DMG'], ['GB - Pocket', 'Pocket'], ['GB - Light', 'Light'], ['GBC - Blue', 'Blue'], ['GBC - Brown', 'Brown'], ['GBC - Dark Blue', 'Dark Blue'], ['GBC - Grayscale', 'Grayscale'], ['GBC - Green', 'Green'], ['GBC - Inverted', 'Inverted'], ['GBC - Orange', 'Orange'], ['GBC - Red', 'Red']] },
      { key: 'gambatte_gbc_color_correction', label: 'GBC color correction', description: 'Adjust GBC colors toward original hardware appearance.', defaultValue: 'enabled', values: [['enabled', 'Enabled'], ['disabled', 'Disabled']] },
      { key: 'gambatte_gbc_color_correction_mode', label: 'Correction mode', description: 'Choose how aggressively GBC colors are corrected.', defaultValue: 'accurate', values: [['accurate', 'Accurate'], ['fast', 'Fast']], advanced: true },
      { key: 'gambatte_mix_frames', label: 'Frame blending', description: 'Blend frames for games relying on LCD persistence effects.', defaultValue: 'disabled', values: [['disabled', 'Disabled'], ['mix', 'Mix'], ['lcd_ghosting', 'LCD ghosting']], advanced: true },
    ],
  },
  mgba: {
    id: 'mgba',
    label: 'GBA / mGBA',
    system: 'GBA',
    description: 'Game Boy Advance core.',
    options: [
      { key: 'mgba_use_bios', label: 'Use BIOS', description: 'Use a real BIOS file when available.', defaultValue: 'ON', values: [['ON', 'On'], ['OFF', 'Off']], requiresRestart: true },
      { key: 'mgba_skip_bios', label: 'Skip BIOS intro', description: 'Skip the boot logo when a BIOS is configured.', defaultValue: 'OFF', values: [['ON', 'On'], ['OFF', 'Off']], requiresRestart: true },
      { key: 'mgba_color_correction', label: 'Color correction', description: 'Adjust colors toward original GBA LCD output.', defaultValue: 'OFF', values: [['ON', 'On'], ['OFF', 'Off']] },
      { key: 'mgba_idle_optimization', label: 'Idle optimization', description: 'Improve performance by skipping idle CPU loops.', defaultValue: 'Remove Known', values: [['Remove Known', 'Remove known'], ['Detect and Remove', 'Detect and remove'], ['Don’t Remove', 'Disabled']], advanced: true },
      { key: 'mgba_frameskip', label: 'Frameskip', description: 'Skip frames if the device cannot keep full speed.', defaultValue: 'disabled', values: [['disabled', 'Disabled'], ['auto', 'Auto'], ['manual', 'Manual']], advanced: true },
      { key: 'mgba_frameskip_threshold', label: 'Frameskip threshold', description: 'Audio buffer threshold before frameskip is applied.', defaultValue: '33', values: [['15', '15%'], ['33', '33%'], ['50', '50%'], ['66', '66%'], ['80', '80%']], advanced: true },
    ],
  },
  mupen64plus_next: {
    id: 'mupen64plus_next',
    label: 'N64 / Mupen64Plus-Next',
    system: 'N64',
    description: 'N64 core for future Ocarina of Time and generic N64 testing.',
    options: [
      { key: 'mupen64plus-cpucore', label: 'CPU core', description: 'Dynamic recompiler is fastest; cached interpreter is safer for edge cases.', defaultValue: 'dynamic_recompiler', values: [['dynamic_recompiler', 'Dynamic recompiler'], ['cached_interpreter', 'Cached interpreter'], ['pure_interpreter', 'Pure interpreter']], requiresRestart: true },
      { key: 'mupen64plus-rdp-plugin', label: 'RDP plugin', description: 'Video plugin used by the N64 core.', defaultValue: 'gliden64', values: [['gliden64', 'GLideN64'], ['angrylion', 'Angrylion']], requiresRestart: true },
      { key: 'mupen64plus-rsp-plugin', label: 'RSP plugin', description: 'Audio/graphics signal processor plugin.', defaultValue: 'hle', values: [['hle', 'HLE'], ['parallel', 'ParaLLEl']], advanced: true, requiresRestart: true },
      { key: 'mupen64plus-43screensize', label: 'Internal resolution', description: 'Rendering resolution for 4:3 games.', defaultValue: '320x240', values: [['320x240', 'Native 320x240'], ['640x480', '640x480'], ['960x720', '960x720'], ['1280x960', '1280x960']], requiresRestart: true },
      { key: 'mupen64plus-aspect', label: 'Aspect ratio', description: 'Video aspect ratio.', defaultValue: '4:3', values: [['4:3', '4:3'], ['16:9 adjusted', '16:9 adjusted'], ['16:9', '16:9']] },
      { key: 'mupen64plus-BilinearMode', label: 'Bilinear filtering', description: 'Texture filtering mode.', defaultValue: 'standard', values: [['standard', 'Standard'], ['3point', '3-point'], ['disabled', 'Disabled']], advanced: true },
      { key: 'mupen64plus-EnableFBEmulation', label: 'Framebuffer emulation', description: 'Required for some effects, but heavier on slower machines.', defaultValue: 'True', values: [['True', 'Enabled'], ['False', 'Disabled']], advanced: true, requiresRestart: true },
    ],
  },
};

const defaultCoreSettings = (): CoreSettingsValues =>
  Object.fromEntries(Object.entries(coreSettingsProfiles).map(([coreId, profile]) => [
    coreId,
    Object.fromEntries(profile.options.map((option) => [option.key, option.defaultValue])),
  ]));

const normalizeClientLocale = (value?: string | null): LocaleCode => {
  const normalized = normalizeLocale(value);
  return normalized === 'fr' || normalized === 'ja' ? normalized : 'en';
};

export default function SettingsPage() {
  const { locale, setLocale, t } = useI18n();
  const [active, setActive] = useState<SettingsSection>('general');
  const [statusRows, setStatusRows] = useState<Array<{ name: string; status: string; cpu?: string; ram?: string; uptime?: string }>>([]);
  const [importStatus, setImportStatus] = useState('');
  const [romImportGameId, setRomImportGameId] = useState('');
  const [pendingRomImport, setPendingRomImport] = useState<{ filePath: string; candidates: RomImportCandidate[] } | null>(null);
  const [language, setLanguage] = useState<LocaleCode>(locale);
  const [theme, setTheme] = useState<ClientTheme>('default');
  const [diagnostics, setDiagnostics] = useState(true);
  const [gamepads, setGamepads] = useState<Array<{ id: string; index: number; buttons: number; axes: number }>>([]);
  const [selectedControlCore, setSelectedControlCore] = useState<ControlCoreId>('snes');
  const [selectedController, setSelectedController] = useState('');
  const [defaultControlCore, setDefaultControlCore] = useState<ControlCoreId>('snes');
  const [controlMappings, setControlMappings] = useState<ControlMappingsByCore>({});
  const [captureTarget, setCaptureTarget] = useState<string | null>(null);
  const [captureStatus, setCaptureStatus] = useState('');
  const [hotkeys, setHotkeys] = useState<HotkeyMappings>(() => defaultHotkeys());
  const [hotkeyCaptureTarget, setHotkeyCaptureTarget] = useState<string | null>(null);
  const [hotkeyStatus, setHotkeyStatus] = useState('');
  const [coreSettingsMode, setCoreSettingsMode] = useState<CoreSettingsMode>('simple');
  const [selectedCoreSettingsCore, setSelectedCoreSettingsCore] = useState('bsnes_mercury_performance');
  const [coreSettings, setCoreSettings] = useState<CoreSettingsValues>(() => defaultCoreSettings());
  const [settings, setSettings] = useState({
    clientFullscreen: false,
    clientQuality: 2,
    startLayout: 'separate',
    emulationSize: 'x2',
    emuFullscreen: false,
    broadcastWindow: true,
    preserveEmuAspect: true,
    preventTrackerOverflow: true,
    chatboxVisible: true,
    chatboxFontSize: 14,
    disableAutotabbing: false,
    masterVolume: 85,
    emuVolume: 85,
    trackerVolume: 70,
    muteBackground: false,
    blockFriendRequests: false,
    minimizeNotifications: false,
    sklmiReconnect: true,
    sklmiVerbose: true,
    sklmiExtractLogs: true,
    hideAiUi: false,
    disableAiProcessing: false,
  });

  const activeTitle = useMemo(() => t(sections.find((section) => section.id === active)?.label || 'Settings'), [active, t]);
  const romImportGames = useMemo(
    () => [...SEKAILINK_GAME_CATALOG].sort((a, b) => a.displayName.localeCompare(b.displayName)),
    []
  );

  const changeLanguage = (value: string) => {
    const next = normalizeClientLocale(value);
    setLanguage(next);
    setLocale(next);
  };

  useEffect(() => {
    let cancelled = false;
    const loadConfig = async () => {
      try {
        const config = await runtime.configGet?.();
        if (cancelled || !config || typeof config !== 'object') return;
        const root = config as any;
        const layout = root.layout && typeof root.layout === 'object' ? root.layout : {};
        const ui = layout.ui && typeof layout.ui === 'object' ? layout.ui : {};
        const client = layout.client && typeof layout.client === 'object' ? layout.client : {};
        const sekaiemu = layout.sekaiemu && typeof layout.sekaiemu === 'object' ? layout.sekaiemu : {};
        const audio = layout.audio && typeof layout.audio === 'object' ? layout.audio : {};
        const aiFeatures = layout.ai_features && typeof layout.ai_features === 'object' ? layout.ai_features : {};
        const sklmi = layout.sklmi && typeof layout.sklmi === 'object' ? layout.sklmi : {};
        const social = layout.social && typeof layout.social === 'object' ? layout.social : {};
        const input = layout.input && typeof layout.input === 'object' ? layout.input : {};
        if (typeof ui.language === 'string') {
          const nextLanguage = normalizeClientLocale(ui.language);
          setLanguage(nextLanguage);
          setLocale(nextLanguage);
        }
        if (typeof ui.theme === 'string') {
          const nextTheme = normalizeClientTheme(ui.theme);
          setTheme(nextTheme);
          applyClientTheme(nextTheme);
        }
        if (typeof root.crash_reporting_opt_in === 'boolean') setDiagnostics(root.crash_reporting_opt_in);
        if (typeof input.selected_controller_id === 'string') setSelectedController(input.selected_controller_id);
        if (typeof input.default_core_id === 'string' && input.default_core_id in controlProfiles) {
          setDefaultControlCore(input.default_core_id as ControlCoreId);
        }
        if (input.core_mappings && typeof input.core_mappings === 'object') {
          setControlMappings(input.core_mappings as ControlMappingsByCore);
        }
        if (layout.hotkeys && typeof layout.hotkeys === 'object') {
          setHotkeys({ ...defaultHotkeys(), ...(layout.hotkeys as HotkeyMappings) });
        }
        const savedCoreSettings = layout.core_settings && typeof layout.core_settings === 'object' ? layout.core_settings : {};
        if (typeof savedCoreSettings.mode === 'string' && (savedCoreSettings.mode === 'simple' || savedCoreSettings.mode === 'advanced')) {
          setCoreSettingsMode(savedCoreSettings.mode);
        }
        if (typeof savedCoreSettings.selected_core === 'string' && savedCoreSettings.selected_core in coreSettingsProfiles) {
          setSelectedCoreSettingsCore(savedCoreSettings.selected_core);
        }
        if (savedCoreSettings.values && typeof savedCoreSettings.values === 'object') {
          setCoreSettings(mergeCoreSettings(savedCoreSettings.values as CoreSettingsValues));
        }
        setSettings((prev) => ({
          ...prev,
          clientFullscreen: typeof client.start_fullscreen === 'boolean' ? client.start_fullscreen : prev.clientFullscreen,
          clientQuality: Number.isFinite(client.quality) ? Number(client.quality) : prev.clientQuality,
          startLayout: layout.mode === 'side_by_side' ? 'side-by-side' : layout.mode === 'separate_displays' ? 'separate' : (sekaiemu.start_layout || prev.startLayout),
          emulationSize: typeof sekaiemu.emulation_size === 'string' ? sekaiemu.emulation_size : prev.emulationSize,
          emuFullscreen: typeof sekaiemu.start_fullscreen === 'boolean' ? sekaiemu.start_fullscreen : prev.emuFullscreen,
          broadcastWindow: typeof sekaiemu.broadcast_window === 'boolean' ? sekaiemu.broadcast_window : prev.broadcastWindow,
          preserveEmuAspect: typeof sekaiemu.preserve_aspect === 'boolean' ? sekaiemu.preserve_aspect : prev.preserveEmuAspect,
          preventTrackerOverflow: typeof sekaiemu.prevent_tracker_overflow === 'boolean' ? sekaiemu.prevent_tracker_overflow : prev.preventTrackerOverflow,
          chatboxVisible: typeof sekaiemu.chatbox_visible === 'boolean' ? sekaiemu.chatbox_visible : prev.chatboxVisible,
          chatboxFontSize: Number.isFinite(sekaiemu.chatbox_font_size) ? Number(sekaiemu.chatbox_font_size) : prev.chatboxFontSize,
          disableAutotabbing: typeof sekaiemu.disable_autotabbing === 'boolean' ? sekaiemu.disable_autotabbing : prev.disableAutotabbing,
          masterVolume: Number.isFinite(audio.master_volume) ? Number(audio.master_volume) : prev.masterVolume,
          emuVolume: Number.isFinite(audio.sekaiemu_volume) ? Number(audio.sekaiemu_volume) : prev.emuVolume,
          trackerVolume: Number.isFinite(audio.tracker_volume) ? Number(audio.tracker_volume) : prev.trackerVolume,
          muteBackground: typeof audio.mute_background_while_playing === 'boolean' ? audio.mute_background_while_playing : prev.muteBackground,
          blockFriendRequests: typeof social.block_friend_requests === 'boolean' ? social.block_friend_requests : prev.blockFriendRequests,
          minimizeNotifications: typeof social.minimize_notifications === 'boolean' ? social.minimize_notifications : prev.minimizeNotifications,
          sklmiReconnect: typeof sklmi.aggressive_reconnect === 'boolean' ? sklmi.aggressive_reconnect : prev.sklmiReconnect,
          sklmiVerbose: typeof sklmi.verbose_logs === 'boolean' ? sklmi.verbose_logs : prev.sklmiVerbose,
          sklmiExtractLogs: typeof sklmi.allow_diagnostic_log_extraction === 'boolean' ? sklmi.allow_diagnostic_log_extraction : prev.sklmiExtractLogs,
          hideAiUi: typeof aiFeatures.hide_ai_ui === 'boolean' ? aiFeatures.hide_ai_ui : prev.hideAiUi,
          disableAiProcessing: typeof aiFeatures.disable_ai_processing === 'boolean' ? aiFeatures.disable_ai_processing : prev.disableAiProcessing,
        }));
        trace('settings-page', 'config_loaded');
      } catch (error) {
        traceError('settings-page', 'config_load_failed', error);
      }
    };
    void loadConfig();
    return () => {
      cancelled = true;
    };
  }, []);

  useEffect(() => {
    const refresh = () => {
      const pads = typeof navigator !== 'undefined' && navigator.getGamepads ? (Array.from(navigator.getGamepads()).filter(Boolean) as Gamepad[]) : [];
      setGamepads(pads.map((pad) => ({ id: pad.id, index: pad.index, buttons: pad.buttons.length, axes: pad.axes.length })));
      setSelectedController((current) => current || (pads[0] ? String(pads[0].index) : ''));
    };
    refresh();
    const timer = window.setInterval(refresh, 1200);
    window.addEventListener('gamepadconnected', refresh);
    window.addEventListener('gamepaddisconnected', refresh);
    return () => {
      window.clearInterval(timer);
      window.removeEventListener('gamepadconnected', refresh);
      window.removeEventListener('gamepaddisconnected', refresh);
    };
  }, []);

  useEffect(() => {
    if (!captureTarget) return;
    const started = performance.now();
    const controllerIndex = Number(selectedController || 0);
    const baseline = readGamepadSnapshot(controllerIndex);
    setCaptureStatus(`${t('Press a button or move an axis')} - ${captureTarget}`);
    let frame = 0;

    const tick = () => {
      if (performance.now() - started >= 5000) {
        setCaptureTarget(null);
        setCaptureStatus(t('Input capture timed out.'));
        return;
      }
      const detected = detectNewGamepadInput(baseline, readGamepadSnapshot(controllerIndex));
      if (detected) {
        const target = captureTarget;
        setControlMappings((prev) => ({
          ...prev,
          [selectedControlCore]: {
            ...(prev[selectedControlCore] || {}),
            [target]: detected,
          },
        }));
        setCaptureTarget(null);
        setCaptureStatus(`${target}: ${detected}`);
        return;
      }
      frame = window.requestAnimationFrame(tick);
    };

    frame = window.requestAnimationFrame(tick);
    return () => {
      window.cancelAnimationFrame(frame);
    };
  }, [captureTarget, selectedController, selectedControlCore, t]);

  useEffect(() => {
    if (!hotkeyCaptureTarget) return;
    const started = performance.now();
    const controllerIndex = Number(selectedController || 0);
    const baseline = readGamepadSnapshot(controllerIndex);
    setHotkeyStatus(`${t('Press a key, button, or axis')} - ${hotkeyLabelFor(hotkeyCaptureTarget)}`);
    let frame = 0;
    let done = false;

    const finish = (binding: string) => {
      if (done) return;
      done = true;
      const target = hotkeyCaptureTarget;
      setHotkeys((prev) => ({ ...prev, [target]: binding }));
      setHotkeyCaptureTarget(null);
      setHotkeyStatus(`${hotkeyLabelFor(target)}: ${formatBinding(binding)}`);
    };

    const onKeyDown = (event: KeyboardEvent) => {
      event.preventDefault();
      event.stopPropagation();
      finish(formatKeyboardBinding(event));
    };

    const tick = () => {
      if (done) return;
      if (performance.now() - started >= 5000) {
        done = true;
        setHotkeyCaptureTarget(null);
        setHotkeyStatus(t('Hotkey capture timed out.'));
        return;
      }
      const detected = detectNewGamepadInput(baseline, readGamepadSnapshot(controllerIndex));
      if (detected) {
        finish(`gamepad:${detected}`);
        return;
      }
      frame = window.requestAnimationFrame(tick);
    };

    window.addEventListener('keydown', onKeyDown, { capture: true });
    frame = window.requestAnimationFrame(tick);
    return () => {
      done = true;
      window.cancelAnimationFrame(frame);
      window.removeEventListener('keydown', onKeyDown, { capture: true } as any);
    };
  }, [hotkeyCaptureTarget, selectedController, t]);

  useEffect(() => {
    if (active !== 'server-status') return;
    let cancelled = false;
    const load = async () => {
      trace('settings-page', 'server_status_load_start');
      try {
        const data = await apiJson<{ servers?: any[] }>('/api/admin/server-status/public');
        if (cancelled) return;
        const rows = Array.isArray(data.servers) ? data.servers : [];
        setStatusRows(rows.map((row, index) => ({
          name: String(row.name || serverNames[index] || 'Server'),
          status: String(row.status || 'unknown'),
          cpu: row.cpu_percent != null ? `${row.cpu_percent}%` : undefined,
          ram: row.ram_percent != null ? `${row.ram_percent}%` : undefined,
          uptime: row.uptime ? String(row.uptime) : undefined,
        })));
      } catch (error) {
        traceError('settings-page', 'server_status_load_failed', error);
        if (!cancelled) {
          setStatusRows(serverNames.map((name) => ({ name, status: 'unknown' })));
        }
      }
    };
    void load();
    const timer = window.setInterval(load, 30000);
    return () => {
      cancelled = true;
      window.clearInterval(timer);
    };
  }, [active]);

  const patchSettings = (patch: Partial<typeof settings>) => {
    setSettings((prev) => ({ ...prev, ...patch }));
  };

  const changeTheme = (value: string) => {
    const next = normalizeClientTheme(value);
    setTheme(next);
    applyClientTheme(next);
  };

  const importRomPath = async (filePath: string, gameId?: string) => {
    const result = await runtime.romsImport?.(
      gameId ? { filePath, gameId } : filePath
    );
    const ok = Boolean((result as any)?.ok ?? result);
    if (!ok) {
      const error = String((result as any)?.error || 'ROM invalide.');
      const candidates = Array.isArray((result as any)?.candidates)
        ? (result as any).candidates
            .map((candidate: any) => ({
              gameId: String(candidate?.gameId || ''),
              moduleId: String(candidate?.moduleId || ''),
              displayName: String(candidate?.displayName || candidate?.gameId || ''),
            }))
            .filter((candidate: RomImportCandidate) => candidate.gameId && candidate.displayName)
        : [];
      if (error === 'rom_game_ambiguous' && candidates.length) {
        setPendingRomImport({ filePath, candidates });
        setImportStatus('Plusieurs jeux peuvent utiliser cette ROM. Choisis le jeu cible ci-dessous.');
        return;
      }
      setPendingRomImport(null);
      const expected = String((result as any)?.expected || (result as any)?.expected_checksum || '');
      setImportStatus(expected ? `ROM invalide: ${error}. ROM attendue: ${expected}` : `ROM invalide: ${error}`);
      return;
    }
    setPendingRomImport(null);
    const importedName = String((result as any)?.displayName || (result as any)?.gameId || '');
    const matchKind = String((result as any)?.matchKind || '');
    setImportStatus(
      importedName
        ? `${t('ROM imported and cached locally.')} ${importedName}${matchKind ? ` (${matchKind})` : ''}.`
        : t('ROM imported and cached locally.')
    );
  };

  const importRom = async () => {
    setImportStatus('');
    setPendingRomImport(null);
    try {
      const file = await runtime.pickFile?.({
      title: t('Import ROM File'),
        filters: [
          { name: 'ROM files', extensions: ['sfc', 'smc', 'nes', 'gb', 'gbc', 'gba', 'z64', 'n64'] },
          { name: 'All files', extensions: ['*'] },
        ],
      });
      const filePath = typeof file === 'string' ? file : String((file as any)?.path || '');
      if (!filePath) return;
      await importRomPath(filePath, romImportGameId || undefined);
    } catch (error) {
      traceError('settings-page', 'rom_import_failed', error);
      setImportStatus(error instanceof Error ? `${t('ROM invalide')}: ${error.message}` : `${t('ROM invalide')}: unable to import file.`);
    }
  };

  const save = async () => {
    trace('settings-page', 'save_start', { active });
    await runtime.configSetLayout?.({
      preset: 'handheld',
      mode: settings.startLayout === 'side-by-side' ? 'side_by_side' : 'separate_displays',
      game_display: 0,
      tracker_display: 1,
      split: 0.7,
      ui: {
        language,
        theme,
      },
      client: {
        start_fullscreen: settings.clientFullscreen,
        quality: settings.clientQuality,
      },
      sekaiemu: {
        start_layout: settings.startLayout,
        emulation_size: settings.emulationSize,
        start_fullscreen: settings.emuFullscreen,
        broadcast_window: settings.broadcastWindow,
        preserve_aspect: settings.preserveEmuAspect,
        prevent_tracker_overflow: settings.preventTrackerOverflow,
        chatbox_visible: settings.chatboxVisible,
        chatbox_font_size: settings.chatboxFontSize,
        disable_autotabbing: settings.disableAutotabbing,
      },
      audio: {
        master_volume: settings.masterVolume,
        sekaiemu_volume: settings.emuVolume,
        tracker_volume: settings.trackerVolume,
        mute_background_while_playing: settings.muteBackground,
      },
      social: {
        block_friend_requests: settings.blockFriendRequests,
        minimize_notifications: settings.minimizeNotifications,
      },
      ai_features: {
        hide_ai_ui: settings.hideAiUi,
        disable_ai_processing: settings.disableAiProcessing,
      },
      sklmi: {
        aggressive_reconnect: settings.sklmiReconnect,
        verbose_logs: settings.sklmiVerbose,
        allow_diagnostic_log_extraction: settings.sklmiExtractLogs,
      },
      input: {
        capture_backend: 'electron-gamepad-api',
        selected_controller_id: selectedController,
        default_core_id: defaultControlCore,
        core_mappings: controlMappings,
      },
      hotkeys,
      core_settings: {
        mode: coreSettingsMode,
        selected_core: selectedCoreSettingsCore,
        values: coreSettings,
      },
    });
    await runtime.setCrashReportingOptIn?.(diagnostics);
  };

  return (
    <div className="h-full p-8 flex gap-6">
      <aside className="w-72 shrink-0 rounded-xl border border-[#2a2b30] bg-[#0e0f13]/90 p-3 overflow-auto">
        {sections.map((section) => (
          <button
            key={section.id}
            onClick={() => setActive(section.id)}
            className={`w-full flex items-center gap-3 px-4 py-3 rounded-lg text-left transition-colors ${
              active === section.id ? 'bg-gradient-to-r from-[#ff6b35] to-[#f38181] text-white' : 'text-[#8e8f94] hover:bg-[#1c1d22] hover:text-white'
            }`}
          >
            {section.icon}
            <span className="font-medium">{t(section.label)}</span>
          </button>
        ))}
      </aside>

      <main className="flex-1 min-w-0 rounded-xl border border-[#2a2b30] bg-[#0e0f13]/90 overflow-hidden flex flex-col">
        <div className="px-6 py-5 border-b border-[#2a2b30] flex items-center justify-between">
          <div>
            <h1 className="text-2xl font-bold">{activeTitle}</h1>
            <p className="text-sm text-[#8e8f94]">{t('Every visible setting is either wired now or explicitly marked Coming soon.')}</p>
          </div>
          <button onClick={() => void save()} className="px-4 py-2.5 rounded-lg bg-[#2a2b30] hover:bg-[#3a3b40] flex items-center gap-2 text-sm font-medium">
            <Save className="w-4 h-4" />
            {t('Save')}
          </button>
        </div>

        <div className="flex-1 overflow-auto p-6 space-y-5">
          {active === 'general' && (
            <>
              <SettingGroup title={t('Language')}>
                <SelectRow label={t('UI Language')} value={language} onChange={changeLanguage} options={[
                  ['fr', t('Français')],
                  ['en', t('English')],
                  ['ja', t('日本語')],
                ]} description={t('First boot uses the system language. Translation cleanup is tracked as part of the BETA-3 pass.')} />
                <SelectRow label={t('UI Theme')} value={theme} onChange={changeTheme} options={[
                  ['default', t('SekaiLink Default')],
                  ['light', t('Light')],
                  ['deep-dark', t('Dark')],
                ]} description={t('Theme tokens are kept stable so the redesigned UI does not mix old BETA-2 colors.')} />
              </SettingGroup>
              <SettingGroup title={t('Diagnostics')}>
                <ToggleRow label={t('Diagnostic & crash reporting')} value={diagnostics} onChange={setDiagnostics} description={t('Reports will be stored in Nexus for admin/dev/mod review.')} />
              </SettingGroup>
              <SettingGroup title={t('AI Features')}>
                <ToggleRow label={t('Opt out of AI UI')} value={settings.hideAiUi} onChange={(value) => patchSettings({ hideAiUi: value })} description={t('Future behavior: hides Pulse and guided AI configuration surfaces. Not active yet.')} />
                <ToggleRow label={t('Disable AI processing')} value={settings.disableAiProcessing} onChange={(value) => patchSettings({ disableAiProcessing: value })} description={t('Future behavior: prevents client requests to Pulse or AI modules. Not active yet.')} />
                <p className="text-xs text-[#8e8f94]">{t('These options are saved now but intentionally not enforced yet.')}</p>
              </SettingGroup>
            </>
          )}

          {active === 'rom-library' && (
            <SettingGroup title="ROM Library">
              <label className="block text-sm font-semibold text-[#f4f6fb]">
                {t('Target game')}
                <select
                  value={romImportGameId}
                  onChange={(event) => setRomImportGameId(event.target.value)}
                  className="mt-2 w-full rounded-lg border border-[#2a2b30] bg-[#0b0d12] px-3 py-3 text-sm text-[#f4f6fb] outline-none focus:border-[#38f3dd]"
                >
                  <option value="">{t('Auto-detect from checksum')}</option>
                  {romImportGames.map((game) => (
                    <option key={game.key} value={game.key}>
                      {game.displayName}
                    </option>
                  ))}
                </select>
              </label>
              <button onClick={() => void importRom()} className="px-4 py-3 rounded-lg bg-gradient-to-r from-[#ff6b35] to-[#f38181] font-bold">Import ROM File</button>
              <p className="text-sm text-[#8e8f94]">Imported ROMs are validated when checksums are known. If a runtime module does not have checksums yet, choose the target game before importing.</p>
              {importStatus && <div className="rounded-lg border border-[#2a2b30] bg-[#14151a] p-3 text-sm">{importStatus}</div>}
              {pendingRomImport && (
                <div className="rounded-xl border border-[#38f3dd]/25 bg-[#10201f] p-3">
                  <div className="mb-3 text-sm font-semibold text-[#f4f6fb]">{t('Import as')}</div>
                  <div className="flex flex-wrap gap-2">
                    {pendingRomImport.candidates.map((candidate) => (
                      <button
                        key={candidate.gameId}
                        type="button"
                        onClick={() => void importRomPath(pendingRomImport.filePath, candidate.gameId)}
                        className="rounded-lg border border-[#38f3dd]/35 bg-[#173533] px-3 py-2 text-sm font-semibold text-[#f4f6fb] hover:bg-[#1f4744]"
                      >
                        {candidate.displayName}
                      </button>
                    ))}
                  </div>
                </div>
              )}
              <DisabledRow label="ROM list management" description="Trash bin removal remains available when the runtime library index is loaded. Show in Folder is intentionally removed." />
            </SettingGroup>
          )}

          {active === 'video-layout' && (
            <>
              <SettingGroup title="Client">
                <ToggleRow label="Start Fullscreen" value={settings.clientFullscreen} onChange={(value) => patchSettings({ clientFullscreen: value })} />
                <RangeRow label="Quality" value={settings.clientQuality} min={1} max={3} labels={['Low', 'Medium', 'High']} onChange={(value) => patchSettings({ clientQuality: value })} />
              </SettingGroup>
              <SettingGroup title="Sekaiemu">
                <SelectRow label="Start Layout" value={settings.startLayout} onChange={(value) => patchSettings({ startLayout: value })} options={[
                  ['separate', 'Separate Windows'],
                  ['side-by-side', 'Side-by-Side'],
                ]} />
                <SelectRow label="Emulation Size" value={settings.emulationSize} onChange={(value) => patchSettings({ emulationSize: value })} options={[
                  ['native', 'Native'],
                  ['x2', 'x2'],
                  ['x3', 'x3'],
                  ['x4', 'x4'],
                ]} />
                <ToggleRow label="Start Fullscreen" value={settings.emuFullscreen} onChange={(value) => patchSettings({ emuFullscreen: value })} />
                <ToggleRow label="Show Broadcast Window" value={settings.broadcastWindow} onChange={(value) => patchSettings({ broadcastWindow: value })} />
                <ToggleRow label="Preserve emulator aspect ratio" value={settings.preserveEmuAspect} onChange={(value) => patchSettings({ preserveEmuAspect: value })} description="Keeps NES, SNES, GB/GBC, and GBA layouts from stretching into the tracker." />
                <ToggleRow label="Prevent tracker overflow" value={settings.preventTrackerOverflow} onChange={(value) => patchSettings({ preventTrackerOverflow: value })} />
                <ToggleRow label="Chatbox visible" value={settings.chatboxVisible} onChange={(value) => patchSettings({ chatboxVisible: value })} />
                <RangeRow label="Chatbox font size" value={settings.chatboxFontSize} min={10} max={24} onChange={(value) => patchSettings({ chatboxFontSize: value })} />
                <ToggleRow label="Disable autotabbing" value={settings.disableAutotabbing} onChange={(value) => patchSettings({ disableAutotabbing: value })} />
              </SettingGroup>
            </>
          )}

          {active === 'audio' && (
            <SettingGroup title="Audio">
              <RangeRow label="Master Volume" value={settings.masterVolume} min={0} max={100} onChange={(value) => patchSettings({ masterVolume: value })} />
              <RangeRow label="Sekaiemu Volume" value={settings.emuVolume} min={0} max={100} onChange={(value) => patchSettings({ emuVolume: value })} />
              <RangeRow label="Tracker / WebView Volume" value={settings.trackerVolume} min={0} max={100} onChange={(value) => patchSettings({ trackerVolume: value })} />
              <ToggleRow label="Mute background music while playing" value={settings.muteBackground} onChange={(value) => patchSettings({ muteBackground: value })} />
            </SettingGroup>
          )}

          {active === 'games-automation' && (
            <>
              <SettingGroup title="Sekaiemu Libretro">
                <SelectRow label="Default core launch mode" value="managed" onChange={() => undefined} options={[['managed', 'Managed by SekaiLink']]} />
              </SettingGroup>
              <SettingGroup title="SM64EX">
                <DisabledRow label="Install / Uninstall / Run Standalone" description="Coming soon" />
                <ToggleRow label="Start Fullscreen" value={false} onChange={() => undefined} disabled />
                <SelectRow label="Screen size" value="1280x720" onChange={() => undefined} options={[['1280x720', '1280x720'], ['1920x1080', '1920x1080']]} disabled />
              </SettingGroup>
              <SettingGroup title="Ship of Harkinian">
                <DisabledRow label="Install / Uninstall / Run Standalone" description="Requires a valid ROM in the ROM Library to generate the O2R." />
                <ToggleRow label="Use HD Graphics" value={true} onChange={() => undefined} disabled />
              </SettingGroup>
              <SettingGroup title="WebView Games and Trackers">
                <DisabledRow label="Persistent WebView settings" description="Coming soon: cache, permissions, zoom, audio, and tracker session restore." />
              </SettingGroup>
            </>
          )}

          {active === 'core-settings' && (
            <SettingGroup title={t('Core Settings')}>
              <SelectRow
                label={t('Core')}
                value={selectedCoreSettingsCore}
                onChange={setSelectedCoreSettingsCore}
                options={Object.entries(coreSettingsProfiles).map(([coreId, profile]) => [coreId, profile.label])}
                description={t(coreSettingsProfiles[selectedCoreSettingsCore]?.description || '')}
              />
              <div className="flex items-center justify-between gap-4 rounded-lg border border-[#2a2b30] bg-[#0e0f13] p-4">
                <div>
                  <div className="font-medium">{t('Settings mode')}</div>
                  <div className="text-sm text-[#8e8f94]">{t('Simple shows safe common options. Advanced exposes technical options that may require restart.')}</div>
                </div>
                <div className="inline-flex rounded-lg border border-[#2a2b30] bg-[#14151a] p-1">
                  {(['simple', 'advanced'] as CoreSettingsMode[]).map((mode) => (
                    <button
                      key={mode}
                      onClick={() => setCoreSettingsMode(mode)}
                      className={`px-4 py-2 rounded-md text-sm font-bold ${coreSettingsMode === mode ? 'bg-[#4ecdc4] text-[#0d1117]' : 'text-[#8e8f94] hover:text-white'}`}
                    >
                      {mode === 'simple' ? t('Simple') : t('Advanced')}
                    </button>
                  ))}
                </div>
              </div>
              <div className="rounded-lg border border-[#2a2b30] bg-[#0e0f13] p-4">
                <div className="mb-4 flex items-center justify-between gap-3">
                  <div>
                    <div className="font-bold">{coreSettingsProfiles[selectedCoreSettingsCore]?.label}</div>
                    <div className="text-sm text-[#8e8f94]">{t(coreSettingsProfiles[selectedCoreSettingsCore]?.description || '')}</div>
                  </div>
                  <button
                    onClick={() => resetCoreSettingsFor(selectedCoreSettingsCore, setCoreSettings)}
                    className="px-3 py-2 rounded-lg bg-[#2a2b30] hover:bg-[#3a3b40] text-sm"
                  >
                    {t('Reset Core Defaults')}
                  </button>
                </div>
                <div className="space-y-3">
                  {(coreSettingsProfiles[selectedCoreSettingsCore]?.options || [])
                    .filter((option) => coreSettingsMode === 'advanced' || !option.advanced)
                    .map((option) => (
                      <CoreSettingRow
                        key={option.key}
                        option={option}
                        value={coreSettings[selectedCoreSettingsCore]?.[option.key] || option.defaultValue}
                        onChange={(value) => updateCoreSetting(selectedCoreSettingsCore, option.key, value, setCoreSettings)}
                      />
                    ))}
                </div>
              </div>
            </SettingGroup>
          )}

          {active === 'control-settings' && (
            <SettingGroup title={t('Control Settings')}>
              <SelectRow
                label={t('Core to configure')}
                value={selectedControlCore}
                onChange={(value) => setSelectedControlCore(value as ControlCoreId)}
                options={Object.entries(controlProfiles).map(([id, profile]) => [id, profile.label])}
                description={t(controlProfiles[selectedControlCore].description)}
              />
              <SelectRow
                label={t('Controller')}
                value={selectedController}
                onChange={setSelectedController}
                options={gamepads.length ? gamepads.map((pad) => [String(pad.index), `${pad.id} (${pad.buttons} buttons, ${pad.axes} axes)`]) : [['', t('No controller detected')]]}
                description={t('Detected through Electron Gamepad API. SDL-native capture can replace this backend later without changing the saved mapping shape.')}
              />
              <div className="grid grid-cols-[minmax(280px,420px)_1fr] gap-5 items-start">
                <ControllerPreview
                  profile={controlProfiles[selectedControlCore]}
                  mappings={controlMappings[selectedControlCore] || {}}
                  captureTarget={captureTarget}
                />
                <div className="space-y-3">
                  <div className="flex items-center justify-between gap-3 rounded-lg border border-[#2a2b30] bg-[#0e0f13] p-4">
                    <div>
                      <div className="font-medium">{t('Use this core mapping by default')}</div>
                      <div className="text-sm text-[#8e8f94]">{t('New compatible games can start from this control profile unless a game overrides it.')}</div>
                    </div>
                    <button
                      onClick={() => setDefaultControlCore(selectedControlCore)}
                      className={`px-4 py-2 rounded-lg text-sm font-bold ${defaultControlCore === selectedControlCore ? 'bg-[#4ecdc4] text-[#0d1117]' : 'bg-[#2a2b30] hover:bg-[#3a3b40]'}`}
                    >
                      {defaultControlCore === selectedControlCore ? t('Default') : t('Set Default')}
                    </button>
                  </div>
                  <div className="rounded-lg border border-[#2a2b30] bg-[#0e0f13] p-4">
                    <div className="mb-3 flex items-center justify-between gap-3">
                      <div>
                        <div className="font-medium">{t('Button mapping')}</div>
                        <div className="text-sm text-[#8e8f94]">{t('Click a row, then press a controller input within 5 seconds.')}</div>
                      </div>
                      <button
                        onClick={() => {
                          setControlMappings((prev) => ({ ...prev, [selectedControlCore]: {} }));
                          setCaptureTarget(null);
                          setCaptureStatus(t('Mapping cleared.'));
                        }}
                        className="px-3 py-2 rounded-lg bg-[#2a2b30] hover:bg-[#3a3b40] text-sm"
                      >
                        {t('Clear')}
                      </button>
                    </div>
                    <div className="grid grid-cols-2 gap-2">
                      {controlProfiles[selectedControlCore].buttons.map((button) => (
                        <button
                          key={button}
                          onClick={() => setCaptureTarget(button)}
                          disabled={!selectedController}
                          className={`rounded-lg border px-3 py-2 text-left transition-colors disabled:opacity-50 ${
                            captureTarget === button
                              ? 'border-[#f69d50] bg-[#f69d50]/15'
                              : 'border-[#2a2b30] bg-[#14151a] hover:border-[#4ecdc4]'
                          }`}
                        >
                          <div className="text-sm font-bold">{button}</div>
                          <div className="text-xs text-[#8e8f94]">{controlMappings[selectedControlCore]?.[button] || t('Unmapped')}</div>
                        </button>
                      ))}
                    </div>
                    {captureStatus && <div className="mt-3 rounded-lg border border-[#2a2b30] bg-[#14151a] px-3 py-2 text-sm text-[#4ecdc4]">{captureStatus}</div>}
                  </div>
                </div>
              </div>
            </SettingGroup>
          )}

          {active === 'hotkeys' && (
            <SettingGroup title={t('Hotkeys')}>
              <div className="rounded-lg border border-[#2a2b30] bg-[#0e0f13] p-4">
                <div className="flex items-center justify-between gap-4">
                  <div>
                    <div className="font-medium">{t('Runtime shortcuts')}</div>
                    <div className="text-sm text-[#8e8f94]">{t('Click an action, then press a keyboard key or controller input within 5 seconds.')}</div>
                  </div>
                  <button
                    onClick={() => {
                      setHotkeys(defaultHotkeys());
                      setHotkeyCaptureTarget(null);
                      setHotkeyStatus(t('Hotkeys reset to defaults.'));
                    }}
                    className="px-3 py-2 rounded-lg bg-[#2a2b30] hover:bg-[#3a3b40] text-sm"
                  >
                    {t('Reset Defaults')}
                  </button>
                </div>
                <div className="mt-4 grid grid-cols-1 gap-2">
                  {hotkeyActions.map((action) => (
                    <button
                      key={action.id}
                      onClick={() => setHotkeyCaptureTarget(action.id)}
                      className={`rounded-lg border px-4 py-3 text-left transition-colors ${
                        hotkeyCaptureTarget === action.id
                          ? 'border-[#f69d50] bg-[#f69d50]/15'
                          : 'border-[#2a2b30] bg-[#14151a] hover:border-[#4ecdc4]'
                      }`}
                    >
                      <div className="flex items-center justify-between gap-4">
                        <div>
                          <div className="font-bold">{t(action.label)}</div>
                          <div className="text-sm text-[#8e8f94]">{t(action.description)}</div>
                        </div>
                        <kbd className="shrink-0 rounded-md border border-[#3a3b40] bg-[#0e0f13] px-3 py-2 text-sm font-bold text-[#4ecdc4]">
                          {formatBinding(hotkeys[action.id] || action.defaultBinding)}
                        </kbd>
                      </div>
                    </button>
                  ))}
                </div>
                {hotkeyStatus && <div className="mt-3 rounded-lg border border-[#2a2b30] bg-[#14151a] px-3 py-2 text-sm text-[#4ecdc4]">{hotkeyStatus}</div>}
              </div>
            </SettingGroup>
          )}

          {active === 'sklmi' && (
            <SettingGroup title="SKLMI">
              <ToggleRow label="Aggressive reconnect checkups" value={settings.sklmiReconnect} onChange={(value) => patchSettings({ sklmiReconnect: value })} />
              <ToggleRow label="Verbose runtime logs" value={settings.sklmiVerbose} onChange={(value) => patchSettings({ sklmiVerbose: value })} />
              <ToggleRow label="Allow user diagnostic log extraction" value={settings.sklmiExtractLogs} onChange={(value) => patchSettings({ sklmiExtractLogs: value })} description="Lets support collect Sekaiemu/SKLMI logs from the client side with user consent." />
              <DisabledRow label="Reconnect SKLMI command" description="Coming soon: admin-safe command endpoint and client runtime hook." />
            </SettingGroup>
          )}

          {active === 'social' && (
            <SettingGroup title="Social">
              <ToggleRow label="Block friend requests" value={settings.blockFriendRequests} onChange={(value) => patchSettings({ blockFriendRequests: value })} />
              <ToggleRow label="Minimize notifications" value={settings.minimizeNotifications} onChange={(value) => patchSettings({ minimizeNotifications: value })} description="Social notifications are hidden from toasts but still visible in the bell." />
            </SettingGroup>
          )}

          {active === 'connections' && (
            <SettingGroup title="Account Connections">
              <ConnectionRow label="Discord" />
              <ConnectionRow label="Patreon" />
              <ConnectionRow label="Twitch" />
            </SettingGroup>
          )}

          {active === 'twitch' && (
            <SettingGroup title="Twitch Bot">
              <DisabledRow label="Bot Join / Bot Leave" description="Coming soon: Twitch account association is required before bot settings can be edited." />
              <DisabledRow label="Command prefix, moderation, lobby announce, stream-safe hints" description="Will be aligned with the Twitch bot source settings." />
            </SettingGroup>
          )}

          {active === 'server-status' && (
            <SettingGroup title="SekaiLink Servers">
              <div className="grid grid-cols-5 gap-3">
                {(statusRows.length ? statusRows : serverNames.map((name) => ({ name, status: 'loading', cpu: undefined as string | undefined, ram: undefined as string | undefined, uptime: undefined as string | undefined }))).map((row) => (
                  <div key={row.name} className="rounded-lg border border-[#2a2b30] bg-[#14151a] p-4">
                    <div className="flex items-center justify-between mb-3">
                      <div className="font-bold">{row.name}</div>
                      <StatusPill status={row.status} />
                    </div>
                    <div className="space-y-1 text-xs text-[#8e8f94]">
                      <div>CPU: <span className="text-white">{row.cpu || 'n/a'}</span></div>
                      <div>RAM: <span className="text-white">{row.ram || 'n/a'}</span></div>
                      <div>Uptime: <span className="text-white">{row.uptime || 'n/a'}</span></div>
                    </div>
                  </div>
                ))}
              </div>
            </SettingGroup>
          )}
        </div>
      </main>
    </div>
  );
}

function SettingGroup({ title, children }: { title: string; children: React.ReactNode }) {
  return (
    <section className="rounded-xl border border-[#2a2b30] bg-[#14151a]/90 p-5">
      <h2 className="text-lg font-bold mb-4">{title}</h2>
      <div className="space-y-4">{children}</div>
    </section>
  );
}

function ToggleRow({ label, value, onChange, description, disabled }: {
  label: string;
  value: boolean;
  onChange: (value: boolean) => void;
  description?: string;
  disabled?: boolean;
}) {
  return (
    <div className="flex items-center justify-between gap-5">
      <div>
        <div className="font-medium">{label}</div>
        {description && <div className="text-sm text-[#8e8f94]">{description}</div>}
      </div>
      <button
        title={disabled ? 'Coming soon' : undefined}
        disabled={disabled}
        onClick={() => onChange(!value)}
        className={`relative w-12 h-6 border-2 transition-all disabled:opacity-50 ${value ? 'bg-[#4ecdc4] border-[#4ecdc4]' : 'bg-[#2a2b30] border-[#2a2b30]'}`}
      >
        <div className={`absolute top-0.5 w-4 h-4 bg-[#0d1117] transition-all ${value ? 'right-0.5' : 'left-0.5'}`} />
      </button>
    </div>
  );
}

function SelectRow({ label, value, onChange, options, description, disabled }: {
  label: string;
  value: string;
  onChange: (value: string) => void;
  options: Array<[string, string]>;
  description?: string;
  disabled?: boolean;
}) {
  return (
    <div className="grid grid-cols-[1fr_260px] items-center gap-5">
      <div>
        <div className="font-medium">{label}</div>
        {description && <div className="text-sm text-[#8e8f94]">{description}</div>}
      </div>
      <select
        title={disabled ? 'Coming soon' : undefined}
        disabled={disabled}
        value={value}
        onChange={(event) => onChange(event.target.value)}
        className="rounded-lg border-2 border-[#2a2b30] bg-[#0e0f13] px-3 py-2.5 outline-none disabled:opacity-50"
      >
        {options.map(([optionValue, optionLabel]) => <option key={optionValue} value={optionValue}>{optionLabel}</option>)}
      </select>
    </div>
  );
}

function RangeRow({ label, value, min, max, labels, onChange }: {
  label: string;
  value: number;
  min: number;
  max: number;
  labels?: string[];
  onChange: (value: number) => void;
}) {
  return (
    <div className="grid grid-cols-[1fr_300px] items-center gap-5">
      <div>
        <div className="font-medium">{label}</div>
        <div className="text-sm text-[#8e8f94]">{labels?.[value - min] || value}</div>
      </div>
      <input type="range" min={min} max={max} value={value} onChange={(event) => onChange(Number(event.target.value))} />
    </div>
  );
}

function DisabledRow({ label, description }: { label: string; description: string }) {
  return (
    <div title="Coming soon" className="rounded-lg border border-dashed border-[#3a3b40] bg-[#0e0f13]/70 p-4 opacity-70">
      <div className="flex items-center gap-2 font-medium">
        <CircleAlert className="w-4 h-4 text-[#f69d50]" />
        {label}
      </div>
      <p className="mt-1 text-sm text-[#8e8f94]">{description || 'Coming soon'}</p>
    </div>
  );
}

function ConnectionRow({ label }: { label: string }) {
  return (
    <div className="flex items-center justify-between rounded-lg border border-[#2a2b30] bg-[#0e0f13] p-4">
      <div className="flex items-center gap-3">
        <Shield className="w-5 h-5 text-[#8e8f94]" />
        <div>
          <div className="font-medium">{label}</div>
          <div className="text-sm text-[#8e8f94]">Coming soon</div>
        </div>
      </div>
      <button disabled title="Coming soon" className="px-4 py-2 rounded-lg bg-[#2a2b30] text-sm opacity-60">Connect</button>
    </div>
  );
}

function readGamepadSnapshot(index: number) {
  const pad = typeof navigator !== 'undefined' && navigator.getGamepads ? navigator.getGamepads()[index] : null;
  return {
    buttons: pad ? pad.buttons.map((button) => button.pressed || button.value > 0.55) : [],
    axes: pad ? pad.axes.map((axis) => axis) : [],
  };
}

function detectNewGamepadInput(
  baseline: ReturnType<typeof readGamepadSnapshot>,
  current: ReturnType<typeof readGamepadSnapshot>,
) {
  for (let index = 0; index < current.buttons.length; index += 1) {
    if (current.buttons[index] && !baseline.buttons[index]) return `button:${index}`;
  }
  for (let index = 0; index < current.axes.length; index += 1) {
    const before = baseline.axes[index] || 0;
    const now = current.axes[index] || 0;
    if (Math.abs(now) > 0.65 && Math.abs(now - before) > 0.35) {
      return `axis:${index}${now > 0 ? '+' : '-'}`;
    }
  }
  return '';
}

function hotkeyLabelFor(id: string) {
  return hotkeyActions.find((action) => action.id === id)?.label || id;
}

function formatKeyboardBinding(event: KeyboardEvent) {
  const parts = [];
  if (event.ctrlKey) parts.push('Ctrl');
  if (event.altKey) parts.push('Alt');
  if (event.shiftKey) parts.push('Shift');
  if (event.metaKey) parts.push('Meta');
  const key = normalizeKeyboardKey(event.key || event.code || '');
  if (key && !['Control', 'Alt', 'Shift', 'Meta'].includes(key)) parts.push(key);
  return `keyboard:${parts.join('+') || key || event.code || 'Unknown'}`;
}

function normalizeKeyboardKey(key: string) {
  if (key === ' ') return 'Space';
  if (key.length === 1) return key.toUpperCase();
  return key;
}

function formatBinding(binding: string) {
  if (!binding) return 'Unmapped';
  if (binding.startsWith('keyboard:')) return binding.slice('keyboard:'.length).replace(/\+/g, ' + ');
  if (binding.startsWith('gamepad:button:')) return `Pad Button ${binding.slice('gamepad:button:'.length)}`;
  if (binding.startsWith('gamepad:axis:')) {
    const axis = binding.slice('gamepad:axis:'.length);
    return `Pad Axis ${axis.replace('+', ' +').replace('-', ' -')}`;
  }
  if (binding.startsWith('button:')) return `Button ${binding.slice('button:'.length)}`;
  if (binding.startsWith('axis:')) return `Axis ${binding.slice('axis:'.length)}`;
  return binding;
}

function mergeCoreSettings(values: CoreSettingsValues) {
  const defaults = defaultCoreSettings();
  for (const [coreId, coreValues] of Object.entries(values || {})) {
    if (!defaults[coreId] || !coreValues || typeof coreValues !== 'object') continue;
    defaults[coreId] = { ...defaults[coreId], ...coreValues };
  }
  return defaults;
}

function updateCoreSetting(
  coreId: string,
  key: string,
  value: string,
  setCoreSettings: React.Dispatch<React.SetStateAction<CoreSettingsValues>>,
) {
  setCoreSettings((prev) => ({
    ...prev,
    [coreId]: {
      ...(prev[coreId] || {}),
      [key]: value,
    },
  }));
}

function resetCoreSettingsFor(
  coreId: string,
  setCoreSettings: React.Dispatch<React.SetStateAction<CoreSettingsValues>>,
) {
  const defaults = defaultCoreSettings();
  setCoreSettings((prev) => ({
    ...prev,
    [coreId]: defaults[coreId] || {},
  }));
}

function CoreSettingRow({ option, value, onChange }: {
  option: CoreSettingOption;
  value: string;
  onChange: (value: string) => void;
}) {
  return (
    <div className="grid grid-cols-[1fr_280px] gap-5 rounded-lg border border-[#2a2b30] bg-[#14151a] p-4">
      <div>
        <div className="flex flex-wrap items-center gap-2">
          <div className="font-medium">{option.label}</div>
          {option.advanced && <span className="rounded-full bg-[#7c5cff]/20 px-2 py-0.5 text-[10px] font-bold text-[#b8a7ff]">ADV</span>}
          {option.requiresRestart && <span className="rounded-full bg-[#f69d50]/20 px-2 py-0.5 text-[10px] font-bold text-[#f69d50]">RESTART</span>}
        </div>
        <div className="mt-1 text-sm text-[#8e8f94]">{option.description}</div>
        <code className="mt-2 block text-xs text-[#5e6068]">{option.key}</code>
      </div>
      <select
        value={value}
        onChange={(event) => onChange(event.target.value)}
        className="self-center rounded-lg border-2 border-[#2a2b30] bg-[#0e0f13] px-3 py-2.5 outline-none"
      >
        {option.values.map(([optionValue, optionLabel]) => (
          <option key={optionValue} value={optionValue}>{optionLabel}</option>
        ))}
      </select>
    </div>
  );
}

function ControllerPreview({ profile, mappings, captureTarget }: {
  profile: (typeof controlProfiles)[ControlCoreId];
  mappings: CoreControlMappings;
  captureTarget: string | null;
}) {
  const mapped = new Set(Object.keys(mappings));
  const isActive = (label: string) => captureTarget === label;
  const hasMap = (label: string) => mapped.has(label);
  const controllerClass = profile.controller === 'n64' ? 'aspect-[1.45]' : 'aspect-[1.7]';
  return (
    <div className="rounded-xl border border-[#2a2b30] bg-[#0e0f13] p-5">
      <div className="mb-4">
        <div className="font-bold">{profile.label}</div>
        <div className="text-sm text-[#8e8f94]">{profile.description}</div>
      </div>
      <div className={`relative w-full ${controllerClass} rounded-[42px] border-2 border-[#2a2b30] bg-gradient-to-br from-[#202127] to-[#111217] shadow-inner`}>
        <PreviewButton label="D-Pad Up" active={isActive('D-Pad Up')} mapped={hasMap('D-Pad Up')} className="left-[16%] top-[28%]" />
        <PreviewButton label="D-Pad Left" active={isActive('D-Pad Left')} mapped={hasMap('D-Pad Left')} className="left-[10%] top-[42%]" />
        <PreviewButton label="D-Pad Right" active={isActive('D-Pad Right')} mapped={hasMap('D-Pad Right')} className="left-[22%] top-[42%]" />
        <PreviewButton label="D-Pad Down" active={isActive('D-Pad Down')} mapped={hasMap('D-Pad Down')} className="left-[16%] top-[56%]" />
        {profile.controller === 'n64' && (
          <>
            <PreviewButton label="Stick Up" active={isActive('Stick Up')} mapped={hasMap('Stick Up')} className="left-[40%] top-[40%]" />
            <PreviewButton label="Z" active={isActive('Z')} mapped={hasMap('Z')} className="left-[45%] top-[62%]" />
            <PreviewButton label="C Up" active={isActive('C Up')} mapped={hasMap('C Up')} className="left-[76%] top-[25%]" />
            <PreviewButton label="C Left" active={isActive('C Left')} mapped={hasMap('C Left')} className="left-[70%] top-[39%]" />
            <PreviewButton label="C Right" active={isActive('C Right')} mapped={hasMap('C Right')} className="left-[82%] top-[39%]" />
            <PreviewButton label="C Down" active={isActive('C Down')} mapped={hasMap('C Down')} className="left-[76%] top-[53%]" />
          </>
        )}
        <PreviewButton label="Select" active={isActive('Select')} mapped={hasMap('Select')} className="left-[39%] top-[48%]" small />
        <PreviewButton label="Start" active={isActive('Start')} mapped={hasMap('Start')} className="left-[51%] top-[48%]" small />
        <PreviewButton label="B" active={isActive('B')} mapped={hasMap('B')} className="left-[70%] top-[48%]" />
        <PreviewButton label="A" active={isActive('A')} mapped={hasMap('A')} className="left-[82%] top-[38%]" />
        <PreviewButton label="Y" active={isActive('Y')} mapped={hasMap('Y')} className="left-[64%] top-[34%]" />
        <PreviewButton label="X" active={isActive('X')} mapped={hasMap('X')} className="left-[76%] top-[24%]" />
        <PreviewButton label="L" active={isActive('L')} mapped={hasMap('L')} className="left-[22%] top-[8%]" small />
        <PreviewButton label="R" active={isActive('R')} mapped={hasMap('R')} className="left-[70%] top-[8%]" small />
      </div>
      <div className="mt-4 flex items-center gap-3 text-xs text-[#8e8f94]">
        <span className="inline-flex items-center gap-1"><span className="h-2.5 w-2.5 rounded-full bg-[#4ecdc4]" />Mapped</span>
        <span className="inline-flex items-center gap-1"><span className="h-2.5 w-2.5 rounded-full bg-[#f69d50]" />Listening</span>
      </div>
    </div>
  );
}

function PreviewButton({ label, active, mapped, className, small = false }: {
  label: string;
  active: boolean;
  mapped: boolean;
  className: string;
  small?: boolean;
}) {
  const size = small ? 'h-8 min-w-12 px-2 text-[10px]' : 'h-10 min-w-10 px-2 text-xs';
  return (
    <div
      className={`absolute -translate-x-1/2 -translate-y-1/2 rounded-full border font-bold flex items-center justify-center ${size} ${
        active
          ? 'border-[#f69d50] bg-[#f69d50] text-[#0d1117] shadow-[0_0_18px_rgba(246,157,80,0.8)]'
          : mapped
            ? 'border-[#4ecdc4] bg-[#4ecdc4] text-[#0d1117]'
            : 'border-[#3a3b40] bg-[#14151a] text-[#8e8f94]'
      } ${className}`}
      title={label}
    >
      {label.replace('D-Pad ', '').replace('Stick ', '')}
    </div>
  );
}

function StatusPill({ status }: { status: string }) {
  const normalized = status.toLowerCase();
  const online = normalized === 'online' || normalized === 'ok' || normalized === 'healthy';
  const loading = normalized === 'loading';
  return (
    <span className={`inline-flex items-center gap-1 rounded-full px-2 py-1 text-[10px] font-bold ${
      online ? 'bg-[#4ecdc4]/20 text-[#4ecdc4]' : loading ? 'bg-[#8e8f94]/20 text-[#8e8f94]' : 'bg-[#f85149]/20 text-[#f85149]'
    }`}>
      {online ? <CheckCircle className="w-3 h-3" /> : loading ? <Cpu className="w-3 h-3" /> : <Database className="w-3 h-3" />}
      {status}
    </span>
  );
}
