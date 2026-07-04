import { ALTTP_SHOWCASE_GAME, type SeedGameEntry } from '../services/seedConfig';
import { publicAssetUrl } from '../utils/publicAssets';

export interface SekaiLinkGameCatalogEntry {
  key: string;
  displayName: string;
  asset: string;
  available: boolean;
  forceUnavailable?: boolean;
  seedEntry?: SeedGameEntry;
}

const gameAsset = (fileName: string) => publicAssetUrl(`assets/img/games/${fileName}`);

export const SEKAILINK_GAME_CATALOG: SekaiLinkGameCatalogEntry[] = [
  { key: 'a_link_to_the_past', displayName: 'A Link to the Past', asset: gameAsset('alttp.jpg'), available: true, seedEntry: { ...ALTTP_SHOWCASE_GAME, boxart: gameAsset('alttp.jpg') } },
  { key: 'a_link_between_worlds', displayName: 'A Link Between Worlds', asset: gameAsset('albw.jpg'), available: false, forceUnavailable: true },
  { key: 'chrono_trigger', displayName: 'Chrono Trigger', asset: gameAsset('chronotrigger.png'), available: false },
  { key: 'donkey_kong_64', displayName: 'Donkey Kong 64', asset: gameAsset('dk64.jpg'), available: false, forceUnavailable: true },
  { key: 'donkey_kong_country', displayName: 'Donkey Kong Country', asset: gameAsset('dkc.png'), available: false, forceUnavailable: true },
  { key: 'donkey_kong_country_2', displayName: 'Donkey Kong Country 2', asset: gameAsset('dkc2.jpg'), available: true },
  { key: 'donkey_kong_country_3', displayName: 'Donkey Kong Country 3', asset: gameAsset('dkc3.jpg'), available: true },
  { key: 'earthbound', displayName: 'EarthBound', asset: gameAsset('earthbound.jpg'), available: true },
  { key: 'final_fantasy', displayName: 'Final Fantasy', asset: gameAsset('ff.jpg'), available: false },
  { key: 'final_fantasy_iv', displayName: 'Final Fantasy IV', asset: gameAsset('ffiv.jpg'), available: false },
  { key: 'final_fantasy_mystic_quest', displayName: 'Final Fantasy Mystic Quest', asset: gameAsset('ffmq.jpg'), available: false },
  { key: 'final_fantasy_tactics_advance', displayName: 'Final Fantasy Tactics Advance', asset: gameAsset('fftadvance.jpg'), available: false, forceUnavailable: true },
  { key: 'final_fantasy_v', displayName: 'Final Fantasy V', asset: gameAsset('ffv.png'), available: false, forceUnavailable: true },
  { key: 'final_fantasy_vi', displayName: 'Final Fantasy VI', asset: gameAsset('ffvi.jpg'), available: false },
  { key: 'kirbys_dream_land_3', displayName: "Kirby's Dream Land 3", asset: gameAsset('kdl3.jpg'), available: true },
  { key: 'links_awakening_dx', displayName: "Link's Awakening DX", asset: gameAsset('ladx.jpg'), available: true },
  { key: 'lufia_ii', displayName: 'Lufia II', asset: gameAsset('lufia2.jpg'), available: true },
  { key: 'luigis_mansion', displayName: "Luigi's Mansion", asset: gameAsset('luigimansion.jpg'), available: false, forceUnavailable: true },
  { key: 'majoras_mask', displayName: "Majora's Mask", asset: gameAsset('majoramask.jpg'), available: false, forceUnavailable: true },
  { key: 'mario_luigi_superstar_saga', displayName: 'Mario & Luigi: Superstar Saga', asset: gameAsset('marioluigisuperstar.jpg'), available: false, forceUnavailable: true },
  { key: 'super_mario_sunshine', displayName: 'Super Mario Sunshine', asset: gameAsset('mariosunshine.png'), available: false, forceUnavailable: true },
  { key: 'metroid_fusion', displayName: 'Metroid Fusion', asset: gameAsset('metroidfusion.jpg'), available: true },
  { key: 'metroid_prime', displayName: 'Metroid Prime', asset: gameAsset('metroidprime.jpg'), available: false, forceUnavailable: true },
  { key: 'metroid_zero_mission', displayName: 'Metroid: Zero Mission', asset: gameAsset('metroidzeromission.jpg'), available: true },
  { key: 'the_minish_cap', displayName: 'The Minish Cap', asset: gameAsset('minish.jpg'), available: true },
  { key: 'mario_kart_double_dash', displayName: 'Mario Kart: Double Dash!!', asset: gameAsset('mkdd.jpg'), available: false, forceUnavailable: true },
  { key: 'mega_man_2', displayName: 'Mega Man 2', asset: gameAsset('mm2.jpg'), available: true },
  { key: 'mega_man_3', displayName: 'Mega Man 3', asset: gameAsset('mm3.jpg'), available: true },
  { key: 'mega_man_battle_network_3', displayName: 'Mega Man Battle Network 3', asset: gameAsset('mmbn3.jpg'), available: false, forceUnavailable: true },
  { key: 'mega_man_x3', displayName: 'Mega Man X3', asset: gameAsset('mmx3.jpg'), available: true },
  { key: 'ocarina_of_time', displayName: 'Ocarina of Time', asset: gameAsset('oot.webm'), available: false, forceUnavailable: true },
  { key: 'oracle_of_ages', displayName: 'Oracle of Ages', asset: gameAsset('oraclesages.jpg'), available: true },
  { key: 'oracle_of_seasons', displayName: 'Oracle of Seasons', asset: gameAsset('oracleseasons.jpg'), available: true },
  { key: 'paper_mario', displayName: 'Paper Mario', asset: gameAsset('papermario.jpg'), available: false, forceUnavailable: true },
  { key: 'pokemon_crystal', displayName: 'Pokemon Crystal', asset: gameAsset('pkmncrystal.jpg'), available: false, forceUnavailable: true },
  { key: 'pokemon_emerald', displayName: 'Pokemon Emerald', asset: gameAsset('pkmnemerald.jpg'), available: true },
  { key: 'pokemon_firered', displayName: 'Pokemon FireRed', asset: gameAsset('pkmnfirered.jpg'), available: true },
  { key: 'pokemon_red_blue', displayName: 'Pokemon Red and Blue', asset: gameAsset('pkmnredblue.jpg'), available: true },
  { key: 'secret_of_evermore', displayName: 'Secret of Evermore', asset: gameAsset('secretevermore.jpg'), available: true },
  { key: 'secret_of_mana', displayName: 'Secret of Mana', asset: gameAsset('secretofmana.jpg'), available: true },
  { key: 'skyward_sword', displayName: 'Skyward Sword', asset: gameAsset('skyward.jpg'), available: false, forceUnavailable: true },
  { key: 'super_mario_64', displayName: 'Super Mario 64', asset: gameAsset('sm64.png'), available: false, forceUnavailable: true },
  { key: 'super_mario_land_2', displayName: 'Super Mario Land 2', asset: gameAsset('sml2.jpg'), available: true },
  { key: 'super_mario_world', displayName: 'Super Mario World', asset: gameAsset('smw.jpg'), available: true },
  { key: 'smz3', displayName: 'SMZ3', asset: gameAsset('smz3.jpg'), available: true },
  { key: 'star_fox_64', displayName: 'Star Fox 64', asset: gameAsset('starfox64.jpg'), available: false, forceUnavailable: true },
  { key: 'super_metroid', displayName: 'Super Metroid', asset: gameAsset('supermetorid.jpg'), available: true },
  { key: 'tetris_attack', displayName: 'Tetris Attack', asset: gameAsset('tetrisattack.jpg'), available: false, forceUnavailable: true },
  { key: 'the_legend_of_zelda', displayName: 'The Legend of Zelda', asset: gameAsset('tloz.jpg'), available: true },
  { key: 'twilight_princess', displayName: 'Twilight Princess', asset: gameAsset('tloztp.jpg'), available: false, forceUnavailable: true },
  { key: 'the_wind_waker', displayName: 'The Wind Waker', asset: gameAsset('tlozww.png'), available: false, forceUnavailable: true },
  { key: 'thousand_year_door', displayName: 'The Thousand-Year Door', asset: gameAsset('ttyd.jpg'), available: false, forceUnavailable: true },
  { key: 'wario_land', displayName: 'Wario Land', asset: gameAsset('warioland.jpg'), available: true },
  { key: 'wario_land_4', displayName: 'Wario Land 4', asset: gameAsset('warioland4.jpg'), available: true },
  { key: 'zelda_ii', displayName: 'Zelda II: The Adventure of Link', asset: gameAsset('zelda2.jpg'), available: true },
];
