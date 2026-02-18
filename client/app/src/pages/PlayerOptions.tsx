import React, { useCallback, useEffect, useMemo, useState } from "react";
import { useNavigate, useParams } from "react-router-dom";
import yaml from "js-yaml";
import { apiFetch, apiJson } from "../services/api";
import { useI18n } from "../i18n";
import { getSimpleOptionSet, makeWorldOptionTranslator } from "./playerOptionsWorldI18n";

type OptionType =
  | "toggle"
  | "choice"
  | "text_choice"
  | "range"
  | "named_range"
  | "free_text"
  | "option_counter"
  | "option_list"
  | "option_set"
  | "item_set"
  | "location_set"
  | "manual";

interface OptionChoice {
  value: string;
  label: string;
}

interface OptionMeta {
  name: string;
  display_name: string;
  doc: string;
  rich_text: boolean;
  default: unknown;
  default_value?: string | number;
  default_is_random?: boolean;
  type: OptionType;
  allow_random?: boolean;
  choices?: OptionChoice[];
  range_start?: number;
  range_end?: number;
  special_range_names?: Record<string, number>;
  valid_keys?: string[];
  verify_item_name?: boolean;
  verify_location_name?: boolean;
}

interface OptionGroup {
  name: string;
  start_collapsed: boolean;
  options: OptionMeta[];
}

interface PlayerOptionsMeta {
  game: string;
  display_name: string;
  rich_text_options_doc: boolean;
  item_names: string[];
  location_names: string[];
  item_name_groups: Record<string, string[]>;
  location_name_groups: Record<string, string[]>;
  groups: OptionGroup[];
  presets: Record<string, Record<string, any>>;
}

type PlayerOptionsMode = "create" | "edit" | "import";

const LOCAL_STORAGE_PREFIX = "skl-player-options:";

const MultiSelector: React.FC<{
  option: OptionMeta;
  meta: PlayerOptionsMeta;
  selected: Set<string>;
  onToggle: (value: string, checked: boolean) => void;
  labels: {
    searchItems: string;
    searchLocations: string;
    showMore: string;
  };
}> = ({ option, meta, selected, onToggle, labels }) => {
  const [filter, setFilter] = useState("");
  const [limit, setLimit] = useState(500);
  const normalized = filter.toLowerCase();
  const allNames =
    option.valid_keys && option.valid_keys.length
      ? option.valid_keys
      : option.type === "item_set"
      ? meta.item_names
      : meta.location_names;
  const groups =
    option.type === "item_set" ? meta.item_name_groups : meta.location_name_groups;

  const groupNames = Object.keys(groups).filter(
    (name) => name !== "Everything" && name !== "Everywhere"
  );
  const checkedGroups = groupNames.filter((name) => selected.has(name));
  const uncheckedGroups = groupNames.filter(
    (name) => !selected.has(name) && name.toLowerCase().includes(normalized)
  );
  const matchedItems = allNames.filter((name) =>
    name.toLowerCase().includes(normalized)
  );
  const checkedItems = Array.from(selected).filter((name) =>
    allNames.includes(name)
  );
  const uncheckedItems = matchedItems.filter((name) => !selected.has(name));

  const renderEntry = (name: string, isGroup = false) => (
    <div className={`option-entry${isGroup ? " group-entry" : ""}`} key={name}>
      <input
        type="checkbox"
        id={`${option.name}-${name}`}
        checked={selected.has(name)}
        onChange={(event) => onToggle(name, event.target.checked)}
      />
      <label htmlFor={`${option.name}-${name}`}>{name}</label>
    </div>
  );

  return (
    <div
      className={`option-container multi-selector${
        option.type === "item_set" ? " item-selector" : ""
      }`}
    >
      <input
        type="text"
        className="multi-search"
        placeholder={option.type === "item_set" ? labels.searchItems : labels.searchLocations}
        value={filter}
        onChange={(event) => {
          setFilter(event.target.value);
          setLimit(500);
        }}
      />
      <div className="multi-list">
        {checkedGroups.sort().map((name) => renderEntry(name, true))}
        {checkedItems.sort().map((name) => renderEntry(name))}
        {(uncheckedGroups.length || uncheckedItems.length) && (
          <div className="option-divider">&nbsp;</div>
        )}
        {uncheckedGroups.sort().map((name) => renderEntry(name, true))}
        {uncheckedGroups.length && uncheckedItems.length ? (
          <div className="option-divider">&nbsp;</div>
        ) : null}
        {uncheckedItems
          .sort()
          .slice(0, limit)
          .map((name) => renderEntry(name))}
        {uncheckedItems.length > limit && (
          <div className="option-more">
            <a
              href="#"
              className="load-more"
              onClick={(event) => {
                event.preventDefault();
                setLimit((prev) => prev + 500);
              }}
            >
              {labels.showMore}
            </a>
          </div>
        )}
      </div>
    </div>
  );
};

