import React, { useEffect, useState } from "react";
import SeedConfigModal from "@/components/SeedConfigModal";
import {
  addActiveSeed,
  ALTTP_SHOWCASE_GAME,
  listSeedGames,
  loadActiveSeeds,
  type SeedEntry,
  type SeedGameEntry,
} from "@/services/seedConfig";

const LinkedWorldLibraryPage: React.FC = () => {
  const [games, setGames] = useState<SeedGameEntry[]>([ALTTP_SHOWCASE_GAME]);
  const [selectedGame, setSelectedGame] = useState<SeedGameEntry>(ALTTP_SHOWCASE_GAME);
  const [activeSeeds, setActiveSeeds] = useState<SeedEntry[]>(() => loadActiveSeeds());
  const [seedModal, setSeedModal] = useState<{ open: boolean; mode: "add" | "manage" | "select" }>({
    open: false,
    mode: "select",
  });

  useEffect(() => {
    let cancelled = false;
    const load = async () => {
      const nextGames = await listSeedGames();
      if (cancelled) return;
      setGames(nextGames.length ? nextGames : [ALTTP_SHOWCASE_GAME]);
      setSelectedGame(nextGames[0] || ALTTP_SHOWCASE_GAME);
    };
    void load();
    return () => {
      cancelled = true;
    };
  }, []);

  const openSeedModal = (mode: "add" | "manage" | "select") => {
    setSeedModal({ open: true, mode });
  };

  return (
    <div className="sklw-os-page">
      <div className="sklw-orbit-grid" aria-hidden="true" />

      <section className="sklw-command-deck sklw-library-showcase">
        <div className="sklw-world-info">
          <div className="sklw-title-line">
            <span>Showcase</span>
          </div>
          <h2>{selectedGame.display_name}</h2>
          <p>{selectedGame.description || ALTTP_SHOWCASE_GAME.description}</p>
        </div>

        <div className="sklw-showcase-art" aria-hidden="true">
          <img src={selectedGame.boxart || ALTTP_SHOWCASE_GAME.boxart} alt="" />
        </div>

        <div className="sklw-actions">
          <button type="button" className="sklw-primary-action" onClick={() => openSeedModal("add")}>
            <span>Add a Seed</span>
            <small>Easy or Advanced config</small>
          </button>
          <button type="button" onClick={() => openSeedModal("manage")}>Manage Seeds</button>
          <button type="button" onClick={() => openSeedModal("select")}>Select Seed</button>
        </div>
      </section>

      <section className="sklw-seed-strip">
        <div>
          <span>Active Seeds</span>
          <strong>{activeSeeds.length}</strong>
        </div>
        <div className="sklw-seed-strip-list">
          {activeSeeds.map((seed) => (
            <button key={seed.id} type="button" onClick={() => setSelectedGame(games[0] || ALTTP_SHOWCASE_GAME)}>
              {seed.title}
            </button>
          ))}
          {!activeSeeds.length && <em>No active seeds yet.</em>}
        </div>
      </section>

      <section className="sklw-footer-hints">
        <span>Library now manages seeds for online Syncs.</span>
      </section>

      <SeedConfigModal
        open={seedModal.open}
        mode={seedModal.mode}
        game={selectedGame}
        onClose={() => setSeedModal((prev) => ({ ...prev, open: false }))}
        onSeedSelected={(seed) => {
          setActiveSeeds(addActiveSeed(seed));
        }}
        onSeedsChanged={() => setActiveSeeds(loadActiveSeeds())}
      />
    </div>
  );
};

export default LinkedWorldLibraryPage;
