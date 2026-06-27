import { apiFetch, apiJson } from "./api";
import { lookupFeaturedGridUrl } from "../data/steamGridDbFeatured";
import { trace, traceError } from "./trace";

export interface SeedGameEntry {
  game_key: string;
  display_name: string;
  description?: string;
  boxart?: string;
}

export interface SeedEntry {
  id: string;
  title: string;
  game: string;
  game_key?: string;
  player_name?: string;
  custom?: boolean;
  created_at?: string;
  updated_at?: string;
  source?: string;
  description?: string;
  values?: Record<string, unknown>;
}

export interface EasySeedChoice {
  id: string;
  label: string;
  description: string;
  tags?: string[];
}

export interface SeedOptionChoice {
  choice_key: string;
  yaml_value: string;
  label: string;
  description?: string;
}

export interface SeedOptionDefinition {
  option_key: string;
  yaml_key?: string;
  label: string;
  description?: string;
  type: "string" | "integer" | "number" | "boolean" | "enum" | "array" | "object" | string;
  default_value?: unknown;
  required?: boolean;
  validation_rules?: Record<string, any> | null;
  visibility_rules?: Record<string, any> | null;
  choices?: SeedOptionChoice[];
}

export interface SeedOptionsSchema {
  game_key: string;
  display_name: string;
  schema_version?: string;
  options: SeedOptionDefinition[];
}

const ACTIVE_SEEDS_STORAGE_KEY = "skl.activeSeeds.v1";
const SEED_GAMES_CACHE_TTL_MS = 5 * 60 * 1000;
const SEED_OPTIONS_CACHE_TTL_MS = 10 * 60 * 1000;
const SEEDS_CACHE_TTL_MS = 15 * 1000;

type TimedCache<T> = {
  value?: T;
  expiresAt: number;
  inFlight?: Promise<T>;
};

const seedGamesCache: TimedCache<SeedGameEntry[]> = { expiresAt: 0 };
const seedOptionsCache = new Map<string, TimedCache<SeedOptionsSchema>>();
const seedsCache = new Map<string, TimedCache<SeedEntry[]>>();

const cacheKey = (value: string) => value || "__all__";

const firstGamesPayload = (payload: any): Array<any> => {
  if (Array.isArray(payload?.games)) return payload.games;
  if (Array.isArray(payload?.items)) return payload.items;
  if (Array.isArray(payload?.data)) return payload.data;
  if (Array.isArray(payload)) return payload;
  return [];
};

export const invalidateSeedConfigCaches = (scope: "all" | "seeds" | "games" | "options" = "all") => {
  if (scope === "all" || scope === "games") {
    seedGamesCache.value = undefined;
    seedGamesCache.expiresAt = 0;
    seedGamesCache.inFlight = undefined;
  }
  if (scope === "all" || scope === "options") {
    seedOptionsCache.clear();
  }
  if (scope === "all" || scope === "seeds") {
    seedsCache.clear();
  }
};

export const SHOWCASE_GAME_KEYS = new Set([
  "alttp",
  "a_link_to_the_past",
  "the_legend_of_zelda_a_link_to_the_past",
]);

export const ALTTP_SHOWCASE_GAME: SeedGameEntry = {
  game_key: "a_link_to_the_past",
  display_name: "A Link to the Past",
  description:
    "Classic Hyrule multiworld showcase with item sharing, live tracker support, and Sekaiemu launch integration.",
  boxart: lookupFeaturedGridUrl("A Link to the Past") || "/assets/boxart/a_link_to_the_past.png",
};

