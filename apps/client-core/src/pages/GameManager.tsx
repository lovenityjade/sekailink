import React, { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { useLocation, useNavigate } from "react-router-dom";
import Editor from "react-simple-code-editor";
import Prism from "prismjs";
import "prismjs/components/prism-yaml";
import "prismjs/components/prism-lua";
import { apiFetch, apiJson } from "../services/api";
import gameRegistry from "../data/games.generated.json";
import { runtime } from "../services/runtime";
import { fetchGameBoxArtMap } from "../services/gamebox";
import { lookupFeaturedGridUrl } from "../data/steamGridDbFeatured";
import { getLaunchReadiness, type LaunchReadiness } from "../services/launchReadiness";
import { useI18n } from "../i18n";
import { getPreferredYamlId, setPreferredYamlId } from "../utils/yamlSelection";
import ChamferedPanel from "../components/ChamferedPanel";

interface YamlEntry {
  id: string;
  title: string;
  game: string;
  player_name: string;
  custom?: boolean;
}

interface GameEntry {
  game_id: string;
  display_name: string;
}

type GameSetupStatus = {
  roms: Record<string, string>;
  trackerPacks: Record<string, { path: string; source?: string }>;
  trackerVariants: Record<string, string>;
};

type RuntimeModuleInfo = {
  moduleId: string;
  manifest: Record<string, any>;
};

type TrackerBadge = {
  label: "Poptracker" | "Webtracker" | "Universal Tracker" | "No Tracker";
  className: "poptracker" | "webtracker" | "no-tracker";
};

type RuntimeReadinessCard = {
  moduleId: string;
  ready: boolean;
  label: string;
  className: "ready" | "missing";
  note: string;
  correctionLabel?: string;
  correctionRoute?: string;
};

const normalizeTrackerKey = (value: unknown) =>
  String(value || "")
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "_")
    .replace(/^_+|_+$/g, "");

const TRACKER_BADGE_OVERRIDES: Record<string, TrackerBadge> = {
  a_link_between_worlds: { label: "Poptracker", className: "poptracker" },
  pokemon_red_and_blue: { label: "Poptracker", className: "poptracker" },
  pokemon_firered_and_leafgreen: { label: "Poptracker", className: "poptracker" },
};

const LINK_VERIFIED_OVERRIDES = new Set<string>([
  "a_link_to_the_past",
  "alttp",
  "alttpr",
  "earthbound",
  "final_fantasy_iv_free_enterprise",
  "final_fantasy_4_free_enterprise",
  "ff4fe",
]);

