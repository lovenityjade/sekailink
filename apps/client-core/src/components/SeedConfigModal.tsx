import React, { useEffect, useMemo, useState } from "react";
import {
  addActiveSeed,
  createAdvancedSeed,
  createEasySeed,
  deleteSeed,
  EASY_SEED_CHOICES,
  listSeedOptions,
  listSeeds,
  type EasySeedChoice,
  type SeedEntry,
  type SeedGameEntry,
  type SeedOptionDefinition,
  type SeedOptionsSchema,
} from "../services/seedConfig";
import { emitToast } from "../services/toast";

type SeedModalMode = "add" | "manage" | "select";

interface SeedConfigModalProps {
  open: boolean;
  mode: SeedModalMode;
  game: SeedGameEntry | null;
  onClose: () => void;
  onSeedSelected?: (seed: SeedEntry) => void | Promise<void>;
  onSeedsChanged?: () => void;
}

type AdvancedGroup = {
  key: string;
  label: string;
  options: SeedOptionDefinition[];
};

type AdvancedOptionControl =
  | "toggle"
  | "choice"
  | "text_choice"
  | "range"
  | "named_range"
  | "counter"
  | "dict"
  | "list"
  | "object"
  | "text"
  | string;

const modeTitle: Record<SeedModalMode, string> = {
  add: "Add a Seed",
  manage: "Manage Seeds",
  select: "Select Seed",
};

const defaultPlayerName = () => {
  try {
    const cached = JSON.parse(window.localStorage.getItem("sekailink_user") || "{}");
    return String(cached?.global_name || cached?.username || "Player");
  } catch {
    return "Player";
  }
};

const cloneRecord = (value: unknown): Record<string, unknown> => {
  if (!value || typeof value !== "object" || Array.isArray(value)) return {};
  try {
    return JSON.parse(JSON.stringify(value));
  } catch {
    return {};
  }
};

const optionControl = (option: SeedOptionDefinition) => {
  const control = String(option.validation_rules?.control || "").trim();
  if (control) return control;
  if (option.type === "boolean") return "toggle";
  if (option.type === "enum") return option.choices?.length ? "choice" : "text";
  if (option.type === "integer" || option.type === "number") return "range";
  if (option.type === "array") return "list";
  if (option.type === "object") return "object";
  return "text";
};

const defaultValueForOption = (option: SeedOptionDefinition): unknown => {
  if (option.default_value !== undefined && option.default_value !== null) return option.default_value;
  if (option.type === "boolean") return false;
  if (option.type === "enum") return option.choices?.[0]?.yaml_value || "";
  if (option.type === "integer" || option.type === "number") return option.validation_rules?.range_start ?? 0;
  if (option.type === "array") return [];
  if (option.type === "object") return {};
  return "";
};

const defaultsFromOptions = (options: SeedOptionDefinition[]) =>
  options.reduce<Record<string, unknown>>((values, option) => {
    values[option.option_key] = defaultValueForOption(option);
    return values;
  }, {});

const isPlandOption = (option: SeedOptionDefinition) => {
  const text = `${option.option_key} ${option.yaml_key || ""} ${option.label}`.toLowerCase();
  return text.includes("plando");
};

const normalizeAdvancedValue = (option: SeedOptionDefinition, raw: unknown): unknown => {
  const control = optionControl(option) as AdvancedOptionControl;
  const value = raw ?? defaultValueForOption(option);
  if (control === "toggle") return Boolean(value);
  if (control === "range" || control === "named_range" || option.type === "integer" || option.type === "number") {
    const fallback = Number(option.validation_rules?.range_start ?? 0);
    const parsed = option.type === "number" ? Number(value) : Number.parseInt(String(value ?? fallback), 10);
    return Number.isFinite(parsed) ? parsed : fallback;
  }
  if (option.type === "array") return Array.isArray(value) ? value : [];
  if (option.type === "object") return value && typeof value === "object" && !Array.isArray(value) ? value : {};
  return value;
};