export const EASY_SEED_CHOICES: EasySeedChoice[] = [
  {
    id: "short",
    label: "Shorter Run",
    description: "Prefer a compact showcase seed with less long-form routing.",
    tags: ["pace"],
  },
  {
    id: "beginner",
    label: "Beginner Friendly",
    description: "Favor accessible routing and fewer punishing requirements.",
    tags: ["difficulty"],
  },
  {
    id: "keysanity",
    label: "Keysanity",
    description: "Include maps, compasses, small keys, and big keys in the item pool.",
    tags: ["shuffle"],
  },
  {
    id: "boots_start",
    label: "Boots Start",
    description: "Prefer a faster opening with Pegasus Boots early or at start.",
    tags: ["comfort"],
  },
  {
    id: "open_mode",
    label: "Open Start",
    description: "Start in a more open state instead of a longer intro route.",
    tags: ["start"],
  },
  {
    id: "ganon_goal",
    label: "Ganon Goal",
    description: "Use the familiar Ganon completion target for the showcase.",
    tags: ["goal"],
  },
];

export const normalizeGameKey = (value: unknown) =>
  String(value || "")
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "_")
    .replace(/^_+|_+$/g, "");

const SEED_GAME_KEY_ALIASES: Record<string, string> = {
  chrono_trigger_jets_of_time: "chrono_trigger",
  final_fantasy_iv_free_enterprise: "final_fantasy_iv",
  final_fantasy_v_career_day: "final_fantasy_v",
  kirby_s_dream_land_3: "kirbys_dream_land_3",
  ladx: "links_awakening_dx",
  links_awakening_dx_beta: "links_awakening_dx",
  link_s_awakening_dx: "links_awakening_dx",
  the_legend_of_zelda_link_s_awakening_dx: "links_awakening_dx",
  the_legend_of_zelda_links_awakening_dx: "links_awakening_dx",
  lufia_ii_ancient_cave: "lufia_ii",
  luigi_s_mansion: "luigis_mansion",
  mario_and_luigi_superstar_saga: "mario_luigi_superstar_saga",
  mega_man_battle_network_3_blue: "mega_man_battle_network_3",
  paper_mario_the_thousand_year_door: "thousand_year_door",
  pokemon_firered_and_leafgreen: "pokemon_firered",
  pokemon_fire_red_and_leaf_green: "pokemon_firered",
  pokemon_red_and_blue: "pokemon_red_blue",
  secret_of_evermore: "secret_of_evermore",
  the_legend_of_zelda_oracle_of_ages: "oracle_of_ages",
  the_legend_of_zelda_oracle_of_seasons: "oracle_of_seasons",
  the_legend_of_zelda_a_link_to_the_past: "a_link_to_the_past",
  the_legend_of_zelda_twilight_princess: "twilight_princess",
  zelda_ii_the_adventure_of_link: "zelda_ii",
};

const SEED_GAME_SERVER_KEY_ALIASES: Record<string, string[]> = {
  zelda_ii: ["zelda_ii_the_adventure_of_link"],
};

export const canonicalSeedGameKey = (value: unknown) => {
  let normalized = normalizeGameKey(value);
  if (normalized.startsWith("linkedworld_")) normalized = normalized.slice("linkedworld_".length);
  if (normalized.startsWith("multiworldgg_")) normalized = normalized.slice("multiworldgg_".length);
  normalized = normalized.replace(/_v\d+$/, "");
  if (
    normalized === "alttp" ||
    normalized === "a_link_to_the_past" ||
    normalized === "a_link_to_the_past_" ||
    normalized === "the_legend_of_zelda_a_link_to_the_past"
  ) {
    return "a_link_to_the_past";
  }
  return SEED_GAME_KEY_ALIASES[normalized] || normalized;
};

const seedGameServerKeys = (gameKey: string) => [gameKey, ...(SEED_GAME_SERVER_KEY_ALIASES[gameKey] || [])];

export const isShowcaseGame = (value: unknown) => SHOWCASE_GAME_KEYS.has(normalizeGameKey(value));

