import React, { useState } from "react";
import { motion } from "framer-motion";
import { useNavigate } from "react-router-dom";
import { useI18n } from "@/i18n";
import { useSfx } from "@/hooks/useSfx";
import ChamferedPanel from "@/components/ChamferedPanel";

type GameEntry = { id: string; name: string; sub?: string; custom?: boolean; selected?: boolean };
type Props = { games: GameEntry[]; onSelectGame: (game: GameEntry) => void };

const GameManagerPanel: React.FC<Props> = ({ games, onSelectGame }) => {
  const { t } = useI18n();
  const sfx = useSfx();
  const navigate = useNavigate();
  const [contextMenu, setContextMenu] = useState<{ x: number; y: number; game: GameEntry } | null>(null);

  return (
    <>
      <ChamferedPanel title="GAME MANAGER" delay={0.3} className="shrink-0 max-h-[40%] overflow-hidden flex flex-col"
        titleRight={<span className="text-[10px] text-phosphor/20 font-code">{games.length} YAMLs</span>}
      >
        <div className="flex-1 overflow-y-auto space-y-1 pr-1">
          {games.map((game, i) => (
            <motion.button
              key={game.id}
              initial={{ opacity: 0, x: 10 }}
              animate={{ opacity: 1, x: 0 }}
              transition={{ delay: 0.3 + i * 0.06 }}
              onClick={() => { sfx.play("select", 0.2); onSelectGame(game); }}
              onContextMenu={(e) => { e.preventDefault(); setContextMenu({ x: e.clientX, y: e.clientY, game }); }}
              className={`w-full text-left px-3 py-2.5 scan-hover overflow-hidden transition-all group ${game.selected ? "bg-teal/10 border border-teal/20" : "bg-gunmetal-light/30 border border-transparent hover:border-teal/10"}`}
            >
              <div className="flex items-center justify-between gap-2">
                <div className="min-w-0">
                  <div className="text-xs font-header text-phosphor/70 tracking-wider truncate group-hover:text-teal transition-colors">
                    {game.name}
                  </div>
                  <div className="text-[10px] font-code text-phosphor/20 mt-0.5 truncate">{game.sub}</div>
                </div>
                <div className="shrink-0 flex items-center gap-1.5">
                  {game.selected && (
                    <span className="text-[9px] font-header text-teal tracking-wider text-glow">LOADED</span>
                  )}
                  {game.custom && (
                    <span className="text-[9px] font-header text-amber tracking-wider">CUSTOM</span>
                  )}
                </div>
              </div>
            </motion.button>
          ))}

          {games.length === 0 && (
            <div className="text-center py-6 text-[11px] font-code text-phosphor/15">
              No YAMLs loaded. Add one below.
            </div>
          )}
        </div>

        <div className="flex gap-1.5 mt-2">
          <button
            className="flex-1 h-8 panel-chamfer-sm text-teal text-[9px] font-header tracking-widest hover:brightness-125 transition-all btn-charge"
            style={{ background: "linear-gradient(180deg, rgba(0,255,200,0.12) 0%, rgba(0,255,200,0.04) 100%)", border: "1px solid rgba(0,255,200,0.18)", boxShadow: "none" }}
            onClick={() => { sfx.play("confirm", 0.18); navigate("/dashboard/yaml/new"); }}
          >
            MY GAMES
          </button>
        </div>
      </ChamferedPanel>

      {/* Context menu */}
      {contextMenu && (
        <div className="fixed inset-0 z-50" onClick={() => setContextMenu(null)}>
          <div className="absolute bg-gunmetal border border-teal/20 panel-chamfer-sm p-1 min-w-[130px]" style={{ left: contextMenu.x, top: contextMenu.y }} onClick={(e) => e.stopPropagation()}>
            <button className="w-full text-left px-3 py-1.5 text-[11px] font-code text-phosphor/50 hover:bg-teal/10 hover:text-phosphor/80 transition-colors"
              onClick={() => { setContextMenu(null); onSelectGame(contextMenu.game); }}>{contextMenu.game.selected ? t("proto.unselect") : t("proto.select")}</button>
            <button className="w-full text-left px-3 py-1.5 text-[11px] font-code text-phosphor/50 hover:bg-teal/10 hover:text-phosphor/80 transition-colors"
              onClick={() => { setContextMenu(null); navigate(`/dashboard/yaml/new?tab=editor&yaml=${encodeURIComponent(contextMenu.game.id)}`); }}>Edit</button>
          </div>
        </div>
      )}
    </>
  );
};

export default GameManagerPanel;
