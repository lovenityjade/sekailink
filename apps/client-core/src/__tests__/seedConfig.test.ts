import { beforeEach, describe, expect, it, vi } from "vitest";
import { createAdvancedSeed, invalidateSeedConfigCaches, listSeedOptions } from "../services/seedConfig";

const jsonResponse = (data: unknown, status = 200) =>
  new Response(JSON.stringify(data), {
    status,
    headers: { "Content-Type": "application/json" },
  });

describe("seed config service", () => {
  beforeEach(() => {
    vi.resetAllMocks();
    window.localStorage.clear();
    invalidateSeedConfigCaches("all");
  });

  it("loads non-ALTTP options from Nexus and saves advanced configs through seed-configs", async () => {
    const fetchMock = vi.mocked(globalThis.fetch as unknown as typeof fetch);
    fetchMock
      .mockResolvedValueOnce(
        jsonResponse({
          ok: true,
          game: {
            game_key: "super_mario_world",
            display_name: "Super Mario World",
            schema_version: "apworld-smw-v1",
            options: [
              {
                option_key: "goal",
                yaml_key: "goal",
                label: "Goal",
                type: "enum",
                default_value: "bowser",
                choices: [
                  { choice_key: "bowser", yaml_value: "bowser", label: "Bowser" },
                  { choice_key: "yoshi_egg_hunt", yaml_value: "yoshi_egg_hunt", label: "Yoshi Egg Hunt" },
                ],
              },
              {
                option_key: "death_link",
                yaml_key: "death_link",
                label: "Death Link",
                type: "boolean",
                default_value: false,
              },
            ],
          },
        })
      )
      .mockResolvedValueOnce(
        jsonResponse({
          ok: true,
          source: "nexus.seed_config_api",
          seed: {
            id: "cfg-smw-1",
            config_id: "cfg-smw-1",
            title: "SMW Advanced",
            game: "Super Mario World",
            game_key: "super_mario_world",
            player_name: "Jade",
            source: "advanced.seed_config_api",
            values: {
              goal: "yoshi_egg_hunt",
              death_link: true,
            },
          },
        })
      );

    const game = { game_key: "super_mario_world", display_name: "Super Mario World" };
    const schema = await listSeedOptions(game);
    expect(schema.game_key).toBe("super_mario_world");
    expect(schema.options.map((option) => option.option_key)).toEqual(["goal", "death_link"]);

    const seed = await createAdvancedSeed(game, "SMW Advanced", "Jade", {
      goal: "yoshi_egg_hunt",
      death_link: true,
    });

    expect(seed).toMatchObject({
      id: "cfg-smw-1",
      title: "SMW Advanced",
      game_key: "super_mario_world",
      source: "advanced.seed_config_api",
      values: {
        goal: "yoshi_egg_hunt",
        death_link: true,
      },
    });

    const postCall = fetchMock.mock.calls.find(
      (call) => String(call[0]).includes("/api/seed-configs") && (call[1] as RequestInit)?.method === "POST"
    );
    expect(postCall).toBeTruthy();
    expect(postCall?.[1]).toMatchObject({ method: "POST" });
    expect(JSON.parse(String((postCall?.[1] as RequestInit).body))).toMatchObject({
      game_key: "super_mario_world",
      title: "SMW Advanced",
      values: {
        goal: "yoshi_egg_hunt",
        death_link: true,
      },
    });
  });
});
