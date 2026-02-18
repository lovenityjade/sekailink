import React from "react";
import { useI18n } from "../i18n";

interface AboutModalProps {
  open: boolean;
  onClose: () => void;
}

const AboutModal: React.FC<AboutModalProps> = ({ open, onClose }) => {
  const { t } = useI18n();
  return (
    <div className={`skl-modal${open ? " open" : ""}`} id="skl-about-modal" aria-hidden={!open}>
      <div className="skl-modal-backdrop" onClick={onClose}></div>
      <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="skl-about-title">
        <div className="skl-modal-header">
          <h3 id="skl-about-title">{t("about.title")}</h3>
          <button className="skl-btn ghost" type="button" onClick={onClose}>{t("common.close")}</button>
        </div>
        <div className="skl-modal-body">
          <p><strong>SekaiLink Beta-0.10.0</strong></p>
          <p>{t("about.created_by")}</p>
          <p>{t("about.powered_by")}</p>
          <ul>
            <li>Archipelago (archipelago.gg)</li>
            <li>MultiworldGG (multiworld.gg)</li>
          </ul>
        </div>
      </div>
    </div>
  );
};

export default AboutModal;
