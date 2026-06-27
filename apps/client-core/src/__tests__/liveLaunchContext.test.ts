import { describe, expect, it } from "vitest";
import {
  buildSelfLaunchContext,
  indexDownloadsBySlot,
  indexPlayersByName,
  isLaunchArtifactEntry,
} from "../services/lobbyLaunchContext";
import {
  buildRoomServerAddress,
  extractRoomIdFromUrl,
  isRoomServerReady,
} from "../services/roomServerContext";

describe("live launch context", () => {
  it("uses the published room host instead of the API host when available", () => {
    expect(
      buildRoomServerAddress("api.sekailink.com:19099", {
        last_port: 38290,
        room_host: "link.sekailink.com",
      }),
    ).toBe("link.sekailink.com:38290");
  });

  it("normalizes API hosts when room_host is not provided", () => {
    expect(
      buildRoomServerAddress("https://api.sekailink.com:19099", {
        last_port: 38291,
      }),
    ).toBe("api.sekailink.com:38291");
  });

  it("treats only positive room ports as ready", () => {
    expect(isRoomServerReady({ last_port: 0, room_port: 38290 })).toBe(false);
    expect(isRoomServerReady({ last_port: 38290, room_port: 38290 })).toBe(true);
  });

  it("extracts room ids from SekaiLink room URLs", () => {
    expect(extractRoomIdFromUrl("/rooms/live-showcase")).toBe("live-showcase");
    expect(extractRoomIdFromUrl("https://link.sekailink.com/rooms/live-showcase")).toBe("live-showcase");
  });

  it("builds the current player's launch entry from slot downloads", () => {
    const downloads = indexDownloadsBySlot(
      [
        { slot: 1, download: "/generation_artifacts/sync/1/player.aplttp" },
        { slot: 2, download: "/generation_artifacts/sync/2/other.aplttp" },
      ],
      (download) => `https://link.sekailink.com${download}`,
    );
    const playersByName = indexPlayersByName([
      { slot: 1, name: "Jade-ALTTP", game: "A Link to the Past" },
      { slot: 2, name: "Second Player", game: "A Link to the Past" },
    ]);

    expect(
      buildSelfLaunchContext({
        roomStatus: {
          players: [{ slot: 1, name: "Jade-ALTTP", game: "A Link to the Past" }],
        },
        playerName: "Jade-ALTTP",
        downloadsBySlot: downloads.single,
        downloadsBySlotMulti: downloads.multi,
        playersByName,
      }),
    ).toEqual({
      playerName: "Jade-ALTTP",
      slotName: "Jade-ALTTP",
      accountName: "Jade-ALTTP",
      slotId: 1,
      downloadUrl: "https://link.sekailink.com/generation_artifacts/sync/1/player.aplttp",
      downloadUrls: ["https://link.sekailink.com/generation_artifacts/sync/1/player.aplttp"],
      apGameName: "A Link to the Past",
      matched: true,
    });
  });

  it("prefers launch entries matched to the selected config over the raw Sync package", () => {
    const downloads = indexDownloadsBySlot(
      [
        { slot: 1, download: "/generation_artifacts/sync/1/wrong-player.aplttp" },
        { slot: 2, download: "/generation_artifacts/sync/2/other-player.aplttp" },
      ],
      (download) => `https://link.sekailink.com${download}`,
    );
    const playersByName = indexPlayersByName([
      { slot: 1, name: "Jade-ALTTP", game: "A Link to the Past" },
      { slot: 2, name: "Jade-ALTTP-Second", game: "A Link to the Past" },
    ]);

    expect(
      buildSelfLaunchContext({
        roomStatus: {
          players: [
            { slot: 1, name: "Jade-ALTTP", game: "A Link to the Past" },
            { slot: 2, name: "Jade-ALTTP-Second", game: "A Link to the Past" },
          ],
          launch_entries: [
            {
              entry_id: "sync-package",
              artifact_extension: ".zip",
              slot: 1,
              slot_name: "Jade-ALTTP",
              download: "/generation_artifacts/sync/full/AP_Complete.zip",
            },
            {
              entry_id: "entry-2",
              config_id: "cfg-alttp",
              slot: 2,
              slot_name: "Jade-ALTTP-Second",
              game: "A Link to the Past",
              download: "/generation_artifacts/sync/entry-2/Jade-ALTTP-Second.aplttp",
            },
          ],
        },
        playerName: "Jade-ALTTP-Second",
        selection: {
          id: "cfg-alttp",
          game: "A Link to the Past",
          player_name: "Jade-ALTTP-Second",
        },
        toUrl: (download) => `https://link.sekailink.com${download}`,
        downloadsBySlot: downloads.single,
        downloadsBySlotMulti: downloads.multi,
        playersByName,
      }),
    ).toEqual({
      playerName: "Jade-ALTTP-Second",
      slotName: "Jade-ALTTP-Second",
      accountName: "Jade-ALTTP-Second",
      slotId: 2,
      downloadUrl: "https://link.sekailink.com/generation_artifacts/sync/entry-2/Jade-ALTTP-Second.aplttp",
      downloadUrls: ["https://link.sekailink.com/generation_artifacts/sync/entry-2/Jade-ALTTP-Second.aplttp"],
      apGameName: "A Link to the Past",
      matched: true,
    });
  });

  it("uses the live compat slot name for runtime launch when it differs from the display username", () => {
    const downloads = indexDownloadsBySlot([], (download) => `https://link.sekailink.com${download}`);
    const playersByName = indexPlayersByName([
      { slot: 1, name: "thelov-alin-5836", game: "A Link to the Past" },
    ]);

    expect(
      buildSelfLaunchContext({
        roomStatus: {
          players: [{ slot: 1, name: "thelov-alin-5836", game: "A Link to the Past" }],
          launch_entries: [
            {
              entry_id: "entry-1",
              config_id: "cfg-alttp",
              username: "thelovenityjade",
              display_player: "thelovenityjade",
              slot: 1,
              slot_name: "thelov-alin-5836",
              compat_player_name: "thelov-alin-5836",
              game: "A Link to the Past",
              artifact_extension: ".aplttp",
              artifact_kind: "patch",
              download: "/generation_artifacts/sync/entry-1/AP_Seed_P1_thelov-alin-5836.aplttp",
            },
          ],
        },
        playerName: "thelovenityjade",
        selection: {
          id: "cfg-alttp",
          game: "A Link to the Past",
          player_name: "thelovenityjade",
        },
        toUrl: (download) => `https://link.sekailink.com${download}`,
        downloadsBySlot: downloads.single,
        downloadsBySlotMulti: downloads.multi,
        playersByName,
      }),
    ).toEqual({
      playerName: "thelov-alin-5836",
      slotName: "thelov-alin-5836",
      accountName: "thelovenityjade",
      slotId: 1,
      downloadUrl: "https://link.sekailink.com/generation_artifacts/sync/entry-1/AP_Seed_P1_thelov-alin-5836.aplttp",
      downloadUrls: ["https://link.sekailink.com/generation_artifacts/sync/entry-1/AP_Seed_P1_thelov-alin-5836.aplttp"],
      apGameName: "A Link to the Past",
      matched: true,
    });
  });

  it("does not use the shared account username as a slot fallback in multi-game rooms", () => {
    const downloads = indexDownloadsBySlot([], (download) => `https://link.sekailink.com${download}`);
    const playersByName = indexPlayersByName([
      { slot: 1, name: "thelov-alin-223f", game: "A Link to the Past" },
      { slot: 2, name: "thelov-donk-3bc7", game: "Donkey Kong Country" },
      { slot: 3, name: "thelov-eart-8ab7", game: "EarthBound" },
    ]);

    expect(
      buildSelfLaunchContext({
        roomStatus: {
          players: [
            { slot: 1, name: "thelov-alin-223f", game: "A Link to the Past" },
            { slot: 2, name: "thelov-donk-3bc7", game: "Donkey Kong Country" },
            { slot: 3, name: "thelov-eart-8ab7", game: "EarthBound" },
          ],
          launch_entries: [
            {
              entry_id: "entry-1",
              username: "thelovenityjade",
              config_id: "cfg-alttp",
              slot: 1,
              slot_name: "thelov-alin-223f",
              game: "A Link to the Past",
              artifact_extension: ".aplttp",
              artifact_kind: "patch",
              download: "/generation_artifacts/sync/1/alttp.aplttp",
            },
            {
              entry_id: "entry-2",
              username: "thelovenityjade",
              config_id: "cfg-dkc",
              slot: 2,
              slot_name: "thelov-donk-3bc7",
              game: "Donkey Kong Country",
              artifact_extension: ".apdkc",
              artifact_kind: "patch",
              download: "/generation_artifacts/sync/2/dkc.apdkc",
            },
            {
              entry_id: "entry-3",
              username: "thelovenityjade",
              config_id: "cfg-earthbound",
              slot: 3,
              slot_name: "thelov-eart-8ab7",
              game: "EarthBound",
              artifact_extension: ".apeb",
              artifact_kind: "patch",
              download: "/generation_artifacts/sync/3/earthbound.apeb",
            },
          ],
        },
        playerName: "thelovenityjade",
        selection: {
          id: "cfg-dkc",
          game: "Donkey Kong Country",
          player_name: "thelovenityjade",
        },
        toUrl: (download) => `https://link.sekailink.com${download}`,
        downloadsBySlot: downloads.single,
        downloadsBySlotMulti: downloads.multi,
        playersByName,
      }),
    ).toEqual({
      playerName: "thelov-donk-3bc7",
      slotName: "thelov-donk-3bc7",
      accountName: "thelovenityjade",
      slotId: 2,
      downloadUrl: "https://link.sekailink.com/generation_artifacts/sync/2/dkc.apdkc",
      downloadUrls: ["https://link.sekailink.com/generation_artifacts/sync/2/dkc.apdkc"],
      apGameName: "Donkey Kong Country",
      matched: true,
    });
  });

  it("refuses ambiguous selected configs instead of falling back to the account username", () => {
    const downloads = indexDownloadsBySlot([], (download) => `https://link.sekailink.com${download}`);
    const playersByName = indexPlayersByName([
      { slot: 1, name: "thelov-alin-1111", game: "A Link to the Past" },
      { slot: 2, name: "thelov-alin-2222", game: "A Link to the Past" },
    ]);

    expect(
      buildSelfLaunchContext({
        roomStatus: {
          players: [
            { slot: 1, name: "thelov-alin-1111", game: "A Link to the Past" },
            { slot: 2, name: "thelov-alin-2222", game: "A Link to the Past" },
          ],
          launch_entries: [
            {
              entry_id: "entry-1",
              username: "thelovenityjade",
              slot: 1,
              slot_name: "thelov-alin-1111",
              game: "A Link to the Past",
              artifact_kind: "patch",
              artifact_extension: ".aplttp",
              download: "/generation_artifacts/sync/1/alttp.aplttp",
            },
            {
              entry_id: "entry-2",
              username: "thelovenityjade",
              slot: 2,
              slot_name: "thelov-alin-2222",
              game: "A Link to the Past",
              artifact_kind: "patch",
              artifact_extension: ".aplttp",
              download: "/generation_artifacts/sync/2/alttp.aplttp",
            },
          ],
        },
        playerName: "thelovenityjade",
        selection: {
          game: "A Link to the Past",
          player_name: "thelovenityjade",
        },
        toUrl: (download) => `https://link.sekailink.com${download}`,
        downloadsBySlot: downloads.single,
        downloadsBySlotMulti: downloads.multi,
        playersByName,
      }),
    ).toMatchObject({
      playerName: "",
      slotName: "",
      accountName: "thelovenityjade",
      matched: false,
      matchError: "ambiguous_launch_entry",
    });
  });

  it("resolves a remote player's display username to their generated AP slot", () => {
    const downloads = indexDownloadsBySlot([], (download) => `https://link.sekailink.com${download}`);
    const playersByName = indexPlayersByName([
      { slot: 1, name: "Certo-alin-54fd", game: "A Link to the Past" },
      { slot: 2, name: "Joueur-alin-bbde", game: "A Link to the Past" },
    ]);

    expect(
      buildSelfLaunchContext({
        roomStatus: {
          players: [
            { slot: 1, name: "Certo-alin-54fd", game: "A Link to the Past" },
            { slot: 2, name: "Joueur-alin-bbde", game: "A Link to the Past" },
          ],
          launch_entries: [
            {
              entry_id: "entry-1",
              user_id: "72",
              username: "Certo",
              config_id: "40",
              display_player: "Certo",
              slot: 1,
              slot_name: "Certo-alin-54fd",
              compat_player_name: "Certo-alin-54fd",
              game: "A Link to the Past",
              artifact_extension: ".aplttp",
              artifact_kind: "patch",
              download: "/generation_artifacts/sync/entry-1/AP_74082915752897734568_P1_Certo-alin-54fd.aplttp",
            },
            {
              entry_id: "entry-2",
              user_id: "74",
              username: "JoueurSansFromage",
              config_id: "42",
              display_player: "JoueurSansFromage",
              slot: 2,
              slot_name: "Joueur-alin-bbde",
              compat_player_name: "Joueur-alin-bbde",
              game: "A Link to the Past",
              artifact_extension: ".aplttp",
              artifact_kind: "patch",
              download: "/generation_artifacts/sync/entry-2/AP_74082915752897734568_P2_Joueur-alin-bbde.aplttp",
            },
          ],
        },
        playerName: "JoueurSansFromage",
        selection: {
          id: "42",
          game: "A Link to the Past",
          player_name: "JoueurSansFromage",
        },
        toUrl: (download) => `https://link.sekailink.com${download}`,
        downloadsBySlot: downloads.single,
        downloadsBySlotMulti: downloads.multi,
        playersByName,
      }),
    ).toEqual({
      playerName: "Joueur-alin-bbde",
      slotName: "Joueur-alin-bbde",
      accountName: "JoueurSansFromage",
      slotId: 2,
      downloadUrl: "https://link.sekailink.com/generation_artifacts/sync/entry-2/AP_74082915752897734568_P2_Joueur-alin-bbde.aplttp",
      downloadUrls: ["https://link.sekailink.com/generation_artifacts/sync/entry-2/AP_74082915752897734568_P2_Joueur-alin-bbde.aplttp"],
      apGameName: "A Link to the Past",
      matched: true,
    });
  });

  it("keeps complete Sync packages out of player launch downloads", () => {
    const downloads = indexDownloadsBySlot(
      [
        {
          slot: 1,
          download: "/generation_artifacts/sync/full/AP_Complete.zip",
          artifact_kind: "sync_package",
          artifact_extension: ".zip",
        },
        {
          slot: 1,
          download: "/generation_artifacts/sync/entry-1/Jade-ALTTP.aplttp",
          artifact_kind: "patch",
          artifact_extension: ".aplttp",
        },
      ],
      (download) => `https://link.sekailink.com${download}`,
    );

    expect(downloads.single.get(1)).toBe("https://link.sekailink.com/generation_artifacts/sync/entry-1/Jade-ALTTP.aplttp");
    expect(downloads.multi.get(1)).toEqual(["https://link.sekailink.com/generation_artifacts/sync/entry-1/Jade-ALTTP.aplttp"]);
  });

  it("ignores a matching Sync zip when choosing the selected player artifact", () => {
    const playersByName = indexPlayersByName([
      { slot: 1, name: "Jade-ALTTP", game: "A Link to the Past" },
    ]);
    const downloads = indexDownloadsBySlot([], (download) => download);

    expect(
      buildSelfLaunchContext({
        roomStatus: {
          players: [{ slot: 1, name: "Jade-ALTTP", game: "A Link to the Past" }],
          launch_entries: [
            {
              entry_id: "sync-package",
              artifact_extension: ".zip",
              artifact_kind: "sync_package",
              slot: 1,
              slot_name: "Jade-ALTTP",
              game: "A Link to the Past",
              download: "/generation_artifacts/sync/full/AP_Complete.zip",
            },
            {
              entry_id: "entry-1",
              artifact_extension: ".aplttp",
              artifact_kind: "patch",
              slot: 1,
              slot_name: "Jade-ALTTP",
              game: "A Link to the Past",
              download: "/generation_artifacts/sync/entry-1/Jade-ALTTP.aplttp",
            },
          ],
        },
        playerName: "Jade-ALTTP",
        selection: {
          game: "A Link to the Past",
          player_name: "Jade-ALTTP",
        },
        toUrl: (download) => `https://link.sekailink.com${download}`,
        downloadsBySlot: downloads.single,
        downloadsBySlotMulti: downloads.multi,
        playersByName,
      }),
    ).toMatchObject({
      slotId: 1,
      downloadUrl: "https://link.sekailink.com/generation_artifacts/sync/entry-1/Jade-ALTTP.aplttp",
      downloadUrls: ["https://link.sekailink.com/generation_artifacts/sync/entry-1/Jade-ALTTP.aplttp"],
      apGameName: "A Link to the Past",
    });
  });

  it("recognizes explicit patch artifacts even when their extension is not known locally", () => {
    expect(
      isLaunchArtifactEntry({
        slot: 1,
        download: "/generation_artifacts/sync/entry-1/player.custom",
        artifact_kind: "patch",
        artifact_extension: ".custom",
      }),
    ).toBe(true);
    expect(
      isLaunchArtifactEntry({
        slot: 1,
        download: "/generation_artifacts/sync/full/AP_Complete.zip",
        artifact_extension: ".zip",
      }),
    ).toBe(false);
  });
});