export const listSeedGames = async (): Promise<SeedGameEntry[]> => {
  const now = Date.now();
  if (seedGamesCache.value && seedGamesCache.expiresAt > now) {
    trace("seed-config", "list_games_cache_hit", { count: seedGamesCache.value.length });
    return seedGamesCache.value;
  }
  if (seedGamesCache.inFlight) {
    trace("seed-config", "list_games_deduped");
    return seedGamesCache.inFlight;
  }
  trace("seed-config", "list_games_start");
  seedGamesCache.inFlight = (async () => {
    const seedConfigGames = await apiJson<any>("/api/seed-configs/games")
      .then(firstGamesPayload)
      .catch((error) => {
        trace("seed-config", "list_games_seed_config_failed", { error: String(error?.message || error) }, "warn");
        return [];
      });
    const clientGames = await apiJson<any>("/api/client/games")
      .then(firstGamesPayload)
      .catch((error) => {
        trace("seed-config", "list_games_client_catalog_failed", { error: String(error?.message || error) }, "warn");
        return [];
      });
    const games = [...seedConfigGames, ...clientGames];
    const mapped = games
      .map((game) => {
        const gameKey = String(game?.game_key || game?.game_id || game?.linkedworld_id || "").trim();
        const displayName = String(game?.display_name || game?.title || game?.name || gameKey).trim();
        if (!gameKey && !displayName) return null;
        return {
          game_key: canonicalSeedGameKey(gameKey || displayName),
          display_name: displayName || gameKey,
          description: typeof game?.description === "string" ? game.description : "",
          boxart:
            (typeof game?.boxart === "string" ? game.boxart : "") ||
            lookupFeaturedGridUrl(displayName) ||
            lookupFeaturedGridUrl(gameKey),
        } satisfies SeedGameEntry;
      })
      .filter(Boolean) as SeedGameEntry[];
    const deduped = Array.from(
      new Map(mapped.map((game) => [canonicalSeedGameKey(game.game_key || game.display_name), game])).values(),
    ).sort((a, b) => a.display_name.localeCompare(b.display_name));
    trace("seed-config", "list_games_success", {
      count: mapped.length,
      seedConfigSourceCount: seedConfigGames.length,
      clientCatalogSourceCount: clientGames.length,
      seedConfigCount: deduped.length,
      showcaseCount: deduped.filter((game) => isShowcaseGame(game.game_key) || isShowcaseGame(game.display_name)).length,
    });
    const next = deduped.length ? deduped : [ALTTP_SHOWCASE_GAME];
    seedGamesCache.value = next;
    seedGamesCache.expiresAt = Date.now() + SEED_GAMES_CACHE_TTL_MS;
    return next;
  })();
  try {
    return await seedGamesCache.inFlight;
  } catch (error) {
    traceError("seed-config", "list_games_failed", error);
    return [ALTTP_SHOWCASE_GAME];
  } finally {
    seedGamesCache.inFlight = undefined;
  }
};

const firstArrayPayload = (payload: any): Array<any> => {
  if (Array.isArray(payload?.yamls)) return payload.yamls;
  if (Array.isArray(payload?.configs)) return payload.configs;
  if (Array.isArray(payload?.seeds)) return payload.seeds;
  if (Array.isArray(payload?.items)) return payload.items;
  if (Array.isArray(payload?.data)) return payload.data;
  if (Array.isArray(payload)) return payload;
  return [];
};

const normalizeSeedValues = (entry: any): Record<string, unknown> | undefined => {
  const raw = entry?.values ?? entry?.config_values ?? entry?.values_json;
  if (raw && typeof raw === "object" && !Array.isArray(raw)) return raw;
  if (typeof raw === "string" && raw.trim()) {
    try {
      const parsed = JSON.parse(raw);
      if (parsed && typeof parsed === "object" && !Array.isArray(parsed)) return parsed;
    } catch {
      // Leave malformed legacy values unset; the title/source still identify the seed.
    }
  }
  return undefined;
};

