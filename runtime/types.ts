export type PatchMode = "ap_patch" | "external_patcher" | "mod_loader" | "none";

export interface RequiredAsset {
  type: string;
  hint?: string;
}

export interface Step {
  action: string;
  args?: Record<string, unknown>;
}

export interface GameDriverRef {
  family: string;
  core_id?: string;
}

export interface GameRuntimeContract {
  game_id: string;
  display_name: string;
  platform: string;
  driver: GameDriverRef;
  patch_mode?: PatchMode;
  required_assets?: RequiredAsset[];
  launch_steps?: Step[];
  connect_steps?: Step[];
  setup_files?: string[];
  notes?: string;
  hints?: {
    emulators?: string[];
    mod_loaders?: string[];
    connect_examples?: string[];
  };
}

export interface DriverContext {
  game: GameRuntimeContract;
  assetsRoot: string;
  outputRoot: string;
  server?: string;
  slot?: string;
  password?: string;
}
