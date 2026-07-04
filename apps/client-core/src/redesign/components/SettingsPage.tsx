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
  RefreshCcw,
  Save,
  Search,
  Server,
  Shield,
  SlidersHorizontal,
  Trash2,
  Twitch,
  Upload,
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
import { publicAssetUrl } from '../../utils/publicAssets';

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
const LEARNING_LABEL = 'In learning';
const LEARNING_DESCRIPTION = 'SekaiLink is learning this flow. It is visible for planning, but disabled until the integration is reliable.';

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

type RuntimeModuleInfo = {
  moduleId?: string;
  manifest?: Record<string, any>;
};

type RomRequirementComponent = {
  label: string;
  expectedBase: string;
  md5?: string;
  sha1?: string;
  sha256?: string;
  keys?: string[];
};

type RomRequirementInfo = {
  gameKey: string;
  platform?: string;
  guidance?: string;
  components: RomRequirementComponent[];
};

type CompatibleRomVersion = {
  label: string;
  expectedBase: string;
  md5: string[];
  sha1: string[];
  sha256: string[];
  key: string;
};

type CompatibleRomGroup = {
  title: string;
  mode: 'alternatives' | 'required';
  versions: CompatibleRomVersion[];
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
  { id: 'toggle_fullscreen', label: 'Cycle window mode', description: 'Switch Sekaiemu between Window, Borderless Window, and Fullscreen.', defaultBinding: 'keyboard:F12' },
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

const normalizeRomKey = (value?: string | null) =>
  String(value || '')
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, '_')
    .replace(/^_+|_+$/g, '');

const ROM_REQUIREMENTS: Record<string, RomRequirementInfo> = {
  a_link_to_the_past: {
    gameKey: 'a_link_to_the_past',
    platform: 'SNES',
    guidance: 'Common Archipelago/SekaiLink base. Use a clean, unmodified ROM.',
    components: [
      {
        label: 'A Link to the Past',
        expectedBase: 'Zelda no Densetsu - Kamigami no Triforce (Japan).sfc',
        md5: '03a63945398191337e896e5771f77173',
        keys: ['a_link_to_the_past', 'alttp'],
      },
    ],
  },
  final_fantasy_tactics_advance: {
    gameKey: 'final_fantasy_tactics_advance',
    platform: 'GBA',
    components: [
      {
        label: 'Final Fantasy Tactics Advance',
        expectedBase: 'Final Fantasy Tactics Advance (USA, Australia).gba',
        md5: 'cd99cdde3d45554c1b36fbeb8863b7bd',
      },
    ],
  },
  final_fantasy_v: {
    gameKey: 'final_fantasy_v',
    platform: 'SNES',
    components: [
      {
        label: 'Final Fantasy V Career Day',
        expectedBase: 'Final Fantasy V (J).sfc',
        md5: 'd69b2115e17d1cf2cb3590d3f75febb9',
      },
    ],
  },
  pokemon_crystal: {
    gameKey: 'pokemon_crystal',
    platform: 'GBC',
    components: [
      {
        label: 'Pokemon Crystal',
        expectedBase: 'Pokemon - Crystal Version (UE) v1.1.gbc',
        md5: '301899b8087289a6436b0a241fbbb474',
      },
    ],
  },
  pokemon_emerald: {
    gameKey: 'pokemon_emerald',
    platform: 'GBA',
    components: [
      {
        label: 'Pokemon Emerald',
        expectedBase: 'Pokemon - Emerald Version (USA, Europe).gba',
        md5: '605b89b67018abcea91e693a4dd25be3',
      },
    ],
  },
  pokemon_firered: {
    gameKey: 'pokemon_firered',
    platform: 'GBA',
    components: [
      {
        label: 'Pokemon FireRed',
        expectedBase: 'Pokemon - FireRed Version (USA, Europe).gba',
        md5: 'e26ee0d44e809351c8ce2d73c7400cdd',
      },
    ],
  },
  pokemon_red_blue: {
    gameKey: 'pokemon_red_blue',
    platform: 'GB',
    guidance: 'Import the version used by the generated seed. Red and Blue validate separately.',
    components: [
      {
        label: 'Pokemon Red',
        expectedBase: 'Pokemon - Red Version (USA, Europe) (SGB Enhanced).gb',
        md5: '3d45c1ee9abd5738df46d2bdda8b57dc',
        keys: ['pokemon_red', 'pokemon_red_blue'],
      },
      {
        label: 'Pokemon Blue',
        expectedBase: 'Pokemon - Blue Version (USA, Europe) (SGB Enhanced).gb',
        md5: '50927e843568814f7ed45ec4f944bd8b',
        keys: ['pokemon_blue'],
      },
    ],
  },
  smz3: {
    gameKey: 'smz3',
    platform: 'SNES',
    guidance: 'SMZ3 is a multi-ROM game. Import both base ROMs; there is no fake SMZ3 ROM.',
    components: [
      {
        label: 'A Link to the Past',
        expectedBase: 'Zelda no Densetsu - Kamigami no Triforce (Japan).sfc',
        md5: '03a63945398191337e896e5771f77173',
        keys: ['a_link_to_the_past', 'alttp'],
      },
      {
        label: 'Super Metroid',
        expectedBase: 'Super Metroid (JU).sfc',
        md5: '21f3e98df4780ee1c667b84e57d88675',
        keys: ['super_metroid'],
      },
    ],
  },
  super_metroid: {
    gameKey: 'super_metroid',
    platform: 'SNES',
    components: [
      {
        label: 'Super Metroid',
        expectedBase: 'Super Metroid (JU).sfc',
        md5: '21f3e98df4780ee1c667b84e57d88675',
      },
    ],
  },
  secret_of_evermore: {
    gameKey: 'secret_of_evermore',
    platform: 'SNES',
    guidance: 'Use this clean USA base ROM. SekaiLink can validate it by checksum.',
    components: [
      {
        label: 'Secret of Evermore',
        expectedBase: 'Secret of Evermore (USA).sfc',
        md5: '6e9c94511d04fac6e0a1e582c170be3a',
        keys: ['secret_of_evermore', 'soe'],
      },
    ],
  },
  wario_land: {
    gameKey: 'wario_land',
    platform: 'GB',
    components: [
      {
        label: 'Wario Land',
        expectedBase: 'Wario Land - Super Mario Land 3 (World).gb',
        md5: 'd9d957771484ef846d4e8d241f6f2815',
      },
    ],
  },
};

const platformForGameKey = (gameKey: string) => {
  if (['metroid_fusion', 'metroid_zero_mission', 'the_minish_cap', 'wario_land_4'].includes(gameKey)) return 'GBA';
  if (['mega_man_2', 'mega_man_3', 'the_legend_of_zelda', 'zelda_ii'].includes(gameKey)) return 'NES';
  if (gameKey === 'wario_land') return 'GB';
  if (gameKey.startsWith('pokemon_')) return gameKey === 'pokemon_crystal' ? 'GBC' : 'GBA/GB';
  return 'SNES';
};

const componentLookupKeys = (gameKey: string, component: RomRequirementComponent, includeGameKey: boolean) =>
  [includeGameKey ? gameKey : '', component.label, ...(component.keys || [])]
    .map(normalizeRomKey)
    .filter(Boolean);

const hasChecksumValue = (value: unknown) =>
  Array.isArray(value)
    ? value.some((entry) => String(entry || '').trim())
    : Boolean(String(value || '').trim());

