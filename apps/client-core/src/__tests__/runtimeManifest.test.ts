import { describe, expect, it } from "vitest";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { gameSetupRegistry } from "../data/gameSetup";
import { deriveRuntimeSetupFlags } from "../services/launchReadiness";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const runtimeRoot = path.resolve(__dirname, "../../../../runtime");
const electronMainPath = path.resolve(__dirname, "../../electron/main.cjs");
const electronBuilderPath = path.resolve(__dirname, "../../electron-builder.yml");

const readRuntimeManifest = (moduleId: string) => {
  const manifestPath = path.join(runtimeRoot, "modules", moduleId, "manifest.json");
  return JSON.parse(fs.readFileSync(manifestPath, "utf8"));
};

describe("runtime manifests", () => {
  it("declares EarthBound as a generic Sekaiemu SNES module", () => {
    const manifest = readRuntimeManifest("earthbound");
    expect(manifest).toMatchObject({
      game_id: "earthbound",
      display_name: "EarthBound",
      emu: "sekaiemu-libretro",
      patcher: "archipelago",
      patch_extensions: [".apeb"],
      patch_world_module: "worlds.earthbound",
      required_roms: ["earthbound"],
      sekaiemu: {
        core_id: "snes",
        profile: "profiles/earthbound-starter.profile",
        ap_game: "EarthBound",
      },
    });
    expect(manifest.sekaiemu.tracker_bundle).toBeUndefined();
  });

  it("ships the AP patch adapter in the active runtime surface", () => {
    expect(fs.existsSync(path.join(runtimeRoot, "patcher_wrapper.py"))).toBe(true);
  });

  it("ships the curated AP/MWGG patcher source bundle inside runtime/ap", () => {
    const apRoot = path.join(runtimeRoot, "ap");
    expect(fs.existsSync(path.join(apRoot, "Patch.py"))).toBe(true);
    expect(fs.existsSync(path.join(apRoot, "BaseClasses.py"))).toBe(true);
    expect(fs.existsSync(path.join(apRoot, "rule_builder", "rules.py"))).toBe(true);
    expect(fs.existsSync(path.join(apRoot, "worlds", "Files.py"))).toBe(true);
    expect(fs.existsSync(path.join(apRoot, "worlds", "alttp", "__init__.py"))).toBe(true);
    expect(fs.existsSync(path.join(apRoot, "worlds", "earthbound", "__init__.py"))).toBe(true);
    expect(fs.existsSync(path.join(apRoot, "sekailink-ap-runtime.json"))).toBe(true);
  });

  it("keeps AP/MWGG bundle packaging anchored to runtime/ap", () => {
    const main = fs.readFileSync(electronMainPath, "utf8");
    const builder = fs.readFileSync(electronBuilderPath, "utf8");
    expect(main).toContain('path.join(process.resourcesPath, "runtime", "ap")');
    expect(main).toContain("SEKAILINK_ALLOW_EXTERNAL_AP_ROOT");
    expect(builder).toContain("runtime/ap");
    expect(builder).not.toContain("from: ../../worlds");
    expect(builder).not.toContain("from: ../../data");
  });

  it("ships the Windows patcher runtime without player-side Python dependencies", () => {
    const portableWinTools = path.join(runtimeRoot, "tools", "python", "portable-win", "tools");
    expect(fs.existsSync(path.join(portableWinTools, "python.exe"))).toBe(true);
    expect(fs.existsSync(path.join(portableWinTools, "python3.dll"))).toBe(true);
    expect(fs.existsSync(path.join(portableWinTools, "python312.dll"))).toBe(true);
    expect(fs.existsSync(path.join(portableWinTools, "vcruntime140.dll"))).toBe(true);
    expect(fs.existsSync(path.join(portableWinTools, "vcruntime140_1.dll"))).toBe(true);

    const wheelhouse = path.join(runtimeRoot, "tools", "python", "wheelhouse", "win-amd64-cp312");
    expect(fs.existsSync(wheelhouse)).toBe(true);
    const wheels = fs.readdirSync(wheelhouse).filter((name) => name.endsWith(".whl"));
    expect(wheels).toEqual(
      expect.arrayContaining([
        expect.stringMatching(/^bsdiff4-.*-win_amd64\.whl$/),
        expect.stringMatching(/^orjson-.*-win_amd64\.whl$/),
        expect.stringMatching(/^pillow-.*-win_amd64\.whl$/),
        expect.stringMatching(/^pyyaml-.*-win_amd64\.whl$/),
        expect.stringMatching(/^pyaes-.*\.whl$/),
      ])
    );
  });

  it("keeps packaged Windows patching on the bundled offline wheelhouse", () => {
    const main = fs.readFileSync(electronMainPath, "utf8");
    expect(main).toContain("PIP_NO_INDEX");
    expect(main).toContain("--no-index");
    expect(main).toContain("python_wheelhouse_missing");
    expect(main).toContain("!app.isPackaged || process.env.SEKAILINK_ALLOW_SYSTEM_PYTHON");
    expect(main).toContain("!app.isPackaged || process.env.SEKAILINK_ALLOW_RUNTIME_DOWNLOAD");
  });

  it("resolves Sekaiemu tracker assets relative to the active Core runtime", () => {
    const main = fs.readFileSync(electronMainPath, "utf8");
    expect(main).toContain("const runtimeDir = getBundledRuntimeDir();");
    expect(main).toContain("const overlayRuntimeDir = getOverlayRuntimeDir();");
    expect(main).toContain("const roots = [\n    overlayRuntimeDir,\n    runtimeDir,");
  });

  it("supports managed client update bundles for Windows and Linux", () => {
    const main = fs.readFileSync(electronMainPath, "utf8");
    const packageJson = JSON.parse(fs.readFileSync(path.resolve(__dirname, "../../package.json"), "utf8"));
    expect(main).toContain('artifactType === "client-bundle"');
    expect(main).toContain("applyClientBundleUpdate");
    expect(main).toContain("install-state.json");
    expect(main).toContain("client_update_bundle_supported");
    expect(packageJson.scripts["electron:pack:update-bundles"]).toBe("node scripts/package-update-bundles.mjs");
  });

  it("keeps EarthBound setup data aligned with the runtime module", () => {
    const entry = gameSetupRegistry.find((game) => game.gameId === "earthbound");
    expect(entry).toMatchObject({
      apGameId: "EarthBound",
      moduleId: "earthbound",
      romKey: "earthbound",
      trackerPack: {
        type: "github_release",
        repo: "PinkSwitch/earthbound_poptracker",
      },
    });
    expect(entry?.moduleId).not.toContain("bizhawk");
  });

  it("derives EarthBound setup gates from manifest data", () => {
    const manifest = readRuntimeManifest("earthbound");
    const missing = deriveRuntimeSetupFlags(manifest, { roms: {}, trackerPacks: {} });
    expect(missing.requiredRomIds).toEqual(["earthbound"]);
    expect(missing.romReady).toBe(false);
    expect(missing.trackerNeeded).toBe(true);

    const ready = deriveRuntimeSetupFlags(manifest, {
      roms: { earthbound: "/games/EarthBound.sfc" },
      trackerPacks: { earthbound: { path: "/packs/earthbound" } },
    });
    expect(ready.ready).toBe(true);
  });
});
