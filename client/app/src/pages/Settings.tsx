import React, { useEffect, useState } from "react";
import { runtime } from "../services/runtime";
import { localeLabelKey, useI18n } from "../i18n";
import { LocaleCode } from "../i18n/types";
import { API_BASE_URL, getDesktopToken } from "../services/api";

type ImportResult = { gameId: string; file: string };
type RomEntry = { gameId: string; path: string };

const SettingsPage: React.FC = () => {
  const CRASH_REPORTING_OPT_IN_KEY = "skl_crash_reporting_opt_in";
  const { t, locale, setLocale, locales } = useI18n();
  const [scanStatus, setScanStatus] = useState("");
  const [scanResults, setScanResults] = useState<ImportResult[]>([]);
  const [romCount, setRomCount] = useState(0);
  const [romEntries, setRomEntries] = useState<RomEntry[]>([]);
  const [romSearch, setRomSearch] = useState("");
  const [gamescopeDetected, setGamescopeDetected] = useState<{ exists: boolean; path: string }>({ exists: false, path: "" });
  const [wmctrlDetected, setWmctrlDetected] = useState<{ exists: boolean; path: string }>({ exists: false, path: "" });
  const [displays, setDisplays] = useState<any[]>([]);
  const [windowingStatus, setWindowingStatus] = useState("");
  const [layoutStatus, setLayoutStatus] = useState("");
  const [gamesStatus, setGamesStatus] = useState("");

  const [gsEnabled, setGsEnabled] = useState(false);
  const [gsMode, setGsMode] = useState<"prefer" | "require">("prefer");
  const [gsFullscreen, setGsFullscreen] = useState(true);
  const [gsWidth, setGsWidth] = useState("");
  const [gsHeight, setGsHeight] = useState("");
  const [gsArgsText, setGsArgsText] = useState("");

  const [factorioModsDir, setFactorioModsDir] = useState("");
  const [factorioExePath, setFactorioExePath] = useState("");
  const [gzdoomExePath, setGzdoomExePath] = useState("");
  const [gzdoomIwadPath, setGzdoomIwadPath] = useState("");
  const [gzdoomGzapPk3Path, setGzdoomGzapPk3Path] = useState("");
  const [gzdoomArgsText, setGzdoomArgsText] = useState("");
  const [sm64exExePath, setSm64exExePath] = useState("");
  const [sm64exRootDir, setSm64exRootDir] = useState("");
  const [sm64exArgsText, setSm64exArgsText] = useState("");
  const [sohExePath, setSohExePath] = useState("");
  const [sohRootDir, setSohRootDir] = useState("");
  const [sohAutoInstall, setSohAutoInstall] = useState(true);
  const [sohArgsText, setSohArgsText] = useState("");

  const [layoutPreset, setLayoutPreset] = useState("handheld");
  const [layoutMode, setLayoutMode] = useState<"" | "side_by_side" | "separate_displays">("");
  const [layoutGameDisplay, setLayoutGameDisplay] = useState("0");
  const [layoutTrackerDisplay, setLayoutTrackerDisplay] = useState("1");
  const [layoutSplit, setLayoutSplit] = useState("0.7");
  const [settingsTab, setSettingsTab] = useState<"general" | "video" | "audio" | "paths" | "roms" | "linux" | "social">("general");
  const [isUiPrototype, setIsUiPrototype] = useState(false);
  const [metalVariant, setMetalVariant] = useState<"a" | "b" | "c">("c");
  const [audioGlobalMuted, setAudioGlobalMuted] = useState(false);
  const [audioGlobalVolume, setAudioGlobalVolume] = useState(0.3);
  const [audioSoundKeys, setAudioSoundKeys] = useState<string[]>([]);
  const [audioSoundMuted, setAudioSoundMuted] = useState<Record<string, boolean>>({});
  const [audioSoundVolume, setAudioSoundVolume] = useState<Record<string, number>>({});
  const [audioBgmEnabled, setAudioBgmEnabled] = useState(true);
  const [audioBgmTrack, setAudioBgmTrack] = useState("menu_teal_01");
  const [audioBgmVolume, setAudioBgmVolume] = useState(0.25);
  const [audioBgmTracks, setAudioBgmTracks] = useState<Array<{ id: string; label: string }>>([]);
  const [crashReportingEnabled, setCrashReportingEnabled] = useState(false);
  const [crashReportStatus, setCrashReportStatus] = useState("");
  type SettingsTab = "general" | "video" | "audio" | "paths" | "roms" | "linux" | "social";

  const sfxApi = () => (window as any).SKL_SFX;

  const refreshAudioState = () => {
    const api = sfxApi();
    if (!api) return;
    const keys: string[] = Array.isArray(api.getSoundKeys?.()) ? api.getSoundKeys() : [];
    const muted: Record<string, boolean> = {};
    const vol: Record<string, number> = {};
    keys.forEach((k) => {
      muted[k] = Boolean(api.getSoundMuted?.(k));
      const raw = Number(api.getSoundVolume?.(k));
      vol[k] = Number.isFinite(raw) ? raw : 1;
    });
    setAudioSoundKeys(keys);
    setAudioSoundMuted(muted);
    setAudioSoundVolume(vol);
    setAudioGlobalMuted(Boolean(api.getGlobalMuted?.() ?? api.getMuted?.()));
    setAudioGlobalVolume(Number(api.getGlobalVolume?.() ?? api.getBaseVolume?.() ?? 0.3));
    setAudioBgmEnabled(Boolean(api.getBgmEnabled?.()));
    setAudioBgmTrack(String(api.getBgmTrack?.() || "menu_teal_01"));
    setAudioBgmVolume(Number(api.getBgmVolume?.() || 0.25));
    const tracks = api.getBgmTracks?.();
    if (Array.isArray(tracks)) setAudioBgmTracks(tracks);
  };

  const applyMetalVariant = (variant: "a" | "b" | "c") => {
    document.body.classList.remove("sklp-metal-b", "sklp-metal-c");
    if (variant === "b") document.body.classList.add("sklp-metal-b");
    if (variant === "c") document.body.classList.add("sklp-metal-c");
    window.localStorage.setItem("sklp_metal_variant", variant);
    setMetalVariant(variant);
  };

  const refreshConfig = async () => {
    const config = await runtime.configGet?.();
    const roms = (config as any)?.roms || {};
    setRomCount(Object.keys(roms).length);
    setRomEntries(
      Object.entries(roms)
        .map(([gameId, path]) => ({ gameId: String(gameId), path: String(path || "") }))
        .sort((a, b) => a.gameId.localeCompare(b.gameId))
    );
    const gs = (config as any)?.windowing?.gamescope || (config as any)?.gamescope || {};
    setGsEnabled(Boolean(gs?.enabled));
    setGsMode(gs?.mode === "require" ? "require" : "prefer");
    setGsFullscreen(typeof gs?.fullscreen === "boolean" ? Boolean(gs.fullscreen) : true);
    setGsWidth(Number.isFinite(gs?.width) ? String(gs.width) : "");
    setGsHeight(Number.isFinite(gs?.height) ? String(gs.height) : "");
    setGsArgsText(Array.isArray(gs?.args) ? gs.args.join("\n") : "");

    const layout = (config as any)?.layout || {};
    setLayoutPreset(typeof layout?.preset === "string" ? layout.preset : "handheld");
    setLayoutMode(layout?.mode === "side_by_side" || layout?.mode === "separate_displays" ? layout.mode : "");
    setLayoutGameDisplay(Number.isFinite(layout?.game_display) ? String(layout.game_display) : "0");
    setLayoutTrackerDisplay(Number.isFinite(layout?.tracker_display) ? String(layout.tracker_display) : "1");
    setLayoutSplit(Number.isFinite(layout?.split) ? String(layout.split) : "0.7");

    const games = (config as any)?.games || {};
    const factorio = games?.factorio || {};
    setFactorioModsDir(typeof factorio?.mods_dir === "string" ? factorio.mods_dir : "");
    setFactorioExePath(typeof factorio?.exe_path === "string" ? factorio.exe_path : "");

    const gzdoom = games?.gzdoom || {};
    setGzdoomExePath(typeof gzdoom?.exe_path === "string" ? gzdoom.exe_path : "");
    setGzdoomIwadPath(typeof gzdoom?.iwad_path === "string" ? gzdoom.iwad_path : "");
    setGzdoomGzapPk3Path(typeof gzdoom?.gzap_pk3_path === "string" ? gzdoom.gzap_pk3_path : "");
    setGzdoomArgsText(Array.isArray(gzdoom?.args) ? gzdoom.args.join("\n") : "");

    const sm64ex = games?.sm64ex || {};
    setSm64exExePath(typeof sm64ex?.exe_path === "string" ? sm64ex.exe_path : "");
    setSm64exRootDir(typeof sm64ex?.root_dir === "string" ? sm64ex.root_dir : "");
    setSm64exArgsText(Array.isArray(sm64ex?.args) ? sm64ex.args.join("\n") : "");

    const soh = games?.oot_soh || {};
    setSohExePath(typeof soh?.exe_path === "string" ? soh.exe_path : "");
    setSohRootDir(typeof soh?.root_dir === "string" ? soh.root_dir : "");
    setSohAutoInstall(typeof soh?.auto_install === "boolean" ? Boolean(soh.auto_install) : true);
    setSohArgsText(Array.isArray(soh?.args) ? soh.args.join("\n") : "");
  };

  useEffect(() => {
    const hasPrototypeClass = document.body.classList.contains("sklp-ui");
    setIsUiPrototype(hasPrototypeClass);
    if (hasPrototypeClass) {
      const stored = (window.localStorage.getItem("sklp_metal_variant") || "c").toLowerCase();
      setMetalVariant(stored === "a" || stored === "b" || stored === "c" ? stored : "c");
    }

    refreshConfig();
    refreshAudioState();
    try {
      setCrashReportingEnabled(window.localStorage.getItem(CRASH_REPORTING_OPT_IN_KEY) === "1");
    } catch {
      setCrashReportingEnabled(false);
    }
    (async () => {
      try {
        const res = await runtime.gamescopeStatus?.();
        if ((res as any)?.ok) {
          setGamescopeDetected({ exists: Boolean((res as any).exists), path: String((res as any).path || "") });
        }
      } catch {
        // ignore
      }
    })();
    (async () => {
      try {
        const res = await runtime.wmctrlStatus?.();
        if ((res as any)?.ok) {
          setWmctrlDetected({ exists: Boolean((res as any).exists), path: String((res as any).path || "") });
        }
      } catch {
        // ignore
      }
      try {
        const res = await runtime.getDisplays?.();
        const list = (res as any)?.displays;
        if ((res as any)?.ok && Array.isArray(list)) setDisplays(list);
      } catch {
        // ignore
      }
    })();
  }, []);

  const onCrashReportingToggle = async (enabled: boolean) => {
    setCrashReportingEnabled(enabled);
    setCrashReportStatus("");
    try {
      window.localStorage.setItem(CRASH_REPORTING_OPT_IN_KEY, enabled ? "1" : "0");
    } catch {
      // ignore
    }
    await runtime.setCrashReportingOptIn?.(enabled);
    setCrashReportStatus(enabled ? "Crash reporting enabled." : "Crash reporting disabled.");
  };

  const sendDiagnosticsReport = async () => {
    setCrashReportStatus("");
    const uploadUrl = API_BASE_URL
      ? `${API_BASE_URL.replace(/\/$/, "")}/api/client/crash-report`
      : "https://sekailink.com/api/client/crash-report";
    try {
      const collected = await runtime.collectDiagnostics?.({
        includeLogTail: true,
        maxLogBytes: 220 * 1024,
        meta: { source: "settings_manual_send" },
      });
      const report = collected?.report;
      if (!report) {
        setCrashReportStatus("Unable to collect diagnostics.");
        return;
      }

      const compactJson = JSON.stringify(report, null, 2);
      await runtime.copyText?.(compactJson);

      if (!crashReportingEnabled) {
        setCrashReportStatus("Diagnostics copied to clipboard. Enable crash reporting to upload automatically.");
        return;
      }

      const token = getDesktopToken() || "";
      const submitted = await runtime.submitDiagnostics?.({
        uploadUrl,
        authToken: token,
        report,
      });
      if (!submitted?.ok) {
        setCrashReportStatus(`Upload failed: ${submitted?.error || "unknown_error"} (copied to clipboard).`);
        return;
      }
      setCrashReportStatus("Diagnostics uploaded successfully.");
    } catch (err) {
      setCrashReportStatus(err instanceof Error ? err.message : "Diagnostics upload failed.");
    }
  };

  const parseArgsText = (value: string) =>
    String(value || "")
      .split(/\r?\n/)
      .map((line) => line.trim())
      .filter(Boolean);

  const saveWindowing = async (preset?: "handheld" | "desktop" | "streamer") => {
    setWindowingStatus("");
    try {
      let enabled = gsEnabled;
      let mode: "prefer" | "require" = gsMode;
      let fullscreen = gsFullscreen;
      let width = gsWidth;
      let height = gsHeight;
      let argsText = gsArgsText;

      if (preset === "handheld") {
        enabled = true;
        mode = "prefer";
        fullscreen = true;
        width = "1280";
        height = "720";
        argsText = "";
      } else if (preset === "desktop") {
        enabled = false;
        mode = "prefer";
        fullscreen = true;
        width = "";
        height = "";
        argsText = "";
      } else if (preset === "streamer") {
        enabled = true;
        mode = "prefer";
        fullscreen = false;
        width = "1280";
        height = "720";
        argsText = "";
      }

      const widthNum = width.trim() ? Number(width) : undefined;
      const heightNum = height.trim() ? Number(height) : undefined;
      const payload = {
        gamescope: {
          enabled,
          mode,
          fullscreen,
          width: Number.isFinite(widthNum) ? widthNum : undefined,
          height: Number.isFinite(heightNum) ? heightNum : undefined,
          args: parseArgsText(argsText),
        }
      };
      const res: any = await runtime.configSetWindowing?.(payload);
      if (!res?.ok) {
        setWindowingStatus(`Save failed: ${res?.error || "unknown error"}`);
        return;
      }
      setGsEnabled(enabled);
      setGsMode(mode);
      setGsFullscreen(fullscreen);
      setGsWidth(width);
      setGsHeight(height);
      setGsArgsText(argsText);
      setWindowingStatus("Saved.");
      refreshConfig();
    } catch (err) {
      setWindowingStatus(err instanceof Error ? err.message : "Save failed.");
    }
  };

  const saveLayout = async (preset?: "handheld" | "desktop" | "desktop_dual" | "streamer_dual") => {
    setLayoutStatus("");
    try {
      let nextPreset = layoutPreset;
      let nextMode = layoutMode;
      let nextGameDisplay = layoutGameDisplay;
      let nextTrackerDisplay = layoutTrackerDisplay;
      let nextSplit = layoutSplit;

      if (preset === "handheld") {
        nextPreset = "handheld";
        nextMode = "side_by_side";
        nextGameDisplay = "0";
        nextTrackerDisplay = "0";
        nextSplit = "0.75";
      } else if (preset === "desktop") {
        nextPreset = "desktop";
        nextMode = "side_by_side";
        nextGameDisplay = "0";
        nextTrackerDisplay = "0";
        nextSplit = "0.7";
      } else if (preset === "desktop_dual") {
        nextPreset = "desktop_dual";
        nextMode = "separate_displays";
        nextGameDisplay = "0";
        nextTrackerDisplay = "1";
        nextSplit = "0.7";
      } else if (preset === "streamer_dual") {
        nextPreset = "streamer_dual";
        nextMode = "separate_displays";
        nextGameDisplay = "0";
        nextTrackerDisplay = "1";
        nextSplit = "0.7";
      }

      const gameDisplay = Number(nextGameDisplay);
      const trackerDisplay = Number(nextTrackerDisplay);
      const split = Number(nextSplit);
      const payload = {
        preset: nextPreset,
        mode: nextMode || undefined,
        game_display: Number.isFinite(gameDisplay) ? gameDisplay : 0,
        tracker_display: Number.isFinite(trackerDisplay) ? trackerDisplay : 1,
        split: Number.isFinite(split) ? split : 0.7
      };

      const res: any = await runtime.configSetLayout?.(payload);
      if (!res?.ok) {
        setLayoutStatus(`Save failed: ${res?.error || "unknown error"}`);
        return;
      }

      setLayoutPreset(nextPreset);
      setLayoutMode(nextMode);
      setLayoutGameDisplay(String(payload.game_display));
      setLayoutTrackerDisplay(String(payload.tracker_display));
      setLayoutSplit(String(payload.split));
      setLayoutStatus("Saved.");
      refreshConfig();
    } catch (err) {
      setLayoutStatus(err instanceof Error ? err.message : "Save failed.");
    }
  };

  const runImport = async () => {
    setScanStatus("Select a ROM file...");
    const pick = await runtime.pickFile?.({
      title: "Select ROM file",
      filters: [{ name: "ROM", extensions: ["gba", "gb", "gbc", "sfc", "smc", "z64", "n64"] }]
    });
    if (!pick || pick.canceled || !pick.path) {
      setScanStatus("Import canceled.");
      return;
    }
    setScanStatus("Importing ROM...");
    const res: any = await runtime.romsImport?.(pick.path);
    if (!res?.ok) {
      setScanStatus(`Import failed: ${res?.error || "unknown error"}`);
      return;
    }
    setScanResults([{ gameId: res.gameId || "unknown", file: pick.path }]);
    setScanStatus("ROM imported.");
    refreshConfig();
  };

  const removeRom = async (gameId: string) => {
    try {
      const res: any = await runtime.configDeleteRom?.(gameId);
      if (!res?.ok) {
        setScanStatus(`Unable to remove ROM mapping (${res?.error || "unknown error"}).`);
        return;
      }
      setScanStatus(`Removed ROM mapping for ${gameId}.`);
      await refreshConfig();
    } catch (err) {
      setScanStatus(err instanceof Error ? err.message : "Unable to remove ROM mapping.");
    }
  };

  const saveFactorio = async () => {
    setGamesStatus("");
    try {
      const patch: any = {
        mods_dir: factorioModsDir.trim() || undefined,
        exe_path: factorioExePath.trim() || undefined,
      };
      const res: any = await runtime.configSetGame?.("factorio", patch);
      if (!res?.ok) {
        setGamesStatus(`Save failed: ${res?.error || "unknown error"}`);
        return;
      }
      setGamesStatus("Saved.");
      refreshConfig();
    } catch (err) {
      setGamesStatus(err instanceof Error ? err.message : "Save failed.");
    }
  };

  const saveGzDoom = async () => {
    setGamesStatus("");
    try {
      const args = String(gzdoomArgsText || "")
        .split(/\r?\n/)
        .map((l) => l.trim())
        .filter(Boolean);
      const patch: any = {
        exe_path: gzdoomExePath.trim() || undefined,
        iwad_path: gzdoomIwadPath.trim() || undefined,
        gzap_pk3_path: gzdoomGzapPk3Path.trim() || undefined,
        args,
      };
      const res: any = await runtime.configSetGame?.("gzdoom", patch);
      if (!res?.ok) {
        setGamesStatus(`Save failed: ${res?.error || "unknown error"}`);
        return;
      }
      setGamesStatus("Saved.");
      refreshConfig();
    } catch (err) {
      setGamesStatus(err instanceof Error ? err.message : "Save failed.");
    }
  };

  const saveSm64Ex = async () => {
    setGamesStatus("");
    try {
      const patch: any = {
        exe_path: sm64exExePath.trim() || undefined,
        root_dir: sm64exRootDir.trim() || undefined,
        args: parseArgsText(sm64exArgsText),
      };
      const res: any = await runtime.configSetGame?.("sm64ex", patch);
      if (!res?.ok) {
        setGamesStatus(`Save failed: ${res?.error || "unknown error"}`);
        return;
      }
      setGamesStatus("Saved.");
      refreshConfig();
    } catch (err) {
      setGamesStatus(err instanceof Error ? err.message : "Save failed.");
    }
  };

  const saveSoh = async () => {
    setGamesStatus("");
    try {
      const patch: any = {
        exe_path: sohExePath.trim() || undefined,
        root_dir: sohRootDir.trim() || undefined,
        auto_install: sohAutoInstall,
        args: parseArgsText(sohArgsText),
      };
      const res: any = await runtime.configSetGame?.("oot_soh", patch);
      if (!res?.ok) {
        setGamesStatus(`Save failed: ${res?.error || "unknown error"}`);
        return;
      }
      setGamesStatus("Saved.");
      refreshConfig();
    } catch (err) {
      setGamesStatus(err instanceof Error ? err.message : "Save failed.");
    }
  };

  return (
    <div className="skl-game-manager skl-app-panel skl-settings-page">
      <div className="skl-gm-header">
        <h1>{t("settings.title")}</h1>
        <p>{t("settings.subtitle")}</p>
      </div>

      <div className="skl-gm-tabs skl-settings-tabs" role="tablist" aria-label={t("settings.title")} data-tutorial="settings-tabs">
        {([
          ["general", t("settings.tab.general"), "M12 4v16M4 12h16"],
          ["video", t("settings.tab.video"), "M5 6h14v12H5z M9 20h6"],
          ["audio", t("settings.tab.audio"), "M5 10h4l5-4v12l-5-4H5z M17 9a4 4 0 0 1 0 6"],
          ["paths", t("settings.tab.paths"), "M4 7h6l2 2h8v8H4z"],
          ["roms", t("settings.tab.roms"), "M6 6h12v12H6z M9 10h6M9 14h6"],
          ["social", t("settings.tab.social"), "M12 7a3 3 0 1 0 0 6a3 3 0 0 0 0-6z M5 20a7 7 0 0 1 14 0"],
          ["linux", t("settings.tab.linux"), "M6 5h12v14H6z M9 8h6M9 12h6M9 16h6"],
        ] as [SettingsTab, string, string][]).map(([tab, label, icon]) => (
          <button key={tab} type="button" className={`skl-gm-tab${settingsTab === tab ? " active" : ""}`} onClick={() => setSettingsTab(tab)}>
            <svg className="skl-gm-tab-icon" viewBox="0 0 24 24" fill="none" aria-hidden="true">
              <path d={icon} stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round" />
            </svg>
            {label}
          </button>
        ))}
      </div>

      {settingsTab === "general" && (
        <div className="skl-gm-panel active" role="tabpanel" style={{ marginTop: 16 }}>
          <div className="skl-settings-grid">
            <div className="yaml-dashboard-card">
              <div className="skl-gm-section-header"><h2>{t("settings.tab.general")}</h2></div>
              <label style={{ display: "inline-flex", gap: 8, alignItems: "center", flexWrap: "wrap" }}>
                {t("settings.language")}
                <select value={locale} onChange={(e) => setLocale(e.target.value as LocaleCode)}>
                  {locales.map((code) => (
                    <option key={code} value={code}>
                      {t(localeLabelKey[code])}
                    </option>
                  ))}
                </select>
              </label>
              <p style={{ marginTop: 8, opacity: 0.8 }}>{t("settings.language_help")}</p>
            </div>
            {isUiPrototype && (
              <div className="yaml-dashboard-card">
                <div className="skl-gm-section-header"><h2>{t("settings.ui_theme_title")}</h2></div>
                <p>{t("settings.ui_theme_desc")}</p>
                <div style={{ display: "flex", gap: 10, flexWrap: "wrap", alignItems: "center" }}>
                  <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
                    {t("settings.variation")}
                    <select
                      value={metalVariant}
                      onChange={(e) => setMetalVariant((e.target.value === "b" || e.target.value === "c") ? e.target.value : "a")}
                    >
                      <option value="a">{t("settings.variation.a")}</option>
                      <option value="b">{t("settings.variation.b")}</option>
                      <option value="c">{t("settings.variation.c")}</option>
                    </select>
                  </label>
                  <button className="skl-btn primary" type="button" onClick={() => applyMetalVariant(metalVariant)}>
                    {t("settings.apply_variation")}
                  </button>
                </div>
              </div>
            )}
            <div className="yaml-dashboard-card">
              <div className="skl-gm-section-header"><h2>Diagnostics</h2></div>
              <div style={{ display: "grid", gap: 8 }}>
                <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
                  <input
                    type="checkbox"
                    checked={crashReportingEnabled}
                    onChange={(e) => void onCrashReportingToggle(e.target.checked)}
                  />
                  Crash reporting (opt-in)
                </label>
                <p style={{ margin: 0, opacity: 0.8 }}>
                  Sends crash diagnostics and recent logs to SekaiLink support endpoint.
                </p>
                <div style={{ display: "flex", gap: 8, flexWrap: "wrap" }}>
                  <button className="skl-btn ghost" type="button" onClick={() => void sendDiagnosticsReport()}>
                    Send diagnostics now
                  </button>
                </div>
                {crashReportStatus && <div className="skl-ready-status">{crashReportStatus}</div>}
              </div>
            </div>
          </div>
        </div>
      )}

      {settingsTab === "roms" && (
      <div className="skl-gm-panel active" role="tabpanel" style={{ marginTop: 16 }}>
        <div className="skl-gm-section-header">
          <h2>{t("settings.rom_library")}</h2>
        </div>
        <div className="yaml-dashboard-card">
          <p>
            {t("settings.imported_roms")}: <strong>{romCount}</strong>
          </p>
          <button className="skl-btn primary" type="button" onClick={runImport}>
            {t("settings.import_rom_file")}
          </button>
          {scanStatus && <div className="skl-ready-status">{scanStatus}</div>}
          {scanResults.length > 0 && (
            <div className="yaml-list" style={{ marginTop: 16 }}>
              {scanResults.map((entry) => (
                <div className="yaml-row" key={`${entry.gameId}:${entry.file}`}>
                  <div className="yaml-meta">
                    <div className="yaml-title">{entry.gameId}</div>
                    <div className="yaml-sub">{t("settings.imported_from", { file: entry.file })}</div>
                  </div>
                </div>
              ))}
            </div>
          )}

          <details open style={{ marginTop: 16 }}>
            <summary style={{ cursor: "pointer", fontWeight: 700 }}>{t("settings.rom_mappings_list")}</summary>
            <div style={{ marginTop: 10, display: "flex", gap: 8 }}>
              <input
                className="input"
                placeholder={t("settings.filter_game_id_or_path")}
                value={romSearch}
                onChange={(e) => setRomSearch(e.target.value)}
                style={{ maxWidth: 420 }}
              />
            </div>
            <div style={{ marginTop: 10, maxHeight: 320, overflow: "auto", border: "1px solid rgba(56, 243, 221, 0.16)", borderRadius: 10, padding: 8 }}>
              {(romEntries.filter((entry) => {
                const q = romSearch.trim().toLowerCase();
                if (!q) return true;
                return entry.gameId.toLowerCase().includes(q) || entry.path.toLowerCase().includes(q);
              })).map((entry) => (
                <div key={`${entry.gameId}:${entry.path}`} className="yaml-row" style={{ marginBottom: 8 }}>
                  <div className="yaml-meta">
                    <div className="yaml-title">{entry.gameId}</div>
                    <div className="yaml-sub" style={{ wordBreak: "break-all" }}>{entry.path}</div>
                  </div>
                  <div style={{ display: "flex", gap: 8 }}>
                    <button className="skl-btn ghost" type="button" onClick={() => runtime.showItemInFolder?.(entry.path)}>{t("auth.folder")}</button>
                    <button className="skl-btn" type="button" onClick={() => removeRom(entry.gameId)}>{t("social.remove")}</button>
                  </div>
                </div>
              ))}
              {romEntries.length === 0 && (
                <div className="skl-ready-status">{t("settings.no_rom_mappings_found")}</div>
              )}
              {romEntries.length > 0 && romEntries.filter((entry) => {
                const q = romSearch.trim().toLowerCase();
                if (!q) return true;
                return entry.gameId.toLowerCase().includes(q) || entry.path.toLowerCase().includes(q);
              }).length === 0 && (
                <div className="skl-ready-status">{t("settings.no_rom_mappings_match_filter")}</div>
              )}
            </div>
          </details>
        </div>
      </div>
      )}

      {settingsTab === "linux" && (
      <div className="skl-gm-panel active" role="tabpanel" style={{ marginTop: 16 }}>
        <div className="skl-gm-section-header">
          <h2>Windowing (Linux)</h2>
        </div>
        <div className="yaml-dashboard-card">
          <p>
            Gamescope detected: <strong>{gamescopeDetected.exists ? "Yes" : "No"}</strong>
            {gamescopeDetected.path ? <span style={{ opacity: 0.75 }}> ({gamescopeDetected.path})</span> : null}
          </p>

          <label style={{ display: "flex", gap: 10, alignItems: "center" }}>
            <input type="checkbox" checked={gsEnabled} onChange={(e) => setGsEnabled(e.target.checked)} />
            Enable Gamescope wrapper
          </label>

          <div style={{ display: "flex", gap: 12, flexWrap: "wrap", marginTop: 12 }}>
            <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
              Mode
              <select value={gsMode} onChange={(e) => setGsMode(e.target.value === "require" ? "require" : "prefer")} disabled={!gsEnabled}>
                <option value="prefer">Prefer (fallback)</option>
                <option value="require">Require (fail if missing)</option>
              </select>
            </label>
            <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
              <input type="checkbox" checked={gsFullscreen} onChange={(e) => setGsFullscreen(e.target.checked)} disabled={!gsEnabled} />
              Fullscreen
            </label>
            <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
              Width
              <input className="input" value={gsWidth} onChange={(e) => setGsWidth(e.target.value)} disabled={!gsEnabled} style={{ width: 110 }} />
            </label>
            <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
              Height
              <input className="input" value={gsHeight} onChange={(e) => setGsHeight(e.target.value)} disabled={!gsEnabled} style={{ width: 110 }} />
            </label>
          </div>

          <div style={{ marginTop: 12 }}>
            <div style={{ fontSize: 12, opacity: 0.8, marginBottom: 6 }}>Extra gamescope args (one per line):</div>
            <textarea
              className="input"
              value={gsArgsText}
              onChange={(e) => setGsArgsText(e.target.value)}
              disabled={!gsEnabled}
              style={{ width: "100%", minHeight: 100, fontFamily: "monospace" }}
              placeholder={"Example:\n--adaptive-sync\n--expose-wayland"}
            />
          </div>

          <div style={{ display: "flex", gap: 10, flexWrap: "wrap", marginTop: 12 }}>
            <button className="skl-btn primary" type="button" onClick={() => saveWindowing()}>
              Save Windowing
            </button>
            <button className="skl-btn ghost" type="button" onClick={() => saveWindowing("handheld")}>
              Apply Handheld Preset
            </button>
            <button className="skl-btn ghost" type="button" onClick={() => saveWindowing("streamer")}>
              Apply Streamer Preset
            </button>
            <button className="skl-btn ghost" type="button" onClick={() => saveWindowing("desktop")}>
              Disable Gamescope
            </button>
          </div>
          {windowingStatus && <div className="skl-ready-status" style={{ marginTop: 10 }}>{windowingStatus}</div>}
        </div>
      </div>
      )}

      {settingsTab === "paths" && (
      <div className="skl-gm-panel active" role="tabpanel" style={{ marginTop: 16 }}>
        <div className="skl-gm-section-header">
          <h2>Games (Automation)</h2>
        </div>
        <div className="yaml-dashboard-card">
          <p style={{ marginTop: 0, opacity: 0.82 }}>Expand the game you want to configure.</p>
          <details open style={{ marginTop: 10 }}>
            <summary style={{ cursor: "pointer", fontWeight: 700 }}>Factorio</summary>
            <div style={{ marginTop: 10 }}>
          <div style={{ fontSize: 12, opacity: 0.85, marginBottom: 10 }}>
            Used for slot-file automation (installing the downloaded mod zip and launching the game).
          </div>

          <div style={{ display: "flex", gap: 12, flexWrap: "wrap" }}>
            <label style={{ display: "flex", flexDirection: "column", gap: 6, flex: "1 1 420px" }}>
              Mods directory (optional override)
              <div style={{ display: "flex", gap: 8 }}>
                <input className="input" value={factorioModsDir} onChange={(e) => setFactorioModsDir(e.target.value)} style={{ flex: 1 }} />
                <button
                  className="skl-btn ghost"
                  type="button"
                  onClick={async () => {
                    const pick = await runtime.pickFolder?.({ title: "Select Factorio mods directory" });
                    if (pick && !pick.canceled && pick.path) setFactorioModsDir(pick.path);
                  }}
                >
                  Browse
                </button>
              </div>
            </label>
          </div>

          <div style={{ display: "flex", gap: 12, flexWrap: "wrap", marginTop: 12 }}>
            <label style={{ display: "flex", flexDirection: "column", gap: 6, flex: "1 1 420px" }}>
              Executable path (optional)
              <div style={{ display: "flex", gap: 8 }}>
                <input className="input" value={factorioExePath} onChange={(e) => setFactorioExePath(e.target.value)} style={{ flex: 1 }} />
                <button
                  className="skl-btn ghost"
                  type="button"
                  onClick={async () => {
                    const pick = await runtime.pickFile?.({ title: "Select Factorio executable" });
                    if (pick && !pick.canceled && pick.path) setFactorioExePath(pick.path);
                  }}
                >
                  Browse
                </button>
              </div>
            </label>
          </div>

          <div style={{ display: "flex", gap: 10, flexWrap: "wrap", marginTop: 12 }}>
            <button className="skl-btn primary" type="button" onClick={saveFactorio}>
              Save Factorio
            </button>
          </div>
            </div>
          </details>

          <details style={{ marginTop: 12 }}>
            <summary style={{ cursor: "pointer", fontWeight: 700 }}>gzDoom</summary>
            <div style={{ marginTop: 10 }}>
          <div style={{ fontSize: 12, opacity: 0.85, marginBottom: 10 }}>
            Used for slot-file automation (.pk3). Requires gzArchipelago.pk3 and an IWAD to launch.
          </div>

          <div style={{ display: "flex", gap: 12, flexWrap: "wrap" }}>
            <label style={{ display: "flex", flexDirection: "column", gap: 6, flex: "1 1 420px" }}>
              Executable path (optional)
              <div style={{ display: "flex", gap: 8 }}>
                <input className="input" value={gzdoomExePath} onChange={(e) => setGzdoomExePath(e.target.value)} style={{ flex: 1 }} />
                <button
                  className="skl-btn ghost"
                  type="button"
                  onClick={async () => {
                    const pick = await runtime.pickFile?.({ title: "Select gzDoom executable" });
                    if (pick && !pick.canceled && pick.path) setGzdoomExePath(pick.path);
                  }}
                >
                  Browse
                </button>
              </div>
            </label>
          </div>

          <div style={{ display: "flex", gap: 12, flexWrap: "wrap", marginTop: 12 }}>
            <label style={{ display: "flex", flexDirection: "column", gap: 6, flex: "1 1 420px" }}>
              IWAD path (required)
              <div style={{ display: "flex", gap: 8 }}>
                <input className="input" value={gzdoomIwadPath} onChange={(e) => setGzdoomIwadPath(e.target.value)} style={{ flex: 1 }} />
                <button
                  className="skl-btn ghost"
                  type="button"
                  onClick={async () => {
                    const pick = await runtime.pickFile?.({
                      title: "Select IWAD file",
                      filters: [{ name: "IWAD", extensions: ["wad", "iwad"] }]
                    });
                    if (pick && !pick.canceled && pick.path) setGzdoomIwadPath(pick.path);
                  }}
                >
                  Browse
                </button>
              </div>
            </label>
          </div>

          <div style={{ display: "flex", gap: 12, flexWrap: "wrap", marginTop: 12 }}>
            <label style={{ display: "flex", flexDirection: "column", gap: 6, flex: "1 1 420px" }}>
              gzArchipelago.pk3 path (required)
              <div style={{ display: "flex", gap: 8 }}>
                <input className="input" value={gzdoomGzapPk3Path} onChange={(e) => setGzdoomGzapPk3Path(e.target.value)} style={{ flex: 1 }} />
                <button
                  className="skl-btn ghost"
                  type="button"
                  onClick={async () => {
                    const pick = await runtime.pickFile?.({
                      title: "Select gzArchipelago.pk3",
                      filters: [{ name: "PK3", extensions: ["pk3"] }]
                    });
                    if (pick && !pick.canceled && pick.path) setGzdoomGzapPk3Path(pick.path);
                  }}
                >
                  Browse
                </button>
              </div>
            </label>
          </div>

          <div style={{ marginTop: 12 }}>
            <div style={{ fontSize: 12, opacity: 0.8, marginBottom: 6 }}>Extra gzDoom args (one per line):</div>
            <textarea
              className="input"
              value={gzdoomArgsText}
              onChange={(e) => setGzdoomArgsText(e.target.value)}
              style={{ width: "100%", minHeight: 80, fontFamily: "monospace" }}
              placeholder={"Example:\n+vid_renderer 1\n+fullscreen 1"}
            />
          </div>

          <div style={{ display: "flex", gap: 10, flexWrap: "wrap", marginTop: 12 }}>
            <button className="skl-btn primary" type="button" onClick={saveGzDoom}>
              Save gzDoom
            </button>
          </div>
            </div>
          </details>

          <details style={{ marginTop: 12 }}>
            <summary style={{ cursor: "pointer", fontWeight: 700 }}>Super Mario 64 EX (sm64ex)</summary>
            <div style={{ marginTop: 10 }}>
          <div style={{ fontSize: 12, opacity: 0.85, marginBottom: 10 }}>
            Used for slot-file automation (<code>.apsm64ex</code>). SekaiLink launches the compiled SM64EX binary with{" "}
            <code>--sm64ap_file</code>.
          </div>

          <div style={{ display: "flex", gap: 12, flexWrap: "wrap" }}>
            <label style={{ display: "flex", flexDirection: "column", gap: 6, flex: "1 1 420px" }}>
              Executable path (recommended)
              <div style={{ display: "flex", gap: 8 }}>
                <input className="input" value={sm64exExePath} onChange={(e) => setSm64exExePath(e.target.value)} style={{ flex: 1 }} />
                <button
                  className="skl-btn ghost"
                  type="button"
                  onClick={async () => {
                    const pick = await runtime.pickFile?.({ title: "Select SM64EX executable" });
                    if (pick && !pick.canceled && pick.path) setSm64exExePath(pick.path);
                  }}
                >
                  Browse
                </button>
              </div>
            </label>
          </div>

          <div style={{ display: "flex", gap: 12, flexWrap: "wrap", marginTop: 12 }}>
            <label style={{ display: "flex", flexDirection: "column", gap: 6, flex: "1 1 420px" }}>
              Root directory (optional search fallback)
              <div style={{ display: "flex", gap: 8 }}>
                <input className="input" value={sm64exRootDir} onChange={(e) => setSm64exRootDir(e.target.value)} style={{ flex: 1 }} />
                <button
                  className="skl-btn ghost"
                  type="button"
                  onClick={async () => {
                    const pick = await runtime.pickFolder?.({ title: "Select SM64EX root directory" });
                    if (pick && !pick.canceled && pick.path) setSm64exRootDir(pick.path);
                  }}
                >
                  Browse
                </button>
              </div>
            </label>
          </div>

          <div style={{ marginTop: 12 }}>
            <div style={{ fontSize: 12, opacity: 0.8, marginBottom: 6 }}>Extra SM64EX args (one per line):</div>
            <textarea
              className="input"
              value={sm64exArgsText}
              onChange={(e) => setSm64exArgsText(e.target.value)}
              style={{ width: "100%", minHeight: 70, fontFamily: "monospace" }}
              placeholder={"Example:\n--fullscreen"}
            />
          </div>

          <div style={{ display: "flex", gap: 10, flexWrap: "wrap", marginTop: 12 }}>
            <button className="skl-btn primary" type="button" onClick={saveSm64Ex}>
              Save SM64EX
            </button>
          </div>
            </div>
          </details>

          <details style={{ marginTop: 12 }}>
            <summary style={{ cursor: "pointer", fontWeight: 700 }}>Ship of Harkinian (oot_soh)</summary>
            <div style={{ marginTop: 10 }}>
          <div style={{ fontSize: 12, opacity: 0.85, marginBottom: 10 }}>
            Native PC port with built-in Archipelago support and built-in tracker. No external tracker is launched by SekaiLink.
          </div>

          <div style={{ display: "flex", gap: 12, flexWrap: "wrap" }}>
            <label style={{ display: "flex", flexDirection: "column", gap: 6, flex: "1 1 420px" }}>
              Executable path (recommended)
              <div style={{ display: "flex", gap: 8 }}>
                <input className="input" value={sohExePath} onChange={(e) => setSohExePath(e.target.value)} style={{ flex: 1 }} />
                <button
                  className="skl-btn ghost"
                  type="button"
                  onClick={async () => {
                    const pick = await runtime.pickFile?.({ title: "Select Ship of Harkinian executable" });
                    if (pick && !pick.canceled && pick.path) setSohExePath(pick.path);
                  }}
                >
                  Browse
                </button>
              </div>
            </label>
          </div>

          <div style={{ display: "flex", gap: 12, flexWrap: "wrap", marginTop: 12 }}>
            <label style={{ display: "flex", flexDirection: "column", gap: 6, flex: "1 1 420px" }}>
              Root directory (optional search/install fallback)
              <div style={{ display: "flex", gap: 8 }}>
                <input className="input" value={sohRootDir} onChange={(e) => setSohRootDir(e.target.value)} style={{ flex: 1 }} />
                <button
                  className="skl-btn ghost"
                  type="button"
                  onClick={async () => {
                    const pick = await runtime.pickFolder?.({ title: "Select Ship of Harkinian root directory" });
                    if (pick && !pick.canceled && pick.path) setSohRootDir(pick.path);
                  }}
                >
                  Browse
                </button>
              </div>
            </label>
          </div>

          <label style={{ display: "flex", gap: 10, alignItems: "center", marginTop: 12 }}>
            <input type="checkbox" checked={sohAutoInstall} onChange={(e) => setSohAutoInstall(e.target.checked)} />
            Auto-install latest Archipelago-SoH release when executable is missing
          </label>

          <div style={{ marginTop: 12 }}>
            <div style={{ fontSize: 12, opacity: 0.8, marginBottom: 6 }}>Extra SoH args (one per line):</div>
            <textarea
              className="input"
              value={sohArgsText}
              onChange={(e) => setSohArgsText(e.target.value)}
              style={{ width: "100%", minHeight: 70, fontFamily: "monospace" }}
              placeholder={"Example:\n--some-flag"}
            />
          </div>

          <div style={{ display: "flex", gap: 10, flexWrap: "wrap", marginTop: 12 }}>
            <button className="skl-btn primary" type="button" onClick={saveSoh}>
              Save SoH
            </button>
          </div>
            </div>
          </details>

          {gamesStatus && <div className="skl-ready-status" style={{ marginTop: 10 }}>{gamesStatus}</div>}
        </div>
      </div>
      )}

      {settingsTab === "video" && (
      <div className="skl-gm-panel active" role="tabpanel" style={{ marginTop: 16 }}>
        <div className="skl-gm-section-header">
          <h2>Layout (X11/XWayland)</h2>
        </div>
        <div className="yaml-dashboard-card">
          <p style={{ marginTop: 0 }}>
            wmctrl detected: <strong>{wmctrlDetected.exists ? "Yes" : "No"}</strong>
            {wmctrlDetected.path ? <span style={{ opacity: 0.75 }}> ({wmctrlDetected.path})</span> : null}
          </p>
          <p style={{ fontSize: 12, opacity: 0.85, marginTop: -6 }}>
            Window positioning is best-effort and only works on X11/XWayland. Wayland-native apps may not be movable.
          </p>

          <div style={{ display: "flex", gap: 12, flexWrap: "wrap", marginTop: 12 }}>
            <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
              Preset
              <input className="input" value={layoutPreset} onChange={(e) => setLayoutPreset(e.target.value)} style={{ width: 160 }} />
            </label>
            <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
              Mode
              <select value={layoutMode} onChange={(e) => setLayoutMode((e.target.value as any) || "")} style={{ width: 180 }}>
                <option value="">Auto</option>
                <option value="side_by_side">Side-by-side</option>
                <option value="separate_displays">Separate displays</option>
              </select>
            </label>
            <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
              Split
              <input className="input" value={layoutSplit} onChange={(e) => setLayoutSplit(e.target.value)} style={{ width: 90 }} />
            </label>
          </div>

          <div style={{ display: "flex", gap: 12, flexWrap: "wrap", marginTop: 12 }}>
            <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
              Game display
              <select value={layoutGameDisplay} onChange={(e) => setLayoutGameDisplay(e.target.value)} style={{ width: 260 }}>
                {displays.map((d) => (
                  <option key={String(d.id)} value={String(d.index)}>
                    {`#${d.index} ${d.workArea?.width || d.bounds?.width}x${d.workArea?.height || d.bounds?.height}`}
                  </option>
                ))}
                {!displays.length && <option value="0">#0</option>}
              </select>
            </label>
            <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
              Tracker display
              <select value={layoutTrackerDisplay} onChange={(e) => setLayoutTrackerDisplay(e.target.value)} style={{ width: 260 }}>
                {displays.map((d) => (
                  <option key={String(d.id) + "-t"} value={String(d.index)}>
                    {`#${d.index} ${d.workArea?.width || d.bounds?.width}x${d.workArea?.height || d.bounds?.height}`}
                  </option>
                ))}
                {!displays.length && <option value="1">#1</option>}
              </select>
            </label>
          </div>

          <div style={{ display: "flex", gap: 10, flexWrap: "wrap", marginTop: 12 }}>
            <button className="skl-btn primary" type="button" onClick={() => saveLayout()}>
              Save Layout
            </button>
            <button className="skl-btn ghost" type="button" onClick={() => saveLayout("handheld")}>
              Apply Handheld Preset
            </button>
            <button className="skl-btn ghost" type="button" onClick={() => saveLayout("desktop")}>
              Apply Desktop (Side-by-side)
            </button>
            <button className="skl-btn ghost" type="button" onClick={() => saveLayout("desktop_dual")}>
              Apply Dual Display
            </button>
            <button className="skl-btn ghost" type="button" onClick={() => saveLayout("streamer_dual")}>
              Apply Streamer Dual
            </button>
          </div>
          {layoutStatus && <div className="skl-ready-status" style={{ marginTop: 10 }}>{layoutStatus}</div>}
        </div>
      </div>
      )}

      {settingsTab === "audio" && (
      <div className="skl-gm-panel active" role="tabpanel" style={{ marginTop: 16 }}>
        <div className="skl-gm-section-header">
          <h2>{t("settings.tab.audio")}</h2>
        </div>
        <div className="yaml-dashboard-card">
          <div style={{ display: "grid", gap: 12 }}>
            <div style={{ display: "flex", gap: 12, flexWrap: "wrap", alignItems: "center" }}>
              <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
                <input
                  type="checkbox"
                  checked={audioGlobalMuted}
                  onChange={(e) => {
                    const api = sfxApi();
                    api?.setGlobalMuted?.(e.target.checked);
                    setAudioGlobalMuted(e.target.checked);
                  }}
                />
                {t("settings.mute_all_sounds")}
              </label>
              <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
                {t("settings.global_volume")}
                <input
                  type="range"
                  min={0}
                  max={1}
                  step={0.01}
                  value={audioGlobalVolume}
                  onChange={(e) => {
                    const v = Number(e.target.value);
                    const api = sfxApi();
                    api?.setGlobalVolume?.(v);
                    setAudioGlobalVolume(v);
                  }}
                />
                <span style={{ minWidth: 44 }}>{Math.round(audioGlobalVolume * 100)}%</span>
              </label>
            </div>

            <div style={{ borderTop: "1px solid rgba(56, 243, 221, 0.14)", paddingTop: 10 }}>
              <div style={{ fontWeight: 700, marginBottom: 8 }}>{t("settings.background_music")}</div>
              <div style={{ display: "flex", gap: 12, flexWrap: "wrap", alignItems: "center" }}>
                <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
                  <input
                    type="checkbox"
                    checked={audioBgmEnabled}
                    onChange={(e) => {
                      const api = sfxApi();
                      api?.setBgmEnabled?.(e.target.checked);
                      setAudioBgmEnabled(e.target.checked);
                    }}
                  />
                  {t("settings.enable_bgm_loop")}
                </label>
                <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
                  {t("settings.track")}
                  <select
                    value={audioBgmTrack}
                    onChange={(e) => {
                      const id = e.target.value;
                      const api = sfxApi();
                      api?.setBgmTrack?.(id);
                      setAudioBgmTrack(id);
                    }}
                  >
                    {audioBgmTracks.map((t) => (
                      <option key={t.id} value={t.id}>{t.label}</option>
                    ))}
                  </select>
                </label>
                <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
                  {t("settings.bgm_volume")}
                  <input
                    type="range"
                    min={0}
                    max={1}
                    step={0.01}
                    value={audioBgmVolume}
                    onChange={(e) => {
                      const v = Number(e.target.value);
                      const api = sfxApi();
                      api?.setBgmVolume?.(v);
                      setAudioBgmVolume(v);
                    }}
                  />
                  <span style={{ minWidth: 44 }}>{Math.round(audioBgmVolume * 100)}%</span>
                </label>
                <button className="skl-btn ghost" type="button" onClick={() => sfxApi()?.startBgm?.()}>{t("settings.play_bgm")}</button>
                <button className="skl-btn ghost" type="button" onClick={() => sfxApi()?.stopBgm?.()}>{t("settings.stop_bgm")}</button>
              </div>
            </div>

            <div style={{ borderTop: "1px solid rgba(56, 243, 221, 0.14)", paddingTop: 10 }}>
              <div style={{ fontWeight: 700, marginBottom: 8 }}>{t("settings.ui_sound_events")}</div>
              <div style={{ maxHeight: 280, overflow: "auto", border: "1px solid rgba(56, 243, 221, 0.16)", borderRadius: 10, padding: 8 }}>
                {audioSoundKeys.map((key) => (
                  <div key={key} style={{ display: "grid", gridTemplateColumns: "180px 1fr auto auto", gap: 10, alignItems: "center", padding: "6px 0" }}>
                    <div style={{ fontFamily: "monospace", fontSize: 12 }}>{key}</div>
                    <input
                      type="range"
                      min={0}
                      max={1}
                      step={0.01}
                      value={audioSoundVolume[key] ?? 1}
                      onChange={(e) => {
                        const v = Number(e.target.value);
                        const api = sfxApi();
                        api?.setSoundVolume?.(key, v);
                        setAudioSoundVolume((prev) => ({ ...prev, [key]: v }));
                      }}
                    />
                    <label style={{ display: "inline-flex", gap: 8, alignItems: "center" }}>
                      <input
                        type="checkbox"
                        checked={Boolean(audioSoundMuted[key])}
                        onChange={(e) => {
                          const checked = e.target.checked;
                          const api = sfxApi();
                          api?.setSoundMuted?.(key, checked);
                          setAudioSoundMuted((prev) => ({ ...prev, [key]: checked }));
                        }}
                      />
                      {t("settings.mute")}
                    </label>
                    <button className="skl-btn ghost" type="button" onClick={() => sfxApi()?.play?.(key, 1)}>{t("settings.test")}</button>
                  </div>
                ))}
              </div>
            </div>
          </div>
        </div>
      </div>
      )}

      {settingsTab === "social" && (
      <div className="skl-gm-panel active" role="tabpanel" style={{ marginTop: 16 }}>
        <div className="skl-gm-section-header">
          <h2>{t("settings.tab.social")}</h2>
        </div>
        <div className="yaml-dashboard-card">
          <p>{t("settings.social_preferences_text")}</p>
          <p style={{ opacity: 0.8 }}>
            {t("settings.social_actions_hint")}
          </p>
        </div>
      </div>
      )}
    </div>
  );
};

export default SettingsPage;
