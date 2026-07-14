export type TrackerPackSource =
  | {
      type: "github_release";
      repo: string;
      assetRegex?: string;
    }
  | {
      type: "direct";
      url: string;
    };

export type GameSetupEntry = {
  gameId: string;
  apGameId: string;
  displayName: string;
  moduleId?: string;
  romKey?: string;
  available?: boolean;
  trackerPack?: TrackerPackSource;
};

export const gameSetupRegistry: GameSetupEntry[] = [
  {
    gameId: "pokemon_firered",
    apGameId: "Pokemon_FireRed_and_LeafGreen",
    displayName: "Pokemon FireRed",
    moduleId: "pokemon_firered_bizhawk",
    romKey: "pokemon_firered",
    available: false,
    trackerPack: {
      type: "github_release",
      repo: "vyneras/pokemon-frlg-tracker",
      assetRegex: "\\.zip$"
    }
  },
  {
    gameId: "pokemon_leafgreen",
    apGameId: "Pokemon_FireRed_and_LeafGreen",
    displayName: "Pokemon LeafGreen",
    moduleId: "pokemon_leafgreen_bizhawk",
    romKey: "pokemon_leafgreen",
    available: false,
    trackerPack: {
      type: "github_release",
      repo: "vyneras/pokemon-frlg-tracker",
      assetRegex: "\\.zip$"
    }
  },
  {
    gameId: "pokemon_emerald",
    apGameId: "Pokemon_Emerald",
    displayName: "Pokemon Emerald",
    moduleId: "pokemon_emerald_bizhawk",
    romKey: "pokemon_emerald",
    available: false,
    trackerPack: {
      type: "github_release",
      repo: "seto10987/Archipelago-Emerald-AP-Tracker",
      assetRegex: "\\.zip$"
    }
  },
  {
    gameId: "alttp",
    apGameId: "A Link to the Past",
    displayName: "A Link to the Past",
    moduleId: "alttp",
    romKey: "alttp",
    trackerPack: {
      type: "github_release",
      repo: "StripesOO7/alttp-ap-poptracker-pack",
      assetRegex: "\\.zip$"
    }
  },
  {
    gameId: "earthbound",
    apGameId: "EarthBound",
    displayName: "EarthBound",
    moduleId: "earthbound",
    romKey: "earthbound",
    trackerPack: {
      type: "github_release",
      repo: "PinkSwitch/earthbound_poptracker",
      assetRegex: "\\.zip$"
    }
  },
  {
    gameId: "ocarina_of_time",
    apGameId: "Ship of Harkinian",
    displayName: "Ocarina of Time - Ship of Harkinian",
    moduleId: "oot_soh"
  },
  {
    gameId: "majoras_mask",
    apGameId: "2 Ship 2 Harkinian",
    displayName: "Majora's Mask - 2Ship2Harkinian",
    moduleId: "twoship"
  },
  {
    gameId: "super_mario_64",
    apGameId: "Super Mario 64",
    displayName: "Super Mario 64",
    moduleId: "sm64ex"
  },
  {
    gameId: "mega_man_battle_network_3",
    apGameId: "MegaMan Battle Network 3",
    displayName: "Mega Man Battle Network 3",
    moduleId: "mega_man_battle_network_3",
    romKey: "mega_man_battle_network_3",
    available: false,
    trackerPack: {
      type: "direct",
      url: "local:MMBN3_AP_Tracker.zip"
    }
  }
];
