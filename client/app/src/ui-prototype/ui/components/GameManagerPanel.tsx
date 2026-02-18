import React from "react";
import { Link, useNavigate } from "react-router-dom";
import { useI18n } from "@/i18n";

type GameRow = { name: string; sub: string };

type Props = {
  games: GameRow[];
  onSelectGame: (name: string) => void;
};

const GameManagerPanel: React.FC<Props> = ({ games, onSelectGame }) => {
  const { t } = useI18n();
  const navigate = useNavigate();

  return (
    <section className="sklp-panel sklp-gm">
      <header className="sklp-panel-head">
        <div>
          <div className="sklp-title">{t("gm.title").toUpperCase()}</div>
          <div className="sklp-subtitle">{t("proto.game_manager.manage").toUpperCase()}</div>
        </div>
        <Link className="sklp-linkbtn" to="/dashboard/yaml/new?tab=add">{t("gm.add_game").toUpperCase()}</Link>
      </header>
      <div className="sklp-gm-list">
        {games.map((g) => (
          <div key={g.name} className="sklp-gm-row">
            <div>
              <div className="sklp-gm-name">{g.name}</div>
              <div className="sklp-gm-sub">{g.sub}</div>
            </div>
            <button
              type="button"
              className="sklp-edit"
              onClick={() => {
                navigate("/dashboard/yaml/new");
                onSelectGame(g.name);
              }}
            >
              {t("proto.edit").toUpperCase()}
            </button>
          </div>
        ))}
      </div>
    </section>
  );
};

export default GameManagerPanel;