const MultiCounter: React.FC<{
  option: OptionMeta;
  meta: PlayerOptionsMeta;
  values: Record<string, number>;
  onChange: (key: string, value: number) => void;
  labels: {
    search: string;
    showMore: string;
  };
}> = ({ option, meta, values, onChange, labels }) => {
  const [filter, setFilter] = useState("");
  const [limit, setLimit] = useState(500);
  const normalized = filter.toLowerCase();
  const allNames =
    option.valid_keys && option.valid_keys.length
      ? option.valid_keys
      : option.verify_item_name
      ? meta.item_names
      : meta.location_names;
  const selected = allNames.filter((name) => (values[name] || 0) > 0);
  const unselected = allNames.filter(
    (name) => (values[name] || 0) === 0 && name.toLowerCase().includes(normalized)
  );

  const renderEntry = (name: string) => (
    <div
      className={`option-entry${(values[name] || 0) > 0 ? " selected-entry" : ""}`}
      key={name}
    >
      <input
        type="number"
        id={`${option.name}-${name}-qty`}
        value={values[name] || 0}
        onChange={(event) => onChange(name, Number(event.target.value))}
      />
      <label htmlFor={`${option.name}-${name}-qty`}>{name}</label>
    </div>
  );

  return (
    <div className="option-container multi-counter">
      <input
        type="text"
        className="multi-search"
        placeholder={labels.search}
        value={filter}
        onChange={(event) => {
          setFilter(event.target.value);
          setLimit(500);
        }}
      />
      <div className="multi-list">
        {selected.sort().map((name) => renderEntry(name))}
        {selected.length && unselected.length ? (
          <div className="option-divider">&nbsp;</div>
        ) : null}
        {unselected
          .sort()
          .slice(0, limit)
          .map((name) => renderEntry(name))}
        {unselected.length > limit && (
          <div className="option-more">
            <a
              href="#"
              className="load-more"
              onClick={(event) => {
                event.preventDefault();
                setLimit((prev) => prev + 500);
              }}
            >
              {labels.showMore}
            </a>
          </div>
        )}
      </div>
    </div>
  );
};