const sanitizeAdvancedValues = (schema: SeedOptionsSchema, values: Record<string, unknown>) =>
  schema.options.reduce<Record<string, unknown>>((payload, option) => {
    if (isPlandOption(option)) return payload;
    const value = normalizeAdvancedValue(option, values[option.option_key]);
    const control = optionControl(option) as AdvancedOptionControl;
    if ((control === "text" || control === "text_choice") && typeof value === "string" && value.trim() === "") {
      return payload;
    }
    if (option.type === "array" && Array.isArray(value) && value.length === 0 && !option.required) {
      return payload;
    }
    payload[option.option_key] = value;
    return payload;
  }, {});

const groupAdvancedOptions = (schema: SeedOptionsSchema | null): AdvancedGroup[] => {
  const groups = new Map<string, AdvancedGroup>();
  for (const option of schema?.options || []) {
    if (isPlandOption(option)) continue;
    const key = String(option.validation_rules?.group_key || "game_options");
    const label = String(option.validation_rules?.group_label || "Game Options");
    if (!groups.has(key)) groups.set(key, { key, label, options: [] });
    groups.get(key)?.options.push(option);
  }
  return [...groups.values()].filter((group) => group.options.length > 0);
};

const displayJson = (value: unknown) => {
  if (value === undefined || value === null) return "";
  try {
    return JSON.stringify(value, null, 2);
  } catch {
    return "";
  }
};

const displayCounter = (value: unknown) => {
  if (!value || typeof value !== "object" || Array.isArray(value)) return "";
  return Object.entries(value as Record<string, unknown>)
    .map(([key, count]) => `${key}: ${count}`)
    .join("\n");
};

const parseCounter = (text: string): Record<string, number> => {
  const out: Record<string, number> = {};
  for (const rawLine of text.split("\n")) {
    const line = rawLine.trim();
    if (!line) continue;
    const separator = line.includes(":") ? ":" : "=";
    const [rawKey, ...rawCount] = line.split(separator);
    const key = rawKey.trim();
    if (!key) continue;
    const count = Number.parseInt(rawCount.join(separator).trim() || "1", 10);
    out[key] = Number.isFinite(count) ? count : 1;
  }
  return out;
};