export const listSeeds = async (game?: SeedGameEntry, options: { force?: boolean } = {}): Promise<SeedEntry[]> => {
  const gameKey = game ? canonicalSeedGameKey(game.game_key || game.display_name) : "";
  const key = cacheKey(gameKey);
  const cached = seedsCache.get(key);
  const now = Date.now();
  if (!options.force && cached?.value && cached.expiresAt > now) {
    trace("seed-config", "list_seeds_cache_hit", { count: cached.value.length, gameKey });
    return cached.value;
  }
  if (!options.force && cached?.inFlight) {
    trace("seed-config", "list_seeds_deduped", { gameKey });
    return cached.inFlight;
  }
  trace("seed-config", "list_seeds_start", { gameKey: game?.game_key || "" });
  const entry = cached || { expiresAt: 0 };
  seedsCache.set(key, entry);
  entry.inFlight = (async () => {
    const data = await apiJson<any>("/api/yamls");
    const all = firstArrayPayload(data);
    const seeds = all
      .map((entry) => {
        const gameDisplay = String(entry?.game_display_name || entry?.game || entry?.display_name || "Unknown");
        const rawGameKey = String(entry?.game_key || entry?.linkedworld_id || gameDisplay || "");
        const values = normalizeSeedValues(entry);
        return {
          id: String(entry?.id || entry?.yaml_id || entry?.config_id || ""),
          title: String(entry?.title || entry?.name || "Untitled Seed"),
          game: gameDisplay,
          game_key: canonicalSeedGameKey(rawGameKey),
          player_name: String(entry?.player_name || ""),
          custom: Boolean(entry?.custom),
          created_at: typeof entry?.created_at === "string" ? entry.created_at : "",
          updated_at: typeof entry?.updated_at === "string" ? entry.updated_at : "",
          source: typeof entry?.source === "string" ? entry.source : typeof entry?.mode === "string" ? entry.mode : "",
          description: typeof entry?.description === "string" ? entry.description : "",
          values,
        };
      })
      .filter((entry) => entry.id)
      .filter((entry) => {
        if (!gameKey) return true;
        return canonicalSeedGameKey(entry.game_key || entry.game) === gameKey || isShowcaseGame(entry.game);
      });
    trace("seed-config", "list_seeds_success", { count: seeds.length, total: all.length, gameKey });
    entry.value = seeds;
    entry.expiresAt = Date.now() + SEEDS_CACHE_TTL_MS;
    return seeds;
  })();
  try {
    return await entry.inFlight;
  } finally {
    entry.inFlight = undefined;
  }
};

const isPlainObject = (value: unknown): value is Record<string, unknown> =>
  !!value && typeof value === "object" && !Array.isArray(value);

const normalizeOption = (option: any): SeedOptionDefinition | null => {
  const optionKey = String(option?.option_key || option?.name || option?.yaml_key || "").trim();
  if (!optionKey) return null;
  return {
    option_key: optionKey,
    yaml_key: typeof option?.yaml_key === "string" ? option.yaml_key : optionKey,
    label: String(option?.label || option?.display_name || optionKey.replace(/_/g, " ")),
    description: typeof option?.description === "string" ? option.description : "",
    type: String(option?.type || "string"),
    default_value: option?.default_value,
    required: option?.required !== false,
    validation_rules: isPlainObject(option?.validation_rules) ? option.validation_rules : null,
    visibility_rules: isPlainObject(option?.visibility_rules) ? option.visibility_rules : null,
    choices: Array.isArray(option?.choices)
      ? option.choices
          .map((choice: any) => ({
            choice_key: String(choice?.choice_key || choice?.value || choice?.yaml_value || ""),
            yaml_value: String(choice?.yaml_value ?? choice?.value ?? choice?.choice_key ?? ""),
            label: String(choice?.label || choice?.name || choice?.choice_key || choice?.yaml_value || ""),
            description: typeof choice?.description === "string" ? choice.description : "",
          }))
          .filter((choice: SeedOptionChoice) => choice.yaml_value || choice.choice_key)
      : [],
  };
};

