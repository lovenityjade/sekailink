import { gameSetupRegistry } from '../data/gameSetup';
import { SEKAILINK_GAME_CATALOG } from '../data/sekailinkGameCatalog';
import { canonicalSeedGameKey } from './seedConfig';
import { runtime, type LaunchSetupValidationResult, type RuntimeModuleInfo } from './runtime';

export type RomReadinessState = 'checking' | 'verified' | 'missing' | 'invalid' | 'not-required' | 'unknown';

export type RomReadiness = {
  state: RomReadinessState;
  game: string;
  moduleId?: string;
  importGameId: string;
  required: boolean;
  blocksLobbyAdd: boolean;
  message: string;
  detail?: string;
};

const normalize = (value: unknown) => canonicalSeedGameKey(String(value || ''));

const moduleCandidates = (entry: RuntimeModuleInfo) => {
  const manifest = entry.manifest || {};
  return [
    entry.moduleId,
    manifest.game_id,
    manifest.game_key,
    manifest.ap_world,
    manifest.display_name,
    manifest.game_name,
    manifest.archipelago_client?.game_key,
    ...(Array.isArray(manifest.aliases) ? manifest.aliases : []),
    ...(Array.isArray(manifest.required_roms) ? manifest.required_roms : []),
  ].map(normalize).filter(Boolean);
};

const unpackModules = (value: unknown): RuntimeModuleInfo[] => {
  if (Array.isArray(value)) return value as RuntimeModuleInfo[];
  const modules = (value as any)?.modules;
  return Array.isArray(modules) ? modules as RuntimeModuleInfo[] : [];
};

export const resolveRomImportGameId = (game: string) => {
  const key = normalize(game);
  const catalog = SEKAILINK_GAME_CATALOG.find((entry) =>
    [entry.key, entry.displayName].map(normalize).includes(key)
  );
  if (catalog) return catalog.key;
  const setup = gameSetupRegistry.find((entry) =>
    [entry.gameId, entry.apGameId, entry.displayName, entry.moduleId, entry.romKey].map(normalize).includes(key)
  );
  return setup?.romKey || setup?.gameId || game;
};

export const checkRomReadiness = async (game: string): Promise<RomReadiness> => {
  const key = normalize(game);
  const result = await runtime.listRuntimeModules?.();
  const modules = unpackModules(result);
  let module = modules.find((entry) => moduleCandidates(entry).includes(key));
  if (!module) {
    const setup = gameSetupRegistry.find((entry) =>
      [entry.gameId, entry.apGameId, entry.displayName, entry.moduleId, entry.romKey].map(normalize).includes(key)
    );
    if (setup?.moduleId) module = modules.find((entry) => normalize(entry.moduleId) === normalize(setup.moduleId));
  }

  const importGameId = resolveRomImportGameId(game);
  if (!module?.moduleId) {
    return {
      state: 'unknown', game, importGameId, required: false, blocksLobbyAdd: false,
      message: 'ROM verification is unavailable for this game.',
    };
  }

  const validation = await runtime.validateRomForModule(module.moduleId) as (LaunchSetupValidationResult & { required?: boolean }) | undefined;
  if (!validation) {
    return {
      state: 'unknown', game, moduleId: module.moduleId, importGameId,
      required: false, blocksLobbyAdd: false, message: 'ROM verification is unavailable for this client build.',
    };
  }
  if (validation.ok) {
    const required = validation.required !== false && Array.isArray(module.manifest?.required_roms)
      ? module.manifest.required_roms.length > 0
      : Boolean(validation.required);
    return {
      state: required ? 'verified' : 'not-required', game, moduleId: module.moduleId, importGameId,
      required, blocksLobbyAdd: false,
      message: required ? 'ROM found and verified.' : 'This game does not require an imported ROM.',
    };
  }

  const missing = validation.error === 'rom_missing';
  return {
    state: missing ? 'missing' : 'invalid', game, moduleId: module.moduleId, importGameId,
    required: true, blocksLobbyAdd: true,
    message: missing ? 'Required ROM is missing.' : 'The imported ROM could not be validated.',
    detail: String(validation.detail || validation.error || ''),
  };
};
