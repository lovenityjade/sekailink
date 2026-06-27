import { useEffect, useMemo, useState } from 'react';
import { X, ChevronLeft, ChevronRight, Search } from 'lucide-react';
import { type SeedGameEntry } from '../../services/seedConfig';
import { SEKAILINK_GAME_CATALOG } from '../../data/sekailinkGameCatalog';

interface GameSelectionModalProps {
  onClose: () => void;
  onSelectGame: (game: SeedGameEntry) => void;
  mode: 'easy' | 'advanced';
  games?: SeedGameEntry[];
}

interface Game {
  id: string;
  name: string;
  image: string;
  available: boolean;
  entry: SeedGameEntry;
}

export default function GameSelectionModal({ onClose, onSelectGame, mode, games: gameEntries }: GameSelectionModalProps) {
  const [currentIndex, setCurrentIndex] = useState(0);
  const [searchQuery, setSearchQuery] = useState('');

  const games: Game[] = useMemo(() => {
    const gameEntryByKey = new Map((gameEntries || []).map((entry) => [entry.game_key, entry]));
    return SEKAILINK_GAME_CATALOG.map((game) => {
      const remoteEntry = gameEntryByKey.get(game.key);
      const configAvailable = Boolean(remoteEntry);
      const available = !game.forceUnavailable && (game.available || configAvailable);
      const entry = available
        ? (game.seedEntry || remoteEntry || {
            game_key: game.key,
            display_name: game.displayName,
            description: '',
            boxart: game.asset,
          })
        : {
            game_key: game.key,
            display_name: game.displayName,
            description: '',
            boxart: game.asset,
          };
      return {
        id: game.key,
        name: game.displayName,
        image: game.asset,
        available,
        entry,
      };
    });
  }, [gameEntries]);
  const filteredGames = useMemo(() => {
    const query = searchQuery.trim().toLowerCase();
    if (!query) return games;
    return games.filter((game) => (
      game.name.toLowerCase().includes(query) ||
      game.id.toLowerCase().includes(query) ||
      game.entry.game_key.toLowerCase().includes(query) ||
      String(game.entry.description || '').toLowerCase().includes(query)
    ));
  }, [games, searchQuery]);
  useEffect(() => {
    setCurrentIndex(0);
  }, [searchQuery]);
  const canCarousel = games.length > 1;
  const visibleCanCarousel = filteredGames.length > 1;
  const currentGame = filteredGames[currentIndex] || filteredGames[0];

  const handlePrevious = () => {
    if (!visibleCanCarousel) return;
    setCurrentIndex((prev) => (prev - 1 + filteredGames.length) % filteredGames.length);
  };

  const handleNext = () => {
    if (!visibleCanCarousel) return;
    setCurrentIndex((prev) => (prev + 1) % filteredGames.length);
  };

  const handleSelect = () => {
    if (!currentGame?.available) return;
    onSelectGame(currentGame.entry);
  };

  // Calculate visible games (show 3: previous, current, next)
  const getVisibleGames = () => {
    if (!visibleCanCarousel) return currentGame ? [{ game: currentGame, offset: 0 }] : [];
    const visible = [];
    for (let i = -1; i <= 1; i++) {
      const index = (currentIndex + i + filteredGames.length) % filteredGames.length;
      visible.push({ game: filteredGames[index], offset: i });
    }
    return visible;
  };

  return (
    <>
      {/* Backdrop */}
      <div className="fixed inset-0 bg-black/80 backdrop-blur-sm z-50" onClick={onClose} />

      {/* Modal */}
      <div className="fixed inset-0 z-[60] flex items-center justify-center p-8 pointer-events-none">
        <div className="bg-gradient-card border-2 border-[#4ecdc4] rounded-lg shadow-2xl max-w-5xl w-full pointer-events-auto" onClick={(e) => e.stopPropagation()}>
          {/* Header */}
          <div className="flex items-center justify-between p-6 border-b border-[#2a2b30]">
            <div>
              <h2 className="text-2xl font-bold">Select Game</h2>
              <p className="text-sm text-[#8e8f94] mt-1">
                Choose a game for your {mode === 'easy' ? 'Easy Mode' : 'Advanced'} configuration
              </p>
            </div>
            <div className="relative ml-auto mr-4 w-full max-w-xs">
              <Search className="pointer-events-none absolute left-3 top-1/2 h-4 w-4 -translate-y-1/2 text-[#8e8f94]" />
              <input
                type="search"
                value={searchQuery}
                onChange={(event) => setSearchQuery(event.target.value)}
                placeholder="Search games..."
                className="w-full rounded-lg border-2 border-[#2a2b30] bg-[#0e0f13] py-2.5 pl-10 pr-3 text-sm text-white outline-none transition-colors placeholder:text-[#8e8f94] focus:border-[#4ecdc4]"
              />
            </div>
            <button
              type="button"
              onClick={onClose}
              className="w-10 h-10 rounded-lg bg-[#2b2c31] hover:bg-[#3a3b40] flex items-center justify-center transition-colors text-[#8e8f94] hover:text-white"
              aria-label="Close game selection"
            >
              <X className="w-5 h-5" />
            </button>
          </div>

          {/* Carousel */}
          <div className="p-10 relative">
            <div className="flex items-center justify-center gap-8">
              {/* Previous Button */}
              <button
                type="button"
                onClick={handlePrevious}
                disabled={!visibleCanCarousel}
                className={`w-12 h-12 rounded-full flex items-center justify-center shadow-lg transition-all ${
                  visibleCanCarousel
                    ? 'bg-gradient-to-r from-[#ff6b35] to-[#f38181] hover:shadow-xl hover:scale-110'
                    : 'pointer-events-none opacity-0'
                }`}
                aria-label="Previous game"
              >
                <ChevronLeft className="w-6 h-6" />
              </button>

              {/* Game Cards */}
              <div className="flex items-center justify-center gap-5 flex-1 max-w-4xl">
                {getVisibleGames().map(({ game, offset }) => (
                  <div
                    key={`${game.id}-${offset}`}
                    className={`transition-all duration-300 ${offset === 0 ? 'opacity-100 z-10' : 'opacity-55'}`}
                    style={{ flex: '0 0 260px' }}
                  >
                    <div className={`bg-gradient-card rounded-lg overflow-hidden border-2 transition-all ${
                      offset === 0
                        ? 'border-[#4ecdc4] shadow-2xl shadow-[#4ecdc4]/20'
                        : 'border-[#2a2b30]'
                    }`}>
                      <div className="relative h-[260px] bg-[#101116] flex items-center justify-center overflow-hidden">
                        <GameMedia game={game} />
                        {!game.available && (
                          <div className="absolute inset-0 bg-black/62 backdrop-grayscale flex items-center justify-center">
                            <div className="rounded-lg border border-white/20 bg-black/60 px-4 py-2 text-sm font-bold uppercase tracking-wider text-white shadow-2xl">
                              Coming Soon
                            </div>
                          </div>
                        )}
                      </div>
                      <div className="h-[86px] p-4 text-center flex flex-col items-center justify-center">
                        <h3 className={`font-bold transition-all ${
                          offset === 0 ? 'text-lg text-white' : 'text-sm text-[#8e8f94]'
                        }`}>
                          {game.name}
                        </h3>
                        <div className={`mt-2 text-xs font-bold ${game.available ? 'text-[#4ecdc4]' : 'text-[#8e8f94]'}`}>
                          {game.available ? 'Available' : 'Coming Soon'}
                        </div>
                      </div>
                    </div>
                  </div>
                ))}
                {!currentGame && (
                  <div className="flex h-[346px] flex-1 items-center justify-center rounded-lg border-2 border-dashed border-[#2a2b30] bg-[#101116]/70 text-center text-sm text-[#8e8f94]">
                    No games match your search.
                  </div>
                )}
              </div>

              {/* Next Button */}
              <button
                type="button"
                onClick={handleNext}
                disabled={!visibleCanCarousel}
                className={`w-12 h-12 rounded-full flex items-center justify-center shadow-lg transition-all ${
                  visibleCanCarousel
                    ? 'bg-gradient-to-r from-[#ff6b35] to-[#f38181] hover:shadow-xl hover:scale-110'
                    : 'pointer-events-none opacity-0'
                }`}
                aria-label="Next game"
              >
                <ChevronRight className="w-6 h-6" />
              </button>
            </div>

            {/* Indicators */}
            <div className="mt-8 text-center text-xs text-[#8e8f94]">
              {currentGame ? `${currentIndex + 1} / ${filteredGames.length}` : `0 / ${games.length}`}
              {searchQuery.trim() && (
                <span className="ml-2 text-[#4ecdc4]">
                  filtered from {games.length}
                </span>
              )}
            </div>
            <div className="mx-auto flex max-w-3xl flex-wrap items-center justify-center gap-1.5 mt-4">
              {filteredGames.map((_, index) => (
                <button
                  type="button"
                  key={index}
                  onClick={() => setCurrentIndex(index)}
                  aria-label={`Select carousel game ${index + 1}`}
                  className={`h-1.5 rounded-full transition-all ${
                    index === currentIndex
                      ? 'w-6 bg-gradient-to-r from-[#4ecdc4] to-[#95e1d3]'
                      : 'w-1.5 bg-[#2a2b30] hover:bg-[#3a3b40]'
                  }`}
                />
              ))}
            </div>
          </div>

          {/* Footer */}
          <div className="flex items-center justify-end gap-3 p-6 border-t border-[#2a2b30]">
            <button
              type="button"
              onClick={onClose}
              className="px-6 py-3 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg font-medium transition-colors"
            >
              Cancel
            </button>
            <button
              type="button"
              onClick={handleSelect}
              disabled={!currentGame?.available}
              className="px-6 py-3 bg-gradient-to-r from-[#4ecdc4] to-[#95e1d3] text-[#14151a] rounded-lg font-bold shadow-lg hover:shadow-xl transition-all disabled:cursor-not-allowed disabled:opacity-50 disabled:grayscale"
            >
              {currentGame ? (currentGame.available ? `Select ${currentGame.name}` : `${currentGame.name} Coming Soon`) : 'No game selected'}
            </button>
          </div>
        </div>
      </div>
    </>
  );
}

function GameMedia({ game }: { game: Pick<Game, 'name' | 'image'> }) {
  if (game.image.toLowerCase().endsWith('.webm')) {
    return (
      <video
        src={game.image}
        className="h-full w-full object-contain"
        muted
        loop
        autoPlay
        playsInline
      />
    );
  }
  return <img src={game.image} alt={game.name} className="h-full w-full object-contain" />;
}