export const listSeedOptions = async (game: SeedGameEntry): Promise<SeedOptionsSchema> => {
  const gameKey = canonicalSeedGameKey(game.game_key || game.display_name);
  const cached = seedOptionsCache.get(gameKey);
  const now = Date.now();
  if (cached?.value && cached.expiresAt > now) {
    trace("seed-config", "list_options_cache_hit", { gameKey, optionCount: cached.value.options.length });
    return cached.value;
  }
  if (cached?.inFlight) {
    trace("seed-config", "list_options_deduped", { gameKey });
    return cached.inFlight;
  }
  trace("seed-config", "list_options_start", { gameKey });
  const entry = cached || { expiresAt: 0 };
  seedOptionsCache.set(gameKey, entry);
  entry.inFlight = (async () => {
    let data: { game?: any } | undefined;
    let lastError: unknown;
    for (const serverKey of seedGameServerKeys(gameKey)) {
      try {
        data = await apiJson<{ game?: any }>(`/api/seed-configs/games/${encodeURIComponent(serverKey)}/options`);
        if (serverKey !== gameKey) {
          trace("seed-config", "list_options_alias_hit", { gameKey, serverKey });
        }
        break;
      } catch (error) {
        lastError = error;
        trace("seed-config", "list_options_alias_failed", { gameKey, serverKey, error: String((error as any)?.message || error) }, "warn");
      }
    }
    if (!data) throw lastError;
    const payload = data?.game || {};
    const options = Array.isArray(payload?.options)
      ? (payload.options.map(normalizeOption).filter(Boolean) as SeedOptionDefinition[])
      : [];
    const schema = {
      game_key: String(payload?.game_key || gameKey),
      display_name: String(payload?.display_name || game.display_name),
      schema_version: typeof payload?.schema_version === "string" ? payload.schema_version : "",
      options,
    };
    trace("seed-config", "list_options_success", { gameKey, optionCount: options.length });
    entry.value = schema;
    entry.expiresAt = Date.now() + SEED_OPTIONS_CACHE_TTL_MS;
    return schema;
  })();
  try {
    return await entry.inFlight;
  } finally {
    entry.inFlight = undefined;
  }
};

const safePlayerName = (value: string) =>
  (value || "Player")
    .trim()
    .replace(/[^A-Za-z0-9_-]+/g, "_")
    .replace(/^_+|_+$/g, "")
    .slice(0, 32) || "Player";

const buildEasyYaml = (game: SeedGameEntry, title: string, playerName: string, choices: EasySeedChoice[]) => {
  const intentIds = choices.map((choice) => choice.id);
  const lines = [
    `name: ${safePlayerName(playerName)}`,
    `game: ${JSON.stringify(game.display_name)}`,
    "description: Generated from SekaiLink Easy Seed choices.",
    "sekailink:",
    `  title: ${JSON.stringify(title.trim() || "Vanilla Showcase")}`,
    "  mode: easy",
    "  assistant_intent: true",
    "  pulse_runtime: pending_live_endpoint",
    "  requested_choices:",
    ...(intentIds.length ? intentIds.map((id) => `    - ${JSON.stringify(id)}`) : ["    - vanilla"]),
    `${game.display_name}: {}`,
    "",
  ];
  return lines.join("\n");
};

const buildEasyValues = (choices: EasySeedChoice[]) => {
  const ids = new Set(choices.map((choice) => choice.id));
  const values: Record<string, unknown> = {};
  if (ids.has("open_mode")) values.mode = "open";
  if (ids.has("ganon_goal")) values.goal = "ganon";
  if (ids.has("keysanity")) {
    values.big_key_shuffle = "own_world";
    values.small_key_shuffle = "own_world";
    values.compass_shuffle = "own_world";
    values.map_shuffle = "own_world";
    values.key_drop_shuffle = true;
  }
  return values;
};

