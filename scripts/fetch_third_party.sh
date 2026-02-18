#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TP_DIR="${ROOT_DIR}/third_party"
export GIT_TERMINAL_PROMPT=0

mkdir -p "${TP_DIR}/emulators" "${TP_DIR}/mod_loaders"
mkdir -p "${TP_DIR}/tools"

clone_or_update() {
  local repo="$1"
  local dest="$2"
  if [ -d "${dest}/.git" ]; then
    echo "Updating ${dest}"
    git -C "${dest}" fetch --all --tags --prune
  else
    echo "Cloning ${repo} -> ${dest}"
    git clone --depth 1 "${repo}" "${dest}"
  fi
}

clone_first_available() {
  local dest="$1"
  shift
  local repo
  for repo in "$@"; do
    if git ls-remote --exit-code "${repo}" >/dev/null 2>&1; then
      clone_or_update "${repo}" "${dest}"
      return 0
    fi
  done
  echo "No available repo for ${dest}" >&2
  return 1
}

EDEN_REPO="https://git.eden-emu.dev/eden-emu/eden.git"
EDEN_USED=0

# Emulators
clone_or_update "https://github.com/TASEmulators/BizHawk.git" "${TP_DIR}/emulators/BizHawk"
clone_or_update "https://github.com/libretro/RetroArch.git" "${TP_DIR}/emulators/RetroArch"
clone_or_update "https://github.com/dolphin-emu/dolphin.git" "${TP_DIR}/emulators/Dolphin"
clone_or_update "https://github.com/PCSX2/pcsx2.git" "${TP_DIR}/emulators/PCSX2"
clone_or_update "https://github.com/stenzek/duckstation.git" "${TP_DIR}/emulators/DuckStation"
clone_or_update "https://github.com/hrydgard/ppsspp.git" "${TP_DIR}/emulators/PPSSPP"
clone_or_update "https://github.com/cemu-project/Cemu.git" "${TP_DIR}/emulators/Cemu"
if clone_first_available "${TP_DIR}/emulators/Ryujinx" \
  "https://github.com/Ryujinx/Ryujinx.git" \
  "https://github.com/Ryubing/Ryujinx.git" \
  "https://github.com/enbyte/ryujinx-mirror-cubed.git"; then
  :
else
  clone_or_update "${EDEN_REPO}" "${TP_DIR}/emulators/Eden"
  EDEN_USED=1
fi

if clone_first_available "${TP_DIR}/emulators/yuzu" \
  "https://github.com/yuzu-emu/yuzu.git"; then
  :
else
  if [ "${EDEN_USED}" -eq 0 ]; then
    clone_or_update "${EDEN_REPO}" "${TP_DIR}/emulators/Eden"
    EDEN_USED=1
  fi
fi
clone_or_update "https://github.com/melonDS-emu/melonDS.git" "${TP_DIR}/emulators/melonDS"
clone_or_update "https://github.com/TASEmulators/desmume.git" "${TP_DIR}/emulators/DeSmuME"
clone_or_update "https://github.com/snes9xgit/snes9x.git" "${TP_DIR}/emulators/Snes9x"
clone_or_update "https://github.com/SourMesen/Mesen.git" "${TP_DIR}/emulators/Mesen"
clone_or_update "https://github.com/mgba-emu/mgba.git" "${TP_DIR}/emulators/mGBA"
clone_or_update "https://github.com/visualboyadvance-m/visualboyadvance-m.git" "${TP_DIR}/emulators/VisualBoyAdvance-M"
clone_or_update "https://github.com/bsnes-emu/bsnes.git" "${TP_DIR}/emulators/bsnes"
clone_or_update "https://github.com/OpenEmu/OpenEmu.git" "${TP_DIR}/emulators/OpenEmu"
clone_or_update "https://github.com/mamedev/mame.git" "${TP_DIR}/emulators/MAME"
clone_or_update "https://github.com/dosbox-staging/dosbox-staging.git" "${TP_DIR}/emulators/DOSBox-Staging"
clone_or_update "https://github.com/scummvm/scummvm.git" "${TP_DIR}/emulators/ScummVM"
clone_or_update "https://github.com/open-goal/jak-project.git" "${TP_DIR}/emulators/OpenGOAL"
clone_or_update "https://github.com/OpenRCT2/OpenRCT2.git" "${TP_DIR}/emulators/OpenRCT2"
clone_or_update "https://github.com/coelckers/gzdoom.git" "${TP_DIR}/emulators/gzdoom"
clone_or_update "https://github.com/azahar-emu/azahar.git" "${TP_DIR}/emulators/Azahar"

# Mod loaders / tooling
clone_or_update "https://github.com/BepInEx/BepInEx.git" "${TP_DIR}/mod_loaders/BepInEx"
clone_or_update "https://github.com/Pathoschild/SMAPI.git" "${TP_DIR}/mod_loaders/SMAPI"
clone_or_update "https://github.com/FabricMC/fabric-loader.git" "${TP_DIR}/mod_loaders/Fabric-Loader"
clone_or_update "https://github.com/MinecraftForge/MinecraftForge.git" "${TP_DIR}/mod_loaders/MinecraftForge"
clone_or_update "https://github.com/sinai-dev/UnityExplorer.git" "${TP_DIR}/mod_loaders/UnityExplorer"
clone_or_update "https://github.com/ebkr/r2modmanPlus.git" "${TP_DIR}/mod_loaders/r2modmanPlus"

# External patchers / utilities
clone_or_update "https://github.com/zsrtp/SaveFileHacker.git" "${TP_DIR}/tools/TP_SaveFileHacker"
clone_or_update "https://github.com/zsrtp/Randomizer.git" "${TP_DIR}/tools/TP_Randomizer"
clone_or_update "https://github.com/zsrtp/Randomizer-Web-Generator.git" "${TP_DIR}/tools/TP_Randomizer_Web_Generator"
clone_or_update "https://github.com/zsrtp/geckopatcher.git" "${TP_DIR}/tools/TP_GeckoPatcher"

echo "Done."
