import React from "react";
import { useI18n } from "../i18n";

interface ContributeModalProps {
  open: boolean;
  onClose: () => void;
}

const ContributeModal: React.FC<ContributeModalProps> = ({ open, onClose }) => {
  const { t } = useI18n();
  return (
    <div className={`skl-modal${open ? " open" : ""}`} id="skl-contribute-modal" aria-hidden={!open}>
      <div className="skl-modal-backdrop" onClick={onClose}></div>
      <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="skl-contribute-title">
        <div className="skl-modal-header">
          <h3 id="skl-contribute-title">{t("contribute.title")}</h3>
          <button className="skl-btn ghost" type="button" onClick={onClose}>{t("common.close")}</button>
        </div>
        <div className="skl-modal-body">
          <p>{t("contribute.line1")}</p>
          <p>{t("contribute.line2")}</p>
        </div>
      </div>
    </div>
  );
};

export default ContributeModal;