const ROM_GAME_ALIASES: Record<string, string[]> = {
  a_link_to_the_past: ['alttp', 'zelda_no_densetsu_kamigami_no_triforce'],
  super_mario_64: ['sm64ex', 'sm64'],
  links_awakening_dx: ['ladx'],
  ocarina_of_time: ['oot', 'oot_soh'],
  the_minish_cap: ['tmc'],
  the_legend_of_zelda: ['tloz'],
};

const APWORLD_EXPECTED_ROM_NAMES: Record<string, string[]> = {
  a_link_between_worlds: ['A Link Between Worlds.3ds', 'A Link Between Worlds.cci'],
  a_link_to_the_past: ['Zelda no Densetsu - Kamigami no Triforce (Japan).sfc'],
  alttp: ['Zelda no Densetsu - Kamigami no Triforce (Japan).sfc'],
  donkey_kong_64: ['dk64.z64'],
  dk64: ['dk64.z64'],
  donkey_kong_country: ['Donkey Kong Country (USA).sfc'],
  donkey_kong_country_2: ["Donkey Kong Country 2 - Diddy's Kong Quest (USA).sfc"],
  donkey_kong_country_3: ["Donkey Kong Country 3 - Dixie Kong's Double Trouble! (USA) (En,Fr).sfc"],
  earthbound: ['EarthBound.sfc'],
  final_fantasy: ['Final Fantasy (USA).nes', 'Final Fantasy(USA).nes'],
  final_fantasy_iv: ['Final Fantasy II (USA) (Rev A).sfc'],
  final_fantasy_mystic_quest: ['Final Fantasy - Mystic Quest (U) (V1.0).sfc', 'Final Fantasy - Mystic Quest (U) (V1.1).sfc'],
  final_fantasy_tactics_advance: ['Final Fantasy Tactics Advance (USA).gba'],
  final_fantasy_v: ['Final Fantasy V (J).sfc'],
  kirbys_dream_land_3: ["Kirby's Dream Land 3.sfc"],
  kdl3: ["Kirby's Dream Land 3.sfc"],
  links_awakening_dx: ["Legend of Zelda, The - Link's Awakening DX (USA, Europe) (SGB Enhanced).gbc"],
  ladx: ["Legend of Zelda, The - Link's Awakening DX (USA, Europe) (SGB Enhanced).gbc"],
  lufia_ii: ['Lufia II - Rise of the Sinistrals (USA).sfc'],
  lufia2ac: ['Lufia II - Rise of the Sinistrals (USA).sfc'],
  mario_kart_double_dash: ['Mario Kart - Double Dash!! (USA).iso', 'Mario Kart - Double Dash!! (USA).rvz'],
  mario_luigi_superstar_saga: ['Mario & Luigi - Superstar Saga (U).gba'],
  mega_man_2: ['Mega Man 2 (USA).nes'],
  mega_man_3: ['Mega Man 3 (USA).nes'],
  mega_man_battle_network_3: ['Mega Man Battle Network 3 - Blue Version (USA).gba'],
  mega_man_x3: ['Mega Man X3 (USA).sfc'],
  metroid_fusion: ['Metroid Fusion (USA).gba'],
  metroidfusion: ['Metroid Fusion (USA).gba'],
  metroid_prime: ['Metroid_Prime.iso'],
  metroid_zero_mission: ['Metroid - Zero Mission (USA).gba'],
  mzm: ['Metroid - Zero Mission (USA).gba'],
  ocarina_of_time: ['The Legend of Zelda - Ocarina of Time.z64'],
  oot: ['The Legend of Zelda - Ocarina of Time.z64'],
  oracle_of_ages: ['Legend of Zelda, The - Oracle of Ages (USA).gbc'],
  oracle_of_seasons: ['Legend of Zelda, The - Oracle of Seasons (USA).gbc'],
  paper_mario: ['Paper Mario (USA).z64'],
  pokemon_crystal: ['Pokemon - Crystal Version (UE) v1.1.gbc'],
  pokemon_emerald: ['Pokemon - Emerald Version (USA, Europe).gba'],
  pokemon_firered: ['Pokemon - FireRed Version (USA, Europe).gba', 'Pokemon - LeafGreen Version (USA, Europe).gba'],
  pokemon_red_blue: ['Pokemon Red (UE) [S][!].gb', 'Pokemon Blue (UE) [S][!].gb'],
  secret_of_evermore: ['Secret of Evermore (USA).sfc'],
  soe: ['Secret of Evermore (USA).sfc'],
  secret_of_mana: ['Secret of Mana (USA).sfc'],
  sm64ex: ['Super Mario 64 (USA).z64'],
  super_mario_64: ['Super Mario 64 (USA).z64'],
  super_mario_land_2: ['Super Mario Land 2 - 6 Golden Coins (USA, Europe).gb'],
  super_mario_sunshine: ['Super Mario Sunshine (USA) NTSC-U.iso'],
  sms: ['Super Mario Sunshine (USA) NTSC-U.iso'],
  super_mario_world: ['Super Mario World (USA).sfc'],
  smw: ['Super Mario World (USA).sfc'],
  super_metroid: ['Super Metroid (JU).sfc'],
  sm: ['Super Metroid (JU).sfc'],
  the_legend_of_zelda: ['Legend of Zelda, The (U) (PRG0) [!].nes'],
  tloz: ['Legend of Zelda, The (U) (PRG0) [!].nes'],
  the_minish_cap: ['Legend of Zelda, The - The Minish Cap (Europe).gba'],
  tmc: ['Legend of Zelda, The - The Minish Cap (Europe).gba'],
  thousand_year_door: ['Paper Mario - The Thousand-Year Door (USA).iso'],
  ttyd: ['Paper Mario - The Thousand-Year Door (USA).iso'],
  the_wind_waker: ['Legend of Zelda, The - The Wind Waker (USA).iso'],
  tww: ['Legend of Zelda, The - The Wind Waker (USA).iso'],
  wario_land: ['Wario Land - Super Mario Land 3 (World).gb'],
  wl: ['Wario Land - Super Mario Land 3 (World).gb'],
  wario_land_4: ['Wario Land 4.gba'],
  wl4: ['Wario Land 4.gba'],
  zelda_ii: ['Zelda 2.nes'],
  zelda2: ['Zelda 2.nes'],
};

const romGameLookupKeys = (gameKey: string) => {
  const normalized = normalizeRomKey(gameKey);
  return [normalized, ...(ROM_GAME_ALIASES[normalized] || [])].map(normalizeRomKey);
};

const expectedRomNamesFor = (gameKey: string, manifest?: Record<string, any>, moduleId?: string) => {
  const keys = [
    gameKey,
    moduleId,
    manifest?.game_id,
    manifest?.game_key,
    manifest?.ap_world,
    manifest?.archipelago_client?.world,
    manifest?.archipelago_client?.game_key,
    ...(Array.isArray(manifest?.required_roms) ? manifest.required_roms : []),
    ...romGameLookupKeys(gameKey),
  ].map(normalizeRomKey);
  return [...new Set(keys.flatMap((key) => APWORLD_EXPECTED_ROM_NAMES[key] || []))];
};

