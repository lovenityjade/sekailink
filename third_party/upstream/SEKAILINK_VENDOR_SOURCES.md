# SekaiLink Upstream Source Staging

This directory stores upstream source snapshots that SekaiLink may integrate or
wrap later. These copies are not part of the active runtime yet.

Fetched on: 2026-06-12

## Dolphin Emulator

- Upstream: https://github.com/dolphin-emu/dolphin
- Official downloads: https://dolphin-emu.org/download/
- Local source: `third_party/upstream/dolphin/dolphin-2603a/`
- Local archive: `third_party/upstream/_archives/dolphin-2603a.tar.gz`
- Version: `2603a`
- Git tag ref: `refs/tags/2603a`
- Tag object SHA: `dd41b1e4865e60785700929254b744ea67184c5b`
- Commit SHA from GitHub tag listing: `5e7cc91d8c9a43ca189b288937f65c9763af9c22`
- Archive SHA256:
  `ae6ca2e812357ae56a31c00498a984e42b5c46946050ae4a946c7e3f63d1ec7b`
- License summary: Dolphin's repository states that most original source is
  GPLv2+, with per-file SPDX licensing and aggregate GPLv3 compatibility.
- SekaiLink purpose: future GameCube/Wii provider, wrapper, or fork base for a
  Sekaiemu-Dolphin layer.
- Integration status: source staged only. No binaries are bundled from this
  snapshot and no runtime code currently depends on it.
- Build note: the GitHub source archive is useful for source review, but a
  production build should use a git checkout and initialize Dolphin submodules
  with `git submodule update --init --recursive`.

## Dolphin Memory Engine

- Upstream: https://github.com/aldelaro5/dolphin-memory-engine
- Release: https://github.com/aldelaro5/dolphin-memory-engine/releases/tag/2026.06.04
- Local source:
  `third_party/upstream/dolphin-memory-engine/dolphin-memory-engine-2026.06.04/`
- Local archive:
  `third_party/upstream/_archives/dolphin-memory-engine-2026.06.04.tar.gz`
- Version: `2026.06.04`
- Release commit shown by GitHub: `8fd1563`
- Archive SHA256:
  `0d429a53b006f22a52fea36ccb19e7bc3a82efb601e3e6cbf50d903f56809b85`
- License summary: MIT.
- SekaiLink purpose: reference implementation for detecting Dolphin processes
  and reading/writing emulated GameCube/Wii memory from an external tool.
- Integration status: source staged only. No SKLMI or Sekaiemu provider has been
  changed by this staging step.

## Shipwright Harkipellago Runtime

- Upstream runtime fork: https://github.com/Symon799/Shipwright_archipellago
- Historical/original fork reference:
  https://github.com/jeromkiller/Shipwright_archipellago
- Important distinction: this is the Ship of Harkinian Archipelago-capable
  runtime fork, not the APWorld repository.
- Local source:
  `third_party/upstream/shipwright-harkipellago/shipwright-harkipellago-SoH_Archipelago_MapTracker_1.1.0/`
- Recursive git checkout:
  `third_party/upstream/shipwright-harkipellago/shipwright-harkipellago-SoH_Archipelago_MapTracker_1.1.0-git/`
- Local archive:
  `third_party/upstream/_archives/shipwright-harkipellago-SoH_Archipelago_MapTracker_1.1.0.tar.gz`
- Version: `SoH_Archipelago_MapTracker_1.1.0`
- Git tag: `SoH_Archipelago_MapTracker_1.1.0`
- Tag commit SHA: `3ae6b44a1ca8629ae7c9e829882211a2f2e7b634`
- Related branch: `Harkipellago`
- Archive SHA256:
  `f61c9726eb068334ad1080f9117e4225eca2248ce1993f205e39ac5b74bc8f82`
- License summary: not normalized in the GitHub repository metadata. Before
  distribution, review the upstream project, bundled Shipwright/HarbourMasters
  licenses, and submodule licenses.
- SekaiLink purpose: future installed/runtime provider for Ship of Harkinian
  Archipelago sessions.
- Integration status: source staged only. No launcher, settings panel, or
  runtime path has been changed by this staging step.
- Build note: the archive is suitable for source review. The `-git` directory is
  the build-oriented checkout with submodules initialized.
- Submodule commits staged in the `-git` checkout:
  - `OTRExporter`: `32e088e28c8cdd055d4bb8f3f219d33ad37963f3`
  - `ZAPDTR`: `ee3397a365c5f350a60538c88f0643f155944836`
  - `libultraship`: `fdcaf6336776d24a6408d016b0a52243f108f250`
  - `subprojects/apclientpp`: `a5b7b96b6bfbdc7f2831796a4e126bcc09c35da1`
  - `subprojects/wswrap`: `47438193ec50427ee28aadf294ba57baefd9f3f1`
  - `subprojects/wswrap/subprojects/wsjs`:
    `69241b520adbe61924b08da900a262e76f17b015`

## SM64EX Archipelago

- Upstream: https://github.com/N00byKing/sm64ex
- Local source:
  `third_party/upstream/sm64ex/sm64ex-archipelago-9289288/`
- Recursive git checkout:
  `third_party/upstream/sm64ex/sm64ex-archipelago-9289288-git/`
- Local archive:
  `third_party/upstream/_archives/sm64ex-archipelago-9289288.tar.gz`
- Version basis: `archipelago` branch snapshot
- Commit SHA: `9289288f241cd03c3287306c715eca0755333075`
- Archive SHA256:
  `fb3af086205902e3b85606e18a73484c290b0655eb34676f949e9461778ea7a3`
- License summary: not normalized in the GitHub repository metadata. Before
  distribution, review upstream sm64ex licensing, enhancement patch licensing,
  and submodule licensing.
- SekaiLink purpose: future installed/runtime provider for `.apsm64ex` slot
  files and Super Mario 64 Archipelago sessions.
- Integration status: source staged only. No launcher, settings panel, or
  runtime path has been changed by this staging step.
- Build note: the archive is suitable for source review. The `-git` directory is
  the build-oriented checkout with submodules initialized.
- Submodule commits staged in the `-git` checkout:
  - `lib/APCpp`: `fdd30018a7a68dcdf4639fc4a3a410dd095c4fa4`
  - `lib/APCpp/IXWebSocket`: `150e3d83b5f6a2a47f456b79330a9afe87cd379c`
  - `lib/APCpp/jsoncpp`: `89e2973c754a9c02a49974d839779b151e95afd6`
  - `lib/APCpp/zlib`: `5a82f71ed1dfc0bec044d9702463dbdf84ea3b71`