const SeedConfigModal: React.FC<SeedConfigModalProps> = ({
  open,
  mode,
  game,
  onClose,
  onSeedSelected,
  onSeedsChanged,
}) => {
  const [tab, setTab] = useState<"easy" | "advanced">("easy");
  const [seeds, setSeeds] = useState<SeedEntry[]>([]);
  const [selectedChoiceIds, setSelectedChoiceIds] = useState<Set<string>>(() => new Set());
  const [seedTitle, setSeedTitle] = useState("");
  const [playerName, setPlayerName] = useState(defaultPlayerName);
  const [busy, setBusy] = useState(false);
  const [status, setStatus] = useState("");
  const [deleteConfirmId, setDeleteConfirmId] = useState("");
  const [advancedSchema, setAdvancedSchema] = useState<SeedOptionsSchema | null>(null);
  const [advancedValues, setAdvancedValues] = useState<Record<string, unknown>>({});
  const [advancedLoading, setAdvancedLoading] = useState(false);
  const [advancedSource, setAdvancedSource] = useState<SeedEntry | null>(null);
  const [advancedGroupKey, setAdvancedGroupKey] = useState("");

  const selectedChoices = useMemo(
    () => EASY_SEED_CHOICES.filter((choice) => selectedChoiceIds.has(choice.id)),
    [selectedChoiceIds]
  );

  const advancedGroups = useMemo(() => groupAdvancedOptions(advancedSchema), [advancedSchema]);
  const activeAdvancedGroup = advancedGroups.find((group) => group.key === advancedGroupKey) || advancedGroups[0] || null;
  const advancedVisible = mode === "add" && tab === "advanced";

  const refreshSeeds = async () => {
    if (!game) return;
    try {
      setSeeds(await listSeeds(game));
    } catch (err) {
      setSeeds([]);
      setStatus(err instanceof Error ? err.message : "Unable to load seeds.");
    }
  };

  useEffect(() => {
    if (!open) return;
    setStatus("");
    setDeleteConfirmId("");
    setTab("easy");
    setSeedTitle("");
    setPlayerName(defaultPlayerName());
    setSelectedChoiceIds(new Set());
    setAdvancedSchema(null);
    setAdvancedValues({});
    setAdvancedSource(null);
    setAdvancedGroupKey("");
    void refreshSeeds();
  }, [open, game?.game_key, game?.display_name]);

  useEffect(() => {
    if (!open || !game || (!advancedVisible && !advancedSource)) return;
    let cancelled = false;
    setAdvancedLoading(true);
    setStatus("");
    void listSeedOptions(game)
      .then((schema) => {
        if (cancelled) return;
        const filtered = { ...schema, options: schema.options.filter((option) => !isPlandOption(option)) };
        setAdvancedSchema(filtered);
        setAdvancedGroupKey((current) => current || String(filtered.options[0]?.validation_rules?.group_key || "game_options"));
        setAdvancedValues((current) => {
          if (Object.keys(current).length > 0) return current;
          return defaultsFromOptions(filtered.options);
        });
      })
      .catch((err) => {
        if (cancelled) return;
        setAdvancedSchema(null);
        setStatus(err instanceof Error ? err.message : "Unable to load advanced seed form.");
      })
      .finally(() => {
        if (!cancelled) setAdvancedLoading(false);
      });
    return () => {
      cancelled = true;
    };
  }, [open, game?.game_key, game?.display_name, advancedVisible, advancedSource?.id]);

  if (!open || !game) return null;

  const toggleChoice = (choice: EasySeedChoice) => {
    setSelectedChoiceIds((prev) => {
      const next = new Set(prev);
      if (next.has(choice.id)) next.delete(choice.id);
      else next.add(choice.id);
      return next;
    });
  };

  const setAdvancedValue = (key: string, value: unknown) => {
    setAdvancedValues((prev) => ({ ...prev, [key]: value }));
  };

  const createSeed = async () => {
    if (!game || busy) return;
    setBusy(true);
    setStatus("");
    try {
      const seed = await createEasySeed(game, seedTitle, playerName, selectedChoices);
      addActiveSeed(seed);
      emitToast({ message: `Seed "${seed.title}" created.`, kind: "success" });
      setSeedTitle("");
      setSelectedChoiceIds(new Set());
      await refreshSeeds();
      setSeeds((current) => [seed, ...current.filter((entry) => entry.id !== seed.id)]);
      onSeedsChanged?.();
      if (mode === "add") {
        await onSeedSelected?.(seed);
      }
    } catch (err) {
      const message = err instanceof Error ? err.message : "Unable to create seed.";
      setStatus(message);
      emitToast({ message, kind: "error", durationMs: 9000 });
    } finally {
      setBusy(false);
    }
  };

  const createAdvanced = async () => {
    if (!game || busy) return;
    if (!advancedSchema) {
      const message = "Advanced seed form is not available yet. Nexus seed options must load before creating this seed.";
      setStatus(message);
      emitToast({ message, kind: "error", durationMs: 9000 });
      return;
    }
    setBusy(true);
    setStatus("");
    try {
      const fallbackTitle = advancedSource ? `${advancedSource.title} Copy` : seedTitle;
      const seed = await createAdvancedSeed(
        game,
        seedTitle || fallbackTitle,
        playerName,
        sanitizeAdvancedValues(advancedSchema, advancedValues)
      );
      addActiveSeed(seed);
      emitToast({ message: `Seed "${seed.title}" created.`, kind: "success" });
      setSeedTitle("");
      setAdvancedValues(defaultsFromOptions(advancedSchema?.options || []));
      setAdvancedSource(null);
      await refreshSeeds();
      setSeeds((current) => [seed, ...current.filter((entry) => entry.id !== seed.id)]);
      onSeedsChanged?.();
      if (mode === "add") {
        await onSeedSelected?.(seed);
      }
    } catch (err) {
      const message = err instanceof Error ? err.message : "Unable to create seed.";
      setStatus(message);
      emitToast({ message, kind: "error", durationMs: 9000 });
    } finally {
      setBusy(false);
    }
  };

  const selectSeed = async (seed: SeedEntry) => {
    addActiveSeed(seed);
    await onSeedSelected?.(seed);
    onSeedsChanged?.();
    emitToast({ message: `Seed "${seed.title}" added.`, kind: "success" });
    onClose();
  };

  const removeSeed = async (seed: SeedEntry) => {
    if (deleteConfirmId !== seed.id) {
      setDeleteConfirmId(seed.id);
      return;
    }
    setBusy(true);
    setStatus("");
    try {
      await deleteSeed(seed.id);
      emitToast({ message: `Seed "${seed.title}" deleted.`, kind: "success" });
      await refreshSeeds();
      onSeedsChanged?.();
    } catch (err) {
      const message = err instanceof Error ? err.message : "Unable to delete seed.";
      setStatus(message);
      emitToast({ message, kind: "error", durationMs: 9000 });
    } finally {
      setBusy(false);
      setDeleteConfirmId("");
    }
  };

  const startAdvancedEditor = (seed?: SeedEntry) => {
    setTab("advanced");
    setAdvancedSource(seed || null);
    setSeedTitle(seed ? `${seed.title} Copy` : "");
    setAdvancedValues(seed?.values ? cloneRecord(seed.values) : {});
    setStatus(seed && !seed.values ? "This seed has no editable values cached yet; the form starts from APWorld defaults." : "");
  };

  const renderAdvancedControl = (option: SeedOptionDefinition) => {
    const value = advancedValues[option.option_key] ?? defaultValueForOption(option);
    const control = optionControl(option);
    const min = Number(option.validation_rules?.range_start ?? 0);
    const max = Number(option.validation_rules?.range_end ?? 100);

    if (control === "toggle") {
      return (
        <label className="skl-seed-toggle">
          <input
            type="checkbox"
            checked={Boolean(value)}
            onChange={(event) => setAdvancedValue(option.option_key, event.target.checked)}
          />
          <span>{Boolean(value) ? "On" : "Off"}</span>
        </label>
      );
    }

    if (control === "choice") {
      return (
        <select value={String(value ?? "")} onChange={(event) => setAdvancedValue(option.option_key, event.target.value)}>
          {(option.choices || []).map((choice) => (
            <option key={`${option.option_key}-${choice.yaml_value}`} value={choice.yaml_value}>
              {choice.label || choice.choice_key || choice.yaml_value}
            </option>
          ))}
        </select>
      );
    }

    if (control === "text_choice") {
      return (
        <div className="skl-seed-inline-fields">
          <input value={String(value ?? "")} onChange={(event) => setAdvancedValue(option.option_key, event.target.value)} />
          <select value="" onChange={(event) => event.target.value && setAdvancedValue(option.option_key, event.target.value)}>
            <option value="">Preset</option>
            {(option.choices || []).map((choice) => (
              <option key={`${option.option_key}-${choice.yaml_value}`} value={choice.yaml_value}>
                {choice.label || choice.choice_key || choice.yaml_value}
              </option>
            ))}
          </select>
        </div>
      );
    }

    if (control === "range" || control === "named_range" || option.type === "integer" || option.type === "number") {
      const numericValue = Number(value ?? min);
      const setNumber = (next: string) => {
        const parsed = option.type === "number" ? Number(next) : Number.parseInt(next || "0", 10);
        setAdvancedValue(option.option_key, Number.isFinite(parsed) ? parsed : min);
      };
      return (
        <div className="skl-seed-range">
          <input type="range" min={min} max={max} value={numericValue} onChange={(event) => setNumber(event.target.value)} />
          <input type="number" min={min} max={max} value={numericValue} onChange={(event) => setNumber(event.target.value)} />
        </div>
      );
    }

    if (option.type === "array") {
      const lines = Array.isArray(value) ? value.join("\n") : "";
      return (
        <textarea
          value={lines}
          placeholder="One entry per line"
          onChange={(event) =>
            setAdvancedValue(
              option.option_key,
              event.target.value
                .split("\n")
                .map((entry) => entry.trim())
                .filter(Boolean)
            )
          }
        />
      );
    }

    if (control === "counter") {
      return (
        <textarea
          value={displayCounter(value)}
          placeholder="Item name: 1"
          onChange={(event) => setAdvancedValue(option.option_key, parseCounter(event.target.value))}
        />
      );
    }

    if (option.type === "object") {
      return (
        <textarea
          value={displayJson(value)}
          placeholder="{}"
          onChange={(event) => {
            try {
              setAdvancedValue(option.option_key, JSON.parse(event.target.value || "{}"));
            } catch {
              // Keep the last valid JSON value until the user finishes typing.
            }
          }}
        />
      );
    }

    return <input value={String(value ?? "")} onChange={(event) => setAdvancedValue(option.option_key, event.target.value)} />;
  };

  const renderAdvancedBody = () => (
    <div className="skl-seed-body">
      <div className="skl-seed-form-grid">
        <label>
          <span>Seed Name</span>
          <input value={seedTitle} onChange={(event) => setSeedTitle(event.target.value)} placeholder="Advanced Showcase Seed" />
        </label>
        <label>
          <span>Player Name</span>
          <input value={playerName} onChange={(event) => setPlayerName(event.target.value)} placeholder="Player" />
        </label>
      </div>

      {advancedSource && (
        <div className="skl-seed-note">
          Editing creates a new personal seed from the current values, so the original stays untouched.
        </div>
      )}

      {advancedLoading && <div className="skl-seed-empty">Loading APWorld form from Nexus...</div>}

      {!advancedLoading && advancedSchema && advancedGroups.length === 0 && (
        <div className="skl-seed-empty">No advanced options are available for {game.display_name} yet.</div>
      )}

      {!advancedLoading && advancedGroups.length > 0 && activeAdvancedGroup && (
        <div className="skl-seed-advanced-groups">
          <div className="skl-seed-group-tabs" role="tablist" aria-label="APWorld option groups">
            {advancedGroups.map((group) => (
              <button
                key={group.key}
                type="button"
                className={activeAdvancedGroup.key === group.key ? "active" : ""}
                onClick={() => setAdvancedGroupKey(group.key)}
              >
                {group.label}
                <span>{group.options.length}</span>
              </button>
            ))}
          </div>
          <section className="skl-seed-advanced-group" key={activeAdvancedGroup.key}>
            <h3>{activeAdvancedGroup.label}</h3>
            <div className="skl-seed-advanced-grid">
              {activeAdvancedGroup.options.map((option) => (
                <label className="skl-seed-advanced-option" key={option.option_key}>
                  <span>
                    {option.label}
                    {option.description && <em title={option.description}>?</em>}
                  </span>
                  {renderAdvancedControl(option)}
                </label>
              ))}
            </div>
          </section>
        </div>
      )}

      <div className="skl-seed-actions">
        {advancedSource ? (
          <button type="button" onClick={() => setAdvancedSource(null)}>Back</button>
        ) : (
          <button type="button" onClick={onClose}>Cancel</button>
        )}
        <button type="button" className="primary" disabled={busy || advancedLoading} onClick={() => void createAdvanced()}>
          {busy ? "Creating..." : advancedSource ? "Save Seed Copy" : "Create Seed"}
        </button>
      </div>
    </div>
  );

  return (
    <div className="skl-seed-modal" role="dialog" aria-modal="true" aria-label={modeTitle[mode]}>
      <button className="skl-seed-modal-backdrop" type="button" onClick={onClose} aria-label="Close seed modal" />
      <div className="skl-seed-modal-panel">
        <div className="skl-seed-modal-header">
          <div>
            <span className="skl-seed-eyebrow">{game.display_name}</span>
            <h2>{advancedSource ? "Edit Seed" : modeTitle[mode]}</h2>
          </div>
          <button type="button" className="skl-seed-close" onClick={onClose}>Close</button>
        </div>

        {mode === "add" && (
          <>
            <div className="skl-seed-tabs" role="tablist" aria-label="Seed creation mode">
              <button type="button" className={tab === "easy" ? "active" : ""} onClick={() => setTab("easy")}>Easy</button>
              <button type="button" className={tab === "advanced" ? "active" : ""} onClick={() => startAdvancedEditor()}>Advanced</button>
            </div>

            {tab === "easy" && (
              <div className="skl-seed-body">
                <div className="skl-seed-form-grid">
                  <label>
                    <span>Seed Name</span>
                    <input value={seedTitle} onChange={(event) => setSeedTitle(event.target.value)} placeholder="Vanilla Showcase Seed" />
                  </label>
                  <label>
                    <span>Player Name</span>
                    <input value={playerName} onChange={(event) => setPlayerName(event.target.value)} placeholder="Player" />
                  </label>
                </div>
                <div className="skl-seed-choice-grid">
                  {EASY_SEED_CHOICES.map((choice) => {
                    const active = selectedChoiceIds.has(choice.id);
                    return (
                      <button
                        key={choice.id}
                        type="button"
                        className={`skl-seed-choice${active ? " active" : ""}`}
                        onClick={() => toggleChoice(choice)}
                      >
                        <strong>{choice.label}</strong>
                        <span>{choice.description}</span>
                      </button>
                    );
                  })}
                </div>
                <div className="skl-seed-note">
                  Easy creates a guided showcase seed now. Pulse assistant routing will use the same saved intent once the live Pulse endpoint is exposed.
                </div>
                <div className="skl-seed-actions">
                  <button type="button" onClick={onClose}>Cancel</button>
                  <button type="button" className="primary" disabled={busy} onClick={() => void createSeed()}>
                    {busy ? "Creating..." : "Create Seed"}
                  </button>
                </div>
              </div>
            )}

            {tab === "advanced" && renderAdvancedBody()}
          </>
        )}

        {mode === "manage" && advancedSource && renderAdvancedBody()}

        {(mode === "manage" || mode === "select") && !advancedSource && (
          <div className="skl-seed-body">
            <div className="skl-seed-list">
              {seeds.map((seed) => (
                <div className="skl-seed-row" key={seed.id}>
                  <div>
                    <strong>{seed.title}</strong>
                    <span>{seed.game}{seed.player_name ? ` / ${seed.player_name}` : ""}</span>
                  </div>
                  {mode === "manage" ? (
                    <div className="skl-seed-row-actions">
                      <button type="button" onClick={() => startAdvancedEditor(seed)}>Edit</button>
                      <button type="button" className={deleteConfirmId === seed.id ? "danger active" : "danger"} disabled={busy} onClick={() => void removeSeed(seed)}>
                        {deleteConfirmId === seed.id ? "Confirm Delete" : "Delete"}
                      </button>
                    </div>
                  ) : (
                    <button type="button" className="primary" onClick={() => void selectSeed(seed)}>Add Seed</button>
                  )}
                </div>
              ))}
              {!seeds.length && (
                <div className="skl-seed-empty">
                  No seeds yet for {game.display_name}. Use Add a Seed first.
                </div>
              )}
            </div>
            <div className="skl-seed-actions">
              <button type="button" onClick={onClose}>Close</button>
              <button
                type="button"
                className="primary"
                onClick={() => {
                  void refreshSeeds();
                  onSeedsChanged?.();
                }}
              >
                Refresh
              </button>
            </div>
          </div>
        )}

        {status && <div className="skl-seed-status">{status}</div>}
      </div>
    </div>
  );
};

export default SeedConfigModal;
