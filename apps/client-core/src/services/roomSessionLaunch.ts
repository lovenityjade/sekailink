import { resolvePlayerApGameName, resolvePlayerSlotId } from "./lobbyLaunchContext";
import { type LaunchRecoveryAction } from "./launchRecovery";
import {
  executeSessionLaunch,
  type SessionLaunchManualDownload,
} from "./sessionLaunch";
import {
  resolveLaunchRoomServer,
  type RoomServerGeneration,
  type RoomServerStatus,
} from "./roomServerContext";

export type RoomSessionLaunchOutcome =
  | {
      ok: true;
      status: "ready";
    }
  | {
      ok: true;
      status: "manual";
      manualDownload: SessionLaunchManualDownload;
    }
  | {
      ok: false;
      surface: "toast";
      message: string;
    }
  | {
      ok: false;
      surface: "launch";
      errorMessage: string;
      recoveryAction: LaunchRecoveryAction | null;
    };

type RoomSessionLaunchOptions<TGeneration extends RoomServerGeneration> = {
  downloadUrls?: string | string[];
  roomStatus?: RoomServerStatus | null;
  generation?: TGeneration | null;
  host: string;
  fetchRoomStatus: (roomUrl?: string, retries?: number) => Promise<RoomServerStatus | null>;
  loadLatestGeneration: () => Promise<TGeneration | null>;
  playerName?: string;
  playerAlias?: string;
  apGameName?: string;
  playersByName: Map<string, number>;
  password?: string;
  forceTrackerVariantPrompt?: boolean;
  lobbyId?: string;
  onLaunchBegin?: () => void;
};

const normalizeDownloadUrls = (downloadUrls?: string | string[]) =>
  Array.isArray(downloadUrls) ? downloadUrls : downloadUrls ? [downloadUrls] : [];

export const executeRoomSessionLaunch = async <TGeneration extends RoomServerGeneration>({
  downloadUrls,
  roomStatus,
  generation,
  host,
  fetchRoomStatus,
  loadLatestGeneration,
  playerName,
  playerAlias,
  apGameName: preferredApGameName,
  playersByName,
  password,
  forceTrackerVariantPrompt,
  lobbyId,
  onLaunchBegin,
}: RoomSessionLaunchOptions<TGeneration>): Promise<RoomSessionLaunchOutcome> => {
  const urls = normalizeDownloadUrls(downloadUrls);
  const slot = String(playerName || "").trim();
  if (!slot) {
    return {
      ok: false,
      surface: "toast",
      message: "Player slot name not available.",
    };
  }

  const roomLaunchContext = await resolveLaunchRoomServer({
    roomStatus,
    generation,
    host,
    fetchRoomStatus,
    loadLatestGeneration,
  });

  if (!roomLaunchContext.serverAddress) {
    return {
      ok: false,
      surface: "toast",
      message: "Room server is not ready yet.",
    };
  }

  const slotId = resolvePlayerSlotId(playersByName, slot);
  const apGameName = String(preferredApGameName || "").trim() || resolvePlayerApGameName(roomLaunchContext.roomStatus, slotId, slot);

  const runSingleLaunch = async (downloadUrl: string) => {
    onLaunchBegin?.();
    return await executeSessionLaunch({
      downloadUrl,
      serverAddress: roomLaunchContext.serverAddress,
      slot,
      playerAlias,
      password,
      apGameName,
      forceTrackerVariantPrompt,
      chatBridge: lobbyId
        ? {
            channelId: `lobby:${lobbyId}`,
            lobbyId,
          }
        : undefined,
    });
  };

  if (!urls.length) {
    if (!apGameName) {
      return {
        ok: false,
        surface: "toast",
        message: "No patch file available for this player.",
      };
    }
    try {
      const launchRes = await runSingleLaunch("");
      if (!launchRes.ok) {
        return {
          ok: false,
          surface: "launch",
          errorMessage: launchRes.errorMessage,
          recoveryAction: launchRes.recoveryAction,
        };
      }
      if (launchRes.status === "manual") {
        return {
          ok: true,
          status: "manual",
          manualDownload: launchRes.manualDownload,
        };
      }
      return { ok: true, status: "ready" };
    } catch (err) {
      return {
        ok: false,
        surface: "launch",
        errorMessage: err instanceof Error ? err.message : "Launch failed.",
        recoveryAction: null,
      };
    }
  }

  for (const downloadUrl of urls) {
    try {
      const launchRes = await runSingleLaunch(downloadUrl);
      if (!launchRes.ok) {
        return {
          ok: false,
          surface: "launch",
          errorMessage: launchRes.errorMessage,
          recoveryAction: launchRes.recoveryAction,
        };
      }
      if (launchRes.status === "manual") {
        return {
          ok: true,
          status: "manual",
          manualDownload: launchRes.manualDownload,
        };
      }
    } catch (err) {
      return {
        ok: false,
        surface: "launch",
        errorMessage: err instanceof Error ? err.message : "Launch failed.",
        recoveryAction: null,
      };
    }
  }

  return { ok: true, status: "ready" };
};
