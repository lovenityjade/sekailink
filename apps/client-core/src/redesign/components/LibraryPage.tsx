import { Plus, Search, Edit, Trash2, Copy, Filter, Zap, FileText, Check, AlertCircle, XCircle } from 'lucide-react';
import { useCallback, useEffect, useState } from 'react';
import AdvancedConfigModal from './AdvancedConfigModal';
import GameSelectionModal from './GameSelectionModal';
import { ErrorModal, LoadingModal } from './FeedbackModal';
import { apiFetch, apiCurrentUser } from '../../services/api';
import { trace, traceError } from '../../services/trace';
import {
  ALTTP_SHOWCASE_GAME,
  createAdvancedSeed,
  deleteSeed,
  listSeedGames,
  listSeeds,
  replaceAdvancedSeed,
  type SeedEntry,
  type SeedGameEntry,
} from '../../services/seedConfig';

type ConfigSource = 'pulse' | 'easy' | 'advanced';
type ConfigStatus = 'valid' | 'draft' | 'error';

interface SeedConfig {
  id: string;
  name: string;
  game: string;
  source: ConfigSource;
  status: ConfigStatus;
  createdAt: string;
  lastModified: string;
  description?: string;
  gameKey?: string;
  values?: Record<string, unknown>;
}

interface LibraryPageProps {
  onNavigateToEasyConfig?: (game: SeedGameEntry) => void;
}

