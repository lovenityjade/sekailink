const IMAGE_FILENAMES: Record<string, string> = {
  "A Link to the Past": "a_link_to_the_past.png",
  "A Link Between Worlds": "a_link_between_worlds.png",
  "Donkey Kong Country": "donkey_kong_country.png",
  "Donkey Kong Country 2": "donkey_kong_country_2.png",
  "Donkey Kong Country 3": "donkey_kong_country_3.png",
  EarthBound: "earthbound.png",
  "Final Fantasy IV Free Enterprise": "final_fantasy_iv_free_enterprise.png",
  "Final Fantasy Tactics Advance": "final_fantasy_tactics_advance.png",
  "Kirbys Dream Land 3": "kirbys_dream_land_3.png",
  "Lufia II Ancient Cave": "lufia_ii_ancient_cave.png",
  "Mega Man 2": "mega_man_2.png",
  "Mega Man 3": "mega_man_3.png",
  "Metroid Fusion": "metroid_fusion.png",
  "Metroid Zero Mission": "metroid_zero_mission.png",
  "Ocarina of Time": "ocarina_of_time.webp",
  "Pokemon Crystal": "pokemon_crystal.png",
  "Pokemon Emerald": "pokemon_emerald.png",
  "Pokemon FireRed and LeafGreen": "pokemon_firered_and_leafgreen.png",
  "Pokemon Red and Blue": "pokemon_red_and_blue.png",
  "Ship of Harkinian": "ship_of_harkinian.png",
  SMZ3: "smz3.png",
  "Super Mario 64": "super_mario_64.png",
  "Super Mario Land 2": "super_mario_land_2.png",
  "Super Mario World": "super_mario_world.png",
  "Super Metroid": "super_metroid.jpg",
  "The Legend of Zelda": "the_legend_of_zelda.png",
  "The Legend of Zelda - Oracle of Seasons": "the_legend_of_zelda_oracle_of_seasons.png",
  "The Minish Cap": "the_minish_cap.png",
  "Wario Land": "wario_land.png",
  "Wario Land 4": "wario_land_4.png",
  "Yoshis Island": "yoshis_island.png",
  "Zelda II: The Adventure of Link": "zelda_ii_the_adventure_of_link.jpg",
};

const ALIASES: Record<string, string> = {
  A_Link_to_the_Past: "A Link to the Past",
  A_Link_Between_Worlds: "A Link Between Worlds",
  Donkey_Kong_Country: "Donkey Kong Country",
  Donkey_Kong_Country_2: "Donkey Kong Country 2",
  Donkey_Kong_Country_3: "Donkey Kong Country 3",
  EarthBound: "EarthBound",
  Final_Fantasy_IV_Free_Enterprise: "Final Fantasy IV Free Enterprise",
  Final_Fantasy_Tactics_Advance: "Final Fantasy Tactics Advance",
  Kirbys_Dream_Land_3: "Kirbys Dream Land 3",
  Lufia_II_Ancient_Cave: "Lufia II Ancient Cave",
  Mega_Man_2: "Mega Man 2",
  Mega_Man_3: "Mega Man 3",
  Metroid_Fusion: "Metroid Fusion",
  Metroid_Zero_Mission: "Metroid Zero Mission",
  Ocarina_of_Time: "Ocarina of Time",
  Pokemon_Crystal: "Pokemon Crystal",
  Pokemon_Emerald: "Pokemon Emerald",
  Pokemon_FireRed_and_LeafGreen: "Pokemon FireRed and LeafGreen",
  Pokemon_Red_and_Blue: "Pokemon Red and Blue",
  Ship_of_Harkinian: "Ship of Harkinian",
  SMZ3: "SMZ3",
  Super_Mario_64: "Super Mario 64",
  Super_Mario_Land_2: "Super Mario Land 2",
  Super_Mario_World: "Super Mario World",
  Super_Metroid: "Super Metroid",
  The_Legend_of_Zelda: "The Legend of Zelda",
  "The_Legend_of_Zelda_-_Oracle_of_Seasons": "The Legend of Zelda - Oracle of Seasons",
  The_Minish_Cap: "The Minish Cap",
  Wario_Land: "Wario Land",
  Wario_Land_4: "Wario Land 4",
  Yoshis_Island: "Yoshis Island",
  Zelda_II_The_Adventure_of_Link: "Zelda II: The Adventure of Link",
};

const normalize = (value: string) =>
  String(value || "")
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "_")
    .replace(/^_+|_+$/g, "");

const normalizedNames: Record<string, string> = Object.keys(IMAGE_FILENAMES).reduce((acc, name) => {
  acc[normalize(name)] = name;
  return acc;
}, {} as Record<string, string>);

const toLocalAssetUrl = (filename: string): string => {
  const base = String(import.meta.env.BASE_URL || "/").replace(/\/?$/, "/");
  if (typeof window !== "undefined" && window.location?.href) {
    return new URL(`${base}assets/boxart/${filename}`, window.location.href).toString();
  }
  return `${base}assets/boxart/${filename}`;
};

export const lookupFeaturedGridUrl = (key: string): string | undefined => {
  const directName = IMAGE_FILENAMES[key];
  if (directName) return toLocalAssetUrl(directName);

  const aliasName = ALIASES[key];
  if (aliasName && IMAGE_FILENAMES[aliasName]) return toLocalAssetUrl(IMAGE_FILENAMES[aliasName]);

  const norm = normalize(key);
  const normalizedName = normalizedNames[norm];
  if (normalizedName && IMAGE_FILENAMES[normalizedName]) return toLocalAssetUrl(IMAGE_FILENAMES[normalizedName]);

  return undefined;
};
