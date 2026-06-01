import { X, Save, RotateCcw, Info, Gamepad2, Palette, Package, Plus, Search, Check } from 'lucide-react';
import { useEffect, useMemo, useState } from 'react';
import { canonicalSeedGameKey, listSeedOptions, type SeedGameEntry, type SeedOptionDefinition } from '../../services/seedConfig';
import { ErrorModal, LoadingModal } from './FeedbackModal';

interface AdvancedConfigModalProps {
  onClose: () => void;
  onSave?: (config: any) => void | Promise<void>;
  game?: string;
  seedGame?: SeedGameEntry;
  initialName?: string;
  initialValues?: Record<string, unknown>;
  title?: string;
  saveLabel?: string;
}

export default function AdvancedConfigModal({
  onClose,
  onSave,
  game = 'A Link to the Past',
  seedGame,
  initialName = '',
  initialValues = {},
  title = 'Advanced Seed Config',
  saveLabel = 'Save Config',
}: AdvancedConfigModalProps) {
  const [activeTab, setActiveTab] = useState('game');
  const [configName, setConfigName] = useState('');
  const [liveOptions, setLiveOptions] = useState<SeedOptionDefinition[]>([]);
  const [liveValues, setLiveValues] = useState<Record<string, unknown>>({});
  const [liveSchemaError, setLiveSchemaError] = useState('');
  const [saveError, setSaveError] = useState('');
  const [saving, setSaving] = useState(false);

  // Game Options State
  const [progressionBalancing, setProgressionBalancing] = useState(50);
  const [accessibility, setAccessibility] = useState('items');
  const [goal, setGoal] = useState('ganon');
  const [mode, setMode] = useState('open');
  const [glitchesRequired, setGlitchesRequired] = useState('no_glitches');
  const [darkRoomLogic, setDarkRoomLogic] = useState('lamp');
  const [openPyramid, setOpenPyramid] = useState('goal');
  const [crystalsGT, setCrystalsGT] = useState(7);
  const [crystalsGanon, setCrystalsGanon] = useState(7);
  const [bigKeyShuffle, setBigKeyShuffle] = useState('original_dungeon');
  const [smallKeyShuffle, setSmallKeyShuffle] = useState('original_dungeon');
  const [keyDropShuffle, setKeyDropShuffle] = useState(true);
  const [compassShuffle, setCompassShuffle] = useState('original_dungeon');
  const [mapShuffle, setMapShuffle] = useState('original_dungeon');
  const [restrictDungeonBoss, setRestrictDungeonBoss] = useState(false);
  const [itemPool, setItemPool] = useState('normal');
  const [itemFunctionality, setItemFunctionality] = useState('normal');
  const [enemyHealth, setEnemyHealth] = useState('default');
  const [enemyDamage, setEnemyDamage] = useState('default');
  const [progressive, setProgressive] = useState('on');
  const [swordless, setSwordless] = useState(false);
  const [hints, setHints] = useState('on');
  const [deathLink, setDeathLink] = useState(false);

  // Cosmetic Options State
  const [owPalettes, setOwPalettes] = useState('default');
  const [uwPalettes, setUwPalettes] = useState('default');
  const [hudPalettes, setHudPalettes] = useState('default');
  const [quickswap, setQuickswap] = useState(true);
  const [menuspeed, setMenuspeed] = useState('normal');
  const [music, setMusic] = useState(true);
  const [reduceFlashing, setReduceFlashing] = useState(true);

  useEffect(() => {
    setConfigName(initialName);
  }, [initialName]);

  useEffect(() => {
    let cancelled = false;
    const targetGame = seedGame || { game_key: canonicalSeedGameKey(game), display_name: game };
    listSeedOptions(targetGame)
      .then((schema) => {
        if (cancelled) return;
        const nextOptions = schema.options.filter((option) => {
          const key = String(option.option_key || option.yaml_key || '').toLowerCase();
          return key && !key.includes('plando');
        });
        setLiveOptions(nextOptions);
        setLiveValues(initialValues || {});
        setLiveSchemaError('');
      })
      .catch((err) => {
        if (cancelled) return;
        setLiveOptions([]);
        setLiveValues(initialValues || {});
        setLiveSchemaError(err instanceof Error ? err.message : 'Live options are unavailable.');
      });
    return () => {
      cancelled = true;
    };
  }, [game, seedGame]);

  const liveGroups = useMemo(() => {
    const groups = new Map<string, SeedOptionDefinition[]>();
    liveOptions.forEach((option) => {
      const rules = option.validation_rules || {};
      const group = String(rules.group_label || rules.group_key || 'Game Options')
        .replace(/_/g, ' ')
        .toUpperCase();
      groups.set(group, [...(groups.get(group) || []), option]);
    });
    return Array.from(groups.entries()).map(([label, options]) => ({ label, options }));
  }, [liveOptions]);

  const setLiveValue = (key: string, value: unknown) => {
    setLiveValues((current) => ({ ...current, [key]: value }));
  };

  const buildLegacyConfig = () => ({
      name: configName,
      game,
      source: 'advanced',
      // Game Options
      progression_balancing: progressionBalancing,
      accessibility,
      goal,
      mode,
      glitches_required: glitchesRequired,
      dark_room_logic: darkRoomLogic,
      open_pyramid: openPyramid,
      crystals_needed_for_gt: crystalsGT,
      crystals_needed_for_ganon: crystalsGanon,
      big_key_shuffle: bigKeyShuffle,
      small_key_shuffle: smallKeyShuffle,
      key_drop_shuffle: keyDropShuffle,
      compass_shuffle: compassShuffle,
      map_shuffle: mapShuffle,
      restrict_dungeon_item_on_boss: restrictDungeonBoss,
      item_pool: itemPool,
      item_functionality: itemFunctionality,
      enemy_health: enemyHealth,
      enemy_damage: enemyDamage,
      progressive,
      swordless,
      hints,
      death_link: deathLink,
      // Cosmetic Options
      ow_palettes: owPalettes,
      uw_palettes: uwPalettes,
      hud_palettes: hudPalettes,
      quickswap,
      menuspeed,
      music,
      reduceflashing: reduceFlashing,
    });

  const handleSave = async () => {
    if (saving) return;
    setSaveError('');
    setSaving(true);
    const config = liveOptions.length
      ? {
          name: configName,
          game,
          source: 'advanced',
          ...liveValues,
        }
      : buildLegacyConfig();

    try {
      if (onSave) {
        await onSave(config);
      }
      onClose();
    } catch (err) {
      setSaveError(err instanceof Error ? err.message : 'Unable to save advanced seed.');
    } finally {
      setSaving(false);
    }
  };

  const handleReset = () => {
    setLiveValues({});
    // Reset legacy fallback to defaults
    setProgressionBalancing(50);
    setAccessibility('items');
    setGoal('ganon');
    setMode('open');
    // ... etc
  };

  return (
    <div className="fixed inset-0 bg-black/80 backdrop-blur-sm flex items-center justify-center z-50 p-8">
      <div className="w-full max-w-4xl h-[90vh] bg-[#161b22] rounded-xl shadow-2xl border-2 border-[#aa96da] card-float overflow-hidden flex flex-col">
        {/* Header */}
        <div className="p-6 border-b-2 border-[#2a2b30] bg-gradient-to-r from-[#1c1d22] to-[#161b22]">
          <div className="flex items-center justify-between mb-4">
            <div>
              <h2 className="text-2xl font-bold">{title}</h2>
              <p className="text-sm text-[#8e8f94]">{game}</p>
            </div>
            <button
              onClick={onClose}
              className="w-10 h-10 rounded-lg bg-[#2a2b30] hover:bg-[#f85149] transition-colors flex items-center justify-center group"
            >
              <X className="w-5 h-5 text-[#8e8f94] group-hover:text-white" />
            </button>
          </div>

          {/* Top Fields */}
          <div>
            <label className="block text-xs font-medium text-[#8e8f94] mb-2">
              CONFIG NAME *
            </label>
            <input
              type="text"
              value={configName}
              onChange={(e) => setConfigName(e.target.value)}
              placeholder="My Advanced Config"
              className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#aa96da] outline-none transition-colors"
            />
          </div>
        </div>

        {/* Tabs */}
        <div className="flex gap-3 px-6 pt-6 pb-2 bg-[#14151a]/90">
          {[
            { id: 'game', label: 'Game Options', icon: <Gamepad2 className="w-5 h-5" /> },
            { id: 'cosmetic', label: 'Cosmetic Options', icon: <Palette className="w-5 h-5" /> },
            { id: 'items', label: 'Item & Location Options', icon: <Package className="w-5 h-5" /> },
          ].map((tab) => (
            <button
              key={tab.id}
              onClick={() => setActiveTab(tab.id)}
              className={`flex-1 flex items-center justify-center gap-2 px-6 py-4 rounded-lg text-sm font-bold transition-all ${
                activeTab === tab.id
                  ? 'bg-gradient-to-r from-[#aa96da] to-[#f38181] text-white shadow-lg'
                  : 'bg-[#2a2b30] text-[#8e8f94] hover:bg-[#3a3b40] hover:text-white'
              }`}
            >
              {tab.icon}
              {tab.label}
            </button>
          ))}
        </div>

        {/* Content */}
        <div className="flex-1 overflow-auto p-6 bg-[#14151a]/90">
          {liveSchemaError && !liveOptions.length && (
            <div className="mb-4 p-3 bg-[#f69d50]/10 border border-[#f69d50]/40 rounded-lg text-sm text-[#f69d50]">
              {liveSchemaError}
            </div>
          )}
          {liveOptions.length > 0 ? (
            <div className="space-y-6">
              {liveGroups.map((group) => (
                <div key={group.label} className="p-5 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg">
                  <h3 className="text-sm font-bold text-[#e6edf3] mb-4">{group.label}</h3>
                  <div className="grid grid-cols-2 gap-4">
                    {group.options.map((option) => {
                      const valueKey = liveOptionValueKey(option);
                      return (
                        <LiveOptionField
                          key={option.option_key}
                          option={option}
                          value={liveValues[valueKey] ?? defaultLiveOptionValue(option)}
                          onChange={(value) => setLiveValue(valueKey, value)}
                        />
                      );
                    })}
                  </div>
                </div>
              ))}
            </div>
          ) : activeTab === 'game' && (
            <div className="space-y-6">
              {/* Basic Settings */}
              <div className="p-5 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg">
                <h3 className="text-sm font-bold text-[#e6edf3] mb-4">BASIC SETTINGS</h3>
                <div className="grid grid-cols-2 gap-4">
                  {/* Goal */}
                  <div>
                    <label className="block text-xs font-medium text-[#8e8f94] mb-2">GOAL</label>
                    <select
                      value={goal}
                      onChange={(e) => setGoal(e.target.value)}
                      className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors"
                    >
                      <option value="ganon">Defeat Ganon</option>
                      <option value="crystals">All Crystals</option>
                      <option value="bosses">All Bosses</option>
                      <option value="pedestal">Master Sword Pedestal</option>
                      <option value="triforce_hunt">Triforce Hunt</option>
                    </select>
                  </div>

                  {/* Mode */}
                  <div>
                    <label className="block text-xs font-medium text-[#8e8f94] mb-2">MODE</label>
                    <select
                      value={mode}
                      onChange={(e) => setMode(e.target.value)}
                      className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors"
                    >
                      <option value="standard">Standard</option>
                      <option value="open">Open</option>
                      <option value="inverted">Inverted</option>
                    </select>
                  </div>

                  {/* Glitches Required */}
                  <div>
                    <label className="block text-xs font-medium text-[#8e8f94] mb-2">GLITCHES REQUIRED</label>
                    <select
                      value={glitchesRequired}
                      onChange={(e) => setGlitchesRequired(e.target.value)}
                      className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors"
                    >
                      <option value="no_glitches">No Glitches</option>
                      <option value="minor_glitches">Minor Glitches</option>
                      <option value="overworld_glitches">Overworld Glitches</option>
                      <option value="hybrid_major_glitches">Hybrid Major Glitches</option>
                      <option value="no_logic">No Logic</option>
                    </select>
                  </div>

                  {/* Accessibility */}
                  <div>
                    <label className="block text-xs font-medium text-[#8e8f94] mb-2">ACCESSIBILITY</label>
                    <select
                      value={accessibility}
                      onChange={(e) => setAccessibility(e.target.value)}
                      className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors"
                    >
                      <option value="full">Full - All locations reachable</option>
                      <option value="items">Items - All items reachable</option>
                      <option value="minimal">Minimal - Beatable</option>
                    </select>
                  </div>
                </div>
              </div>

              {/* Progression */}
              <div className="p-5 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg">
                <h3 className="text-sm font-bold text-[#e6edf3] mb-4">PROGRESSION</h3>
                <div className="space-y-4">
                  {/* Progression Balancing */}
                  <div>
                    <div className="flex items-center justify-between mb-2">
                      <label className="text-xs font-medium text-[#8e8f94]">PROGRESSION BALANCING</label>
                      <span className="text-sm font-bold text-white">{progressionBalancing}</span>
                    </div>
                    <input
                      type="range"
                      min="0"
                      max="100"
                      value={progressionBalancing}
                      onChange={(e) => setProgressionBalancing(parseInt(e.target.value))}
                      className="w-full h-2 bg-[#2a2b30] rounded-lg appearance-none cursor-pointer accent-[#aa96da]"
                    />
                  </div>

                  <div className="grid grid-cols-2 gap-4">
                    {/* Crystals for GT */}
                    <div>
                      <div className="flex items-center justify-between mb-2">
                        <label className="text-xs font-medium text-[#8e8f94]">CRYSTALS FOR GT</label>
                        <span className="text-sm font-bold text-white">{crystalsGT}</span>
                      </div>
                      <input
                        type="range"
                        min="0"
                        max="7"
                        value={crystalsGT}
                        onChange={(e) => setCrystalsGT(parseInt(e.target.value))}
                        className="w-full h-2 bg-[#2a2b30] rounded-lg appearance-none cursor-pointer accent-[#aa96da]"
                      />
                    </div>

                    {/* Crystals for Ganon */}
                    <div>
                      <div className="flex items-center justify-between mb-2">
                        <label className="text-xs font-medium text-[#8e8f94]">CRYSTALS FOR GANON</label>
                        <span className="text-sm font-bold text-white">{crystalsGanon}</span>
                      </div>
                      <input
                        type="range"
                        min="0"
                        max="7"
                        value={crystalsGanon}
                        onChange={(e) => setCrystalsGanon(parseInt(e.target.value))}
                        className="w-full h-2 bg-[#2a2b30] rounded-lg appearance-none cursor-pointer accent-[#aa96da]"
                      />
                    </div>
                  </div>
                </div>
              </div>

              {/* Key Shuffle */}
              <div className="p-5 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg">
                <h3 className="text-sm font-bold text-[#e6edf3] mb-4">KEY & MAP SHUFFLE</h3>
                <div className="grid grid-cols-2 gap-4">
                  {/* Big Key Shuffle */}
                  <div>
                    <label className="block text-xs font-medium text-[#8e8f94] mb-2">BIG KEY SHUFFLE</label>
                    <select
                      value={bigKeyShuffle}
                      onChange={(e) => setBigKeyShuffle(e.target.value)}
                      className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors text-sm"
                    >
                      <option value="original_dungeon">Original Dungeon</option>
                      <option value="own_dungeons">Own Dungeons</option>
                      <option value="own_world">Own World</option>
                      <option value="any_world">Any World</option>
                      <option value="different_world">Different World</option>
                      <option value="start_with">Start With</option>
                    </select>
                  </div>

                  {/* Small Key Shuffle */}
                  <div>
                    <label className="block text-xs font-medium text-[#8e8f94] mb-2">SMALL KEY SHUFFLE</label>
                    <select
                      value={smallKeyShuffle}
                      onChange={(e) => setSmallKeyShuffle(e.target.value)}
                      className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors text-sm"
                    >
                      <option value="original_dungeon">Original Dungeon</option>
                      <option value="own_dungeons">Own Dungeons</option>
                      <option value="own_world">Own World</option>
                      <option value="any_world">Any World</option>
                      <option value="different_world">Different World</option>
                      <option value="start_with">Start With</option>
                      <option value="universal">Universal</option>
                    </select>
                  </div>

                  {/* Compass Shuffle */}
                  <div>
                    <label className="block text-xs font-medium text-[#8e8f94] mb-2">COMPASS SHUFFLE</label>
                    <select
                      value={compassShuffle}
                      onChange={(e) => setCompassShuffle(e.target.value)}
                      className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors text-sm"
                    >
                      <option value="original_dungeon">Original Dungeon</option>
                      <option value="own_dungeons">Own Dungeons</option>
                      <option value="own_world">Own World</option>
                      <option value="any_world">Any World</option>
                      <option value="different_world">Different World</option>
                      <option value="start_with">Start With</option>
                    </select>
                  </div>

                  {/* Map Shuffle */}
                  <div>
                    <label className="block text-xs font-medium text-[#8e8f94] mb-2">MAP SHUFFLE</label>
                    <select
                      value={mapShuffle}
                      onChange={(e) => setMapShuffle(e.target.value)}
                      className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors text-sm"
                    >
                      <option value="original_dungeon">Original Dungeon</option>
                      <option value="own_dungeons">Own Dungeons</option>
                      <option value="own_world">Own World</option>
                      <option value="any_world">Any World</option>
                      <option value="different_world">Different World</option>
                      <option value="start_with">Start With</option>
                    </select>
                  </div>
                </div>

                {/* Toggles */}
                <div className="mt-4 space-y-3">
                  <ToggleOption
                    label="Key Drop Shuffle"
                    value={keyDropShuffle}
                    onChange={setKeyDropShuffle}
                    description="Shuffle enemy key drops"
                  />
                  <ToggleOption
                    label="Prevent Dungeon Item on Boss"
                    value={restrictDungeonBoss}
                    onChange={setRestrictDungeonBoss}
                    description="Dungeon items won't be on boss"
                  />
                </div>
              </div>

              {/* Difficulty */}
              <div className="p-5 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg">
                <h3 className="text-sm font-bold text-[#e6edf3] mb-4">DIFFICULTY</h3>
                <div className="grid grid-cols-2 gap-4">
                  <div>
                    <label className="block text-xs font-medium text-[#8e8f94] mb-2">ITEM POOL</label>
                    <select
                      value={itemPool}
                      onChange={(e) => setItemPool(e.target.value)}
                      className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors"
                    >
                      <option value="easy">Easy</option>
                      <option value="normal">Normal</option>
                      <option value="hard">Hard</option>
                      <option value="expert">Expert</option>
                    </select>
                  </div>

                  <div>
                    <label className="block text-xs font-medium text-[#8e8f94] mb-2">ITEM FUNCTIONALITY</label>
                    <select
                      value={itemFunctionality}
                      onChange={(e) => setItemFunctionality(e.target.value)}
                      className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors"
                    >
                      <option value="easy">Easy</option>
                      <option value="normal">Normal</option>
                      <option value="hard">Hard</option>
                      <option value="expert">Expert</option>
                    </select>
                  </div>

                  <div>
                    <label className="block text-xs font-medium text-[#8e8f94] mb-2">ENEMY HEALTH</label>
                    <select
                      value={enemyHealth}
                      onChange={(e) => setEnemyHealth(e.target.value)}
                      className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors"
                    >
                      <option value="easy">Easy</option>
                      <option value="default">Default</option>
                      <option value="hard">Hard</option>
                      <option value="expert">Expert</option>
                    </select>
                  </div>

                  <div>
                    <label className="block text-xs font-medium text-[#8e8f94] mb-2">ENEMY DAMAGE</label>
                    <select
                      value={enemyDamage}
                      onChange={(e) => setEnemyDamage(e.target.value)}
                      className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors"
                    >
                      <option value="default">Default</option>
                      <option value="shuffled">Shuffled</option>
                      <option value="chaos">Chaos</option>
                    </select>
                  </div>
                </div>
              </div>

              {/* Gameplay Options */}
              <div className="p-5 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg">
                <h3 className="text-sm font-bold text-[#e6edf3] mb-4">GAMEPLAY OPTIONS</h3>
                <div className="space-y-3">
                  <div className="grid grid-cols-2 gap-4 mb-4">
                    <div>
                      <label className="block text-xs font-medium text-[#8e8f94] mb-2">HINTS</label>
                      <select
                        value={hints}
                        onChange={(e) => setHints(e.target.value)}
                        className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors"
                      >
                        <option value="off">Off</option>
                        <option value="on">On</option>
                        <option value="full">Full</option>
                      </select>
                    </div>

                    <div>
                      <label className="block text-xs font-medium text-[#8e8f94] mb-2">PROGRESSIVE ITEMS</label>
                      <select
                        value={progressive}
                        onChange={(e) => setProgressive(e.target.value)}
                        className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors"
                      >
                        <option value="off">Off</option>
                        <option value="grouped_random">Grouped Random</option>
                        <option value="on">On</option>
                      </select>
                    </div>
                  </div>

                  <ToggleOption
                    label="Swordless"
                    value={swordless}
                    onChange={setSwordless}
                    description="Beat the game without swords"
                  />
                  <ToggleOption
                    label="Death Link"
                    value={deathLink}
                    onChange={setDeathLink}
                    description="Share deaths with other players"
                  />
                </div>
              </div>
            </div>
          )}

          {activeTab === 'cosmetic' && (
            <div className="space-y-6">
              {/* Palettes */}
              <div className="p-5 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg">
                <h3 className="text-sm font-bold text-[#e6edf3] mb-4">PALETTES</h3>
                <div className="grid grid-cols-2 gap-4">
                  <div>
                    <label className="block text-xs font-medium text-[#8e8f94] mb-2">OVERWORLD PALETTE</label>
                    <select
                      value={owPalettes}
                      onChange={(e) => setOwPalettes(e.target.value)}
                      className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors"
                    >
                      <option value="default">Default</option>
                      <option value="good">Good</option>
                      <option value="blackout">Blackout</option>
                      <option value="puke">Puke</option>
                      <option value="classic">Classic</option>
                      <option value="grayscale">Grayscale</option>
                      <option value="negative">Negative</option>
                    </select>
                  </div>

                  <div>
                    <label className="block text-xs font-medium text-[#8e8f94] mb-2">UNDERWORLD PALETTE</label>
                    <select
                      value={uwPalettes}
                      onChange={(e) => setUwPalettes(e.target.value)}
                      className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors"
                    >
                      <option value="default">Default</option>
                      <option value="good">Good</option>
                      <option value="blackout">Blackout</option>
                      <option value="puke">Puke</option>
                      <option value="classic">Classic</option>
                      <option value="grayscale">Grayscale</option>
                      <option value="negative">Negative</option>
                    </select>
                  </div>

                  <div>
                    <label className="block text-xs font-medium text-[#8e8f94] mb-2">MENU PALETTE</label>
                    <select
                      value={hudPalettes}
                      onChange={(e) => setHudPalettes(e.target.value)}
                      className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors"
                    >
                      <option value="default">Default</option>
                      <option value="good">Good</option>
                      <option value="blackout">Blackout</option>
                      <option value="puke">Puke</option>
                      <option value="classic">Classic</option>
                      <option value="grayscale">Grayscale</option>
                      <option value="negative">Negative</option>
                    </select>
                  </div>

                  <div>
                    <label className="block text-xs font-medium text-[#8e8f94] mb-2">MENU SPEED</label>
                    <select
                      value={menuspeed}
                      onChange={(e) => setMenuspeed(e.target.value)}
                      className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors"
                    >
                      <option value="half">Half</option>
                      <option value="normal">Normal</option>
                      <option value="double">Double</option>
                      <option value="triple">Triple</option>
                      <option value="quadruple">Quadruple</option>
                      <option value="instant">Instant</option>
                    </select>
                  </div>
                </div>
              </div>

              {/* Gameplay Feel */}
              <div className="p-5 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg">
                <h3 className="text-sm font-bold text-[#e6edf3] mb-4">GAMEPLAY FEEL</h3>
                <div className="space-y-3">
                  <ToggleOption
                    label="L/R Quickswapping"
                    value={quickswap}
                    onChange={setQuickswap}
                    description="Quick item swap with L/R buttons"
                  />
                  <ToggleOption
                    label="Play Music"
                    value={music}
                    onChange={setMusic}
                    description="Enable background music"
                  />
                  <ToggleOption
                    label="Reduce Screen Flashes"
                    value={reduceFlashing}
                    onChange={setReduceFlashing}
                    description="Reduce epilepsy-inducing flashes"
                  />
                </div>
              </div>
            </div>
          )}

          {activeTab === 'items' && (
            <div className="space-y-6">
              {/* Local Items */}
              <ItemLocationSection
                title="LOCAL ITEMS"
                description="Items that will only appear in your own world"
                items={[
                  'Progressive Sword', 'Progressive Shield', 'Progressive Armor', 'Progressive Bow',
                  'Hookshot', 'Mushroom', 'Magic Powder', 'Fire Rod', 'Ice Rod', 'Bombos',
                  'Ether', 'Quake', 'Lamp', 'Hammer', 'Shovel', 'Flute', 'Bug Catching Net',
                  'Book of Mudora', 'Bottle', 'Cane of Somaria', 'Cane of Byrna', 'Cape',
                  'Magic Mirror', 'Boots', 'Progressive Glove', 'Flippers', 'Moon Pearl',
                  'Half Magic', 'Piece of Heart', 'Boss Heart Container', 'Sanctuary Heart Container'
                ]}
              />

              {/* Non-local Items */}
              <ItemLocationSection
                title="NON-LOCAL ITEMS"
                description="Items that cannot appear in your own world"
                items={[
                  'Progressive Sword', 'Progressive Shield', 'Progressive Armor', 'Progressive Bow',
                  'Hookshot', 'Mushroom', 'Magic Powder', 'Fire Rod', 'Ice Rod', 'Bombos',
                ]}
              />

              {/* Start Inventory */}
              <div className="p-5 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg">
                <div className="flex items-center justify-between mb-4">
                  <div>
                    <h3 className="text-sm font-bold text-[#e6edf3]">START INVENTORY</h3>
                    <p className="text-xs text-[#8e8f94] mt-1">Items you begin the game with</p>
                  </div>
                  <button className="px-4 py-2 bg-[#aa96da] hover:bg-[#aa96da]/80 rounded-lg text-sm font-medium transition-colors flex items-center gap-2">
                    <Plus className="w-4 h-4" />
                    Add Item
                  </button>
                </div>
                <div className="text-center py-8 text-sm text-[#8e8f94]">
                  No starting items configured
                </div>
              </div>

              {/* Excluded Locations */}
              <ItemLocationSection
                title="EXCLUDED LOCATIONS"
                description="Locations that will not contain progression items"
                items={[
                  'Mushroom', 'King Zora', 'Sahasrahla', 'Blacksmith', 'Purple Chest',
                  'Ether Tablet', 'Bombos Tablet', 'King\'s Tomb', 'Light World Swamp (2)',
                  'Lumberjack Tree', 'Spectacle Rock Cave', 'South of Grove', 'Checkerboard Cave',
                  'Mini Moldorm Cave (5)', 'Ice Rod Cave', 'Hobo', 'Pyramid',
                  'Catfish', 'Stumpy', 'Hype Cave (5)', 'Brewery', 'C-Shaped House',
                ]}
                isLocationMode={true}
              />

              {/* Priority Locations */}
              <ItemLocationSection
                title="PRIORITY LOCATIONS"
                description="Locations that will contain progression items early"
                items={[
                  'Mushroom', 'King Zora', 'Sahasrahla', 'Blacksmith', 'Purple Chest',
                  'Ether Tablet', 'Bombos Tablet',
                ]}
                isLocationMode={true}
              />
            </div>
          )}
        </div>

        {/* Footer */}
        <div className="p-6 border-t-2 border-[#2a2b30] bg-[#14151a]/90 flex justify-between items-center">
          <button
            onClick={handleReset}
            className="px-6 py-3 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg font-medium transition-colors flex items-center gap-2"
          >
            <RotateCcw className="w-4 h-4" />
            Reset to Defaults
          </button>

          <div className="flex gap-3">
            <button
              onClick={onClose}
              className="px-6 py-3 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg font-medium transition-colors"
            >
              Cancel
            </button>
            <button
              onClick={handleSave}
              disabled={!configName || saving}
              className="px-8 py-3 bg-gradient-to-r from-[#aa96da] to-[#f38181] rounded-lg font-bold shadow-lg hover:shadow-xl transition-all disabled:opacity-50 disabled:cursor-not-allowed flex items-center gap-2"
            >
              <Save className="w-4 h-4" />
              {saveLabel}
            </button>
          </div>
        </div>
      </div>
      {saving && (
        <LoadingModal
          title="Saving to server..."
          message="Your Sync configuration is being validated and stored in Nexus."
        />
      )}
      {saveError && (
        <ErrorModal
          title="Could not save seed config"
          message={saveError}
          code="ADVANCED_SEED_SAVE_FAILED"
          onClose={() => setSaveError('')}
        />
      )}
    </div>
  );
}