export default function LibraryPage({ onNavigateToEasyConfig }: LibraryPageProps = {}) {
  const [searchQuery, setSearchQuery] = useState('');
  const [filterGame, setFilterGame] = useState<string>('all');
  const [filterSource, setFilterSource] = useState<string>('all');
  const [filterStatus, setFilterStatus] = useState<string>('all');
  const [showCreateMenu, setShowCreateMenu] = useState(false);
  const [showAdvancedConfig, setShowAdvancedConfig] = useState(false);
  const [showGameSelection, setShowGameSelection] = useState(false);
  const [selectedMode, setSelectedMode] = useState<'easy' | 'advanced'>('easy');
  const [selectedGame, setSelectedGame] = useState<SeedGameEntry>(ALTTP_SHOWCASE_GAME);
  const [editingConfig, setEditingConfig] = useState<SeedConfig | null>(null);
  const [games, setGames] = useState<SeedGameEntry[]>([ALTTP_SHOWCASE_GAME]);
  const [configs, setConfigs] = useState<SeedConfig[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');
  const [playerName, setPlayerName] = useState('Player');

  const mapSeed = (seed: SeedEntry): SeedConfig => {
    const sourceText = [
      seed.source,
      seed.description,
      seed.title,
      (seed.values as any)?.sekailink?.mode,
      (seed.values as any)?.mode,
    ].map((value) => String(value || '').toLowerCase()).join(' ');
    const source: ConfigSource = sourceText.includes('pulse')
      ? 'pulse'
      : sourceText.includes('easy')
        ? 'easy'
        : 'advanced';
    const createdAt = String(seed.created_at || '').slice(0, 10) || 'Unknown';
    const modifiedAt = String(seed.updated_at || '').slice(0, 10) || createdAt;
    return {
      id: seed.id,
      name: seed.title || 'Untitled Seed',
      game: seed.game || seed.game_key || 'Unknown',
      source,
      status: 'valid',
      createdAt,
      lastModified: modifiedAt,
      description: seed.description || (source === 'pulse'
        ? 'Pulse-created seed configuration'
        : source === 'easy'
          ? 'Easy Mode seed configuration'
          : 'Advanced seed configuration'),
      gameKey: seed.game_key,
      values: seed.values,
    };
  };

  const loadLibrary = useCallback(async (options: { forceSeeds?: boolean } = {}) => {
    setLoading(true);
    trace('library-page', 'load_start');
    try {
      const [nextGames, nextSeeds, user] = await Promise.all([
        listSeedGames(),
        listSeeds(undefined, { force: Boolean(options.forceSeeds) }),
        apiCurrentUser().catch(() => null),
      ]);
      setGames(nextGames.length ? nextGames : [ALTTP_SHOWCASE_GAME]);
      setConfigs(nextSeeds.map(mapSeed));
      setPlayerName(user?.display_name || user?.username || 'Player');
      setError('');
      trace('library-page', 'load_success', {
        gameCount: nextGames.length,
        seedCount: nextSeeds.length,
        hasUser: Boolean(user),
      });
    } catch (err) {
      traceError('library-page', 'load_failed', err);
      setError(err instanceof Error ? err.message : 'Unable to load seed configurations.');
    } finally {
      setLoading(false);
    }
  }, []);

  const mergeSavedSeedIntoLibrary = useCallback((seed: SeedEntry) => {
    const mapped = mapSeed(seed);
    setConfigs((current) => {
      const exists = current.some((config) => config.id === mapped.id);
      if (exists) return current.map((config) => (config.id === mapped.id ? mapped : config));
      return [mapped, ...current];
    });
  }, []);

  useEffect(() => {
    void loadLibrary();
  }, [loadLibrary]);

  const filteredConfigs = configs.filter(config => {
    const matchesSearch = config.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
                         config.game.toLowerCase().includes(searchQuery.toLowerCase());
    const matchesGame = filterGame === 'all' || config.game === filterGame;
    const matchesSource = filterSource === 'all' || config.source === filterSource;
    const matchesStatus = filterStatus === 'all' || config.status === filterStatus;
    return matchesSearch && matchesGame && matchesSource && matchesStatus;
  });

  const stats = {
    total: configs.length,
    easy: configs.filter(c => c.source === 'easy' || c.source === 'pulse').length,
    advanced: configs.filter(c => c.source === 'advanced').length,
    valid: configs.filter(c => c.status === 'valid').length,
  };

  const getStatusConfig = (status: ConfigStatus) => {
    switch (status) {
      case 'valid':
        return { icon: <Check className="w-4 h-4" />, label: 'Valid', color: 'text-[#4ecdc4]', bg: 'bg-[#4ecdc4]/20' };
      case 'draft':
        return { icon: <AlertCircle className="w-4 h-4" />, label: 'Draft', color: 'text-[#f69d50]', bg: 'bg-[#f69d50]/20' };
      case 'error':
        return { icon: <XCircle className="w-4 h-4" />, label: 'Error', color: 'text-[#f85149]', bg: 'bg-[#f85149]/20' };
    }
  };

  const handleDelete = async (id: string) => {
    if (!window.confirm('Delete this seed configuration?')) return;
    trace('library-page', 'delete_seed_start', { seedId: id });
    try {
      await deleteSeed(id);
      setConfigs(configs.filter(c => c.id !== id));
      setError('');
      trace('library-page', 'delete_seed_success', { seedId: id });
    } catch (err) {
      traceError('library-page', 'delete_seed_failed', err, { seedId: id });
      setError(err instanceof Error ? err.message : 'Unable to delete seed configuration.');
    }
  };

  const handleDuplicate = async (config: SeedConfig) => {
    trace('library-page', 'duplicate_seed_start', { seedId: config.id, name: config.name });
    try {
      const response = await apiFetch(`/api/yamls/${encodeURIComponent(config.id)}/duplicate`, { method: 'POST' });
      const data = await response.json().catch(() => ({}));
      if (!response.ok) throw new Error(String((data as any)?.error || 'Unable to duplicate seed configuration.'));
      await loadLibrary();
      setError('');
      trace('library-page', 'duplicate_seed_success', { seedId: config.id });
    } catch (err) {
      traceError('library-page', 'duplicate_seed_failed', err, { seedId: config.id });
      setError(err instanceof Error ? err.message : 'Unable to duplicate seed configuration.');
    }
  };

  const handleEdit = (config: SeedConfig) => {
    const nextGame =
      games.find((game) => game.game_key === config.gameKey || game.display_name === config.game) ||
      {
        ...ALTTP_SHOWCASE_GAME,
        game_key: config.gameKey || ALTTP_SHOWCASE_GAME.game_key,
        display_name: config.game || ALTTP_SHOWCASE_GAME.display_name,
      };
    trace('library-page', 'edit_seed_open', { seedId: config.id, gameKey: nextGame.game_key });
    setSelectedGame(nextGame);
    setEditingConfig(config);
    setShowAdvancedConfig(true);
  };

  return (
    <div className="p-8 space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold mb-1">Library</h1>
          <p className="text-[#8e8f94]">Manage your seed configurations</p>
        </div>

        {/* Create New Config */}
        <div className="relative">
          <button
            onClick={() => setShowCreateMenu(!showCreateMenu)}
            className="px-6 py-3 bg-gradient-to-r from-[#ff6b35] to-[#f38181] rounded-lg font-medium shadow-lg hover:shadow-xl glow-hover transition-all flex items-center gap-2"
          >
            <Plus className="w-5 h-5" />
            Create New Config
          </button>

          {/* Create Menu Dropdown */}
          {showCreateMenu && (
            <>
              <div
                className="fixed inset-0 z-40"
                onClick={() => setShowCreateMenu(false)}
              />
              <div className="absolute top-full right-0 mt-2 bg-[#1c1d22] rounded-lg shadow-2xl z-50 card-float border-2 border-[#2a2b30] overflow-hidden min-w-[250px]">
                <div className="py-2">
                  <button
                    onClick={() => {
                      trace('library-page', 'create_menu_select', { mode: 'easy' });
                      setSelectedMode('easy');
                      setShowGameSelection(true);
                      setShowCreateMenu(false);
                    }}
                    className="w-full px-4 py-3 flex items-start gap-3 hover:bg-gradient-to-r hover:from-[#4ecdc4]/10 hover:to-transparent transition-all text-left"
                  >
                    <Zap className="w-5 h-5 text-[#4ecdc4] flex-shrink-0 mt-0.5" />
                    <div>
                      <div className="font-medium text-white">Easy Mode</div>
                      <div className="text-xs text-[#8e8f94] mt-0.5">Quick setup with Pulse assistant</div>
                    </div>
                  </button>

                  <button
                    onClick={() => {
                      trace('library-page', 'create_menu_select', { mode: 'advanced' });
                      setSelectedMode('advanced');
                      setShowGameSelection(true);
                      setShowCreateMenu(false);
                    }}
                    className="w-full px-4 py-3 flex items-start gap-3 hover:bg-gradient-to-r hover:from-[#aa96da]/10 hover:to-transparent transition-all text-left"
                  >
                    <FileText className="w-5 h-5 text-[#aa96da] flex-shrink-0 mt-0.5" />
                    <div>
                      <div className="font-medium text-white">Advanced Mode</div>
                      <div className="text-xs text-[#8e8f94] mt-0.5">Full control with APWorld settings</div>
                    </div>
                  </button>
                </div>
              </div>
            </>
          )}
        </div>
      </div>

      {/* Stats Cards */}
      <div className="grid grid-cols-4 gap-6">
        <div className="p-4 bg-gradient-card rounded-lg card-float border-2 border-[#2a2b30]">
          <div className="text-sm text-[#8e8f94] mb-1">Total Configs</div>
          <div className="text-3xl font-bold text-white">{loading ? '...' : stats.total}</div>
        </div>
        <div className="p-4 bg-gradient-card rounded-lg card-float border-2 border-[#2a2b30]">
          <div className="text-sm text-[#8e8f94] mb-1">Easy Mode</div>
          <div className="text-3xl font-bold text-[#4ecdc4]">{stats.easy}</div>
        </div>
        <div className="p-4 bg-gradient-card rounded-lg card-float border-2 border-[#2a2b30]">
          <div className="text-sm text-[#8e8f94] mb-1">Advanced</div>
          <div className="text-3xl font-bold text-[#aa96da]">{stats.advanced}</div>
        </div>
        <div className="p-4 bg-gradient-card rounded-lg card-float border-2 border-[#2a2b30]">
          <div className="text-sm text-[#8e8f94] mb-1">Valid</div>
          <div className="text-3xl font-bold text-[#4ecdc4]">{stats.valid}</div>
        </div>
      </div>

      {/* Search and Filters */}
      <div className="flex gap-4">
        {/* Search */}
        <div className="flex-1 relative">
          <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-5 h-5 text-[#8e8f94]" />
          <input
            type="text"
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="Search configs by name or game..."
            className="w-full pl-10 pr-4 py-3 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4ecdc4] outline-none transition-colors"
          />
        </div>

        {/* Filter by Game */}
        <select
          value={filterGame}
          onChange={(e) => setFilterGame(e.target.value)}
          className="px-4 py-3 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none transition-colors"
        >
          <option value="all">All Games</option>
          {games.map(game => (
            <option key={game.game_key} value={game.display_name}>{game.display_name}</option>
          ))}
        </select>

        {/* Filter by Source */}
        <select
          value={filterSource}
          onChange={(e) => setFilterSource(e.target.value)}
          className="px-4 py-3 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none transition-colors"
        >
          <option value="all">All Sources</option>
          <option value="pulse">Pulse</option>
          <option value="easy">Easy Mode</option>
          <option value="advanced">Advanced Mode</option>
        </select>

        {/* Filter by Status */}
        <select
          value={filterStatus}
          onChange={(e) => setFilterStatus(e.target.value)}
          className="px-4 py-3 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#4ecdc4] outline-none transition-colors"
        >
          <option value="all">All Status</option>
          <option value="valid">Valid</option>
          <option value="draft">Draft</option>
          <option value="error">Error</option>
        </select>
      </div>

      {/* Configs Grid */}
      <div className="grid grid-cols-1 gap-4">
        {filteredConfigs.length > 0 ? (
          filteredConfigs.map((config) => {
            const statusConfig = getStatusConfig(config.status);
            return (
              <div
                key={config.id}
                className="p-5 bg-gradient-card rounded-lg border-2 border-[#2a2b30] hover:border-[#4ecdc4] transition-all card-float-hover"
              >
                <div className="flex items-start justify-between">
                  {/* Config Info */}
                  <div className="flex-1">
                    <div className="flex items-center gap-3 mb-2">
                      <h3 className="text-lg font-bold">{config.name}</h3>
                      <span className={`px-2 py-0.5 rounded text-xs font-bold ${
                        config.source === 'pulse'
                          ? 'bg-[#f381ff]/20 text-[#f381ff]'
                          : config.source === 'easy'
                            ? 'bg-[#4ecdc4]/20 text-[#4ecdc4]'
                            : 'bg-[#aa96da]/20 text-[#aa96da]'
                      }`}>
                        {config.source === 'pulse' ? 'PULSE' : config.source === 'easy' ? 'EASY' : 'ADVANCED'}
                      </span>
                      <span className={`flex items-center gap-1.5 px-2 py-0.5 rounded text-xs font-bold ${statusConfig.bg} ${statusConfig.color}`}>
                        {statusConfig.icon}
                        {statusConfig.label}
                      </span>
                    </div>

                    <div className="flex items-center gap-4 text-sm text-[#8e8f94] mb-2">
                      <div className="flex items-center gap-1.5">
                        <span className="font-medium text-white">{config.game}</span>
                      </div>
                      <span>•</span>
                      <div>Created {config.createdAt}</div>
                      <span>•</span>
                      <div>Modified {config.lastModified}</div>
                    </div>

                    {config.description && (
                      <p className="text-sm text-[#8e8f94]">{config.description}</p>
                    )}
                  </div>

                  {/* Actions */}
                  <div className="flex items-center gap-2 ml-4">
                    <button
                      onClick={() => handleEdit(config)}
                      className="w-10 h-10 rounded-lg bg-[#2a2b30] hover:bg-[#4ecdc4] transition-colors flex items-center justify-center group"
                      title="Edit"
                    >
                      <Edit className="w-4 h-4 text-[#8e8f94] group-hover:text-[#14151a]" />
                    </button>
                    <button
                      onClick={() => handleDuplicate(config)}
                      className="w-10 h-10 rounded-lg bg-[#2a2b30] hover:bg-[#aa96da] transition-colors flex items-center justify-center group"
                      title="Duplicate"
                    >
                      <Copy className="w-4 h-4 text-[#8e8f94] group-hover:text-white" />
                    </button>
                    <button
                      onClick={() => handleDelete(config.id)}
                      className="w-10 h-10 rounded-lg bg-[#2a2b30] hover:bg-[#f85149] transition-colors flex items-center justify-center group"
                      title="Delete"
                    >
                      <Trash2 className="w-4 h-4 text-[#8e8f94] group-hover:text-white" />
                    </button>
                  </div>
                </div>
              </div>
            );
          })
        ) : (
          <div className="text-center py-20">
            <FileText className="w-16 h-16 text-[#8e8f94] mx-auto mb-4 opacity-50" />
            <p className="text-lg text-[#8e8f94] mb-2">No configs found</p>
            <p className="text-sm text-[#8e8f94]">Try adjusting your filters or create a new config</p>
          </div>
        )}
      </div>

      {/* Game Selection Modal */}
      {showGameSelection && (
        <GameSelectionModal
          onClose={() => setShowGameSelection(false)}
          mode={selectedMode}
          games={games}
          onSelectGame={(game) => {
            trace('library-page', 'game_selected_for_seed', { mode: selectedMode, gameKey: game.game_key, displayName: game.display_name });
            setSelectedGame(game);
            setShowGameSelection(false);

            if (selectedMode === 'easy') {
              onNavigateToEasyConfig?.(game);
            } else {
              setEditingConfig(null);
              setShowAdvancedConfig(true);
            }
          }}
        />
      )}

      {/* Advanced Config Modal */}
      {showAdvancedConfig && (
        <AdvancedConfigModal
          onClose={() => {
            setShowAdvancedConfig(false);
            setEditingConfig(null);
          }}
          onSave={async (config) => {
            const { name, game: _game, source: _source, ...values } = config;
            try {
              let savedSeed: SeedEntry;
              if (editingConfig) {
                savedSeed = await replaceAdvancedSeed(
                  editingConfig.id,
                  selectedGame,
                  String(name || editingConfig.name || 'Advanced Seed'),
                  playerName,
                  values,
                );
              } else {
                savedSeed = await createAdvancedSeed(selectedGame, String(name || 'Advanced Seed'), playerName, values);
              }
              await loadLibrary({ forceSeeds: true });
              mergeSavedSeedIntoLibrary(savedSeed);
              setShowAdvancedConfig(false);
              setEditingConfig(null);
              setError('');
              trace('library-page', editingConfig ? 'advanced_seed_edited' : 'advanced_seed_created', {
                gameKey: selectedGame.game_key,
                name: String(name || 'Advanced Seed'),
                seedId: editingConfig?.id || '',
              });
            } catch (err) {
              const message = err instanceof Error ? err.message : 'Unable to save advanced seed.';
              traceError('library-page', 'advanced_seed_save_failed', err, { gameKey: selectedGame.game_key });
              setError(message);
              throw new Error(message);
            }
          }}
          game={selectedGame.display_name}
          seedGame={selectedGame}
          initialName={editingConfig?.name || ''}
          initialValues={editingConfig?.values || {}}
          title={editingConfig ? 'Edit Advanced Seed Config' : 'Advanced Seed Config'}
          saveLabel={editingConfig ? 'Save Changes' : 'Save Config'}
        />
      )}
      {error && (
        <ErrorModal
          title="Library error"
          message={error}
          code="LIBRARY_ACTION_FAILED"
          onClose={() => setError('')}
        />
      )}
      {loading && (
        <LoadingModal
          title="Loading Library"
          message="Loading your games and seed configs..."
        />
      )}
    </div>
  );
}
