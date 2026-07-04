import React from "react";
import type { LaunchRecoveryAction } from "../services/launchRecovery";
import type { TrackerVariantModalState } from "../hooks/useSessionLaunchState";

type Props = {
  launchModalOpen: boolean;
  launchError: string | null;
  launchRecoveryAction: LaunchRecoveryAction | null;
  launchStatus: string;
  launchProgress: number;
  launchDownloadPct: number | null;
  onCloseLaunch: () => void;
  onOpenRecoveryRoute: (route: string) => void;
  trackerVariantModal: TrackerVariantModalState | null;
  onTrackerVariantChange: (variantId: string) => void;
  onTrackerVariantCancel: () => void;
  onTrackerVariantContinue: () => void;
};

const SessionLaunchModals: React.FC<Props> = ({
  launchModalOpen,
  launchError,
  launchRecoveryAction,
  launchStatus,
  launchProgress,
  launchDownloadPct,
  onCloseLaunch,
  onOpenRecoveryRoute,
  trackerVariantModal,
  onTrackerVariantChange,
  onTrackerVariantCancel,
  onTrackerVariantContinue,
}) => {
  return (
    <>
      <div className={`skl-modal${launchModalOpen ? " open" : ""}`} id="lobby-launch-modal" aria-hidden={!launchModalOpen}>
        <div className="skl-modal-backdrop"></div>
        <div className="skl-modal-panel skl-launch-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-launch-title">
          <div className="skl-modal-header">
            <h3 id="lobby-launch-title">{launchError ? "Launch Failed" : "Launching Game"}</h3>
          </div>
          <div className="skl-modal-body skl-launch-modal-body">
            {!launchError && (
              <div className="skl-launch-spinner">
                <svg viewBox="0 0 50 50" className="skl-spinner-svg">
                  <circle cx="25" cy="25" r="20" fill="none" strokeWidth="4" />
                </svg>
              </div>
            )}
            {!launchError && (
              <div className="skl-launch-progress" aria-label="Launch progress">
                <div className="skl-launch-progress-bar" style={{ width: `${Math.max(0, Math.min(100, launchProgress))}%` }} />
              </div>
            )}
            {launchError && (
              <div className="skl-launch-error-icon">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                  <circle cx="12" cy="12" r="10" />
                  <line x1="15" y1="9" x2="9" y2="15" />
                  <line x1="9" y1="9" x2="15" y2="15" />
                </svg>
              </div>
            )}
            <div className="skl-launch-status">
              {launchError || (launchDownloadPct !== null && String(launchStatus || "").toLowerCase().includes("downloading")
                ? `${launchStatus} (${launchDownloadPct.toFixed(1)}%)`
                : launchStatus) || "Initializing..."}
            </div>
          </div>
          {launchError && (
            <div className="skl-modal-actions">
              {launchRecoveryAction?.route && (
                <button
                  className="skl-btn ghost"
                  type="button"
                  onClick={() => onOpenRecoveryRoute(String(launchRecoveryAction.route || "").trim())}
                >
                  {launchRecoveryAction.label}
                </button>
              )}
              <button className="skl-btn primary" type="button" onClick={onCloseLaunch}>
                Close
              </button>
            </div>
          )}
        </div>
      </div>

      <div className={`skl-modal${trackerVariantModal ? " open" : ""}`} id="lobby-tracker-variant-modal" aria-hidden={!trackerVariantModal}>
        <div className="skl-modal-backdrop"></div>
        <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-tracker-variant-title">
          <div className="skl-modal-header">
            <h3 id="lobby-tracker-variant-title">{trackerVariantModal?.title || "Choose PopTracker Layout"}</h3>
          </div>
          <div className="skl-modal-body">
            <p>{trackerVariantModal?.message || "Select a tracker layout."}</p>
            <div style={{ marginTop: 10, display: "grid", gap: 8 }}>
              {(trackerVariantModal?.choices || []).map((choice) => (
                <label key={`${choice.id}-${choice.name}`} style={{ display: "flex", alignItems: "center", gap: 8 }}>
                  <input
                    type="radio"
                    name="tracker-variant-choice"
                    checked={(trackerVariantModal?.selected || "") === choice.id}
                    onChange={() => onTrackerVariantChange(choice.id)}
                  />
                  <span>{choice.name}</span>
                </label>
              ))}
            </div>
          </div>
          <div className="skl-modal-actions">
            <button className="skl-btn ghost" type="button" onClick={onTrackerVariantCancel}>
              Cancel
            </button>
            <button className="skl-btn primary" type="button" onClick={onTrackerVariantContinue}>
              Continue
            </button>
          </div>
        </div>
      </div>
    </>
  );
};

export default SessionLaunchModals;
