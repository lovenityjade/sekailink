import { useState } from 'react';
import { X, ChevronLeft, ChevronRight } from 'lucide-react';
import { ImageWithFallback } from './figma/ImageWithFallback';
import defaultGameImage from '../../imports/image-12.png';
import { ALTTP_SHOWCASE_GAME, type SeedGameEntry } from '../../services/seedConfig';

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
  entry: SeedGameEntry;
}

export default function GameSelectionModal({ onClose, onSelectGame, mode, games: gameEntries }: GameSelectionModalProps) {
  const [currentIndex, setCurrentIndex] = useState(0);

  const games: Game[] = (gameEntries?.length ? gameEntries : [ALTTP_SHOWCASE_GAME]).map((game) => ({
    id: game.game_key,
    name: game.display_name,
    image: game.boxart || defaultGameImage,
    entry: game,
  }));
  const canCarousel = games.length > 1;

  const handlePrevious = () => {
    if (!canCarousel) return;
    setCurrentIndex((prev) => (prev - 1 + games.length) % games.length);
  };

  const handleNext = () => {
    if (!canCarousel) return;
    setCurrentIndex((prev) => (prev + 1) % games.length);
  };

  const handleSelect = () => {
    onSelectGame(games[currentIndex].entry);
  };

  // Calculate visible games (show 3: previous, current, next)
  const getVisibleGames = () => {
    if (!canCarousel) return [{ game: games[currentIndex], offset: 0 }];
    const visible = [];
    for (let i = -1; i <= 1; i++) {
      const index = (currentIndex + i + games.length) % games.length;
      visible.push({ game: games[index], offset: i });
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
          <div className="p-12 relative">
            <div className="flex items-center justify-center gap-8">
              {/* Previous Button */}
              <button
                type="button"
                onClick={handlePrevious}
                disabled={!canCarousel}
                className={`w-12 h-12 rounded-full flex items-center justify-center shadow-lg transition-all ${
                  canCarousel
                    ? 'bg-gradient-to-r from-[#ff6b35] to-[#f38181] hover:shadow-xl hover:scale-110'
                    : 'pointer-events-none opacity-0'
                }`}
                aria-label="Previous game"
              >
                <ChevronLeft className="w-6 h-6" />
              </button>

              {/* Game Cards */}
              <div className="flex items-center justify-center gap-6 flex-1 max-w-3xl">
                {getVisibleGames().map(({ game, offset }) => (
                  <div
                    key={`${game.id}-${offset}`}
                    className={`transition-all duration-300 ${
                      offset === 0
                        ? 'scale-100 opacity-100 z-10'
                        : 'scale-75 opacity-40'
                    }`}
                    style={{
                      flex: offset === 0 ? '0 0 350px' : '0 0 280px',
                    }}
                  >
                    <div className={`bg-gradient-card rounded-lg overflow-hidden border-2 transition-all ${
                      offset === 0
                        ? 'border-[#4ecdc4] shadow-2xl shadow-[#4ecdc4]/20'
                        : 'border-[#2a2b30]'
                    }`}>
                      <div className={`${offset === 0 ? 'h-[300px]' : 'h-[190px]'} bg-[#1c1d22] flex items-center justify-center overflow-hidden`}>
                        <ImageWithFallback
                          src={game.image}
                          alt={game.name}
                          className="w-full h-full object-cover"
                        />
                      </div>
                      <div className="p-4 text-center">
                        <h3 className={`font-bold transition-all ${
                          offset === 0 ? 'text-lg text-white' : 'text-sm text-[#8e8f94]'
                        }`}>
                          {game.name}
                        </h3>
                      </div>
                    </div>
                  </div>
                ))}
              </div>

              {/* Next Button */}
              <button
                type="button"
                onClick={handleNext}
                disabled={!canCarousel}
                className={`w-12 h-12 rounded-full flex items-center justify-center shadow-lg transition-all ${
                  canCarousel
                    ? 'bg-gradient-to-r from-[#ff6b35] to-[#f38181] hover:shadow-xl hover:scale-110'
                    : 'pointer-events-none opacity-0'
                }`}
                aria-label="Next game"
              >
                <ChevronRight className="w-6 h-6" />
              </button>
            </div>

            {/* Indicators */}
            <div className="flex items-center justify-center gap-2 mt-8">
              {games.map((_, index) => (
                <button
                  type="button"
                  key={index}
                  onClick={() => setCurrentIndex(index)}
                  aria-label={`Select carousel game ${index + 1}`}
                  className={`h-2 rounded-full transition-all ${
                    index === currentIndex
                      ? 'w-8 bg-gradient-to-r from-[#4ecdc4] to-[#95e1d3]'
                      : 'w-2 bg-[#2a2b30] hover:bg-[#3a3b40]'
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
              className="px-6 py-3 bg-gradient-to-r from-[#4ecdc4] to-[#95e1d3] text-[#14151a] rounded-lg font-bold shadow-lg hover:shadow-xl transition-all"
            >
              Select {games[currentIndex].name}
            </button>
          </div>
        </div>
      </div>
    </>
  );
}
