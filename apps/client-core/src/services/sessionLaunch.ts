import { getLaunchErrorMessage, getLaunchRecoveryAction, type LaunchRecoveryAction } from "./launchRecovery";
import { API_BASE_URL, getDesktopToken, getDeviceId } from "./api";
import { runtime, type SessionLaunchPlanResult } from "./runtime";

export type SessionLaunchManualDownload = {
  path: string;
  ext: string;
  moduleId: string;
  apGameName: string;
  installedPath: string;
  note: string;
};

export type SessionLaunchRequest = {
  downloadUrl?: string;
  serverAddress: string;
  slot: string;
  password?: string;
  apGameName?: string;
  forceTrackerVariantPrompt?: boolean;
  chatBridge?: {
    channelId?: string;
    lobbyId?: string;
  };
};

export type SessionLaunchOutcome =
  | {
      ok: true;
      plan: SessionLaunchPlanResult;
      status: "ready";
    }
  | {
      ok: true;
      plan: SessionLaunchPlanResult;
      status: "manual";
      manualDownload: SessionLaunchManualDownload;
    }
  | {
      ok: false;
      plan?: SessionLaunchPlanResult | null;
      errorMessage: string;
      recoveryAction: LaunchRecoveryAction | null;
    };

export const prepareSessionLaunch = async (
  options: Pick<SessionLaunchRequest, "downloadUrl" | "apGameName">
): Promise<SessionLaunchPlanResult> => {
  if (!runtime.planSessionAutoLaunch) {
    return { ok: false, error: "launch_planner_unavailable" };
  }
  return await runtime.planSessionAutoLaunch({
    downloadUrl: String(options.downloadUrl || "").trim(),
    apGameName: String(options.apGameName || "").trim(),
  });
};

export const executeSessionLaunch = async (request: SessionLaunchRequest): Promise<SessionLaunchOutcome> => {
  if (!runtime.sessionAutoLaunch) {
    return {
      ok: false,
      plan: null,
      errorMessage: "SekaiLink launch integration is unavailable in this build.",
      recoveryAction: null,
    };
  }

  const plan = await prepareSessionLaunch({
    downloadUrl: request.downloadUrl,
    apGameName: request.apGameName,
  });

  if (!plan.ok) {
    return {
      ok: false,
      plan,
      errorMessage: getLaunchErrorMessage(plan),
      recoveryAction: getLaunchRecoveryAction(plan),
    };
  }

  const launchRes = await runtime.sessionAutoLaunch({
    downloadUrl: String(request.downloadUrl || "").trim(),
    serverAddress: request.serverAddress,
    slot: request.slot,
    password: request.password,
    apGameName: request.apGameName,
    forceTrackerVariantPrompt: request.forceTrackerVariantPrompt === true,
    chatBridge: request.chatBridge,
    apiBaseUrl: API_BASE_URL,
    authToken: getDesktopToken(),
    deviceId: getDeviceId(),
  });

  const launchData = launchRes as any;
  if (!launchData?.ok) {
    return {
      ok: false,
      plan,
      errorMessage: getLaunchErrorMessage(launchData),
      recoveryAction: getLaunchRecoveryAction(launchData),
    };
  }

  if (launchData?.manual && typeof launchData?.downloadedPath === "string" && launchData.downloadedPath) {
    return {
      ok: true,
      plan,
      status: "manual",
      manualDownload: {
        path: String(launchData.downloadedPath),
        ext: typeof launchData.ext === "string" ? launchData.ext : "",
        moduleId: typeof launchData.moduleId === "string" ? launchData.moduleId : "",
        apGameName: String(request.apGameName || ""),
        installedPath: typeof launchData.installedPath === "string" ? launchData.installedPath : "",
        note: typeof launchData.note === "string" ? launchData.note : "",
      },
    };
  }

  return {
    ok: true,
    plan,
    status: "ready",
  };
};