const GameManagerPage: React.FC = () => {
  const { t } = useI18n();
  const [activeTab, setActiveTab] = useState<"my" | "add" | "editor">("my");
  const [yamls, setYamls] = useState<YamlEntry[]>([]);
  const [yamlContent, setYamlContent] = useState("");
  const [editorLang, setEditorLang] = useState<"yaml" | "lua">("yaml");
  const [yamlStatus, setYamlStatus] = useState("");
  const [yamlSelect, setYamlSelect] = useState("");
  const [importedName, setImportedName] = useState("");
  const [search, setSearch] = useState("");
  const [customModalOpen, setCustomModalOpen] = useState(false);
  const [pendingYamlLoad, setPendingYamlLoad] = useState<string | null>(null);
  const [preferredYamlId, setPreferredYamlIdState] = useState<string>(() => getPreferredYamlId());
  const [setupStatus, setSetupStatus] = useState("");
  const [setupConfig, setSetupConfig] = useState<GameSetupStatus>({ roms: {}, trackerPacks: {}, trackerVariants: {} });
  const [setupBusy, setSetupBusy] = useState(false);
  const [runtimeModules, setRuntimeModules] = useState<RuntimeModuleInfo[]>([]);
  const [variantsByGameId, setVariantsByGameId] = useState<Record<string, Array<{ id: string; name: string }>>>({});
  const [runtimeReadinessByKey, setRuntimeReadinessByKey] = useState<Record<string, RuntimeReadinessCard>>({});
  const [serverGames, setServerGames] = useState<GameEntry[]>([]);
  const [gamesLoaded, setGamesLoaded] = useState(false);
  const [gameBoxArtById, setGameBoxArtById] = useState<Record<string, string>>({});
  const carouselRef = useRef<HTMLDivElement | null>(null);
  const [carouselCanPrev, setCarouselCanPrev] = useState(false);
  const [carouselCanNext, setCarouselCanNext] = useState(false);
  const location = useLocation();
  const navigate = useNavigate();

  const games = useMemo(() => {
    // Server is source of truth. If it fails, keep a local fallback so the UI isn't empty.
    // Do not filter by local runtime modules: this hides valid server-supported games.
    const base = (serverGames.length ? serverGames : ((gameRegistry as GameEntry[]) || []))
      .filter((entry) => String(entry?.game_id || "").trim() && String(entry?.display_name || "").trim());
    return base;
  }, [serverGames]);

  const loadYamls = async () => {
    try {
      const data = await apiJson<{ yamls: YamlEntry[] }>("/api/yamls");
      setYamls(data.yamls || []);
    } catch {
      setYamls([]);
    }
  };

  useEffect(() => {
    loadYamls();
  }, []);

  const refreshSetupConfig = useCallback(async () => {
    const config = await runtime.configGet?.();
    const roms = (config as any)?.roms || {};
    const trackerPacks = (config as any)?.trackerPacks || {};
    const trackerVariants = (config as any)?.trackerVariants || {};
    setSetupConfig({ roms, trackerPacks, trackerVariants });
  }, []);

  useEffect(() => {
    refreshSetupConfig();
  }, [refreshSetupConfig]);

  useEffect(() => {
    const loadGames = async () => {
      try {
        const data = await apiJson<{ games?: GameEntry[] }>("/api/client/games");
        const list = Array.isArray(data?.games) ? data.games : [];
        setServerGames(list);
      } catch {
        setServerGames([]);
      } finally {
        setGamesLoaded(true);
      }
    };
    void loadGames();
  }, []);

  useEffect(() => {
    const ids = games.map((g) => g.game_id);
    let cancelled = false;
    const loadBoxArts = async () => {
      const mapped = await fetchGameBoxArtMap(ids);
      if (!cancelled) setGameBoxArtById(mapped);
    };
    void loadBoxArts();
    return () => {
      cancelled = true;
    };
  }, [games]);

  useEffect(() => {
    const load = async () => {
      try {
        const res = await runtime.listRuntimeModules?.();
        const modules = (res as any)?.modules;
        if ((res as any)?.ok && Array.isArray(modules)) {
          setRuntimeModules(modules);
        } else {
          setRuntimeModules([]);
        }
      } catch {
        setRuntimeModules([]);
      }
    };
    load();
  }, []);

  const filteredGames = useMemo(() => {
    const query = search.toLowerCase().trim();
    if (!query) return games;
    return games.filter((game) => game.display_name.toLowerCase().includes(query));
  }, [games, search]);

  const updateCarouselState = useCallback(() => {
    const node = carouselRef.current;
    if (!node) {
      setCarouselCanPrev(false);
      setCarouselCanNext(false);
      return;
    }
    const maxLeft = Math.max(0, node.scrollWidth - node.clientWidth);
    setCarouselCanPrev(node.scrollLeft > 4);
    setCarouselCanNext(node.scrollLeft < maxLeft - 4);
  }, []);

  const scrollCarouselByStep = useCallback((direction: -1 | 1) => {
    const node = carouselRef.current;
    if (!node) return;
    const card = node.querySelector<HTMLElement>(".yaml-carousel-card");
    const step = card ? card.offsetWidth + 12 : Math.max(300, Math.floor(node.clientWidth * 0.82));
    const maxScroll = node.scrollWidth - node.clientWidth;

    if (direction === 1 && node.scrollLeft >= maxScroll - 4) {
      node.scrollTo({ left: 0, behavior: "smooth" });
    } else if (direction === -1 && node.scrollLeft <= 4) {
      node.scrollTo({ left: maxScroll, behavior: "smooth" });
    } else {
      node.scrollBy({ left: direction * step, behavior: "smooth" });
    }
    window.setTimeout(updateCarouselState, 280);
  }, [updateCarouselState]);

  const scrollHoldRef = useRef<number | null>(null);

  const startHoldScroll = useCallback((direction: -1 | 1) => {
    if (scrollHoldRef.current) return;
    const tick = () => {
      const node = carouselRef.current;
      if (!node) return;
      const maxScroll = node.scrollWidth - node.clientWidth;
      if (direction === 1 && node.scrollLeft >= maxScroll - 2) {
        node.scrollLeft = 0;
      } else if (direction === -1 && node.scrollLeft <= 2) {
        node.scrollLeft = maxScroll;
      }
      node.scrollBy({ left: direction * 3, behavior: "auto" });
    };
    scrollHoldRef.current = window.setInterval(tick, 16);
  }, []);

  const stopHoldScroll = useCallback(() => {
    if (scrollHoldRef.current) {
      window.clearInterval(scrollHoldRef.current);
      scrollHoldRef.current = null;
      updateCarouselState();
    }
  }, [updateCarouselState]);

  useEffect(() => {
    return () => { if (scrollHoldRef.current) window.clearInterval(scrollHoldRef.current); };
  }, []);

  useEffect(() => {
    const node = carouselRef.current;
    if (!node) return;
    const onScroll = () => updateCarouselState();
    node.addEventListener("scroll", onScroll, { passive: true });
    window.addEventListener("resize", onScroll);
    onScroll();
    return () => {
      node.removeEventListener("scroll", onScroll);
      window.removeEventListener("resize", onScroll);
    };
  }, [updateCarouselState]);

  useEffect(() => {
    const node = carouselRef.current;
    if (!node) return;
    if (typeof node.scrollTo === "function") {
      node.scrollTo({ left: 0, behavior: "auto" });
    } else {
      node.scrollLeft = 0;
    }
    updateCarouselState();
  }, [search, filteredGames.length, updateCarouselState]);

  const placeholderBoxArtUrl = useMemo(() => {
    const base = String(import.meta.env.BASE_URL || "/").replace(/\/?$/, "/");
    if (typeof window !== "undefined" && window.location?.href) {
      return new URL(`${base}assets/img/yaml_placeholder.svg`, window.location.href).toString();
    }
    return `${base}assets/img/yaml_placeholder.svg`;
  }, []);

  const getBoxArtUrl = useCallback((game: GameEntry) => {
    return (
      lookupFeaturedGridUrl(game.display_name) ||
      lookupFeaturedGridUrl(game.game_id) ||
      gameBoxArtById[game.game_id] ||
      placeholderBoxArtUrl
    );
  }, [gameBoxArtById, placeholderBoxArtUrl]);

  const trackerBadgeByKey = useMemo(() => {
    const map: Record<string, TrackerBadge> = {};

    for (const mod of runtimeModules) {
      const manifest = mod?.manifest || {};
      const gameId = normalizeTrackerKey(manifest.game_id);
      const display = normalizeTrackerKey(manifest.display_name);
      const trackerPack = String(manifest.tracker_pack_uid || "").trim();
      const trackerWeb = String(manifest.tracker_web_url || "").trim();
      const trackerType = normalizeTrackerKey(manifest.tracker_type || trackerWeb);
      const badge: TrackerBadge =
        trackerType === "universal_tracker" || trackerType === "universaltracker"
          ? { label: "Universal Tracker", className: "webtracker" }
          : trackerWeb
            ? { label: "Webtracker", className: "webtracker" }
            : trackerPack
              ? { label: "Poptracker", className: "poptracker" }
              : { label: "No Tracker", className: "no-tracker" };
      if (gameId) map[gameId] = badge;
      if (display) map[display] = badge;
    }
    for (const [key, badge] of Object.entries(TRACKER_BADGE_OVERRIDES)) {
      if (!map[key]) map[key] = badge;
    }
    return map;
  }, [runtimeModules]);

  const getTrackerBadge = useCallback((game: GameEntry): TrackerBadge => {
    const normalizedId = normalizeTrackerKey(game.game_id);
    const normalizedName = normalizeTrackerKey(game.display_name);
    return (
      trackerBadgeByKey[normalizedId] ||
      trackerBadgeByKey[normalizedName] || {
        label: "No Tracker",
        className: "no-tracker",
      }
    );
  }, [trackerBadgeByKey]);

  const isLinkVerifiedGame = useCallback((game: GameEntry) => {
    const normalizedId = normalizeTrackerKey(game.game_id);
    const normalizedName = normalizeTrackerKey(game.display_name);
    if (LINK_VERIFIED_OVERRIDES.has(normalizedId) || LINK_VERIFIED_OVERRIDES.has(normalizedName)) return true;
    return normalizedId.startsWith("pokemon_") || normalizedName.startsWith("pokemon_");
  }, []);

  useEffect(() => {
    let cancelled = false;

    const toReadinessCard = (readiness: LaunchReadiness): RuntimeReadinessCard => {
      if (readiness.ready) {
        return {
          moduleId: readiness.moduleId,
          ready: true,
          label: "Ready",
          className: "ready",
          note: readiness.trackerNeeded ? "ROM and tracker setup are ready." : "Game runtime is ready.",
        };
      }

      const err = String(readiness.error || "").trim();
      if (err === "rom_missing") {
        return {
          moduleId: readiness.moduleId,
          ready: false,
          label: "ROM Required",
          className: "missing",
          note: "Add the base game file before this title can launch.",
          correctionLabel: readiness.correctionAction?.label || "Open ROM Library",
          correctionRoute: readiness.correctionAction?.route || "/settings?tab=roms",
        };
      }

      if (err === "poptracker_missing" || err === "mono_missing" || String(readiness.setupArea || "").trim().toLowerCase() === "paths") {
        return {
          moduleId: readiness.moduleId,
          ready: false,
          label: "Setup Required",
          className: "missing",
          note: "Finish the runtime or tracker setup before launch.",
          correctionLabel: readiness.correctionAction?.label || "Open Games Setup",
          correctionRoute: readiness.correctionAction?.route || "/settings?tab=paths",
        };
      }

      return {
        moduleId: readiness.moduleId,
        ready: false,
        label: "Setup Pending",
        className: "missing",
        note: "SekaiLink still needs additional setup details for this game.",
        correctionLabel: readiness.correctionAction?.label,
        correctionRoute: readiness.correctionAction?.route,
      };
    };

    const loadRuntimeReadiness = async () => {
      if (!runtimeModules.length) {
        setRuntimeReadinessByKey({});
        return;
      }

      const entries = await Promise.all(
        runtimeModules.map(async (module) => {
          const manifest = module?.manifest || {};
          const readiness = await getLaunchReadiness(module.moduleId).catch(
            () =>
              ({
                ok: false,
                ready: false,
                error: "validation_unavailable",
                moduleId: module.moduleId,
                manifest,
                correctionAction: null,
                requiredRomIds: [],
                romReady: false,
                trackerNeeded: Boolean((manifest as any).tracker_pack_uid),
                trackerReady: false,
              }) as LaunchReadiness
          );
          const card = toReadinessCard(readiness);
          const gameKey = normalizeTrackerKey(manifest.game_id);
          const nameKey = normalizeTrackerKey(manifest.display_name);
          return { card, gameKey, nameKey };
        })
      );

      if (cancelled) return;

      const next: Record<string, RuntimeReadinessCard> = {};
      for (const entry of entries) {
        if (entry.gameKey && !next[entry.gameKey]) next[entry.gameKey] = entry.card;
        if (entry.nameKey && !next[entry.nameKey]) next[entry.nameKey] = entry.card;
      }
      setRuntimeReadinessByKey(next);
    };

    void loadRuntimeReadiness();

    return () => {
      cancelled = true;
    };
  }, [runtimeModules, setupConfig]);

  const loadYaml = async (yamlId = yamlSelect) => {
    if (!yamlId) {
      setYamlStatus("Select a YAML first.");
      return;
    }
    try {
      setYamlStatus("Loading...");
      const res = await apiFetch(`/api/yamls/${encodeURIComponent(yamlId)}/raw`);
      if (!res.ok) throw new Error("Unable to load YAML.");
      const data = await res.json();
      setYamlContent(data.yaml || "");
      setYamlStatus("");
    } catch (err) {
      setYamlStatus(err instanceof Error ? err.message : "Unable to load YAML.");
    }
  };

  useEffect(() => {
    const params = new URLSearchParams(location.search);
    const tab = params.get("tab");
    const yamlId = params.get("yaml");
    if (tab === "my" || tab === "add" || tab === "editor") {
      setActiveTab(tab);
    }
    if (yamlId) {
      setYamlSelect(yamlId);
      setActiveTab("editor");
      setPendingYamlLoad(yamlId);
    }
  }, [location.search]);

  useEffect(() => {
    if (!pendingYamlLoad) return;
    loadYaml(pendingYamlLoad);
    setPendingYamlLoad(null);
  }, [pendingYamlLoad]);

  const saveYaml = async () => {
    try {
      setYamlStatus("Saving...");
      const endpoint = yamlSelect
        ? `/api/yamls/${encodeURIComponent(yamlSelect)}/raw`
        : "/api/yamls/new";
      const res = await apiFetch(endpoint, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ content: yamlContent, save_as_new: true })
      });
      if (!res.ok) {
        const data = await res.json().catch(() => ({}));
        throw new Error(data.error || "Unable to save YAML.");
      }
      const data = await res.json().catch(() => ({}));
      if (data?.yaml) {
        setYamls((prev) => [...prev, data.yaml]);
        setYamlSelect(data.yaml.id);
        setYamlStatus("Saved as Custom.");
        setImportedName("");
      }
    } catch (err) {
      setYamlStatus(err instanceof Error ? err.message : "Unable to save YAML.");
    }
  };

  const duplicateYaml = async (yamlId: string) => {
    try {
      setYamlStatus("Duplicating...");
      const res = await apiFetch(`/api/yamls/${encodeURIComponent(yamlId)}/duplicate`, { method: "POST" });
      if (!res.ok) {
        const data = await res.json().catch(() => ({}));
        throw new Error(data.error || "Unable to duplicate YAML.");
      }
      const data = await res.json().catch(() => ({}));
      if (data?.yaml) {
        setYamls((prev) => [...prev, data.yaml]);
        setYamlStatus("YAML duplicated.");
      }
    } catch (err) {
      setYamlStatus(err instanceof Error ? err.message : "Unable to duplicate YAML.");
    }
  };

  const downloadYaml = async (yamlId: string) => {
    if (!yamlId) return;
    try {
      setYamlStatus("Preparing download...");
      const res = await apiFetch(`/api/yamls/${encodeURIComponent(yamlId)}/raw`);
      if (!res.ok) throw new Error("Unable to download YAML.");
      const data = await res.json();
      const blob = new Blob([data.yaml || ""], { type: "text/yaml" });
      const url = URL.createObjectURL(blob);
      const anchor = document.createElement("a");
      anchor.href = url;
      anchor.download = `${yamlId}.yaml`;
      document.body.appendChild(anchor);
      anchor.click();
      document.body.removeChild(anchor);
      URL.revokeObjectURL(url);
      setYamlStatus("");
    } catch (err) {
      setYamlStatus(err instanceof Error ? err.message : "Unable to download YAML.");
    }
  };

  const deleteYaml = async (yamlId: string) => {
    if (!yamlId) return;
    try {
      setYamlStatus("Deleting...");
      const res = await apiFetch(`/api/yamls/${encodeURIComponent(yamlId)}/delete`, { method: "POST" });
      if (!res.ok) throw new Error("Unable to delete YAML.");
      setYamls((prev) => prev.filter((item) => item.id !== yamlId));
      if (yamlSelect === yamlId) {
        setYamlSelect("");
        setYamlContent("");
      }
      setYamlStatus("Deleted.");
    } catch (err) {
      setYamlStatus(err instanceof Error ? err.message : "Unable to delete YAML.");
    }
  };

  const selectPreferredYaml = (yaml: YamlEntry) => {
    setPreferredYamlId(yaml.id);
    setPreferredYamlIdState(yaml.id);
    setYamlStatus(`Marked "${yaml.title}" as your preferred config in Game Manager.`);
  };

  const normalizeExtensions = (extensions: unknown) => {
    const list = Array.isArray(extensions) ? extensions : [];
    return list
      .map((ext) => String(ext || "").trim().toLowerCase())
      .filter(Boolean)
      .map((ext) => (ext.startsWith(".") ? ext.slice(1) : ext));
  };

  const loadPackVariants = async (gameId: string) => {
    if (!gameId) return;
    setSetupStatus("");
    setSetupBusy(true);
    try {
      const res = await runtime.trackerListPackVariants?.({ gameId });
      const variants = (res as any)?.variants;
      if ((res as any)?.ok && Array.isArray(variants)) {
        setVariantsByGameId((prev) => ({ ...prev, [gameId]: variants }));
      } else {
        setVariantsByGameId((prev) => ({ ...prev, [gameId]: [] }));
      }
    } catch (err) {
      setSetupStatus(err instanceof Error ? err.message : "Unable to load pack variants.");
      setVariantsByGameId((prev) => ({ ...prev, [gameId]: [] }));
    } finally {
      setSetupBusy(false);
    }
  };

  const importRom = async (title: string, extensions: string[]) => {
    setSetupStatus("");
    const result = await runtime.pickFile?.({
      title,
      filters: [{ name: "ROM", extensions }]
    });
    if (!result || result.canceled || !result.path) return;
    setSetupBusy(true);
    try {
      const res = await runtime.romsImport?.(result.path);
      await refreshSetupConfig();
      if (res && (res as any).ok) {
        setSetupStatus("ROM imported.");
      } else {
        setSetupStatus(`ROM import failed: ${(res as any)?.error || "hash mismatch"}`);
      }
    } catch (err) {
      setSetupStatus(err instanceof Error ? err.message : "Unable to import ROM.");
    } finally {
      setSetupBusy(false);
    }
  };

  const installTrackerPack = async (gameId: string, repo: string, assetRegex?: string) => {
    setSetupStatus("");
    setSetupBusy(true);
    try {
      const res = await runtime.trackerInstallPack?.({
        gameId,
        packRepo: repo,
        assetRegex
      });
      if (res && (res as any).ok) {
        await refreshSetupConfig();
        setSetupStatus("Tracker pack installed.");
      } else {
        setSetupStatus(`Tracker install failed: ${(res as any)?.error || "unknown error"}`);
      }
    } catch (err) {
      setSetupStatus(err instanceof Error ? err.message : "Tracker install failed.");
    } finally {
      setSetupBusy(false);
    }
  };

  const getRuntimeReadinessCard = useCallback((game: GameEntry) => {
    const normalizedId = normalizeTrackerKey(game.game_id);
    const normalizedName = normalizeTrackerKey(game.display_name);
    return runtimeReadinessByKey[normalizedId] || runtimeReadinessByKey[normalizedName] || null;
  }, [runtimeReadinessByKey]);


  const onImportYaml = async (event: React.ChangeEvent<HTMLInputElement>) => {
    if (!event.target.files?.length) return;
    const file = event.target.files[0];
    try {
      setYamlStatus("Importing...");
      const content = await file.text();
      const res = await apiFetch("/api/yamls/import", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ content })
      });
      if (!res.ok) {
        const data = await res.json().catch(() => ({}));
        throw new Error(data.error || "Unable to import YAML.");
      }
      const data = await res.json().catch(() => ({}));
      if (data.import_id) {
        navigate(`/player-options/import/${encodeURIComponent(data.import_id)}`);
        setYamlStatus("Opening YAML in player options…");
      } else {
        setYamlStatus("Import ready.");
      }
      setImportedName(file.name);
    } catch (err) {
      setYamlStatus(err instanceof Error ? err.message : "Unable to import YAML.");
    } finally {
      event.target.value = "";
    }
  };

  return (
    <div className="skl-game-manager-wrap">
    <ChamferedPanel className="skl-game-manager" noPadding>

      <div className="skl-gm-tabs" role="tablist" data-tutorial="gm-tabs">
        <button
          className={`skl-gm-tab${activeTab === "my" ? " active" : ""}`}
          type="button"
          onClick={() => setActiveTab("my")}
        >
          <svg className="skl-gm-tab-icon" viewBox="0 0 24 24" fill="none" aria-hidden="true">
            <path d="M4 7.5h16M4 12h16M4 16.5h16" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" />
          </svg>
          {t("gm.my_games")}
        </button>
        <button
          className={`skl-gm-tab${activeTab === "add" ? " active" : ""}`}
          type="button"
          onClick={() => setActiveTab("add")}
        >
          <svg className="skl-gm-tab-icon" viewBox="0 0 24 24" fill="none" aria-hidden="true">
            <path d="M12 5v14M5 12h14" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" />
          </svg>
          {t("gm.add_game")}
        </button>
        <button
          className={`skl-gm-tab${activeTab === "editor" ? " active" : ""}`}
          type="button"
          onClick={() => setActiveTab("editor")}
        >
          <svg className="skl-gm-tab-icon" viewBox="0 0 24 24" fill="none" aria-hidden="true">
            <path d="M5 19h14M7 16l8-8 2 2-8 8H7v-2Z" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round" />
          </svg>
          {t("gm.editor")}
        </button>
      </div>

      <div className={`skl-gm-panel${activeTab === "my" ? " active" : ""}`} data-gm-panel="my" role="tabpanel">
        {yamls.length ? (
          <div className="skl-yaml-listbox" role="listbox">
            {yamls.map((item) => {
              const isSelected = preferredYamlId === item.id;
              return (
                <div
                  className={`skl-yaml-item${isSelected ? " selected" : ""}`}
                  key={item.id}
                  role="option"
                  aria-selected={isSelected}
                  onClick={() => selectPreferredYaml(item)}
                >
                  <div className="skl-yaml-indicator" />
                  <div className="skl-yaml-info">
                    <span className="skl-yaml-name">
                      {item.title}
                      {item.custom && <span className="skl-yaml-badge">CUSTOM</span>}
                    </span>
                    <span className="skl-yaml-detail">{item.game} &middot; {item.player_name}</span>
                  </div>
                  <div className="skl-yaml-toolbar" onClick={(e) => e.stopPropagation()}>
                    {/* Mark preferred for editor convenience */}
                    <button
                      type="button"
                      title={isSelected ? "Clear preferred config" : "Mark as preferred config"}
                      aria-label={isSelected ? "Clear preferred config" : "Mark as preferred config"}
                      onClick={() => {
                        if (isSelected) {
                          setPreferredYamlId("");
                          setPreferredYamlIdState("");
                          setYamlStatus(`Cleared preferred config "${item.title}".`);
                          return;
                        }
                        selectPreferredYaml(item);
                      }}
                    >
                      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
                        <path d="M5 12.5 9.5 17 19 7.5" />
                      </svg>
                    </button>
                    {/* Edit / Advanced */}
                    <button
                      type="button"
                      title="Edit YAML"
                      onClick={() => { setActiveTab("editor"); setYamlSelect(item.id); loadYaml(item.id); }}
                    >
                      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
                        <path d="M11 4H4a2 2 0 00-2 2v14a2 2 0 002 2h14a2 2 0 002-2v-7" /><path d="M18.5 2.5a2.121 2.121 0 013 3L12 15l-4 1 1-4 9.5-9.5z" />
                      </svg>
                    </button>
                    {/* Duplicate */}
                    <button type="button" title="Duplicate" onClick={() => duplicateYaml(item.id)}>
                      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
                        <rect x="9" y="9" width="13" height="13" rx="2" /><path d="M5 15H4a2 2 0 01-2-2V4a2 2 0 012-2h9a2 2 0 012 2v1" />
                      </svg>
                    </button>
                    {/* Download */}
                    <button type="button" title="Download" onClick={() => downloadYaml(item.id)}>
                      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
                        <path d="M21 15v4a2 2 0 01-2 2H5a2 2 0 01-2-2v-4" /><polyline points="7 10 12 15 17 10" /><line x1="12" y1="15" x2="12" y2="3" />
                      </svg>
                    </button>
                    {/* Delete */}
                    <button type="button" title="Delete" className="skl-yaml-btn-danger" onClick={() => deleteYaml(item.id)}>
                      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
                        <polyline points="3 6 5 6 21 6" /><path d="M19 6l-1 14a2 2 0 01-2 2H8a2 2 0 01-2-2L5 6" /><path d="M10 11v6" /><path d="M14 11v6" /><path d="M9 6V4a1 1 0 011-1h4a1 1 0 011 1v2" />
                      </svg>
                    </button>
                  </div>
                </div>
              );
            })}
          </div>
        ) : (
          <div className="skl-yaml-empty">
            <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round" style={{ opacity: 0.3 }}>
              <path d="M14 2H6a2 2 0 00-2 2v16a2 2 0 002 2h12a2 2 0 002-2V8z" /><polyline points="14 2 14 8 20 8" />
            </svg>
            <span>No YAMLs saved yet. Add a game to get started.</span>
          </div>
        )}
      </div>

      <div className={`skl-gm-panel${activeTab === "add" ? " active" : ""}`} data-gm-panel="add" role="tabpanel">
            <div className={`skl-gm-search${search ? " has-value" : ""}`}>
              <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <circle cx="11" cy="11" r="8" /><path d="m21 21-4.35-4.35" />
              </svg>
              <input
                id="yaml-game-search"
                type="search"
                placeholder={t("legacy.term.search_game")}
                value={search}
                onChange={(event) => setSearch(event.target.value)}
                autoComplete="off"
              />
              {search && (
                <button className="skl-gm-search-clear" type="button" aria-label={t("gm.clear_search")} onClick={() => setSearch("")}>
                  <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round">
                    <path d="M18 6 6 18M6 6l12 12" />
                  </svg>
                </button>
              )}
            </div>
                  <div className="yaml-carousel-shell" id="yaml-game-grid">
                    <button
                      type="button"
                      className="yaml-carousel-nav"
                      aria-label={t("gm.carousel_prev")}
                      onClick={() => scrollCarouselByStep(-1)}
                      onMouseDown={() => startHoldScroll(-1)}
                      onMouseUp={stopHoldScroll}
                      onMouseLeave={stopHoldScroll}
                    >
                      <svg width="22" height="22" viewBox="0 0 24 24" fill="none" aria-hidden="true">
                        <path d="M15 4l-8 8 8 8" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
                      </svg>
                    </button>
                    <div className="yaml-carousel-track" ref={carouselRef}>
                      {filteredGames.map((game) => {
                        const trackerBadge = getTrackerBadge(game);
                        const linkVerified = isLinkVerifiedGame(game);
                        const readinessCard = getRuntimeReadinessCard(game);
                        return (
                          <a
                            className="yaml-game-card yaml-carousel-card"
                            key={game.game_id}
                            href={`#/player-options/${encodeURIComponent(game.game_id)}/create`}
                            onClick={(event) => {
                              event.preventDefault();
                              navigate(`/player-options/${encodeURIComponent(game.game_id)}/create`);
                            }}
                            data-game-name={game.display_name}
                          >
                            <div className="yaml-game-thumb">
                              {linkVerified && (
                                <span className="yaml-link-verified-badge" title={t("gm.link_verified_badge")} aria-label={t("gm.link_verified_badge")}>
                                  <span aria-hidden="true">✓</span>
                                  {t("gm.link_verified_badge")}
                                </span>
                              )}
                              <span className={`yaml-tracker-badge ${trackerBadge.className}`}>{trackerBadge.label}</span>
                              <img
                                src={getBoxArtUrl(game)}
                                alt=""
                                loading="lazy"
                                onError={(event) => {
                                  const target = event.currentTarget;
                                  if (target.dataset.fallbackApplied === "1") return;
                                  target.dataset.fallbackApplied = "1";
                                  target.src = placeholderBoxArtUrl;
                                }}
                              />
                            </div>
                            <div className="yaml-game-name">{game.display_name}</div>
                            {readinessCard && (
                              <div className={`yaml-game-readiness ${readinessCard.className}`}>
                                <div className="yaml-game-readiness-label">{readinessCard.label}</div>
                                <div className="yaml-game-readiness-note">{readinessCard.note}</div>
                                {readinessCard.correctionRoute && readinessCard.correctionLabel && (
                                  <button
                                    type="button"
                                    className="yaml-game-setup"
                                    onClick={(event) => {
                                      event.preventDefault();
                                      event.stopPropagation();
                                      navigate(readinessCard.correctionRoute || "/settings");
                                    }}
                                  >
                                    {readinessCard.correctionLabel}
                                  </button>
                                )}
                              </div>
                            )}
                          </a>
                        );
                      })}
                    </div>
                    <button
                      type="button"
                      className="yaml-carousel-nav"
                      aria-label={t("gm.carousel_next")}
                      onClick={() => scrollCarouselByStep(1)}
                      onMouseDown={() => startHoldScroll(1)}
                      onMouseUp={stopHoldScroll}
                      onMouseLeave={stopHoldScroll}
                    >
                      <svg width="22" height="22" viewBox="0 0 24 24" fill="none" aria-hidden="true">
                        <path d="M9 4l8 8-8 8" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
                      </svg>
                    </button>
                  </div>
                  <div className="yaml-game-empty" id="yaml-game-empty" hidden={filteredGames.length > 0}>
                    {!gamesLoaded ? "Loading games..." : "No games match your search (or this game is not supported yet)."}
                  </div>
            </div>

      <div className={`skl-gm-panel${activeTab === "editor" ? " active" : ""}`} data-gm-panel="editor" role="tabpanel">
        <div className="skl-gm-section-header">
          <h2>{t("gm.editor")}</h2>
          <div className="skl-gm-actions">
            <div className="skl-editor-lang-toggle" role="group" aria-label="Editor language">
              <button
                type="button"
                className={`skl-btn ghost${editorLang === "yaml" ? " active" : ""}`}
                onClick={() => setEditorLang("yaml")}
              >
                YAML
              </button>
              <button
                type="button"
                className={`skl-btn ghost${editorLang === "lua" ? " active" : ""}`}
                onClick={() => setEditorLang("lua")}
              >
                Lua
              </button>
            </div>
            <label className="skl-btn ghost">
              {t("gm.import_yaml")}
              <input type="file" accept=".yml,.yaml" style={{ display: "none" }} onChange={onImportYaml} />
            </label>
            <select id="gm-yaml-select" className="skl-gm-select" value={yamlSelect} onChange={(event) => setYamlSelect(event.target.value)}>
              <option value="">Select a YAML</option>
              {yamls.map((item) => (
                <option key={item.id} value={item.id}>
                  {item.title} · {item.game}{item.custom ? " (Custom)" : ""}
                </option>
              ))}
            </select>
            <button className="skl-btn ghost" id="gm-yaml-load" type="button" onClick={() => loadYaml()}>Load</button>
          </div>
        </div>
        {importedName && <div className="skl-ready-status">Imported: {importedName}</div>}
        <div className="skl-gm-editor">
          <Editor
            value={yamlContent}
            onValueChange={(code) => setYamlContent(code)}
            highlight={(code) =>
              Prism.highlight(
                code,
                editorLang === "lua" ? Prism.languages.lua : Prism.languages.yaml,
                editorLang
              )
            }
            padding={16}
            textareaId="gm-yaml-editor"
            textareaClassName="skl-gm-editor-input"
            preClassName="skl-gm-editor-preview"
            className="skl-gm-code-editor"
            style={{ fontFamily: "\"JetBrains Mono\", \"Fira Code\", \"Consolas\", monospace", fontSize: 14 }}
          />
        </div>
        <div className="skl-gm-editor-actions">
          <button className="skl-btn primary" id="gm-yaml-save" type="button" onClick={() => setCustomModalOpen(true)}>
            Save Custom YAML
          </button>
          <span id="gm-yaml-status" className="skl-ready-status">{yamlStatus}</span>
        </div>
      </div>

      <div className={`skl-modal${customModalOpen ? " open" : ""}`} id="yaml-custom-modal" aria-hidden={!customModalOpen}>
        <div className="skl-modal-backdrop" data-custom-close onClick={() => setCustomModalOpen(false)}></div>
        <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="yaml-custom-title">
          <div className="skl-modal-header">
            <h3 id="yaml-custom-title">Save Custom YAML</h3>
            <button className="skl-modal-close" type="button" data-custom-close onClick={() => setCustomModalOpen(false)}>Close</button>
          </div>
          <div className="skl-modal-body">
            <p>Saving this YAML marks it as <strong>Custom</strong>.</p>
            <p>Custom YAMLs can cause generation errors. Use at your own risk.</p>
          </div>
          <div className="skl-modal-actions">
            <button className="skl-btn ghost" type="button" data-custom-close onClick={() => setCustomModalOpen(false)}>Cancel</button>
            <button className="skl-btn primary" type="button" id="gm-yaml-confirm" onClick={() => { setCustomModalOpen(false); saveYaml(); }}>
              Save Custom
            </button>
          </div>
        </div>
      </div>
    </ChamferedPanel>
    </div>
  );
};

export default GameManagerPage;
