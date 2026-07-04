import React from "react";
import { apiUrl } from "../services/api";
import { useI18n } from "../i18n";

const HelpPage: React.FC = () => {
  const { t } = useI18n();
  return (
    <div className="skl-app-panel">
      <div className="skl-gm-header">
        <h1>{t("help.title")}</h1>
        <p>{t("help.desc")}</p>
        <a className="skl-btn primary" href={apiUrl("/help")}>{t("help.open_web")}</a>
      </div>
    </div>
  );
};

export default HelpPage;