const PlayerOptionsPage: React.FC<{ mode: PlayerOptionsMode }> = ({ mode }) => {
  const { game: gameParam, yamlId, importId } = useParams<{
    game?: string;
    yamlId?: string;
    importId?: string;
  }>();
  const navigate = useNavigate();
  const { locale, t } = useI18n();
  const [meta, setMeta] = useState<PlayerOptionsMeta | null>(null);
  const [loading, setLoading] = useState(true);
  const [status, setStatus] = useState("");
  const [toast, setToast] = useState<{ text: string; kind: "success" | "error" } | null>(null);
  const [playerName, setPlayerName] = useState("");
  const [yamlTitle, setYamlTitle] = useState("");
  const [presetName, setPresetName] = useState("default");
  const [values, setValues] = useState<Record<string, any>>({});
  const [randomized, setRandomized] = useState<Record<string, boolean>>({});
  const [sets, setSets] = useState<Record<string, Set<string>>>({});
  const [counters, setCounters] = useState<Record<string, Record<string, number>>>({});
  const [textCustom, setTextCustom] = useState<Record<string, string>>({});
  const [collapsed, setCollapsed] = useState<Record<string, boolean>>({});
  const [uiMode, setUiMode] = useState<"simple" | "advanced">("simple");
  const [showExpertInGroup, setShowExpertInGroup] = useState<Record<string, boolean>>({});

  const game = meta?.game || gameParam || "";
  const storageKey = game ? `${LOCAL_STORAGE_PREFIX}${game}` : "";

  useEffect(() => {
    if (!toast) return;
    const timer = window.setTimeout(() => setToast(null), 3500);
    return () => window.clearTimeout(timer);
  }, [toast]);

  const showToast = (text: string, kind: "success" | "error" = "success") => {
    setToast({ text, kind });
  };
  const groupColumns = useMemo(() => {
    if (!meta) {
      return { left: [] as OptionGroup[], right: [] as OptionGroup[] };
    }

    const left: OptionGroup[] = [];
    const right: OptionGroup[] = [];
    let leftWeight = 0;
    let rightWeight = 0;

    meta.groups.forEach((group) => {
      const weight = group.options.length + 1;
      if (leftWeight <= rightWeight) {
        left.push(group);
        leftWeight += weight;
      } else {
        right.push(group);
        rightWeight += weight;
      }
    });

    return { left, right };
  }, [meta]);

  const initFromMeta = useCallback(
    (payload: PlayerOptionsMeta) => {
      const initialValues: Record<string, any> = {};
      const initialRandom: Record<string, boolean> = {};
      const initialSets: Record<string, Set<string>> = {};
      const initialCounters: Record<string, Record<string, number>> = {};
      const initialTextCustom: Record<string, string> = {};
      const initialCollapsed: Record<string, boolean> = {};

      payload.groups.forEach((group) => {
        initialCollapsed[group.name] = group.start_collapsed;
        group.options.forEach((option) => {
          if (option.type === "option_set" || option.type === "option_list" || option.type === "item_set" || option.type === "location_set") {
            const defaults = Array.isArray(option.default) ? option.default : [];
            initialSets[option.name] = new Set(defaults as string[]);
            return;
          }

          if (option.type === "option_counter") {
            const defaultMap: Record<string, number> = {};
            const rawDefaults = option.default && typeof option.default === "object" ? (option.default as Record<string, any>) : {};
            Object.entries(rawDefaults).forEach(([key, val]) => {
              const parsed = Number(val);
              defaultMap[key] = Number.isNaN(parsed) ? 0 : parsed;
            });
            initialCounters[option.name] = defaultMap;
            return;
          }

          if (option.allow_random && option.default_is_random) {
            initialRandom[option.name] = true;
          }

          if (option.type === "range" || option.type === "named_range") {
            initialValues[option.name] = option.default_value ?? option.range_start ?? 0;
            return;
          }

          if (option.type === "toggle" || option.type === "choice" || option.type === "text_choice") {
            const defaultChoice = option.default_value ?? option.choices?.[0]?.value ?? "";
            initialValues[option.name] = defaultChoice;
            return;
          }

          if (option.type === "free_text") {
            initialValues[option.name] = option.default ?? "";
            return;
          }

          initialValues[option.name] = option.default ?? "";
        });
      });

      setValues(initialValues);
      setRandomized(initialRandom);
      setSets(initialSets);
      setCounters(initialCounters);
      setTextCustom(initialTextCustom);
      setCollapsed(initialCollapsed);
    },
    []
  );

  const loadLocalStorage = useCallback(
    (payload: PlayerOptionsMeta) => {
      if (!storageKey) return false;
      try {
        const stored = window.localStorage.getItem(storageKey);
        if (!stored) return false;
        const parsed = JSON.parse(stored);
        if (!parsed) return false;
        setPlayerName(parsed.playerName || "");
        setYamlTitle(parsed.yamlTitle || "");
        setPresetName(parsed.presetName || "custom");
        setValues(parsed.values || {});
        setRandomized(parsed.randomized || {});
        setTextCustom(parsed.textCustom || {});
        const storedSets: Record<string, string[]> = parsed.sets || {};
        const nextSets: Record<string, Set<string>> = {};
        Object.entries(storedSets).forEach(([key, list]) => {
          nextSets[key] = new Set(list || []);
        });
        setSets(nextSets);
        setCounters(parsed.counters || {});
        if (parsed.collapsed) {
          setCollapsed(parsed.collapsed);
        } else {
          const defaults: Record<string, boolean> = {};
          payload.groups.forEach((group) => {
            defaults[group.name] = group.start_collapsed;
          });
          setCollapsed(defaults);
        }
        return true;
      } catch {
        return false;
      }
    },
    [storageKey]
  );

  const persistLocalStorage = useCallback(() => {
    if (!storageKey) return;
    try {
      const setsPayload: Record<string, string[]> = {};
      Object.entries(sets).forEach(([key, setValue]) => {
        setsPayload[key] = Array.from(setValue);
      });
      window.localStorage.setItem(
        storageKey,
        JSON.stringify({
          playerName,
          yamlTitle,
          presetName,
          values,
          randomized,
          textCustom,
          sets: setsPayload,
          counters,
          collapsed
        })
      );
    } catch {
      // ignore
    }
  }, [storageKey, playerName, yamlTitle, presetName, values, randomized, textCustom, sets, counters, collapsed]);

  const applyYaml = useCallback(
    (yamlText: string, payload: PlayerOptionsMeta) => {
      let parsed: any;
      try {
        parsed = yaml.load(yamlText);
      } catch {
        setStatus(t("po.yaml_parse_failed"));
        return;
      }
      if (!parsed || typeof parsed !== "object") {
        setStatus(t("po.yaml_invalid_format"));
        return;
      }
      const yamlGame = parsed.game || payload.game;
      const yamlOptions = parsed[yamlGame] || {};
      const nextValues: Record<string, any> = {};
      const nextRandom: Record<string, boolean> = {};
      const nextSets: Record<string, Set<string>> = {};
      const nextCounters: Record<string, Record<string, number>> = {};
      const nextTextCustom: Record<string, string> = {};

      payload.groups.forEach((group) => {
        group.options.forEach((option) => {
          if (option.type === "option_set" || option.type === "option_list" || option.type === "item_set" || option.type === "location_set") {
            const list = yamlOptions[option.name];
            if (Array.isArray(list)) {
              nextSets[option.name] = new Set(list);
            }
            return;
          }

          if (option.type === "option_counter") {
            const dictVal = yamlOptions[option.name];
            if (dictVal && typeof dictVal === "object") {
              const map: Record<string, number> = {};
              Object.entries(dictVal).forEach(([key, val]) => {
                const parsedVal = Number(val);
                map[key] = Number.isNaN(parsedVal) ? 0 : parsedVal;
              });
              nextCounters[option.name] = map;
            }
            return;
          }

          const value = yamlOptions[option.name];
          if (value === undefined) return;
          if (value === "random") {
            nextRandom[option.name] = true;
            return;
          }

          if (option.type === "text_choice") {
            const choices = option.choices?.map((choice) => choice.value) || [];
            const stringValue = String(value);
            if (choices.includes(stringValue)) {
              nextValues[option.name] = stringValue;
            } else {
              nextValues[option.name] = "custom";
              nextTextCustom[option.name] = stringValue;
            }
            return;
          }

          if (option.type === "toggle" || option.type === "choice") {
            if (typeof value === "boolean") {
              nextValues[option.name] = value ? "true" : "false";
              return;
            }
            nextValues[option.name] = String(value);
            return;
          }

          if (option.type === "range" || option.type === "named_range") {
            const numValue = Number(value);
            nextValues[option.name] = Number.isNaN(numValue) ? value : numValue;
            return;
          }

          nextValues[option.name] = value;
        });
      });

      setPlayerName(parsed.name || "");
      setYamlTitle((parsed.sekailink && parsed.sekailink.title) || "");
      setPresetName("custom");
      setValues((prev) => ({ ...prev, ...nextValues }));
      setRandomized((prev) => ({ ...prev, ...nextRandom }));
      setSets((prev) => ({ ...prev, ...nextSets }));
      setCounters((prev) => ({ ...prev, ...nextCounters }));
      setTextCustom((prev) => ({ ...prev, ...nextTextCustom }));
    },
    [t]
  );

  const loadMeta = useCallback(async (targetGame: string, yamlText?: string) => {
    setLoading(true);
    try {
      const data = await apiJson<PlayerOptionsMeta>(
        `/api/player-options/${encodeURIComponent(targetGame)}?lang=${encodeURIComponent(locale)}`
      );
      setMeta(data);
      initFromMeta(data);
      if (mode === "create" && !yamlText) {
        loadLocalStorage(data);
      }
      if (yamlText) {
        applyYaml(yamlText, data);
      }
      setLoading(false);
    } catch (err) {
      setStatus(err instanceof Error ? err.message : t("po.load_failed"));
      setLoading(false);
    }
  }, [applyYaml, initFromMeta, loadLocalStorage, locale, mode]);

  useEffect(() => {
    const run = async () => {
      if (mode === "import" && importId) {
        try {
          const data = await apiJson<{ content: string; game: string; yaml_title: string }>(`/api/yamls/import/${encodeURIComponent(importId)}`);
          if (!data.game) {
            setStatus(t("po.import_missing_game"));
            setLoading(false);
            return;
          }
          setYamlTitle(data.yaml_title || "");
          await loadMeta(data.game, data.content || "");
        } catch (err) {
          setStatus(err instanceof Error ? err.message : t("po.import_load_failed"));
          setLoading(false);
        }
        return;
      }

      if (!gameParam) {
        setStatus(t("po.missing_game"));
        setLoading(false);
        return;
      }

      if (mode === "edit" && yamlId) {
        try {
          const res = await apiFetch(`/api/yamls/${encodeURIComponent(yamlId)}/raw`);
          if (!res.ok) throw new Error(t("po.yaml_load_failed"));
          const data = await res.json();
          await loadMeta(gameParam, data.yaml || "");
          return;
        } catch (err) {
          setStatus(err instanceof Error ? err.message : t("po.yaml_load_failed"));
          setLoading(false);
          return;
        }
      }

      await loadMeta(gameParam);
    };
    run();
  }, [gameParam, importId, loadMeta, mode, t, yamlId]);

  useEffect(() => {
    if (!meta) return;
    persistLocalStorage();
  }, [meta, persistLocalStorage]);

  const optionLookup = useMemo(() => {
    const map: Record<string, OptionMeta> = {};
    meta?.groups.forEach((group) => {
      group.options.forEach((option) => {
        map[option.name] = option;
      });
    });
    return map;
  }, [meta]);

  const worldTranslator = useMemo(
    () => makeWorldOptionTranslator(locale, game),
    [game, locale]
  );

  const simpleOptionSet = useMemo(() => {
    if (!meta) return new Set<string>();
    const optionNames: string[] = [];
    meta.groups.forEach((group) => {
      group.options.forEach((opt) => optionNames.push(opt.name));
    });
    return getSimpleOptionSet(meta.game, optionNames);
  }, [meta]);

  const handlePresetChange = (value: string) => {
    if (!meta) return;
    if (value === "custom") {
      setPresetName("custom");
      return;
    }
    if (value === "default") {
      initFromMeta(meta);
      setPresetName("default");
      return;
    }
    const preset = meta.presets[value];
    if (!preset) return;

    const nextValues: Record<string, any> = {};
    const nextRandom: Record<string, boolean> = {};
    const nextSets: Record<string, Set<string>> = {};
    const nextCounters: Record<string, Record<string, number>> = {};

    Object.entries(preset).forEach(([optionName, optionValue]) => {
      const option = optionLookup[optionName];
      if (!option) return;
      if (optionValue === "random") {
        nextRandom[optionName] = true;
        return;
      }
      if (Array.isArray(optionValue)) {
        nextSets[optionName] = new Set(optionValue);
        return;
      }
      if (optionValue && typeof optionValue === "object") {
        const map: Record<string, number> = {};
        Object.entries(optionValue as Record<string, any>).forEach(([key, val]) => {
          const parsed = Number(val);
          map[key] = Number.isNaN(parsed) ? 0 : parsed;
        });
        nextCounters[optionName] = map;
        return;
      }
      nextValues[optionName] = optionValue;
    });

    setValues((prev) => ({ ...prev, ...nextValues }));
    setRandomized((prev) => ({ ...prev, ...nextRandom }));
    setSets((prev) => ({ ...prev, ...nextSets }));
    setCounters((prev) => ({ ...prev, ...nextCounters }));
    setPresetName(value);
  };

  const onOptionChange = () => {
    if (presetName !== "custom") setPresetName("custom");
  };

  const updateValue = (name: string, value: any) => {
    setValues((prev) => ({ ...prev, [name]: value }));
    onOptionChange();
  };

  const updateRandom = (name: string, value: boolean) => {
    setRandomized((prev) => ({ ...prev, [name]: value }));
    onOptionChange();
  };

  const updateSet = (name: string, value: string, checked: boolean) => {
    setSets((prev) => {
      const next = new Set(prev[name] || []);
      if (checked) next.add(value);
      else next.delete(value);
      return { ...prev, [name]: next };
    });
    onOptionChange();
  };

  const updateCounter = (name: string, key: string, value: number) => {
    setCounters((prev) => {
      const next = { ...(prev[name] || {}) };
      next[key] = value;
      return { ...prev, [name]: next };
    });
    onOptionChange();
  };

  const updateTextCustom = (name: string, value: string) => {
    setTextCustom((prev) => ({ ...prev, [name]: value }));
    if (value) {
      setValues((prev) => ({ ...prev, [name]: "custom" }));
    }
    onOptionChange();
  };

  const buildOptionsPayload = () => {
    if (!meta) return {};
    const payload: Record<string, any> = {};
    meta.groups.forEach((group) => {
      group.options.forEach((option) => {
        if (option.type === "manual") return;
        if (randomized[option.name]) {
          payload[option.name] = "random";
          return;
        }
        if (option.type === "option_set" || option.type === "option_list" || option.type === "item_set" || option.type === "location_set") {
          payload[option.name] = Array.from(sets[option.name] || []);
          return;
        }
        if (option.type === "option_counter") {
          payload[option.name] = counters[option.name] || {};
          return;
        }
        if (option.type === "text_choice") {
          if (textCustom[option.name]) {
            payload[option.name] = textCustom[option.name];
          } else {
            payload[option.name] = values[option.name];
          }
          return;
        }
        payload[option.name] = values[option.name];
      });
    });
    return payload;
  };

  const saveYaml = async () => {
    if (!meta) return;
    setStatus("");
    const trimmedPlayer = playerName.trim();
    const trimmedTitle = yamlTitle.trim();
    if (!trimmedPlayer) {
      showToast(t("po.player_name_required"), "error");
      return;
    }
    if (!/^[A-Za-z0-9_-]+$/.test(trimmedPlayer)) {
      showToast(t("po.player_name_invalid"), "error");
      return;
    }
    if (!trimmedTitle) {
      showToast(t("po.yaml_title_required"), "error");
      return;
    }
    try {
      const res = await apiFetch(`/api/player-options/${encodeURIComponent(meta.game)}/dashboard-save`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          player_name: trimmedPlayer,
          yaml_title: trimmedTitle,
          yaml_id: yamlId || undefined,
          locale,
          preset_name: presetName || "custom",
          options: buildOptionsPayload()
        })
      });
      if (!res.ok) {
        const data = await res.json().catch(() => ({}));
        throw new Error(data.error || t("po.save_failed"));
      }
      const data = await res.json().catch(() => ({}));
      if (data?.yaml?.id && mode !== "edit") {
        navigate(`/player-options/${encodeURIComponent(meta.game)}/edit/${encodeURIComponent(data.yaml.id)}`);
      }
      showToast(t("po.save_success"), "success");
    } catch (err) {
      showToast(err instanceof Error ? err.message : t("po.save_failed"), "error");
    }
  };

  const renderTooltip = (option: OptionMeta) => {
    const text = worldTranslator.doc(option.name, option.doc || t("po.no_description"));
    return (
      <span className="interactive tooltip-container" data-tooltip={text}>
        (?)
      </span>
    );
  };

  const renderOptionField = (option: OptionMeta) => {
    if (!meta) return null;

    if (option.type === "manual") {
      return (
        <div className="manual-container">
          <div className="manual-config-message">{t("po.manual_option_notice")}</div>
        </div>
      );
    }

    const isRandom = randomized[option.name];
    const randomCheckbox = option.allow_random ? (
      <div className="randomize-button" data-tooltip={t("po.randomize_option_tip")}>
        <label htmlFor={`random-${option.name}`}>
          <input
            type="checkbox"
            id={`random-${option.name}`}
            checked={Boolean(isRandom)}
            onChange={(event) => updateRandom(option.name, event.target.checked)}
          />
          🎲
        </label>
      </div>
    ) : null;

    if (option.type === "toggle" || option.type === "choice") {
      return (
        <div className="select-container">
          <select
            id={option.name}
            value={String(values[option.name] ?? "")}
            disabled={isRandom}
            onChange={(event) => updateValue(option.name, event.target.value)}
          >
            {option.choices?.map((choice) => (
              <option key={choice.value} value={choice.value}>
                {worldTranslator.choice(option.name, choice.value, choice.label)}
              </option>
            ))}
          </select>
          {randomCheckbox}
        </div>
      );
    }

    if (option.type === "text_choice") {
      return (
        <div className="text-choice-container">
          <div className="text-choice-wrapper">
            <select
              id={option.name}
              value={String(values[option.name] ?? "")}
              disabled={isRandom}
              onChange={(event) => {
                updateValue(option.name, event.target.value);
                if (event.target.value !== "custom") {
                  updateTextCustom(option.name, "");
                }
              }}
            >
              {option.choices?.map((choice) => (
                <option key={choice.value} value={choice.value}>
                  {worldTranslator.choice(option.name, choice.value, choice.label)}
                </option>
              ))}
              <option value="custom" hidden>
                {t("po.custom")}
              </option>
            </select>
            {randomCheckbox}
          </div>
          <input
            type="text"
            id={`${option.name}-custom`}
            placeholder={t("legacy.term.custom_value")}
            disabled={isRandom}
            value={textCustom[option.name] || ""}
            onChange={(event) => updateTextCustom(option.name, event.target.value)}
          />
        </div>
      );
    }

    if (option.type === "range") {
      const min = option.range_start ?? 0;
      const max = option.range_end ?? 0;
      const value = Number(values[option.name] ?? min);
      return (
        <div className="range-container">
          <input
            type="range"
            id={option.name}
            min={min}
            max={max}
            value={value}
            disabled={isRandom}
            onChange={(event) => updateValue(option.name, Number(event.target.value))}
          />
          <span id={`${option.name}-value`} className="range-value">
            {value}
          </span>
          {randomCheckbox}
        </div>
      );
    }

    if (option.type === "named_range") {
      const min = option.range_start ?? 0;
      const max = option.range_end ?? 0;
      const value = Number(values[option.name] ?? min);
      const specialNames = option.special_range_names || {};
      const matched = Object.entries(specialNames).find(([, v]) => v === value);
      const selectValue = matched ? matched[1] : "custom";
      return (
        <div className="named-range-container">
          <select
            id={`${option.name}-select`}
            data-option-name={option.name}
            value={selectValue}
            disabled={isRandom}
            onChange={(event) => {
              if (event.target.value === "custom") return;
              updateValue(option.name, Number(event.target.value));
            }}
          >
            {Object.entries(specialNames).map(([key, val]) => (
              <option key={key} value={val}>
                {`${key.replace(/_/g, " ").replace(/\b\w/g, (c) => c.toUpperCase())} (${val})`}
              </option>
            ))}
            <option value="custom" hidden>
              {t("po.custom")}
            </option>
          </select>
          <div className="named-range-wrapper">
            <input
              type="range"
              id={option.name}
              min={min}
              max={max}
              value={value}
              disabled={isRandom}
              onChange={(event) => updateValue(option.name, Number(event.target.value))}
            />
            <span id={`${option.name}-value`} className="range-value">
              {value}
            </span>
            {randomCheckbox}
          </div>
        </div>
      );
    }

    if (option.type === "free_text") {
      return (
        <div className="free-text-container">
          <input type="text" id={option.name} value={values[option.name] ?? ""} onChange={(event) => updateValue(option.name, event.target.value)} />
        </div>
      );
    }

    if (option.type === "option_list" || option.type === "option_set") {
      const keys = option.valid_keys || [];
      const selected = sets[option.name] || new Set<string>();
      return (
        <div className="option-container">
          <input type="hidden" value="_ensure-empty-list" />
          {keys.map((key) => (
            <div className="option-entry" key={key}>
              <input
                type="checkbox"
                id={`${option.name}-${key}`}
                checked={selected.has(key)}
                onChange={(event) => updateSet(option.name, key, event.target.checked)}
              />
              <label htmlFor={`${option.name}-${key}`}>{key}</label>
            </div>
          ))}
        </div>
      );
    }

    if (option.type === "item_set" || option.type === "location_set") {
      const selected = sets[option.name] || new Set<string>();
      return (
        <MultiSelector
          option={option}
          meta={meta}
          selected={selected}
          onToggle={(value, checked) => updateSet(option.name, value, checked)}
          labels={{
            searchItems: t("po.search_items"),
            searchLocations: t("po.search_locations"),
            showMore: t("po.show_more_500")
          }}
        />
      );
    }

    if (option.type === "option_counter") {
      const current = counters[option.name] || {};
      return (
        <MultiCounter
          option={option}
          meta={meta}
          values={current}
          onChange={(key, value) => updateCounter(option.name, key, value)}
          labels={{
            search: t("legacy.term.search_ellipsis"),
            showMore: t("po.show_more_500")
          }}
        />
      );
    }

    return null;
  };

  if (loading) {
    return (
      <div className="skl-app-panel skl-player-options-loading">
        <div className="skl-loading-block">
          <div className="skl-loading-title">{t("po.loading")}</div>
          <div className="skl-loading-bars">
            <span></span>
            <span></span>
            <span></span>
            <span></span>
          </div>
        </div>
      </div>
    );
  }

  if (!meta) {
    return <div className="skl-app-panel">{t("po.load_failed")}</div>;
  }

  return (
    <div className="skl-player-options-page">
      <div id="player-options" className="skl-app-panel">
        <div id="user-message" className={status ? "show" : ""}>
          {status}
        </div>

        <div id="player-options-header">
          <h1 className="po-title-left">{t("po.title")}</h1>
          <h1 className="po-title-right">{meta.display_name}</h1>
        </div>
        <p>{t("po.subtitle")}</p>

        <div className="po-mode-bar" role="tablist" aria-label={t("po.mode_label")}>
          <button
            type="button"
            className={`po-mode-btn${uiMode === "simple" ? " is-active" : ""}`}
            onClick={() => setUiMode("simple")}
          >
            {t("po.mode_simple")}
          </button>
          <button
            type="button"
            className={`po-mode-btn${uiMode === "advanced" ? " is-active" : ""}`}
            onClick={() => setUiMode("advanced")}
          >
            {t("po.mode_advanced")}
          </button>
        </div>

        <form
          id="options-form"
          onSubmit={(event) => {
            event.preventDefault();
            saveYaml();
          }}
        >
          <div id="meta-options">
            <div>
              <label htmlFor="player-name">
                {t("po.player_name")}: <span className="interactive tooltip-container" data-tooltip={t("po.player_name_tip")}>(?)</span>
              </label>
              <input id="player-name" placeholder={t("legacy.term.player")} maxLength={16} value={playerName} onChange={(event) => setPlayerName(event.target.value)} />
            </div>
            <div>
              <label htmlFor="yaml-name">
                {t("po.yaml_title")}: <span className="interactive tooltip-container" data-tooltip={t("po.yaml_title_tip")}>(?)</span>
              </label>
              <input id="yaml-name" placeholder={t("legacy.term.my_yaml")} maxLength={48} value={yamlTitle} onChange={(event) => setYamlTitle(event.target.value)} />
            </div>
            <div>
              <label htmlFor="game-options-preset">{t("po.option_preset")}:</label>
              <select
                id="game-options-preset"
                value={presetName}
                onChange={(event) => handlePresetChange(event.target.value)}
              >
                <option value="default">{t("po.default")}</option>
                {Object.keys(meta.presets).map((preset) => (
                  <option key={preset} value={preset}>
                    {preset.replace(/_/g, " ")}
                  </option>
                ))}
                <option value="custom">{t("po.custom")}</option>
              </select>
            </div>
          </div>

          <div className="game-options">
            {[groupColumns.left, groupColumns.right].map((column, columnIndex) => (
              <div className="game-options-column" key={`column-${columnIndex}`}>
                {column.map((group) => (
                  <div className="options-group" key={group.name}>
                    <div
                      className={`group-header${collapsed[group.name] ? " collapsed" : ""}`}
                      onClick={() => setCollapsed((prev) => ({ ...prev, [group.name]: !prev[group.name] }))}
                    >
                      <h2>{worldTranslator.group(group.name)}</h2>
                      <span className="group-toggle">{collapsed[group.name] ? "+" : "−"}</span>
                    </div>
                    {!collapsed[group.name] && (
                      <div className="option-grid">
                        {group.options
                          .filter((option, index) => {
                            if (uiMode === "advanced") return true;
                            if (showExpertInGroup[group.name]) return true;
                            if (simpleOptionSet.has(option.name.toLowerCase())) return true;
                            return index < 2;
                          })
                          .map((option) => (
                          <div className="option-row" key={option.name}>
                            <label htmlFor={option.name}>
                              {worldTranslator.option(option.name, option.display_name)} {renderTooltip(option)}
                            </label>
                            <div className="option-field">{renderOptionField(option)}</div>
                          </div>
                        ))}
                        {uiMode === "simple" &&
                          !showExpertInGroup[group.name] &&
                          group.options.some((option, index) => !simpleOptionSet.has(option.name.toLowerCase()) && index >= 2) && (
                            <div className="option-row option-row-expert-toggle">
                              <label>&nbsp;</label>
                              <div className="option-field">
                                <button
                                  type="button"
                                  className="skl-btn ghost compact"
                                  onClick={() =>
                                    setShowExpertInGroup((prev) => ({ ...prev, [group.name]: true }))
                                  }
                                >
                                  {t("po.show_expert_options")}
                                </button>
                              </div>
                            </div>
                          )}
                      </div>
                    )}
                  </div>
                ))}
              </div>
            ))}
          </div>

          <div id="player-options-button-row">
            <button className="skl-btn primary" type="submit">
              {t("po.save_yaml")}
            </button>
            <button className="skl-btn ghost" type="button" onClick={() => navigate("/dashboard/yaml/new")}>
              {t("po.back_to_gm")}
            </button>
          </div>
        </form>
      </div>
      {toast && (
        <div className="skl-toast-stack" aria-live="polite">
          <div className={`skl-toast ${toast.kind}`}>{toast.text}</div>
        </div>
      )}
    </div>
  );
};

export default PlayerOptionsPage;
