import { useEffect, useRef, useState } from "react";
import { getLaunchErrorMessage, getLaunchRecoveryAction, type LaunchRecoveryAction } from "../services/launchRecovery";
import { runtime } from "../services/runtime";

export type SessionManualDownload = {
  path: string;
  ext?: string;
  moduleId?: string;
  apGameName?: string;
  installedPath?: string;
  note?: string;
};

export type TrackerVariantModalState = {
  requestId: string;
  title: string;
  message: string;
  gameLabel: string;
  detail?: string;
  choices: Array<{ id: string; name: string }>;
  selected: string;
};

type UseSessionLaunchStateOptions = {
  onWarning?: (text: string) => void;
  onSm64ManualTutorial?: () => void;
};

export const useSessionLaunchState = (options: UseSessionLaunchStateOptions = {}) => {
  const { onWarning, onSm64ManualTutorial } = options;
  const [launchStatus, setLaunchStatus] = useState<string>("");
  const [manualDownload, setManualDownload] = useState<SessionManualDownload | null>(null);
  const [launchModalOpen, setLaunchModalOpen] = useState(false);
  const [launchError, setLaunchError] = useState<string | null>(null);
  const [launchRecoveryAction, setLaunchRecoveryAction] = useState<LaunchRecoveryAction | null>(null);
  const [launchProgress, setLaunchProgress] = useState<number>(0);
  const [launchDownloadPct, setLaunchDownloadPct] = useState<number | null>(null);
  const [trackerVariantModal, setTrackerVariantModal] = useState<TrackerVariantModalState | null>(null);
  const trackerVariantRespondingRef = useRef(false);

  useEffect(() => {
    const off = runtime.onSessionEvent?.((data: unknown) => {
      const evt = data as any;
      if (!evt || typeof evt !== "object") return;
      if (evt.event === "status" && typeof evt.status === "string") {
        setLaunchStatus(evt.status);
        setLaunchError(null);
        if (!String(evt.status || "").toLowerCase().includes("downloading")) {
          setLaunchDownloadPct(null);
        }
        setLaunchModalOpen(true);

        const s = evt.status.toLowerCase();
        if (s.includes("starting")) setLaunchProgress(6);
        else if (s.includes("preparing launch runtime")) setLaunchProgress(10);
        else if (s.includes("preparing launch runtime component")) setLaunchProgress(14);
        else if (s.includes("downloading")) setLaunchProgress(18);
        else if (s.includes("reusing prepared game image")) setLaunchProgress(34);
        else if (s.includes("preparing game image") || s.includes("patching")) setLaunchProgress(38);
        else if (s.includes("preparing snes connection")) setLaunchProgress(64);
        else if (s.includes("launching") || s.includes("checking oot connector") || s.includes("checking sni device")) setLaunchProgress(56);
        else if (s.includes("connecting")) setLaunchProgress(72);
        else if (s.includes("launching tracker")) setLaunchProgress(88);
        else if (s.includes("ready")) setLaunchProgress(100);
      } else if (evt.event === "download-progress") {
        const pct = Number.isFinite(Number(evt.percent)) ? Number(evt.percent) : null;
        setLaunchDownloadPct(pct);
        if (pct !== null) {
          const start = 18;
          const end = 38;
          const scaled = start + Math.max(0, Math.min(1, pct / 100)) * (end - start);
          setLaunchProgress(scaled);
        }
      } else if (evt.event === "ready") {
        setLaunchStatus("Ready.");
        setManualDownload(null);
        setLaunchModalOpen(false);
        setLaunchError(null);
        setLaunchProgress(100);
        setLaunchDownloadPct(null);
      } else if (evt.event === "manual" && typeof evt.downloadedPath === "string" && evt.downloadedPath) {
        const ext = typeof evt.ext === "string" ? String(evt.ext) : "";
        const apGameName = typeof evt.apGameName === "string" ? String(evt.apGameName) : "";
        setManualDownload({
          path: String(evt.downloadedPath),
          ext,
          moduleId: typeof evt.moduleId === "string" ? evt.moduleId : "",
          apGameName,
          installedPath: typeof evt.installedPath === "string" ? evt.installedPath : "",
          note: typeof evt.note === "string" ? evt.note : "",
        });
        setLaunchStatus("Manual action required.");
        setLaunchModalOpen(false);
        setLaunchDownloadPct(null);
        if (onSm64ManualTutorial && (ext === ".apsm64ex" || apGameName.trim().toLowerCase() === "super mario 64")) {
          onSm64ManualTutorial();
        }
      } else if (evt.event === "error") {
        setLaunchError(getLaunchErrorMessage(evt as any));
        setLaunchRecoveryAction(getLaunchRecoveryAction(evt as any));
        setLaunchStatus("");
        setLaunchProgress(0);
        setLaunchDownloadPct(null);
      } else if (evt.event === "warning") {
        const err = String(evt.error || "warning");
        if (!["wmctrl_missing", "no_pack", "pack_missing", "poptracker_missing"].includes(err)) {
          onWarning?.(`Warning: ${err}`);
        }
      } else if (evt.event === "tracker_variant_request") {
        const choices = Array.isArray(evt.choices) ? evt.choices : [];
        const normalized = choices
          .map((c: any) => ({
            id: String(c?.id || ""),
            name: String(c?.name || c?.id || "Variant"),
          }))
          .filter((c: { id: string; name: string }) => c.name);
        setTrackerVariantModal({
          requestId: String(evt.requestId || ""),
          title: String(evt.title || "Choose PopTracker Layout"),
          message: String(evt.message || "Select a tracker layout."),
          gameLabel: String(evt.gameLabel || ""),
          detail: String(evt.detail || ""),
          choices: normalized,
          selected: String(normalized[0]?.id || ""),
        });
      }
    });
    return () => {
      if (typeof off === "function") off();
    };
  }, [onSm64ManualTutorial, onWarning]);

  const beginLaunch = () => {
    setManualDownload(null);
    setLaunchError(null);
    setLaunchRecoveryAction(null);
    setLaunchModalOpen(true);
    setLaunchStatus("Starting...");
    setLaunchProgress(6);
    setLaunchDownloadPct(null);
  };

  const reportLaunchFailure = (message: string, recoveryAction: LaunchRecoveryAction | null) => {
    setLaunchRecoveryAction(recoveryAction);
    setLaunchError(message);
    setLaunchStatus("");
    setLaunchProgress(0);
    setLaunchDownloadPct(null);
  };

  const setLaunchManual = (download: SessionManualDownload) => {
    setManualDownload(download);
    setLaunchStatus("Manual action required.");
    setLaunchModalOpen(false);
    setLaunchDownloadPct(null);
  };

  const setLaunchReady = () => {
    setLaunchStatus("Ready.");
    setLaunchRecoveryAction(null);
  };

  const closeLaunchModal = () => {
    setLaunchModalOpen(false);
    setLaunchError(null);
    setLaunchRecoveryAction(null);
    setLaunchStatus("");
  };

  const respondTrackerVariantModal = async (cancel: boolean) => {
    if (trackerVariantRespondingRef.current) return;
    const modal = trackerVariantModal;
    if (!modal) return;
    trackerVariantRespondingRef.current = true;
    setTrackerVariantModal(null);
    try {
      await runtime.sessionTrackerVariantResponse?.({
        requestId: modal.requestId,
        cancel,
        variant: cancel ? "" : modal.selected,
      });
    } catch {
      // ignore
    } finally {
      trackerVariantRespondingRef.current = false;
    }
  };

  return {
    launchStatus,
    manualDownload,
    launchModalOpen,
    launchError,
    launchRecoveryAction,
    launchProgress,
    launchDownloadPct,
    trackerVariantModal,
    setTrackerVariantModal,
    beginLaunch,
    reportLaunchFailure,
    setLaunchManual,
    setLaunchReady,
    closeLaunchModal,
    respondTrackerVariantModal,
  };
};
