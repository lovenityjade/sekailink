import React, { useEffect } from "react";
import { useI18n } from "../i18n";

interface BootScreenProps {
  status?: string;
  progress?: number;
  version?: string;
}

const BootScreen: React.FC<BootScreenProps> = ({ status, progress, version }) => {
  const { t } = useI18n();
  // progress is 0..1 (best-effort). Keep the meter honest; no fake minimum.
  const pct = Math.min(100, Math.max(0, Math.round((progress ?? 0) * 100)));

  useEffect(() => {
    const w = window as Window & { __sklBootSfxPlayed?: boolean; SKL_SFX?: { play?: (name: string, volume?: number) => void } };
    if (w.__sklBootSfxPlayed) return;
    w.__sklBootSfxPlayed = true;
    window.setTimeout(() => {
      try {
        w.SKL_SFX?.play?.("appopen", 0.45);
      } catch {
        // no-op
      }
    }, 120);
  }, []);

  return (
    <>
      <div className="page-bg"></div>
      <div className="skl-boot" id="skl-boot-screen">
        <div className="skl-boot-card">
          <div className="skl-boot-scanline"></div>
          <div className="skl-boot-content">
            <div className="skl-boot-orbit" aria-hidden="true">
              <div className="skl-boot-orbit-ring"></div>
              <div className="skl-boot-orbit-ring inner"></div>
              <div className="skl-boot-orbit-core">
                <img src="assets/img/sekailink-logo-image.png" alt="" />
              </div>
              <div className="skl-boot-orbit-dot dot-a"></div>
              <div className="skl-boot-orbit-dot dot-b"></div>
            </div>
            <div className="skl-boot-logo">
              <img src="assets/img/sekailink-logo-text.png" alt="SekaiLink" />
            </div>
            <p className="skl-boot-text">{t("boot.text")}</p>
            <div className="skl-boot-status" aria-live="polite">
              <span className="skl-boot-status-label">{status || t("boot.status_default")}</span>
              <span className="skl-boot-status-meter">
                <span className="skl-boot-status-fill" style={{ width: `${pct}%` }}></span>
              </span>
            </div>
            <div className="skl-boot-loader">
              <span></span><span></span><span></span>
            </div>
            <div className="skl-boot-version">{version || "v0"}</div>
          </div>
        </div>
      </div>
    </>
  );
};

export default BootScreen;
