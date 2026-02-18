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
  trackerPack?: TrackerPackSource;
};

export const gameSetupRegistry: GameSetupEntry[] = [
  {
    gameId: "pokemon_firered",
    apGameId: "Pokemon_FireRed_and_LeafGreen",
    displayName: "Pokemon FireRed",
    moduleId: "pokemon_firered_bizhawk",
    romKey: "pokemon_firered",
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
    moduleId: "alttp_bizhawk",
    romKey: "alttp",
    trackerPack: {
      type: "github_release",
      repo: "StripesOO7/alttp-ap-poptracker-pack",
      assetRegex: "\\.zip$"
    }
  },
  {
    gameId: "oot",
    apGameId: "Ocarina of Time",
    displayName: "Ocarina of Time",
    moduleId: "oot_bizhawk",
    romKey: "oot"
  },
  {
    gameId: "oot_soh",
    apGameId: "Ship of Harkinian",
    displayName: "Ship of Harkinian",
    moduleId: "oot_soh"
  }
];
