# Client Autolaunch Integration (Families)

This document defines the automation strategy per emulator/client family and records which source
projects are vendored in `third_party/`.

## 1) BizHawk family (GB/GBC/GBA/N64/PS1/etc.)
Automation plan:
- Bundle BizHawk and preconfigure Lua core (Lua+LuaInterface) via config file.
- Launch EmuHawk with ROM/ISO (user-provided) and MWGG Lua connector script.
- Launch MWGG/BizHawk client and auto-connect to room.

Source:
- `third_party/emulators/BizHawk`

## 2) RetroArch family (SMS/Genesis/SNES/etc.)
Automation plan:
- Bundle RetroArch and required cores.
- Preconfigure core paths and network commands (if needed).
- Launch RetroArch with core + ROM; start client auto-connect.

Source:
- `third_party/emulators/RetroArch`

## 3) Dolphin family (GC/Wii)
Automation plan:
- Bundle Dolphin, configure paths.
- Install required mods/patches (if any) to Dolphin load folders.
- Launch Dolphin with game file and start client auto-connect.

Source:
- `third_party/emulators/Dolphin`

## 4) PCSX2 / DuckStation / PPSSPP (PS2/PS1/PSP)
Automation plan:
- Bundle emulator, set BIOS and paths.
- Apply patch to ISO if needed; launch emulator with patched ISO.
- Launch client and auto-connect.

Source:
- `third_party/emulators/PCSX2`
- `third_party/emulators/DuckStation`
- `third_party/emulators/PPSSPP`

## 5) Nintendo Switch (Ryujinx / Yuzu / Eden)
Automation plan:
- Prefer Ryujinx source; if unavailable, use Eden (Yuzu fork).
- Configure keys and firmware paths (user-provided).
- Launch emulator with game file and auto-connect client.

Source:
- `third_party/emulators/Ryujinx`
- `third_party/emulators/Eden`

## 6) Wii U (Cemu)
Automation plan:
- Bundle Cemu, configure paths.
- Launch with game file and start client.

Source:
- `third_party/emulators/Cemu`

## 7) 3DS (Azahar)
Automation plan:
- Bundle Azahar, configure mods/load folders if required.
- Launch with decrypted ROM and start client.

Source:
- `third_party/emulators/Azahar`

## 8) DS (melonDS / DeSmuME)
Automation plan:
- Bundle emulator, configure firmware/bios if required.
- Launch ROM and client.

Source:
- `third_party/emulators/melonDS`
- `third_party/emulators/DeSmuME`

## 9) NES/SNES/GB/GBA (Mesen / Snes9x / bsnes / mGBA / VBA-M)
Automation plan:
- Bundle emulator per game requirement.
- Launch ROM and client; apply patch if required.

Source:
- `third_party/emulators/Mesen`
- `third_party/emulators/Snes9x`
- `third_party/emulators/bsnes`
- `third_party/emulators/mGBA`
- `third_party/emulators/VisualBoyAdvance-M`

## 10) Misc emulators/tools (DOSBox / ScummVM / OpenGOAL / OpenRCT2 / gzDoom)
Automation plan:
- Bundle emulator/tool.
- Install mods/patches in required directories.
- Launch game and connect client as required by the guide.

Source:
- `third_party/emulators/DOSBox-Staging`
- `third_party/emulators/ScummVM`
- `third_party/emulators/OpenGOAL`
- `third_party/emulators/OpenRCT2`
- `third_party/emulators/gzdoom`

## 11) Mod loader family (BepInEx / SMAPI / Fabric / Forge / UnityExplorer / r2modman)
Automation plan:
- Bundle mod loader/tooling.
- Install mods into game directory or profile and write config with server/slot/password if supported.
- Launch via loader and auto-connect.

Source:
- `third_party/mod_loaders/BepInEx`
- `third_party/mod_loaders/SMAPI`
- `third_party/mod_loaders/Fabric-Loader`
- `third_party/mod_loaders/MinecraftForge`
- `third_party/mod_loaders/UnityExplorer`
- `third_party/mod_loaders/r2modmanPlus`

## Unsupported
- Zandronum is not supported and is intentionally excluded.
  Provide an alternate public source or credentials to vendor it.
