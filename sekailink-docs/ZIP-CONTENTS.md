# ZIP-CONTENTS.md

## Archive
- Name: `sekailink-laptop-pack.zip`
- Source: `/home/thelovenityjade/Projects/sekailink`
- Includes `third_party/` and build outputs (including PopTracker build).
- Excludes: `venv/`, `__pycache__/`, `*.pyc`, `.pytest_cache/`, `logs/`, `.git/`.

## What Changed Recently (summary)
- Added PopTracker CLI support for listing pack variants:
  - New flag: `--list-pack-variants <uid|path>`
  - Docs updated in `sekailink-docs/POPTRACKER.md` and `third_party/PopTracker/doc/commandline.txt`
- Added BizHawk connector game list with patch extensions in `sekailink-docs/BIZHAWK-CONNECTORS.md`.
- Added emulator research notes in `sekailink-docs/EMULATOR-RESEARCH.md`.
- Added ideas/brainstorm notes in `sekailink-docs/IDEAS.md`.
- Build: PopTracker compiled on Linux.
  - Binary: `third_party/PopTracker/build/linux-x86_64/poptracker`
- Dolphin build attempt blocked due to missing submodules (no network access to fetch).

## Notable Dependencies Installed On This Machine (not inside zip)
- `mono-complete`
- `cmake`
- `libXi-devel`, `libXrandr-devel`, `libudev-devel` (via `systemd-devel`), `libevdev-devel`

## PopTracker CLI Examples
- List variants from a local pack zip:
  - `third_party/PopTracker/build/linux-x86_64/poptracker --list-pack-variants /home/thelovenityjade/PopTracker/packs/pokemon-frlg-tracker.zip`
- Load pack + variant:
  - `third_party/PopTracker/build/linux-x86_64/poptracker --load-pack <uid> --pack-variant <variant>`