const romRequirementForGame = (gameKey: string, displayName: string, manifest?: Record<string, any>, moduleId?: string): RomRequirementInfo => {
  const normalized = normalizeRomKey(gameKey);
  const known = ROM_REQUIREMENTS[normalized];
  if (known) return known;
  const expectedNames = expectedRomNamesFor(normalized, manifest, moduleId);
  return {
    gameKey: normalized,
    platform: platformForGameKey(normalized),
  guidance: expectedNames.length
      ? 'Use one of the listed base ROM filenames. SekaiLink validates checksums when that game provides them.'
      : 'SekaiLink does not have a filename entry for this game yet. Use Select or Scan; if the game provides validation data, SekaiLink will still verify the file.',
    components: expectedNames.length
      ? expectedNames.map((expectedBase, index) => ({
          label: expectedNames.length > 1 ? `${displayName} ${index + 1}` : displayName,
          expectedBase,
          keys: [normalized],
        }))
      : [
          {
            label: displayName,
            expectedBase: `${displayName} base ROM filename not documented in SekaiLink yet`,
            keys: [normalized],
          },
        ],
  };
};

const summarizeRomRequirement = (requirement: RomRequirementInfo) =>
  requirement.components
    .map((component) => `${component.label}: ${component.expectedBase}${component.md5 ? ` (MD5 ${component.md5})` : ''}`)
    .join(' | ');