const buildAdvancedYaml = (
  game: SeedGameEntry,
  title: string,
  playerName: string,
  values: Record<string, unknown>
) => {
  const safeValues = values && typeof values === "object" && !Array.isArray(values) ? values : {};
  const lines = [
    `name: ${safePlayerName(playerName)}`,
    `game: ${JSON.stringify(game.display_name)}`,
    "description: Created from SekaiLink Advanced seed form.",
    "sekailink:",
    `  title: ${JSON.stringify(title.trim() || "Advanced Showcase Seed")}`,
    "  mode: advanced",
    "  editor: advanced_form",
    `${game.display_name}:`,
  ];
  for (const [key, value] of Object.entries(safeValues)) {
    lines.push(`  ${key}: ${JSON.stringify(value)}`);
  }
  lines.push("");
  return lines.join("\n");
};

export const createEasySeed = async (
  game: SeedGameEntry,
  title: string,
  playerName: string,
  choices: EasySeedChoice[]
): Promise<SeedEntry> => {
  const seedTitle = title.trim() || (choices.length ? "Easy Showcase Seed" : "Vanilla Showcase Seed");
  trace("seed-config", "create_easy_seed_start", {
    gameKey: game.game_key,
    title: seedTitle,
    choiceCount: choices.length,
  });
  const content = buildEasyYaml(game, seedTitle, playerName, choices);
  const values = buildEasyValues(choices);
  const response = await apiFetch("/api/yamls/new", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      title: seedTitle,
      game: game.display_name,
      game_key: canonicalSeedGameKey(game.game_key || game.display_name),
      values,
      content,
      easy_choices: choices.map((choice) => choice.id),
      source: "easy",
      mode: "easy",
      description: "Created from SekaiLink Easy Mode choices.",
    }),
  });
  const data = await response.json().catch(() => ({}));
  if (!response.ok) {
    const error = String(data?.error || "Unable to create seed.");
    trace("seed-config", "create_easy_seed_failed", { status: response.status, error }, "warn");
    throw new Error(error);
  }
  const saved = data?.yaml || data;
  const seed = {
    id: String(saved?.id || data?.id || ""),
    title: String(saved?.title || seedTitle),
    game: String(saved?.game || game.display_name),
    game_key: canonicalSeedGameKey(game.game_key || game.display_name),
    player_name: safePlayerName(playerName),
    custom: true,
    created_at: typeof saved?.created_at === "string" ? saved.created_at : "",
    updated_at: typeof saved?.updated_at === "string" ? saved.updated_at : "",
    source: "easy",
    description: "Created from SekaiLink Easy Mode choices.",
    values,
  };
  trace("seed-config", "create_easy_seed_success", { seedId: seed.id, title: seed.title, gameKey: seed.game_key });
  invalidateSeedConfigCaches("seeds");
  return seed;
};

