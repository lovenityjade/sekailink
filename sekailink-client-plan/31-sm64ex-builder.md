# SM64EX (PC port) build automation plan

Context: the Archipelago world for "Super Mario 64" in this repo is `worlds/sm64ex/` and the webhost serves a slot file `*.apsm64ex` (`WebHostLib/downloads.py`). The game binary consumes this file via `--sm64ap_file <path>` (offline) or connects directly with `--sm64ap_name/--sm64ap_ip/--sm64ap_passwd` (online).

## Key constraints (non-negotiable)
- Legal: SekaiLink cannot ship a prebuilt SM64EX binary that contains ROM-derived assets. Users must provide a legally-dumped ROM and build locally.
- Compatibility: Archipelago’s setup docs explicitly point to `N00byKing/sm64ex` on the `archipelago` branch (not `sm64pc/sm64ex`). Using the wrong repo risks protocol/data incompatibility.

## What the upstream launcher does
The recommended upstream path for casual players is the SM64AP-Launcher:
- It guides requirements, downloads the sources, and compiles the Archipelago build of sm64ex.
- It expects the repo `https://github.com/N00byKing/sm64ex` and branch `archipelago`.
- On Linux it requires build deps (SDL2/GLEW/CMake/Python/Make/etc). On Windows it leverages MSYS2 for the toolchain.

Implication: SekaiLink should either:
- Embed/drive SM64AP-Launcher as the “official builder” (fastest to ship), or
- Reimplement the same build pipeline headlessly (best UX long-term), while still matching repo+branch.

## SekaiLink target UX
Goal: casual player presses `Play` and never touches a terminal.

Proposed “installer” flow when `.apsm64ex` is downloaded:
1. If `config.games.sm64ex.exe_path` is set and exists: launch immediately.
2. Else if a build output already exists in SekaiLink cache: link it and launch.
3. Else: run an in-app builder wizard:
   - Pick ROM file (US/JP) once; SekaiLink caches the selection.
   - Pick region (or detect via ROM hash if we add hashing).
   - One-click “Build SM64EX”.
   - Show progress + logs; on success, store `exe_path` automatically.

## Build pipeline (headless) - recommended long-term
Cache root (per-user):
- `userData/runtime/tools/sm64ex/src/` (git checkout)
- `userData/runtime/tools/sm64ex/builds/<region>/<git_rev>/` (compiled output)

Steps:
1. Fetch sources (no GUI):
   - Prefer `git` if available: `git clone --recursive` then checkout `archipelago` branch.
   - Fallback: download a source archive zip (if we choose to support it) and vendor submodules another way.
2. Stage ROM:
   - Copy user ROM into repo root as `baserom.us.z64` or `baserom.jp.z64`.
3. Compile:
   - Run `make -jN` with sane defaults.
   - Capture stdout/stderr into session logs for UI (“Build Console”).
4. Locate output:
   - Linux: `build/<region>_pc/sm64.<region>.f3dex2e`
   - Windows: `build/<region>_pc/sm64.<region>.f3dex2e.exe`
5. Register output:
   - Save `config.games.sm64ex.exe_path`.
   - Optionally persist a “build fingerprint” (git rev + region + makeflags).

Dependencies:
- Linux: `sdl2`, `glew`, `cmake`, `python`, `make`, plus `patch` and `git` for tooling.
- Windows: MSYS2 MinGW toolchain (gcc, SDL2, glew, cmake, git, make, python).

## MVP (what we ship first)
For the MVP, SekaiLink does not compile SM64EX itself:
- SekaiLink downloads the `.apsm64ex` slot file and launches a user-provided SM64EX build via `config.games.sm64ex.exe_path` (or a best-effort search under `root_dir`).
- This keeps the runtime simple and avoids bundling compilers/toolchains inside the AppImage.

## Short-term automation (optional, still MVP-friendly)
If we need to reduce friction without reimplementing the build pipeline:
- Add a “Install/Run SM64AP-Launcher” helper that downloads the launcher zip, runs it, and then auto-detects the resulting SM64EX binary to set `exe_path`.
This still respects the legal constraints (ROM stays user-provided) and keeps Windows support practical.

## Long-term (Option 2): SekaiLink headless builder
This is the preferred end-state and should be tracked as a planned feature rather than required for the MVP.

## Next implementation work items
- Add a dedicated `.apsm64ex` handler that can build-on-missing instead of only showing an error note.
- Extend `Settings -> Games (Automation)` with:
  - ROM path + region (cached)
  - build options (threads, optional makeflags)
  - “Build/Update” and “Clean” actions
- Add a “Build Logs” panel (reuses existing session log plumbing).
