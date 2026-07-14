import { X, Users, Lock, Settings, Info } from 'lucide-react';
import { useState } from 'react';
import { createLobby } from '../../services/lobbyClient';
import { trace, traceError } from '../../services/trace';
import { ErrorModal, LoadingModal } from './FeedbackModal';

interface CreateLobbyModalProps {
  onClose: () => void;
  onCreateSuccess?: (lobbyId?: string, lobbyName?: string) => void;
  asyncRoomEligible?: boolean;
}

export default function CreateLobbyModal({ onClose, onCreateSuccess, asyncRoomEligible = false }: CreateLobbyModalProps) {
  const [roomName, setRoomName] = useState('');
  const [description, setDescription] = useState('');
  const [password, setPassword] = useState('');
  const [maxPlayers, setMaxPlayers] = useState('5');
  const [spoiler, setSpoiler] = useState('off');
  const [itemCheat, setItemCheat] = useState(false);
  const [asynchronous, setAsynchronous] = useState(false);
  const [submitting, setSubmitting] = useState(false);
  const [error, setError] = useState('');

  const handleCreate = async () => {
    const name = roomName.trim();
    if (!name || submitting) return;
    setSubmitting(true);
    setError('');
    trace('create-lobby-modal', 'create_start', {
      hasDescription: Boolean(description.trim()),
      private: Boolean(password),
      maxPlayers,
      spoiler,
      itemCheat,
      asynchronous,
    });
    try {
      const result = await createLobby({
        name,
        description,
        server_password: password,
        max_players: maxPlayers,
        spoiler,
        item_cheat: itemCheat,
        asynchronous,
      });
      onClose();
      onCreateSuccess?.(result.lobbyId, name);
      trace('create-lobby-modal', 'create_success', { lobbyId: result.lobbyId || '' });
    } catch (err) {
      traceError('create-lobby-modal', 'create_failed', err);
      setError(err instanceof Error ? err.message : 'Unable to create lobby.');
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <div className="fixed left-0 right-0 bottom-0 top-[32px] bg-black/80 backdrop-blur-sm flex items-center justify-center z-50 p-8">
      <div className="w-full max-w-2xl bg-[#161b22] rounded-xl shadow-2xl border-2 border-[#4dffd2] card-float overflow-hidden">
        {/* Header */}
        <div className="p-6 border-b-2 border-[#2a2b30] bg-gradient-to-r from-[#1c1d22] to-[#161b22] flex items-center justify-between">
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 rounded-lg bg-gradient-to-br from-[#ff6b35] to-[#f38181] flex items-center justify-center">
              <Users className="w-5 h-5 text-white" />
            </div>
            <h2 className="text-2xl font-bold">Create Lobby</h2>
          </div>
          <button
            onClick={onClose}
            className="w-10 h-10 rounded-lg bg-[#2a2b30] hover:bg-[#f85149] transition-colors flex items-center justify-center group"
          >
            <X className="w-5 h-5 text-[#8e8f94] group-hover:text-white" />
          </button>
        </div>

        {/* Content */}
        <div className="p-6 space-y-6 max-h-[600px] overflow-y-auto">
          {/* Room Name */}
          <div>
            <label className="block text-sm font-semibold text-[#e6edf3] mb-2">
              ROOM NAME
            </label>
            <input
              type="text"
              value={roomName}
              onChange={(e) => setRoomName(e.target.value)}
              placeholder="Enter room name..."
              className="w-full px-4 py-3 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4dffd2] outline-none transition-colors"
            />
          </div>

          {/* Description */}
          <div>
            <label className="block text-sm font-semibold text-[#e6edf3] mb-2">
              DESCRIPTION
            </label>
            <textarea
              value={description}
              onChange={(e) => setDescription(e.target.value)}
              placeholder="Short room description..."
              rows={2}
              className="w-full px-4 py-3 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4dffd2] outline-none transition-colors resize-none"
            />
          </div>

          {/* Password */}
          <div>
            <label className="block text-sm font-semibold text-[#e6edf3] mb-2 flex items-center gap-2">
              <Lock className="w-4 h-4" />
              PASSWORD (OPTIONAL)
            </label>
            <input
              type="password"
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              placeholder="Leave blank for public"
              className="w-full px-4 py-3 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4dffd2] outline-none transition-colors"
            />
          </div>

          {/* Room Config Info */}
          <div className="p-4 bg-gradient-to-r from-[#4dffd2]/5 to-transparent border-l-2 border-[#4dffd2] rounded-lg">
            <div className="flex items-start gap-3">
              <Info className="w-5 h-5 text-[#4dffd2] flex-shrink-0 mt-0.5" />
              <div className="text-sm text-[#8e8f94]">
                <span className="font-semibold text-[#e6edf3]">ROOM-LOCAL CONFIG</span>
                <p className="mt-1">
                  Games and configs are selected after entering the room. Players can add or remove selections until Ready → Generation locks the room snapshot.
                </p>
              </div>
            </div>
          </div>

          {/* Sync Settings */}
          <div>
            <h3 className="text-sm font-semibold text-[#e6edf3] mb-4 flex items-center gap-2">
              <Settings className="w-4 h-4" />
              SYNC SETTINGS
            </h3>
            <div className="grid grid-cols-2 gap-4">
              {/* Max Players */}
              <div>
                <label className="block text-xs font-medium text-[#8e8f94] mb-2">
                  MAX PLAYERS
                </label>
                <input
                  type="number"
                  value={maxPlayers}
                  onChange={(e) => setMaxPlayers(e.target.value)}
                  min="1"
                  max="100"
                  className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4dffd2] outline-none transition-colors"
                />
              </div>

              {/* Spoiler */}
              <div>
                <label className="block text-xs font-medium text-[#8e8f94] mb-2">
                  SPOILER
                </label>
                <select
                  value={spoiler}
                  onChange={(e) => setSpoiler(e.target.value)}
                  className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4dffd2] outline-none transition-colors"
                >
                  <option value="off">Off</option>
                  <option value="on">On</option>
                  <option value="auto">Auto</option>
                </select>
              </div>
            </div>
          </div>

          <div>
            <h3 className="text-sm font-semibold text-[#e6edf3] mb-3">LOBBY LIFETIME</h3>
            <button
              type="button"
              onClick={() => {
                if (asyncRoomEligible) setAsynchronous((value) => !value);
              }}
              disabled={!asyncRoomEligible}
              className={`w-full p-4 border-2 rounded-lg text-left transition-colors ${
                !asyncRoomEligible
                  ? 'cursor-not-allowed border-[#2a2b30] bg-[#0d1117] opacity-55'
                  : asynchronous
                    ? 'border-[#aa96da] bg-[#aa96da]/10'
                    : 'border-[#2a2b30] bg-[#0d1117] hover:border-[#aa96da]'
              }`}
            >
              <div className="flex items-center justify-between gap-4">
                <div>
                  <div className="font-semibold text-white">Asynchronous</div>
                  <p className="mt-1 text-xs text-[#8e8f94]">
                    Active lobbies expire after inactivity. Async lobbies stay until the host closes them or every participant reaches goal.
                  </p>
                  <p className="mt-2 text-xs text-[#aa96da]">
                    {asyncRoomEligible
                      ? 'Unlocked by your active Patreon Tier 2/3 membership.'
                      : 'Requires active Patreon Tier 2 (Super Supporter) or Tier 3 (Ultra Supporter).'}
                  </p>
                </div>
                <div className={`w-12 h-6 border-2 transition-all ${asynchronous ? 'bg-[#aa96da] border-[#aa96da]' : 'bg-[#2a2b30] border-[#2a2b30]'}`}>
                  <div className={`mt-0.5 w-4 h-4 bg-[#0d1117] transition-all ${asynchronous ? 'ml-6' : 'ml-0.5'}`} />
                </div>
              </div>
            </button>
          </div>

          {/* Host Tools */}
          <div>
            <h3 className="text-sm font-semibold text-[#e6edf3] mb-3">HOST TOOLS</h3>
            <div
              onClick={() => setItemCheat(!itemCheat)}
              className="p-4 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg hover:border-[#4dffd2] transition-colors cursor-pointer"
            >
              <div className="flex items-center justify-between mb-2">
                <span className="font-medium text-white">ITEM CHEAT</span>
                <button
                  type="button"
                  className={`
                    relative w-12 h-6 border-2 transition-all
                    ${itemCheat ? 'bg-[#4dffd2] border-[#4dffd2]' : 'bg-[#2a2b30] border-[#2a2b30]'}
                  `}
                >
                  <div
                    className={`
                      absolute top-0.5 w-4 h-4 bg-[#0d1117] transition-all
                      ${itemCheat ? 'right-0.5' : 'left-0.5'}
                    `}
                  />
                </button>
              </div>
              <p className="text-xs text-[#8e8f94]">
                Disabled - Testing helper for the host during showcase validation.
              </p>
            </div>
          </div>

        </div>

        {/* Footer */}
        <div className="p-6 border-t-2 border-[#2a2b30] bg-[#14151a] flex justify-end gap-3">
          <button
            onClick={onClose}
            className="px-6 py-3 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg font-medium transition-colors"
          >
            Cancel
          </button>
          <button
            onClick={handleCreate}
            disabled={!roomName.trim() || submitting}
            className="px-8 py-3 bg-gradient-to-r from-[#ff6b35] to-[#f38181] rounded-lg font-bold shadow-lg hover:shadow-xl glow-hover transition-all disabled:opacity-50 disabled:cursor-not-allowed"
          >
            {submitting ? 'Creating...' : 'Create Sync'}
          </button>
        </div>
      </div>
      {error && (
        <ErrorModal
          title="Could not create Sync"
          message={error}
          code="CREATE_SYNC_FAILED"
          onClose={() => setError('')}
        />
      )}
      {submitting && (
        <LoadingModal
          title="Creating Sync"
          message={`Creating ${roomName.trim() || 'your Sync'} on Nexus...`}
        />
      )}
    </div>
  );
}
