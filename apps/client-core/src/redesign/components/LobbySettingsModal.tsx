import { X, Settings, Lock, Zap, Info } from 'lucide-react';
import { useState } from 'react';
import { ErrorModal } from './FeedbackModal';

interface LobbySettingsModalProps {
  onClose: () => void;
  onSave?: (settings: LobbySettings) => void | Promise<void>;
}

interface LobbySettings {
  password: string;
  disableItemCheat: boolean;
  locationCheckPoints: number;
  hintCost: number;
  releaseMode: string;
  collectMode: string;
  remainingMode: string;
  hintMode: string;
  spoiler: number;
}

export default function LobbySettingsModal({ onClose, onSave }: LobbySettingsModalProps) {
  const [password, setPassword] = useState('');
  const [disableItemCheat, setDisableItemCheat] = useState(false);
  const [locationCheckPoints, setLocationCheckPoints] = useState('1');
  const [hintCost, setHintCost] = useState('10');
  const [releaseMode, setReleaseMode] = useState('auto');
  const [collectMode, setCollectMode] = useState('auto');
  const [remainingMode, setRemainingMode] = useState('goal');
  const [hintMode, setHintMode] = useState('default');
  const [spoiler, setSpoiler] = useState('3');
  const [saveError, setSaveError] = useState('');

  const handleSave = async () => {
    setSaveError('');
    const settings: LobbySettings = {
      password,
      disableItemCheat,
      locationCheckPoints: parseInt(locationCheckPoints),
      hintCost: parseInt(hintCost),
      releaseMode,
      collectMode,
      remainingMode,
      hintMode,
      spoiler: parseInt(spoiler)
    };

    try {
      if (onSave) {
        await onSave(settings);
      }
      onClose();
    } catch (err) {
      setSaveError(err instanceof Error ? err.message : 'Unable to save lobby settings.');
    }
  };

  return (
    <div className="fixed left-0 right-0 bottom-0 top-[32px] bg-black/80 backdrop-blur-sm flex items-center justify-center z-50 p-8">
      <div className="w-full max-w-3xl bg-[#161b22] rounded-xl shadow-2xl border-2 border-[#4ecdc4] card-float overflow-hidden max-h-[90vh] flex flex-col">
        {/* Header */}
        <div className="p-6 border-b-2 border-[#2a2b30] bg-gradient-to-r from-[#1c1d22] to-[#161b22] flex items-center justify-between">
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 rounded-lg bg-gradient-to-br from-[#4ecdc4] to-[#95e1d3] flex items-center justify-center">
              <Settings className="w-5 h-5 text-[#14151a]" />
            </div>
            <div>
              <h2 className="text-2xl font-bold">Lobby Settings</h2>
              <p className="text-sm text-[#8e8f94]">Configure room and sync options</p>
            </div>
          </div>
          <button
            onClick={onClose}
            className="w-10 h-10 rounded-lg bg-[#2a2b30] hover:bg-[#f85149] transition-colors flex items-center justify-center group"
          >
            <X className="w-5 h-5 text-[#8e8f94] group-hover:text-white" />
          </button>
        </div>

        {/* Content */}
        <div className="flex-1 overflow-y-auto p-6 space-y-6">
          {/* Room Security */}
          <div>
            <h3 className="text-sm font-semibold text-[#e6edf3] mb-4 flex items-center gap-2">
              <Lock className="w-4 h-4" />
              ROOM SECURITY
            </h3>
            <div>
              <label className="block text-sm font-semibold text-[#e6edf3] mb-2">
                PASSWORD (OPTIONAL)
              </label>
              <input
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                placeholder="Leave blank for public lobby"
                className="w-full px-4 py-3 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4ecdc4] outline-none transition-colors"
              />
            </div>
          </div>

          {/* Host Tools */}
          <div>
            <h3 className="text-sm font-semibold text-[#e6edf3] mb-4 flex items-center gap-2">
              <Zap className="w-4 h-4" />
              HOST TOOLS
            </h3>
            <div
              onClick={() => setDisableItemCheat(!disableItemCheat)}
              className="p-4 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg hover:border-[#4ecdc4] transition-colors cursor-pointer"
            >
              <div className="flex items-center justify-between mb-2">
                <span className="font-medium text-white">DISABLE ITEM CHEAT</span>
                <button
                  type="button"
                  className={`
                    relative w-12 h-6 border-2 transition-all
                    ${disableItemCheat ? 'bg-[#4ecdc4] border-[#4ecdc4]' : 'bg-[#2a2b30] border-[#2a2b30]'}
                  `}
                >
                  <div
                    className={`
                      absolute top-0.5 w-4 h-4 bg-[#0d1117] transition-all
                      ${disableItemCheat ? 'right-0.5' : 'left-0.5'}
                    `}
                  />
                </button>
              </div>
              <p className="text-xs text-[#8e8f94]">
                {disableItemCheat ? 'Item cheat (!getitem) is blocked for all players' : 'Item cheat (!getitem) is allowed for host'}
              </p>
            </div>
          </div>

          {/* Hint System */}
          <div>
            <h3 className="text-sm font-semibold text-[#e6edf3] mb-4">HINT SYSTEM</h3>
            <div className="grid grid-cols-2 gap-4">
              {/* Location Check Points */}
              <div>
                <label className="block text-xs font-medium text-[#8e8f94] mb-2">
                  LOCATION CHECK POINTS
                </label>
                <input
                  type="number"
                  value={locationCheckPoints}
                  onChange={(e) => setLocationCheckPoints(e.target.value)}
                  min="0"
                  className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none transition-colors"
                />
                <p className="text-[10px] text-[#8e8f94] mt-1">Points gained per location check</p>
              </div>

              {/* Hint Cost */}
              <div>
                <label className="block text-xs font-medium text-[#8e8f94] mb-2">
                  HINT COST
                </label>
                <input
                  type="number"
                  value={hintCost}
                  onChange={(e) => setHintCost(e.target.value)}
                  min="0"
                  className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none transition-colors"
                />
                <p className="text-[10px] text-[#8e8f94] mt-1">Points required to purchase a hint</p>
              </div>

              {/* Hint Mode */}
              <div className="col-span-2">
                <label className="block text-xs font-medium text-[#8e8f94] mb-2">
                  HINT MODE
                </label>
                <select
                  value={hintMode}
                  onChange={(e) => setHintMode(e.target.value)}
                  className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none transition-colors"
                >
                  <option value="default">Default - Standard hint behavior</option>
                  <option value="own">Own - Only hints for your own items</option>
                  <option value="all">All - Hints for all players' items</option>
                </select>
              </div>
            </div>
          </div>

          {/* Item Release & Collection */}
          <div>
            <h3 className="text-sm font-semibold text-[#e6edf3] mb-4">ITEM RELEASE & COLLECTION</h3>
            <div className="space-y-4">
              {/* Release Mode */}
              <div>
                <label className="block text-xs font-medium text-[#8e8f94] mb-2">
                  RELEASE MODE
                </label>
                <select
                  value={releaseMode}
                  onChange={(e) => setReleaseMode(e.target.value)}
                  className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none transition-colors"
                >
                  <option value="disabled">Disabled - No automatic release</option>
                  <option value="manual">Manual Release - Host controls release</option>
                  <option value="auto">Auto Release on Goal - Release when goal is met</option>
                  <option value="slow">Slow-Release on Goal - Gradual release after goal</option>
                </select>
              </div>

              {/* Collect Mode */}
              <div>
                <label className="block text-xs font-medium text-[#8e8f94] mb-2">
                  COLLECT MODE
                </label>
                <select
                  value={collectMode}
                  onChange={(e) => setCollectMode(e.target.value)}
                  className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none transition-colors"
                >
                  <option value="disabled">Disabled - No auto-collect</option>
                  <option value="enabled">Enabled - Always auto-collect</option>
                  <option value="auto">Auto - Automatic based on game state</option>
                  <option value="auto-enabled">Auto-Enabled - Enhanced automatic collection</option>
                  <option value="goal">Goal - Collect when goal is reached</option>
                </select>
              </div>

              {/* Remaining Mode */}
              <div>
                <label className="block text-xs font-medium text-[#8e8f94] mb-2">
                  REMAINING MODE
                </label>
                <select
                  value={remainingMode}
                  onChange={(e) => setRemainingMode(e.target.value)}
                  className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none transition-colors"
                >
                  <option value="disabled">Disabled - Don't track remaining items</option>
                  <option value="enabled">Enabled - Always track remaining</option>
                  <option value="goal">Goal - Track after goal is reached</option>
                </select>
              </div>
            </div>
          </div>

          {/* Generation Settings */}
          <div>
            <h3 className="text-sm font-semibold text-[#e6edf3] mb-4">GENERATION SETTINGS</h3>
            <div>
              <label className="block text-xs font-medium text-[#8e8f94] mb-2">
                SPOILER LOG DETAIL
              </label>
              <select
                value={spoiler}
                onChange={(e) => setSpoiler(e.target.value)}
                className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none transition-colors"
              >
                <option value="0">None - No spoiler log generated</option>
                <option value="1">Basic - Basic item locations only</option>
                <option value="2">Playthrough - Shows playthrough path</option>
                <option value="3">Full Paths - Complete spoiler with all paths</option>
              </select>
              <p className="text-[10px] text-[#8e8f94] mt-2">
                Controls the detail level of the generated spoiler log
              </p>
            </div>
          </div>

          {/* Info Box */}
          <div className="p-4 bg-gradient-to-r from-[#4ecdc4]/5 to-transparent border-l-2 border-[#4ecdc4] rounded-lg">
            <div className="flex items-start gap-3">
              <Info className="w-5 h-5 text-[#4ecdc4] flex-shrink-0 mt-0.5" />
              <div className="text-sm text-[#8e8f94]">
                <span className="font-semibold text-[#e6edf3]">HOST ONLY</span>
                <p className="mt-1">
                  These settings can only be modified by the lobby host and will apply to all players in this sync.
                </p>
              </div>
            </div>
          </div>
        </div>

        {/* Footer */}
        <div className="p-6 border-t-2 border-[#2a2b30] bg-[#14151a]/90 flex justify-end gap-3">
          <button
            onClick={onClose}
            className="px-6 py-3 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg font-medium transition-colors"
          >
            Cancel
          </button>
          <button
            onClick={handleSave}
            className="px-8 py-3 bg-gradient-to-r from-[#4ecdc4] to-[#95e1d3] text-[#14151a] rounded-lg font-bold shadow-lg hover:shadow-xl transition-all"
          >
            Save Settings
          </button>
        </div>
      </div>
      {saveError && (
        <ErrorModal
          title="Could not save lobby settings"
          message={saveError}
          code="LOBBY_SETTINGS_SAVE_FAILED"
          onClose={() => setSaveError('')}
        />
      )}
    </div>
  );
}
