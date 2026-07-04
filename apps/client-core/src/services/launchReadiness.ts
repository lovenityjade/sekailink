import { getLaunchRecoveryAction, type LaunchRecoveryAction } from "./launchRecovery";
import { runtime, type LaunchSetupValidationResult } from "./runtime";

export type SetupConfigSnapshot = {
  roms?: Record<string, string>;
  trackerPacks?: Record<string, { path: string; source?: string }>;
};

export type RuntimeSetupFlags = {
  requiredRomIds: string[];
  romReady: boolean;
  trackerNeeded: boolean;
  trackerReady: boolean;
  ready: boolean;
};

export type LaunchReadiness = LaunchSetupValidationResult & {
  moduleId: string;
  correctionAction: LaunchRecoveryAction | null;
  requiredRomIds: string[];
  romReady: boolean;
  trackerNeeded: boolean;
  trackerReady: boolean;
  ready: boolean;
};

export const deriveRuntimeSetupFlags = (
  manifest: Record<string, any> | null | undefined,
  setupConfig: SetupConfigSnapshot | null | undefined
): RuntimeSetupFlags => {
  const safeManifest = manifest && typeof manifest === "object" ? manifest : {};
  const patcher = String((safeManifest as any).patcher || "")
    .trim()
    .toLowerCase();
  const gameId = String((safeManifest as any).game_id || "").trim();
  const requiredRomIds: string[] = Array.isArray((safeManifest as any).required_roms)
    ? (safeManifest as any).required_roms.map((entry: unknown) => String(entry || "").trim()).filter(Boolean)
    : patcher === "none"
      ? []
      : gameId
        ? [gameId]
        : [];

  const roms = setupConfig?.roms && typeof setupConfig.roms === "object" ? setupConfig.roms : {};
  const trackerPacks =
    setupConfig?.trackerPacks && typeof setupConfig.trackerPacks === "object" ? setupConfig.trackerPacks : {};
  const romReady = requiredRomIds.every((id) => Boolean(roms[id]));
  const trackerNeeded = Boolean((safeManifest as any).tracker_pack_uid);
  const trackerReady = !trackerNeeded || Boolean(gameId && trackerPacks[gameId]);

  return {
    requiredRomIds,
    romReady,
    trackerNeeded,
    trackerReady,
    ready: romReady && trackerReady,
  };
};

export const getLaunchReadiness = async (moduleId: string): Promise<LaunchReadiness> => {
  const safeModuleId = String(moduleId || "").trim();
  const manifestRes = safeModuleId ? await runtime.getModuleManifest?.(safeModuleId) : null;
  const manifest = (manifestRes as any)?.manifest && typeof (manifestRes as any).manifest === "object" ? (manifestRes as any).manifest : {};
  const config = await runtime.configGet?.();
  const gamesConfig = config && typeof config === "object" ? ((config as any).games || {}) : {};
  let setup = safeModuleId
    ? await runtime.validateSetupForModule?.(safeModuleId)
    : ({ ok: false, error: "missing_module_id" } as LaunchSetupValidationResult);

  if (!setup || typeof setup !== "object" || !("ok" in setup)) {
    setup = { ok: false, error: "validation_unavailable" } as LaunchSetupValidationResult;
  }

  if ((setup as LaunchSetupValidationResult).ok) {
    const gameSettings = gamesConfig && typeof gamesConfig === "object" ? gamesConfig : {};
    if (safeModuleId === "sm64ex") {
      const sm64ex = gameSettings.sm64ex && typeof gameSettings.sm64ex === "object" ? gameSettings.sm64ex : {};
      const exePath = String(sm64ex.exe_path || "").trim();
      const rootDir = String(sm64ex.root_dir || "").trim();
      if (!exePath && !rootDir) {
        setup = { ok: false, error: "sm64ex_not_found", setupArea: "paths" };
      }
    } else if (safeModuleId === "oot_soh") {
      const soh = gameSettings.oot_soh && typeof gameSettings.oot_soh === "object" ? gameSettings.oot_soh : {};
      const exePath = String(soh.exe_path || "").trim();
      const rootDir = String(soh.root_dir || "").trim();
      const autoInstall = soh.auto_install !== false;
      if (!exePath && !rootDir && !autoInstall) {
        setup = { ok: false, error: "soh_not_found", setupArea: "paths" };
      }
    } else if (safeModuleId === "gzdoom") {
      const gzdoom = gameSettings.gzdoom && typeof gameSettings.gzdoom === "object" ? gameSettings.gzdoom : {};
      const iwadPath = String(gzdoom.iwad_path || "").trim();
      const gzapPk3Path = String(gzdoom.gzap_pk3_path || "").trim();
      if (!iwadPath) {
        setup = { ok: false, error: "iwad_missing", setupArea: "paths" };
      } else if (!gzapPk3Path) {
        setup = { ok: false, error: "gzap_pk3_missing", setupArea: "paths" };
      }
    }
  }

  const flags = deriveRuntimeSetupFlags(manifest, null);
  const setupResult =
    setup && typeof setup === "object" && "ok" in setup
      ? (setup as LaunchSetupValidationResult)
      : ({ ok: false, error: "validation_unavailable" } as LaunchSetupValidationResult);

  return {
    ...setupResult,
    moduleId: safeModuleId,
    manifest,
    correctionAction: getLaunchRecoveryAction(setupResult),
    requiredRomIds: flags.requiredRomIds,
    romReady: setupResult.ok ? true : flags.romReady,
    trackerNeeded: flags.trackerNeeded,
    trackerReady: setupResult.ok ? true : flags.trackerReady,
    ready: Boolean(setupResult.ok),
  };
};