export const createAdvancedSeed = async (
  game: SeedGameEntry,
  title: string,
  playerName: string,
  values: Record<string, unknown>
): Promise<SeedEntry> => {
  const seedTitle = title.trim() || "Advanced Showcase Seed";
  const advancedValues = values || {};
  trace("seed-config", "create_advanced_seed_start", {
    gameKey: game.game_key,
    title: seedTitle,
    valueCount: Object.keys(advancedValues || {}).length,
  });
  const body = {
    title: seedTitle,
    name: seedTitle,
    game: game.display_name,
    game_key: canonicalSeedGameKey(game.game_key || game.display_name),
    player_name: safePlayerName(playerName),
    values: advancedValues,
    source: "advanced",
    mode: "advanced",
    description: "Created from SekaiLink Advanced seed form.",
  };
  let response = await apiFetch("/api/seed-configs", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  let data = await response.json().catch(() => ({}));
  if (response.status === 404 || data?.error === "not_found" || data?.error === "route_not_found") {
    const content = buildAdvancedYaml(game, seedTitle, playerName, advancedValues);
    response = await apiFetch("/api/yamls/new", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ ...body, content }),
    });
    data = await response.json().catch(() => ({}));
  }
  if (!response.ok) {
    const issueText = Array.isArray(data?.issues)
      ? data.issues
          .map((issue: any) => `${issue?.option_key || "option"}: ${issue?.detail || issue?.code || "invalid"}`)
          .join("; ")
      : "";
    const detail = typeof data?.detail === "string" ? data.detail : "";
    const error = String(issueText || detail || data?.error || "Unable to create seed.");
    trace("seed-config", "create_advanced_seed_failed", { status: response.status, error }, "warn");
    throw new Error(error);
  }
  const saved = data?.seed || data?.yaml || data?.config || data;
  const seed = {
    id: String(saved?.id || saved?.config_id || data?.id || ""),
    title: String(saved?.title || saved?.name || seedTitle),
    game: String(saved?.game || saved?.game_display_name || game.display_name),
    game_key: canonicalSeedGameKey(saved?.game_key || game.game_key || game.display_name),
    player_name: String(saved?.player_name || safePlayerName(playerName)),
    custom: true,
    created_at: typeof saved?.created_at === "string" ? saved.created_at : "",
    updated_at: typeof saved?.updated_at === "string" ? saved.updated_at : "",
    source: typeof saved?.source === "string" ? saved.source : "advanced",
    description: "Created from SekaiLink Advanced seed form.",
    values: normalizeSeedValues(saved) || advancedValues,
  };
  trace("seed-config", "create_advanced_seed_success", { seedId: seed.id, title: seed.title, gameKey: seed.game_key });
  invalidateSeedConfigCaches("seeds");
  return seed;
};

export const deleteSeed = async (seedId: string): Promise<void> => {
  trace("seed-config", "delete_seed_start", { seedId });
  const response = await apiFetch(`/api/yamls/${encodeURIComponent(seedId)}/delete`, { method: "POST" });
  if (!response.ok) {
    const data = await response.json().catch(() => ({}));
    const error = String(data?.error || "Unable to delete seed.");
    trace("seed-config", "delete_seed_failed", { seedId, status: response.status, error }, "warn");
    throw new Error(error);
  }
  trace("seed-config", "delete_seed_success", { seedId });
  invalidateSeedConfigCaches("seeds");
};

export const replaceAdvancedSeed = async (
  seedId: string,
  game: SeedGameEntry,
  title: string,
  playerName: string,
  values: Record<string, unknown>
): Promise<SeedEntry> => {
  trace("seed-config", "replace_advanced_seed_start", {
    seedId,
    gameKey: game.game_key,
    title,
    valueCount: Object.keys(values || {}).length,
  });
  const replacement = await createAdvancedSeed(game, title, playerName, values);
  try {
    await deleteSeed(seedId);
    trace("seed-config", "replace_advanced_seed_old_deleted", { seedId, replacementId: replacement.id });
  } catch (error) {
    traceError("seed-config", "replace_advanced_seed_old_delete_failed", error, {
      seedId,
      replacementId: replacement.id,
    });
  }
  trace("seed-config", "replace_advanced_seed_success", { seedId, replacementId: replacement.id });
  return replacement;
};

export const loadActiveSeeds = (): SeedEntry[] => {
  try {
    const raw = window.localStorage.getItem(ACTIVE_SEEDS_STORAGE_KEY);
    const parsed = raw ? JSON.parse(raw) : [];
    return Array.isArray(parsed) ? parsed.filter((entry) => entry?.id) : [];
  } catch {
    return [];
  }
};

export const saveActiveSeeds = (seeds: SeedEntry[]) => {
  try {
    window.localStorage.setItem(ACTIVE_SEEDS_STORAGE_KEY, JSON.stringify(seeds.slice(0, 20)));
  } catch {
    // Ignore storage failures; the live room selection is persisted server-side.
  }
};

export const addActiveSeed = (seed: SeedEntry): SeedEntry[] => {
  const next = [seed, ...loadActiveSeeds().filter((entry) => entry.id !== seed.id)].slice(0, 20);
  saveActiveSeeds(next);
  return next;
};
