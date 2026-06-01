export type LaunchRecoveryAction = {
  label: string;
  route?: string;
};

export type LaunchFailureData = {
  error?: string;
  detail?: string;
  setupArea?: string;
};

export const getLaunchRecoveryAction = (launchData: LaunchFailureData | null | undefined): LaunchRecoveryAction | null => {
  const err = String(launchData?.error || "").trim();
  const setupArea = String(launchData?.setupArea || "")
    .trim()
    .toLowerCase();

  if (err === "rom_missing" || setupArea === "roms") {
    return { label: "Open ROM Library", route: "/settings?tab=roms" };
  }

  if (
    err === "mono_missing" ||
    err === "soh_not_found" ||
    err === "soh_install_failed" ||
    err === "sekaiemu_not_found" ||
    err === "sekaiemu_core_missing" ||
    err === "sm64ex_not_found" ||
    err === "iwad_missing" ||
    err === "gzap_pk3_missing" ||
    err === "native_runtime_pending" ||
    err === "native_core_not_yet_integrated" ||
    err === "native_handler_not_yet_integrated" ||
    setupArea === "paths"
  ) {
    return { label: "Open Games Setup", route: "/settings?tab=paths" };
  }

  return null;
};

export const getLaunchErrorMessage = (launchData: LaunchFailureData | null | undefined): string => {
  const err = String(launchData?.error || "Launch failed.");
  let errorMsg = err;

  if (err === "rom_missing") {
    errorMsg = "Game setup incomplete: the base game file is missing. Open Game Setup and add your game first.";
  } else if (err === "poptracker_missing") {
    errorMsg = "Game setup incomplete: the tracker runtime is missing. Open Game Setup and finish the tracker setup first.";
  } else if (err === "no_pack") {
    errorMsg = "Game setup incomplete: no tracker pack is configured for this game yet.";
  } else if (err === "pack_missing") {
    errorMsg = "Game setup incomplete: the tracker pack for this game is missing or no longer installed.";
  } else if (err === "unsupported_patch_type") {
    errorMsg = "This patch format is not supported by the current SekaiLink launch flow yet.";
  } else if (err === "missing_patch_url") {
    errorMsg = "No patch file was needed for this game, but SekaiLink could not find a compatible launch route yet.";
  } else if (err === "native_runtime_pending") {
    errorMsg = "This game is recognized, but its SekaiLink runtime is not fully integrated yet.";
  } else if (err === "launch_planner_unavailable") {
    errorMsg = "This build is missing the SekaiLink launch planner integration.";
  } else if (err === "unknown_bizhawk_handler") {
    errorMsg = "SekaiLink could not match this game to a supported runtime bridge.";
  } else if (err === "native_core_not_yet_integrated") {
    errorMsg = "This game still depends on an emulator core that is not integrated into SekaiLink yet.";
  } else if (err === "native_handler_not_yet_integrated") {
    errorMsg = "This game still depends on runtime bridge logic that is not integrated into SekaiLink yet.";
  } else if (err === "soh_not_found") {
    errorMsg = "Ship of Harkinian was not found. Set its executable path in Settings -> Games.";
  } else if (err === "soh_install_failed") {
    errorMsg = "SekaiLink could not install Ship of Harkinian automatically.";
  } else if (err === "sekaiemu_not_found") {
    errorMsg = "Sekaiemu was not found. Set the Sekaiemu executable path in Settings -> Games or ship it with the runtime bundle.";
  } else if (err === "sekaiemu_core_missing") {
    errorMsg = "Sekaiemu is configured, but the required libretro core for this game was not found.";
  } else if (err === "sklmi_bridge_pending") {
    errorMsg = "Sekaiemu launched, but the SKLMI bridge for this game is not wired into Core yet.";
  } else if (err === "sm64ex_not_found") {
    errorMsg = "SM64EX is not configured yet. Set an executable path or root directory in Settings -> Games.";
  } else if (err === "iwad_missing") {
    errorMsg = "gzDoom still needs an IWAD path before this game can launch.";
  } else if (err === "gzap_pk3_missing") {
    errorMsg = "gzDoom still needs a gzArchipelago.pk3 path before this game can launch.";
  } else if (err === "tracker_variant_cancelled") {
    errorMsg = "Launch cancelled while choosing the tracker layout.";
  } else if (err === "patch_failed") {
    errorMsg = "SekaiLink could not prepare the game image for launch.";
  } else if (err === "download_failed") {
    errorMsg = "SekaiLink could not download your generated game package from the room server.";
  } else if (err === "emu_failed" || err === "sekaiemu_launch_failed") {
    errorMsg = "Sekaiemu could not be started for this launch.";
  } else if (err === "connect_failed") {
    errorMsg = "The game was prepared, but SekaiLink could not connect it to the room server.";
  } else if (err === "room_server_not_ready") {
    errorMsg = "The room server is not ready yet. Try again in a moment.";
  }

  const detail = typeof launchData?.detail === "string" ? launchData.detail.trim() : "";
  if (detail) {
    const httpMatch = detail.match(/download_http_(\d+)/i);
    if (httpMatch) {
      errorMsg = `${errorMsg}\n\nDetails: The download endpoint returned HTTP ${httpMatch[1]}.`;
    } else {
      errorMsg = `${errorMsg}\n\nDetails: ${detail}`;
    }
  }
  return errorMsg;
};