export default function SettingsPage() {
  const { locale, setLocale, t } = useI18n();
  const [active, setActive] = useState<SettingsSection>('general');
  const [statusRows, setStatusRows] = useState<Array<{ name: string; status: string; cpu?: string; ram?: string; uptime?: string }>>([]);
  const [importStatus, setImportStatus] = useState('');
  const [cacheStatus, setCacheStatus] = useState('');
  const [releaseChannel, setReleaseChannel] = useState('canonical');
  const [releaseChannelStatus, setReleaseChannelStatus] = useState('');
  const [releaseChannelBusy, setReleaseChannelBusy] = useState(false);
  const [trackerActionStatus, setTrackerActionStatus] = useState('');
  const [clearingCache, setClearingCache] = useState(false);
  const [romImportGameId, setRomImportGameId] = useState('');
  const [romImportModalGameId, setRomImportModalGameId] = useState('');
  const [romSearch, setRomSearch] = useState('');
  const [romShowAllGames, setRomShowAllGames] = useState(true);
  const [pendingRomImport, setPendingRomImport] = useState<{ filePath: string; candidates: RomImportCandidate[] } | null>(null);
  const [configuredRoms, setConfiguredRoms] = useState<Record<string, string>>({});
  const [runtimeModules, setRuntimeModules] = useState<RuntimeModuleInfo[]>([]);
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
    emuWindowMode: 'borderless-window',
    broadcastWindow: true,
    preserveEmuAspect: true,
    preventTrackerOverflow: true,
    hudButtonsVisible: true,
    backgroundGamepadInput: false,
    chatboxVisible: true,
    chatboxFontSize: 14,
    disableAutotabbing: false,
    masterVolume: 85,
    emuVolume: 35,
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
  const romLibraryRows = useMemo(() => {
    const normalizedRoms = Object.fromEntries(
      Object.entries(configuredRoms || {}).map(([key, value]) => [normalizeRomKey(key), String(value || '')])
    );
    return romImportGames.map((game) => {
      const gameKey = normalizeRomKey(game.key);
      const gameLookupKeys = romGameLookupKeys(game.key);
      const module = runtimeModules.find((entry) => {
        const manifest = entry?.manifest || {};
        const keys = [
          entry?.moduleId,
          manifest.game_id,
          manifest.game_key,
          manifest.ap_world,
          manifest.display_name,
          manifest.game_name,
          manifest.archipelago_client?.game_key,
          ...(Array.isArray(manifest.required_roms) ? manifest.required_roms : []),
          ...(Array.isArray(manifest.aliases) ? manifest.aliases : []),
        ].map(normalizeRomKey);
        return gameLookupKeys.some((key) => keys.includes(key));
      });
      const manifest = module?.manifest || {};
      const lookupKeys = [
        game.key,
        ...gameLookupKeys,
        module?.moduleId,
        manifest.game_id,
        manifest.game_key,
        manifest.ap_world,
        manifest.display_name,
        manifest.game_name,
        manifest.archipelago_client?.game_key,
        ...(Array.isArray(manifest.required_roms) ? manifest.required_roms : []),
        ...(Array.isArray(manifest.aliases) ? manifest.aliases : []),
      ].map(normalizeRomKey);
      const romPath = lookupKeys.map((key) => normalizedRoms[key]).find(Boolean) || '';
      const req = manifest.rom_requirements || {};
      const requirement = romRequirementForGame(game.key, game.displayName, manifest, module?.moduleId);
      const importedComponents = romPath
        ? requirement.components.length
        : requirement.components.filter((component) =>
            componentLookupKeys(game.key, component, requirement.components.length === 1).some((key) => Boolean(normalizedRoms[key]))
          ).length;
      const requiredComponents = requirement.components.length;
      const hasKnownChecksum = Boolean(
        hasChecksumValue(req.md5) ||
        hasChecksumValue(req.sha1) ||
        hasChecksumValue(req.sha256) ||
        hasChecksumValue(req.sha3) ||
        hasChecksumValue(req.sha3_256) ||
        hasChecksumValue(req['sha3-256']) ||
        requirement.components.some((component) => hasChecksumValue(component.md5))
      );
      const supportStatus = String(manifest.support_status || '').trim();
      const explicitlyUnavailable = Boolean(game.forceUnavailable || supportStatus === 'unsupported' || supportStatus === 'not_available');
      const importable = Boolean(!explicitlyUnavailable && (game.available || module?.moduleId));
      return {
        key: game.key,
        displayName: game.displayName,
        asset: game.asset,
        manifest,
        moduleId: module?.moduleId || '',
        supportStatus,
        importable,
        romPath,
        hasRom: importedComponents >= requiredComponents,
        isIncomplete: importedComponents > 0 && importedComponents < requiredComponents,
        importedComponents,
        requiredComponents,
        hasKnownChecksum,
        requirement,
      };
    });
  }, [configuredRoms, romImportGames, runtimeModules]);
  const selectedRomRow = useMemo(
    () => romLibraryRows.find((entry) => entry.key === romImportGameId) || null,
    [romImportGameId, romLibraryRows]
  );
  const modalRomRow = useMemo(
    () => romLibraryRows.find((entry) => entry.key === romImportModalGameId) || null,
    [romImportModalGameId, romLibraryRows]
  );
  const filteredRomLibraryRows = useMemo(() => {
    const needle = normalizeRomKey(romSearch);
    return romLibraryRows.filter((row) => {
      if (!romShowAllGames && !row.importable) return false;
      if (!needle) return true;
      return `${normalizeRomKey(row.displayName)} ${normalizeRomKey(row.key)} ${normalizeRomKey(row.moduleId)}`.includes(needle);
    });
  }, [romLibraryRows, romSearch, romShowAllGames]);

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
	        const roms = root.roms && typeof root.roms === 'object' ? root.roms : {};
	        setConfiguredRoms(roms as Record<string, string>);
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
          emuWindowMode: typeof sekaiemu.window_mode === 'string'
            ? sekaiemu.window_mode
            : (sekaiemu.start_fullscreen === true ? 'fullscreen' : prev.emuWindowMode),
          broadcastWindow: typeof sekaiemu.broadcast_window === 'boolean' ? sekaiemu.broadcast_window : prev.broadcastWindow,
          preserveEmuAspect: typeof sekaiemu.preserve_aspect === 'boolean' ? sekaiemu.preserve_aspect : prev.preserveEmuAspect,
          preventTrackerOverflow: typeof sekaiemu.prevent_tracker_overflow === 'boolean' ? sekaiemu.prevent_tracker_overflow : prev.preventTrackerOverflow,
          hudButtonsVisible: typeof sekaiemu.hud_buttons_visible === 'boolean' ? sekaiemu.hud_buttons_visible : prev.hudButtonsVisible,
          backgroundGamepadInput: typeof sekaiemu.background_gamepad_input === 'boolean' ? sekaiemu.background_gamepad_input : prev.backgroundGamepadInput,
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
	    let cancelled = false;
	    const loadRuntimeModules = async () => {
	      try {
	        const result = await runtime.listRuntimeModules?.();
	        if (cancelled) return;
	        setRuntimeModules(Array.isArray(result) ? result as RuntimeModuleInfo[] : []);
	      } catch (error) {
	        traceError('settings-page', 'runtime_modules_load_failed', error);
	      }
	    };
	    void loadRuntimeModules();
	    return () => {
	      cancelled = true;
	    };
	  }, []);

  useEffect(() => {
    let cancelled = false;
    const loadReleaseChannel = async () => {
      try {
        const result = await runtime.bootstrapperGetReleaseChannel?.() as { ok?: boolean; channel?: string; installedChannel?: string; installedVersion?: string } | undefined;
        if (cancelled) return;
        const channel = String(result?.channel || 'canonical').trim().toLowerCase();
        setReleaseChannel(channel === 'canari' ? 'canari' : 'canonical');
      } catch (error) {
        traceError('settings-page', 'release_channel_load_failed', error);
      }
    };
    void loadReleaseChannel();
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

  const refreshConfiguredRoms = async () => {
    const config = await Promise.resolve(runtime.configGet?.()).catch(() => null);
    if (config && typeof config === 'object' && (config as any).roms && typeof (config as any).roms === 'object') {
      setConfiguredRoms((config as any).roms as Record<string, string>);
    }
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
        return false;
      }
      setPendingRomImport(null);
      const expected = String((result as any)?.expected || (result as any)?.expected_checksum || '');
      const selectedGame = romImportGames.find((game) => game.key === (gameId || romImportGameId));
      const requirement = selectedGame ? romRequirementForGame(selectedGame.key, selectedGame.displayName) : null;
      const expectedHint = requirement ? summarizeRomRequirement(requirement) : expected;
      setImportStatus(expectedHint ? `ROM invalide: ${error}. ROM attendue: ${expectedHint}` : `ROM invalide: ${error}. Choisis le jeu cible si l'auto-detection ne reconnait pas cette ROM.`);
      return false;
    }
    await refreshConfiguredRoms();
    setPendingRomImport(null);
    const importedName = String((result as any)?.displayName || (result as any)?.gameId || '');
    const matchKind = String((result as any)?.matchKind || '');
    setImportStatus(
      importedName
        ? `${t('ROM imported and cached locally.')} ${importedName}${matchKind ? ` (${matchKind})` : ''}.`
        : t('ROM imported and cached locally.')
    );
    return true;
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

  const openRomImportModal = (gameId: string) => {
    setRomImportGameId(gameId);
    setRomImportModalGameId(gameId);
    setPendingRomImport(null);
    setImportStatus('');
  };

  const importModalRom = async (gameId: string) => {
    setRomImportGameId(gameId);
    setImportStatus('');
    setPendingRomImport(null);
    try {
      const file = await runtime.pickFile?.({
        title: t('Import ROM File'),
        filters: [
          { name: 'ROM files', extensions: ['sfc', 'smc', 'nes', 'gb', 'gbc', 'gba', 'z64', 'n64', 'iso', 'rvz', 'gcm'] },
          { name: 'All files', extensions: ['*'] },
        ],
      });
      const filePath = typeof file === 'string' ? file : String((file as any)?.path || '');
      if (!filePath) return;
      const imported = await importRomPath(filePath, gameId);
      if (imported) {
        setRomImportModalGameId('');
      }
    } catch (error) {
      traceError('settings-page', 'rom_modal_import_failed', error);
      setImportStatus(error instanceof Error ? `${t('ROM invalide')}: ${error.message}` : `${t('ROM invalide')}: unable to import file.`);
    }
  };

  const importPendingRomCandidate = async (gameId: string) => {
    const filePath = pendingRomImport?.filePath || '';
    if (!filePath) return;
    try {
      const imported = await importRomPath(filePath, gameId);
      if (imported) {
        setRomImportModalGameId('');
      }
    } catch (error) {
      traceError('settings-page', 'rom_modal_candidate_import_failed', error);
      setImportStatus(error instanceof Error ? `${t('ROM invalide')}: ${error.message}` : `${t('ROM invalide')}: unable to import file.`);
    }
  };

  const scanModalRoms = async () => {
    setImportStatus('');
    setPendingRomImport(null);
    try {
      const folder = await runtime.pickFolder?.({ title: t('Scan ROM folder') });
      const folderPath = typeof folder === 'string' ? folder : String((folder as any)?.path || '');
      if (!folderPath) return;
      const result = await runtime.romsScan?.(folderPath);
      const results = Array.isArray((result as any)?.results) ? (result as any).results : [];
      await refreshConfiguredRoms();
      setImportStatus(results.length ? `${results.length} ${t('ROM(s) imported from folder scan.')}` : t('No compatible ROM found in this folder.'));
      if (results.length) {
        setRomImportModalGameId('');
      }
    } catch (error) {
      traceError('settings-page', 'rom_modal_scan_failed', error);
      setImportStatus(error instanceof Error ? `${t('Scan failed')}: ${error.message}` : t('Scan failed'));
    }
  };

  const configureWithSekaiemu = async () => {
    setCaptureTarget(null);
    setCaptureStatus(t('Opening Sekaiemu controller capture...'));
    try {
      const result = await runtime.sekaiEmuInputCapture?.({ profile: selectedControlCore, coreId: selectedControlCore });
      if (!result?.ok) {
        const detail = String(result?.detail || result?.error || 'capture_failed');
        setCaptureStatus(`Sekaiemu capture failed: ${detail}`);
        return;
      }
      const mappings = result.mappings && typeof result.mappings === 'object' ? result.mappings : {};
      const nextCoreMappings = {
        ...controlMappings,
        [selectedControlCore]: {
          ...(controlMappings[selectedControlCore] || {}),
          ...mappings,
        },
      };
      setControlMappings((prev) => ({
        ...prev,
        [selectedControlCore]: {
          ...(prev[selectedControlCore] || {}),
          ...mappings,
        },
      }));
      const config = await runtime.configGet?.();
      const currentLayout = config && typeof config === 'object' && (config as any).layout && typeof (config as any).layout === 'object'
        ? (config as any).layout
        : {};
      await runtime.configSetLayout?.({
        ...currentLayout,
        input: {
          ...(currentLayout.input && typeof currentLayout.input === 'object' ? currentLayout.input : {}),
          capture_backend: 'sekaiemu-sdl',
          selected_controller_id: selectedController,
          default_core_id: defaultControlCore,
          core_mappings: nextCoreMappings,
        },
      });
      const count = Object.keys(mappings).length;
      setCaptureStatus(count ? `Sekaiemu capture imported and saved ${count} bindings.` : 'Sekaiemu capture completed with no bindings.');
    } catch (error) {
      traceError('settings-page', 'sekaiemu_input_capture_failed', error);
      setCaptureStatus(error instanceof Error ? `Sekaiemu capture failed: ${error.message}` : 'Sekaiemu capture failed.');
    }
  };

  const clearSeedCache = async () => {
    const confirmed = window.confirm(
      [
        'Clear generated seed cache?',
        '',
        'This removes cached generated seeds, downloaded patches, and patched ROM outputs.',
        'Imported base ROMs stay in the ROM Library.',
        '',
        'If a sync is currently in progress, its generated seed files may be erased and you may need to relaunch/re-download that seed.',
        '',
        'Continue?',
      ].join('\n')
    );
    if (!confirmed || clearingCache) return;
    setClearingCache(true);
    setCacheStatus('');
    try {
      const result = await runtime.clearSeedCache?.();
      if (!result?.ok) {
        const failedCount = Array.isArray(result?.failed) ? result.failed.length : 0;
        setCacheStatus(`Cache cleanup finished with ${failedCount || 1} warning(s). Some files may still be in use.`);
        return;
      }
      const clearedCount = Array.isArray(result.cleared) ? result.cleared.length : 0;
      const activeCount = Number(result.activeRuntimeCount || 0);
      setCacheStatus(
        activeCount > 0
          ? `Seed cache cleared (${clearedCount} folders). ${activeCount} runtime session(s) were active; relaunch any affected seed if needed.`
          : `Seed cache cleared (${clearedCount} folders).`
      );
    } catch (error) {
      traceError('settings-page', 'clear_seed_cache_failed', error);
      setCacheStatus(error instanceof Error ? `Cache cleanup failed: ${error.message}` : 'Cache cleanup failed.');
    } finally {
      setClearingCache(false);
    }
  };

  const switchReleaseChannel = async (channel: 'canonical' | 'canari') => {
    if (releaseChannelBusy) return;
    setReleaseChannelBusy(true);
    setReleaseChannelStatus('');
    try {
      const result = await runtime.bootstrapperSetReleaseChannel?.(channel) as { ok?: boolean; channel?: string; error?: string } | undefined;
      if (!result?.ok) {
        setReleaseChannelStatus(result?.error || 'Unable to change release channel.');
        return;
      }
      const next = String(result.channel || channel).toLowerCase() === 'canari' ? 'canari' : 'canonical';
      setReleaseChannel(next);
      setReleaseChannelStatus(
        next === 'canari'
          ? 'Canari enabled. Restart SekaiLink to let the bootstrapper download Canari builds.'
          : 'Canonical selected. Restart SekaiLink to let the bootstrapper return to Canonical builds.'
      );
    } catch (error) {
      traceError('settings-page', 'release_channel_set_failed', error);
      setReleaseChannelStatus(error instanceof Error ? error.message : 'Unable to change release channel.');
    } finally {
      setReleaseChannelBusy(false);
    }
  };

  const openPopTrackerBroadcast = async () => {
    setTrackerActionStatus('');
    try {
      const result = await runtime.trackerOpenBroadcast?.() as { ok?: boolean; error?: string; detail?: string } | undefined;
      if (!result?.ok) {
        const message =
          result?.error === 'no_runtime_tracker'
            ? 'No active SekaiLink PopTracker runtime was found.'
            : result?.error === 'runtime_control_unavailable'
              ? 'The active PopTracker build does not expose runtime controls.'
              : result?.error || result?.detail || 'Unable to open PopTracker broadcast.';
        setTrackerActionStatus(message);
        return;
      }
      setTrackerActionStatus('Broadcast view requested.');
    } catch (error) {
      setTrackerActionStatus(error instanceof Error ? error.message : 'Unable to open PopTracker broadcast.');
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
        start_fullscreen: settings.emuWindowMode === 'fullscreen',
        window_mode: settings.emuWindowMode,
        broadcast_window: settings.broadcastWindow,
        preserve_aspect: settings.preserveEmuAspect,
        prevent_tracker_overflow: settings.preventTrackerOverflow,
        hud_buttons_visible: settings.hudButtonsVisible,
        background_gamepad_input: settings.backgroundGamepadInput,
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
            <p className="text-sm text-[#8e8f94]">{t('Every visible setting is either wired now or clearly marked as In learning while the integration is being stabilized.')}</p>
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
              <SettingGroup title={t('Maintenance')}>
                <div className="rounded-xl border border-[#38f3dd]/25 bg-[#0f1b1d] p-4">
                  <div className="flex flex-col gap-4 lg:flex-row lg:items-start lg:justify-between">
                    <div className="min-w-0">
                      <div className="flex items-center gap-2 font-semibold text-[#f4f6fb]">
                        <RefreshCcw className="h-4 w-4 text-[#38f3dd]" />
                        {t('Release channel')}
                      </div>
                      <p className="mt-2 text-sm leading-5 text-[#d7dde5]">
                        {releaseChannel === 'canari'
                          ? t('Canari is active for the next bootstrapper launch. Builds stay Canari until they are approved as Canonical.')
                          : t('Canonical is active. This is the default release track for players.')}
                      </p>
                      <p className="mt-2 text-xs leading-5 text-[#8e8f94]">
                        {t('Changing this setting requires restarting SekaiLink so the bootstrapper can download the selected release.')}
                      </p>
                    </div>
                    <div className="grid min-w-[260px] grid-cols-1 gap-2 sm:grid-cols-2">
                      <button
                        type="button"
                        onClick={() => void switchReleaseChannel('canari')}
                        disabled={releaseChannelBusy || releaseChannel === 'canari'}
                        className="rounded-lg border border-[#f69d50]/60 bg-[#f69d50]/10 px-4 py-2.5 text-sm font-bold text-[#ffd6a3] transition hover:bg-[#f69d50]/20 disabled:cursor-not-allowed disabled:opacity-50"
                      >
                        {t('Enable Canari')}
                      </button>
                      <button
                        type="button"
                        onClick={() => void switchReleaseChannel('canonical')}
                        disabled={releaseChannelBusy || releaseChannel === 'canonical'}
                        className="rounded-lg border border-[#4ecdc4]/60 bg-[#4ecdc4]/10 px-4 py-2.5 text-sm font-bold text-[#95e1d3] transition hover:bg-[#4ecdc4]/20 disabled:cursor-not-allowed disabled:opacity-50"
                      >
                        {t('Revert to Canonical')}
                      </button>
                    </div>
                  </div>
                  {releaseChannelStatus && <div className="mt-3 rounded-lg border border-[#2a2b30] bg-[#14151a] p-3 text-sm text-[#d7dde5]">{releaseChannelStatus}</div>}
                </div>
                <div className="rounded-xl border border-[#f38181]/35 bg-[#201316] p-4">
                  <div className="flex flex-col gap-4 sm:flex-row sm:items-start sm:justify-between">
                    <div className="min-w-0">
                      <div className="flex items-center gap-2 font-semibold text-[#f4f6fb]">
                        <CircleAlert className="h-4 w-4 text-[#f38181]" />
                        {t('Generated seed cache')}
                      </div>
                      <p className="mt-2 text-sm leading-5 text-[#d7dde5]">
                        {t('Clears cached generated seeds, downloaded patches, and patched ROM outputs. Imported base ROMs are kept.')}
                      </p>
                      <p className="mt-2 text-xs leading-5 text-[#f4b8ad]">
                        {t('Warning: if a sync is currently running, its generated seed files may be erased and that seed may need to be relaunched.')}
                      </p>
                    </div>
                    <button
                      onClick={() => void clearSeedCache()}
                      disabled={clearingCache}
                      className="inline-flex items-center justify-center gap-2 rounded-lg bg-[#f38181] px-4 py-2.5 text-sm font-bold text-[#12080a] hover:bg-[#ff9a9a] disabled:cursor-not-allowed disabled:opacity-60"
                    >
                      <Trash2 className="h-4 w-4" />
                      {clearingCache ? t('Clearing...') : t('Clear cache')}
                    </button>
                  </div>
                  {cacheStatus && <div className="mt-3 rounded-lg border border-[#2a2b30] bg-[#14151a] p-3 text-sm text-[#d7dde5]">{cacheStatus}</div>}
                </div>
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
              <div className="rounded-xl border border-[#2a2b30] bg-[#101318] p-4">
                <div className="mb-4 flex flex-col gap-3 lg:flex-row lg:items-center lg:justify-between">
                  <div>
                    <div className="text-sm font-semibold text-[#f4f6fb]">{t('Choose a game')}</div>
                    <div className="text-xs text-[#8e8f94]">{t('Games without a valid imported ROM are dimmed and marked Missing ROM.')}</div>
                  </div>
                  <div className="flex flex-col gap-2 sm:flex-row sm:items-center">
                    <div className="relative min-w-[240px]">
                      <Search className="absolute left-3 top-2.5 h-4 w-4 text-[#8e8f94]" />
                      <input
                        value={romSearch}
                        onChange={(event) => setRomSearch(event.target.value)}
                        className="w-full rounded-lg border border-[#2a2b30] bg-[#0b0d12] py-2 pl-9 pr-3 text-sm text-[#f4f6fb] outline-none focus:border-[#38f3dd]"
                        placeholder={t('Search game')}
                      />
                    </div>
                    <button
                      type="button"
                      onClick={() => setRomShowAllGames((value) => !value)}
                      className={`rounded-lg px-3 py-2 text-xs font-bold ${romShowAllGames ? 'bg-[#f69d50] text-[#130b03]' : 'bg-[#232830] text-[#d7dde5] hover:bg-[#303744]'}`}
                    >
                      {romShowAllGames ? t('All games') : t('Supported')}
                    </button>
                  </div>
                </div>

                <div className="flex gap-4 overflow-x-auto pb-3">
                  {filteredRomLibraryRows.map((row) => {
                    const selected = romImportGameId === row.key;
                    const missing = !row.hasRom;
                    const manual = !row.hasKnownChecksum;
                    return (
                      <button
                        key={row.key}
                        type="button"
                        onClick={() => openRomImportModal(row.key)}
                        className={`group relative w-[210px] shrink-0 overflow-hidden rounded-lg border text-left transition-colors ${
                          selected
                            ? 'border-[#38f3dd] bg-[#12312f]'
                            : 'border-[#252a33] bg-[#0b0d12] hover:border-[#4ecdc4]'
                        }`}
                      >
                        <div className="relative aspect-[4/3] bg-[#080b10]">
                          {row.asset ? (
                            <img
                              src={row.asset}
                              alt=""
                              className={`h-full w-full object-cover transition-transform duration-200 group-hover:scale-[1.03] ${missing ? 'grayscale opacity-45' : ''}`}
                            />
                          ) : (
                            <div className={`grid h-full place-items-center px-3 text-center text-xs text-[#8e8f94] ${missing ? 'opacity-50' : ''}`}>{row.displayName}</div>
                          )}
                          {missing && <div className="absolute inset-0 bg-[#05070a]/35" />}
                          <div className="absolute left-2 top-2 flex flex-wrap gap-1">
                            <span className={`rounded-full px-2 py-1 text-[10px] font-bold ${row.hasRom ? 'bg-[#4ecdc4] text-[#061311]' : 'bg-[#f38181] text-[#16090b]'}`}>
                              {row.hasRom ? t('Ready') : t('Missing ROM')}
                            </span>
                            <span className={`rounded-full px-2 py-1 text-[10px] font-bold ${manual ? 'bg-[#f69d50] text-[#160d03]' : 'bg-[#38f3dd] text-[#061311]'}`}>
                              {manual ? t('Manual') : t('Verified')}
                            </span>
                          </div>
                        </div>
                        <div className="p-3">
                          <div className={`line-clamp-2 min-h-[2.25rem] text-sm font-semibold leading-[1.1rem] ${missing ? 'text-[#9ca0a8]' : 'text-[#f4f6fb]'}`}>{row.displayName}</div>
                          <div className="mt-2 flex items-center justify-between gap-2 text-xs">
                            <span className="truncate text-[#8e8f94]">{row.moduleId || row.key}</span>
                            <span className={row.importable ? 'font-bold text-[#4ecdc4]' : 'font-bold text-[#8e8f94]'}>
                              {row.importable ? t('Supported') : t(LEARNING_LABEL)}
                            </span>
                          </div>
                        </div>
                      </button>
                    );
                  })}
                  {!filteredRomLibraryRows.length && <div className="rounded-lg border border-[#2a2b30] bg-[#0b0d12] p-3 text-sm text-[#8e8f94]">{t('No games match this search.')}</div>}
                </div>
              </div>

              {importStatus && <div className="rounded-lg border border-[#2a2b30] bg-[#14151a] p-3 text-sm">{importStatus}</div>}
              {modalRomRow && (
                <RomImportModal
                  row={modalRomRow}
                  t={t}
                  pendingRomImport={pendingRomImport}
                  onClose={() => {
                    setRomImportModalGameId('');
                    setPendingRomImport(null);
                  }}
                  onSelect={() => void importModalRom(modalRomRow.key)}
                  onScan={() => void scanModalRoms()}
                  onImportCandidate={(candidateGameId) => void importPendingRomCandidate(candidateGameId)}
                />
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
                <SelectRow label="Window Mode" value={settings.emuWindowMode} onChange={(value) => patchSettings({ emuWindowMode: value })} options={[
                  ['borderless-window', 'Borderless Window'],
                  ['window', 'Window'],
                  ['fullscreen', 'Fullscreen'],
                ]} />
                <ToggleRow label="Show Broadcast Window" value={settings.broadcastWindow} onChange={(value) => patchSettings({ broadcastWindow: value })} />
                <ToggleRow label="Preserve emulator aspect ratio" value={settings.preserveEmuAspect} onChange={(value) => patchSettings({ preserveEmuAspect: value })} description="Keeps NES, SNES, GB/GBC, and GBA layouts from stretching into the tracker." />
                <ToggleRow label="Prevent tracker overflow" value={settings.preventTrackerOverflow} onChange={(value) => patchSettings({ preventTrackerOverflow: value })} />
                <ToggleRow label="Runtime HUD buttons" value={settings.hudButtonsVisible} onChange={(value) => patchSettings({ hudButtonsVisible: value })} />
                <ToggleRow label="Background gamepad input" value={settings.backgroundGamepadInput} onChange={(value) => patchSettings({ backgroundGamepadInput: value })} description="Allows controller input to keep reaching Sekaiemu when another window has focus. Keyboard input still follows the focused window." />
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
              <DisabledRow label="Install / Uninstall / Run Standalone" description={LEARNING_DESCRIPTION} />
                <ToggleRow label="Start Fullscreen" value={false} onChange={() => undefined} disabled />
                <SelectRow label="Screen size" value="1280x720" onChange={() => undefined} options={[['1280x720', '1280x720'], ['1920x1080', '1920x1080']]} disabled />
              </SettingGroup>
              <SettingGroup title="Ship of Harkinian">
                <DisabledRow label="Install / Uninstall / Run Standalone" description="Requires a valid ROM in the ROM Library to generate the O2R." />
                <ToggleRow label="Use HD Graphics" value={true} onChange={() => undefined} disabled />
              </SettingGroup>
              <SettingGroup title="WebView Games and Trackers">
                <ActionRow
                  label="Open PopTracker Broadcast"
                  description="Opens or focuses the broadcast view for the active SekaiLink PopTracker runtime."
                  actionLabel="Open"
                  onAction={openPopTrackerBroadcast}
                />
                {trackerActionStatus && <div className="rounded-lg border border-[#2a2b30] bg-[#0e0f13] px-3 py-2 text-sm text-[#8e8f94]">{trackerActionStatus}</div>}
                <DisabledRow label="Persistent WebView settings" description="In learning: cache, permissions, zoom, audio, and tracker session restore are being stabilized." />
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
                  coreId={selectedControlCore}
                  profile={controlProfiles[selectedControlCore]}
                  mappings={controlMappings[selectedControlCore] || {}}
                  captureTarget={captureTarget}
                  disabled={!selectedController}
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
                      <div className="flex items-center gap-2">
                        <button
                          onClick={configureWithSekaiemu}
                          className="px-3 py-2 rounded-lg bg-[#4ecdc4] text-[#0d1117] hover:bg-[#7adbd5] text-sm font-bold"
                        >
                          {t('Configure with Sekaiemu')}
                        </button>
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
                          <div className="text-xs text-[#8e8f94]">{controlMappings[selectedControlCore]?.[button] ? formatBinding(controlMappings[selectedControlCore]?.[button] || '') : t('Unmapped')}</div>
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
              <DisabledRow label="Reconnect SKLMI command" description="In learning: admin-safe command endpoint and client runtime hook are being stabilized." />
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
              <DisabledRow label="Bot Join / Bot Leave" description="In learning: Twitch account association is required before bot settings can be edited safely." />
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

function checksumList(value: unknown) {
  if (Array.isArray(value)) return value.map((entry) => String(entry || '').trim()).filter(Boolean);
  const single = String(value || '').trim();
  return single ? [single] : [];
}

function compatibleRomGroup(row: any): CompatibleRomGroup {
  const manifest = row?.manifest || {};
  const req = manifest.rom_requirements || {};
  const manifestMd5 = checksumList(req.md5);
  const manifestSha1 = checksumList(req.sha1);
  const manifestSha256 = [
    ...checksumList(req.sha256),
    ...checksumList(req.sha3_256),
    ...checksumList(req['sha3-256']),
  ];
  const components = Array.isArray(row?.requirement?.components) ? row.requirement.components : [];
  const requiredRomCount = Array.isArray(manifest.required_roms) ? manifest.required_roms.length : 0;
  const isMultiRequired = row?.key === 'smz3' || requiredRomCount > 1 || components.some((component: RomRequirementComponent) => (component.keys || []).some((key) => normalizeRomKey(key) !== normalizeRomKey(row?.key)));
  const versions = components.map((component: RomRequirementComponent, index: number) => {
      const useManifestChecksums = components.length === 1;
      return {
        label: component.label,
        expectedBase: component.expectedBase,
        md5: checksumList(component.md5).concat(useManifestChecksums ? manifestMd5.filter((hash) => hash !== component.md5) : []),
        sha1: checksumList(component.sha1).concat(useManifestChecksums ? manifestSha1.filter((hash) => hash !== component.sha1) : []),
        sha256: checksumList(component.sha256).concat(useManifestChecksums ? manifestSha256.filter((hash) => hash !== component.sha256) : []),
        key: `${component.label}-${component.expectedBase}-${index}`,
      };
    });
  return {
    title: isMultiRequired ? 'Required ROMs' : 'Compatible versions',
    mode: isMultiRequired ? 'required' : 'alternatives',
    versions,
  };
}

function RomImportModal({
  row,
  t,
  pendingRomImport,
  onClose,
  onSelect,
  onScan,
  onImportCandidate,
}: {
  row: any;
  t: (value: string) => string;
  pendingRomImport: { filePath: string; candidates: RomImportCandidate[] } | null;
  onClose: () => void;
  onSelect: () => void;
  onScan: () => void;
  onImportCandidate: (gameId: string) => void;
}) {
  const romGroup = compatibleRomGroup(row);
  return (
    <div className="fixed inset-0 z-50 grid place-items-center bg-black/70 px-4 py-6 backdrop-blur-sm">
      <div className="max-h-[92vh] w-full max-w-3xl overflow-hidden rounded-xl border border-[#38f3dd] bg-[#101318] shadow-[0_0_40px_rgba(56,243,221,0.18)]">
        <div className="relative h-44 border-b border-[#2a2b30] bg-[#080b10]">
          {row.asset ? <img src={row.asset} alt="" className={`h-full w-full object-cover ${row.hasRom ? '' : 'grayscale opacity-55'}`} /> : null}
          <div className="absolute inset-0 bg-gradient-to-t from-[#05070a] via-[#05070a]/45 to-[#05070a]/10" />
          <div className="absolute bottom-4 left-5 right-5">
            <div className="flex flex-wrap items-center gap-2 text-xs font-bold">
              <span className={`rounded-full px-2 py-1 ${row.hasRom ? 'bg-[#4ecdc4] text-[#061311]' : 'bg-[#f38181] text-[#16090b]'}`}>
                {row.hasRom ? t('Ready') : t('Missing ROM')}
              </span>
              <span className={`rounded-full px-2 py-1 ${row.hasKnownChecksum ? 'bg-[#38f3dd] text-[#061311]' : 'bg-[#f69d50] text-[#160d03]'}`}>
                {row.hasKnownChecksum ? t('Checksum verified') : t('Manual import')}
              </span>
              <span className="rounded-full bg-[#232830] px-2 py-1 text-[#d7dde5]">{row.importable ? t('Supported') : t(LEARNING_LABEL)}</span>
            </div>
            <div className="mt-2 text-2xl font-bold text-white">{row.displayName}</div>
          </div>
        </div>

        <div className="max-h-[calc(92vh-11rem)] overflow-y-auto p-5">
          <div className="mb-3">
            <div className="text-sm font-semibold text-[#f4f6fb]">{t(romGroup.title)}</div>
            <div className={`mt-2 rounded-lg border px-3 py-2 text-sm font-semibold ${
              romGroup.mode === 'required'
                ? 'border-[#f69d50]/35 bg-[#2a1a0c] text-[#ffd6a3]'
                : 'border-[#38f3dd]/30 bg-[#10201f] text-[#8cf5e8]'
            }`}>
              {romGroup.mode === 'required'
                ? t('Import every ROM listed here.')
                : t('Use one of these compatible ROM versions.')}
            </div>
          </div>
          <div className="grid gap-3">
            {romGroup.versions.map((version: CompatibleRomVersion) => (
              <div key={version.key} className="rounded-lg border border-[#2a2b30] bg-[#0b0d12] p-4">
                <div className="flex flex-col gap-1 sm:flex-row sm:items-start sm:justify-between">
                  <div>
                    {romGroup.mode === 'required' && <div className="font-semibold text-[#f4f6fb]">{version.label}</div>}
                    <div className="mt-2 text-[11px] font-bold uppercase text-[#8e8f94]">{t('Compatible filename')}</div>
                    <div className="mt-1 break-words rounded-md bg-[#14151a] px-3 py-2 font-mono text-sm text-[#f4f6fb]">{version.expectedBase}</div>
                  </div>
                  {row.requirement?.platform && <div className="rounded-full border border-[#38f3dd]/25 px-2 py-1 text-xs font-semibold text-[#8cf5e8]">{row.requirement.platform}</div>}
                </div>
                <div className="mt-3 grid gap-2 text-xs text-[#8e8f94]">
                  {version.md5.length > 0 && <ChecksumLine label="MD5" values={version.md5} />}
                  {version.sha1.length > 0 && <ChecksumLine label="SHA1" values={version.sha1} />}
                  {version.sha256.length > 0 && <ChecksumLine label="SHA256" values={version.sha256} />}
                  {!version.md5.length && !version.sha1.length && !version.sha256.length && (
                    <div>{t('No verified checksum bundled yet. Select this game before importing.')}</div>
                  )}
                </div>
              </div>
            ))}
          </div>

          {row.requirement?.guidance && <div className="mt-3 rounded-lg border border-[#2a2b30] bg-[#14151a] p-3 text-xs leading-5 text-[#d7dde5]">{row.requirement.guidance}</div>}
          {row.romPath && <div className="mt-3 truncate rounded-lg border border-[#2a2b30] bg-[#14151a] p-3 text-xs text-[#8e8f94]">{row.romPath}</div>}
          {pendingRomImport && (
            <div className="mt-4 rounded-xl border border-[#38f3dd]/25 bg-[#10201f] p-3">
              <div className="mb-3 text-sm font-semibold text-[#f4f6fb]">{t('Import as')}</div>
              <div className="grid gap-2 sm:grid-cols-2">
                {pendingRomImport.candidates.map((candidate) => (
                  <button
                    key={candidate.gameId}
                    type="button"
                    onClick={() => onImportCandidate(candidate.gameId)}
                    className="rounded-lg border border-[#38f3dd]/35 bg-[#173533] px-3 py-2 text-left text-sm font-semibold text-[#f4f6fb] hover:bg-[#1f4744]"
                  >
                    <div>{candidate.displayName}</div>
                    <div className="mt-1 text-xs font-normal text-[#8e8f94]">{candidate.moduleId || candidate.gameId}</div>
                  </button>
                ))}
              </div>
            </div>
          )}
        </div>

        <div className="flex flex-col gap-2 border-t border-[#2a2b30] bg-[#0b0d12] p-4 sm:flex-row sm:justify-end">
          <button onClick={onClose} className="rounded-lg bg-[#2a2b30] px-4 py-2.5 text-sm font-bold hover:bg-[#3a3b40]">{t('Close')}</button>
          <button onClick={onScan} className="inline-flex items-center justify-center gap-2 rounded-lg bg-[#232830] px-4 py-2.5 text-sm font-bold text-[#f4f6fb] hover:bg-[#303744]">
            <Search className="h-4 w-4" />
            {t('Scan')}
          </button>
          <button
            onClick={onSelect}
            disabled={!row.importable}
            className="inline-flex items-center justify-center gap-2 rounded-lg bg-gradient-to-r from-[#ff6b35] to-[#f38181] px-4 py-2.5 text-sm font-bold text-[#180b08] disabled:cursor-not-allowed disabled:opacity-50"
          >
            <Upload className="h-4 w-4" />
            {t('Select')}
          </button>
        </div>
      </div>
    </div>
  );
}

function ChecksumLine({ label, values }: { label: string; values: string[] }) {
  return (
    <div className="grid gap-1">
      <div className="font-semibold text-[#d7dde5]">{label}</div>
      {values.map((value) => (
        <code key={value} className="break-all rounded-md bg-[#14151a] px-2 py-1 font-mono text-[11px] text-[#8e8f94]">{value}</code>
      ))}
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
        title={disabled ? LEARNING_LABEL : undefined}
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
        title={disabled ? LEARNING_LABEL : undefined}
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

function ActionRow({ label, description, actionLabel, onAction, disabled }: {
  label: string;
  description?: string;
  actionLabel: string;
  onAction: () => void;
  disabled?: boolean;
}) {
  return (
    <div className="grid grid-cols-[1fr_160px] items-center gap-5">
      <div>
        <div className="font-medium">{label}</div>
        {description && <div className="text-sm text-[#8e8f94]">{description}</div>}
      </div>
      <button
        type="button"
        disabled={disabled}
        onClick={onAction}
        className="rounded-lg border border-[#4ecdc4]/60 bg-[#4ecdc4]/10 px-4 py-2.5 text-sm font-bold text-[#95e1d3] transition hover:bg-[#4ecdc4]/20 disabled:cursor-not-allowed disabled:opacity-50"
      >
        {actionLabel}
      </button>
    </div>
  );
}

function DisabledRow({ label, description }: { label: string; description: string }) {
  return (
    <div title={LEARNING_LABEL} className="rounded-lg border border-dashed border-[#3a3b40] bg-[#0e0f13]/70 p-4 opacity-70">
      <div className="flex items-center gap-2 font-medium">
        <CircleAlert className="w-4 h-4 text-[#f69d50]" />
        {label}
      </div>
      <p className="mt-1 text-sm text-[#8e8f94]">{description || LEARNING_DESCRIPTION}</p>
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
          <div className="text-sm text-[#8e8f94]">{LEARNING_LABEL}</div>
        </div>
      </div>
      <button disabled title={LEARNING_LABEL} className="px-4 py-2 rounded-lg bg-[#2a2b30] text-sm opacity-60">Connect</button>
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
  if (binding.startsWith('sdl:button:')) return `SDL Button ${binding.slice('sdl:button:'.length)}`;
  if (binding.startsWith('sdl:axis+:')) return `SDL Axis + ${binding.slice('sdl:axis+:'.length)}`;
  if (binding.startsWith('sdl:axis-:')) return `SDL Axis - ${binding.slice('sdl:axis-:'.length)}`;
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

const controllerPreviewAssets: Record<ControlCoreId, string> = {
  snes: publicAssetUrl('assets/img/controllers/snes-controller.png'),
  nes: publicAssetUrl('assets/img/controllers/nes-controller.png'),
  gbc: publicAssetUrl('assets/img/controllers/gameboy-controller.png'),
  gba: publicAssetUrl('assets/img/controllers/gba-controller.png'),
  n64: publicAssetUrl('assets/img/controllers/n64-controller.png'),
};

function ControllerPreview({ coreId, profile, mappings, captureTarget, disabled }: {
  coreId: ControlCoreId;
  profile: (typeof controlProfiles)[ControlCoreId];
  mappings: CoreControlMappings;
  captureTarget: string | null;
  disabled: boolean;
}) {
  const mappedCount = Object.keys(mappings).length;
  const controllerAsset = controllerPreviewAssets[coreId] || controllerPreviewAssets.snes;
  return (
    <div className="rounded-xl border border-[#2a2b30] bg-[#0e0f13] p-5">
      <div className="mb-4">
        <div className="font-bold">{profile.label}</div>
        <div className="text-sm text-[#8e8f94]">{profile.description}</div>
      </div>
      <div className="relative grid w-full aspect-[4/3] place-items-center overflow-hidden rounded-lg border border-[#20242c] bg-[#090b0f]">
        <img
          src={controllerAsset}
          alt=""
          draggable={false}
          className={`h-full w-full object-contain ${disabled ? 'opacity-65 grayscale' : ''}`}
        />
      </div>
      <div className="mt-4 flex flex-wrap items-center gap-3 text-xs text-[#8e8f94]">
        <span className="inline-flex items-center gap-1"><span className="h-2.5 w-2.5 rounded-full bg-[#4ecdc4]" />{mappedCount} mapped</span>
        {captureTarget && <span className="inline-flex items-center gap-1"><span className="h-2.5 w-2.5 rounded-full bg-[#f69d50]" />Listening: {captureTarget}</span>}
      </div>
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