function LiveOptionField({
  option,
  value,
  onChange,
}: {
  option: SeedOptionDefinition;
  value: unknown;
  onChange: (value: unknown) => void;
}) {
  const label = option.label || option.option_key.replace(/_/g, ' ');
  const description = option.description || '';
  const rules = option.validation_rules || {};
  const min = Number(rules.range_start ?? rules.min ?? 0);
  const max = Number(rules.range_end ?? rules.max ?? 100);
  const numericValue = typeof value === 'number' ? value : Number(value || min);

  if (option.type === 'boolean') {
    return (
      <div className="col-span-2">
        <ToggleOption
          label={label}
          value={Boolean(value)}
          onChange={onChange}
          description={description}
        />
      </div>
    );
  }

  if (option.type === 'enum' && option.choices?.length) {
    return (
      <div>
        <label className="block text-xs font-medium text-[#8e8f94] mb-2">{label.toUpperCase()}</label>
        <select
          value={String(value ?? '')}
          onChange={(event) => onChange(event.target.value)}
          title={description}
          className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors"
        >
          {option.choices.map((choice) => {
            const choiceValue = choice.yaml_value || choice.choice_key;
            return (
              <option key={choice.choice_key || choiceValue} value={choiceValue}>
                {choice.label || choiceValue}
              </option>
            );
          })}
        </select>
      </div>
    );
  }

  if (option.type === 'integer' || option.type === 'number') {
    const hasRange = Number.isFinite(min) && Number.isFinite(max) && max > min;
    return (
      <div>
        <div className="flex items-center justify-between mb-2">
          <label className="text-xs font-medium text-[#8e8f94]" title={description}>{label.toUpperCase()}</label>
          <span className="text-sm font-bold text-white">{numericValue}</span>
        </div>
        {hasRange ? (
          <input
            type="range"
            min={min}
            max={max}
            value={numericValue}
            onChange={(event) => onChange(option.type === 'integer' ? parseInt(event.target.value, 10) : parseFloat(event.target.value))}
            className="w-full h-2 bg-[#2a2b30] rounded-lg appearance-none cursor-pointer accent-[#aa96da]"
          />
        ) : (
          <input
            type="number"
            value={numericValue}
            onChange={(event) => onChange(option.type === 'integer' ? parseInt(event.target.value, 10) : parseFloat(event.target.value))}
            className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors"
          />
        )}
      </div>
    );
  }

  if (option.type === 'array') {
    const text = Array.isArray(value) ? value.join('\n') : String(value || '');
    return (
      <div className="col-span-2">
        <label className="block text-xs font-medium text-[#8e8f94] mb-2">{label.toUpperCase()}</label>
        <textarea
          value={text}
          onChange={(event) => onChange(event.target.value.split('\n').map((line) => line.trim()).filter(Boolean))}
          placeholder="One entry per line"
          title={description}
          className="w-full min-h-28 px-4 py-3 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#aa96da] outline-none transition-colors"
        />
      </div>
    );
  }

  if (option.type === 'object') {
    const text = value && typeof value === 'object' ? JSON.stringify(value, null, 2) : String(value || '{}');
    return (
      <div className="col-span-2">
        <label className="block text-xs font-medium text-[#8e8f94] mb-2">{label.toUpperCase()}</label>
        <textarea
          value={text}
          onChange={(event) => {
            try {
              onChange(JSON.parse(event.target.value || '{}'));
            } catch {
              onChange(event.target.value);
            }
          }}
          title={description}
          className="w-full min-h-32 px-4 py-3 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors font-mono text-xs"
        />
      </div>
    );
  }

  return (
    <div>
      <label className="block text-xs font-medium text-[#8e8f94] mb-2">{label.toUpperCase()}</label>
      <input
        type="text"
        value={String(value || '')}
        onChange={(event) => onChange(event.target.value)}
        title={description}
        className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#aa96da] outline-none transition-colors"
      />
    </div>
  );
}

function liveOptionValueKey(option: SeedOptionDefinition) {
  return String(option.yaml_key || option.option_key || '').trim();
}

function defaultLiveOptionValue(option: SeedOptionDefinition): unknown {
  if (option.type === 'enum' && option.choices?.length) {
    const defaultText = option.default_value === undefined || option.default_value === null
      ? ''
      : String(option.default_value);
    const defaultChoice = option.choices.find((choice) => {
      const choiceValue = choice.yaml_value || choice.choice_key;
      return String(choiceValue) === defaultText;
    });
    return defaultChoice?.yaml_value || defaultChoice?.choice_key || option.choices[0]?.yaml_value || option.choices[0]?.choice_key || '';
  }
  if (option.default_value !== undefined && option.default_value !== null) return option.default_value;
  if (option.type === 'boolean') return false;
  if (option.type === 'integer' || option.type === 'number') return option.validation_rules?.range_start ?? 0;
  if (option.type === 'array') return [];
  if (option.type === 'object') return {};
  return '';
}

function ItemLocationSection({ title, description, items, isLocationMode = false }: {
  title: string;
  description: string;
  items: string[];
  isLocationMode?: boolean;
}) {
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedItems, setSelectedItems] = useState<Set<string>>(new Set());

  const filteredItems = items.filter(item =>
    item.toLowerCase().includes(searchQuery.toLowerCase())
  );

  const toggleItem = (item: string) => {
    const newSelected = new Set(selectedItems);
    if (newSelected.has(item)) {
      newSelected.delete(item);
    } else {
      newSelected.add(item);
    }
    setSelectedItems(newSelected);
  };

  const toggleAll = () => {
    if (selectedItems.size === filteredItems.length) {
      setSelectedItems(new Set());
    } else {
      setSelectedItems(new Set(filteredItems));
    }
  };

  return (
    <div className="p-5 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg">
      <div className="flex items-center justify-between mb-4">
        <div>
          <h3 className="text-sm font-bold text-[#e6edf3]">{title}</h3>
          <p className="text-xs text-[#8e8f94] mt-1">{description}</p>
        </div>
        <div className="flex items-center gap-2">
          <span className="text-xs text-[#8e8f94]">
            {selectedItems.size} / {items.length} selected
          </span>
          <button
            onClick={toggleAll}
            className="px-3 py-1.5 bg-[#2a2b30] hover:bg-[#3a3b40] rounded text-xs font-medium transition-colors"
          >
            {selectedItems.size === filteredItems.length ? 'Clear All' : 'Select All'}
          </button>
        </div>
      </div>

      {/* Search */}
      <div className="relative mb-4">
        <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-[#8e8f94]" />
        <input
          type="text"
          value={searchQuery}
          onChange={(e) => setSearchQuery(e.target.value)}
          placeholder={`Search ${isLocationMode ? 'locations' : 'items'}...`}
          className="w-full pl-9 pr-4 py-2 bg-[#0d1117] border border-[#2a2b30] rounded-lg text-sm text-white placeholder:text-[#8e8f94] focus:border-[#aa96da] outline-none transition-colors"
        />
      </div>

      {/* Items Grid */}
      <div className="grid grid-cols-3 gap-2 max-h-[300px] overflow-y-auto p-2">
        {filteredItems.map((item) => (
          <div
            key={item}
            onClick={() => toggleItem(item)}
            className={`p-2.5 rounded-lg border-2 transition-all cursor-pointer ${
              selectedItems.has(item)
                ? 'bg-[#aa96da]/10 border-[#aa96da] shadow-[0_0_10px_rgba(170,150,218,0.3)]'
                : 'bg-[#0d1117] border-[#2a2b30] hover:border-[#aa96da]/50'
            }`}
          >
            <div className="flex items-center gap-2">
              <div className={`w-4 h-4 rounded border-2 flex items-center justify-center transition-all ${
                selectedItems.has(item)
                  ? 'bg-[#aa96da] border-[#aa96da]'
                  : 'border-[#2a2b30]'
              }`}>
                {selectedItems.has(item) && (
                  <Check className="w-3 h-3 text-white" />
                )}
              </div>
              <span className="text-xs font-medium truncate">{item}</span>
            </div>
          </div>
        ))}
        {filteredItems.length === 0 && (
          <div className="col-span-3 text-center py-8 text-sm text-[#8e8f94]">
            No {isLocationMode ? 'locations' : 'items'} found
          </div>
        )}
      </div>
    </div>
  );
}

function ToggleOption({ label, value, onChange, description }: {
  label: string;
  value: boolean;
  onChange: (value: boolean) => void;
  description?: string;
}) {
  return (
    <div
      onClick={() => onChange(!value)}
      className="p-4 bg-[#0d1117] border border-[#2a2b30] rounded-lg hover:border-[#aa96da] transition-colors cursor-pointer"
    >
      <div className="flex items-center justify-between">
        <div className="flex-1">
          <div className="font-medium text-sm text-white mb-1">{label}</div>
          {description && (
            <div className="text-xs text-[#8e8f94]">{description}</div>
          )}
        </div>
        <button
          type="button"
          className={`
            relative w-12 h-6 border-2 transition-all
            ${value ? 'bg-[#aa96da] border-[#aa96da]' : 'bg-[#2a2b30] border-[#2a2b30]'}
          `}
        >
          <div
            className={`
              absolute top-0.5 w-4 h-4 bg-[#0d1117] transition-all
              ${value ? 'right-0.5' : 'left-0.5'}
            `}
          />
        </button>
      </div>
    </div>
  );
}
