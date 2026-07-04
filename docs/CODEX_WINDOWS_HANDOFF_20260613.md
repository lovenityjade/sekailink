# Codex Windows Handoff - 2026-06-13

This document is the compact working context for moving SekaiLink development to
the Windows Box while reducing SSH round trips.

## Current Priority

Stabilize the Windows client and continue Sekaiemu ImGui integration. Do not
switch back to CDN/GitHub publishing unless explicitly requested.

## Machine

- Windows host: `10.0.0.119`
- SSH config alias: `sekailink-windows`
- Windows user: `sekailink`
- Main workbench: `D:\SekaiLink`
- Canonical repo: `D:\SekaiLink\repos\sekailink-canonical`
- MSYS2 shell: `C:\msys64\usr\bin\bash.exe`
- Preferred build environment: MSYS2 UCRT64

Recommended PATH inside MSYS2 commands:

```sh
export PATH=/ucrt64/bin:/usr/bin:$PATH
```

## Big Rule

Avoid many tiny SSH transactions. Prefer:

- One source sync from Linux to Windows.
- Then build and test directly from Windows/MSYS2.
- Use local Windows shortcuts for visual testing.
- Keep handoff notes in this file when context changes.

The MSYS2 worker entry point is:

```sh
cd /d/SekaiLink/repos/sekailink-canonical
./tools/windows-worker/sekai-worker-msys.sh status
```

Common local commands:

```sh
./tools/windows-worker/sekai-worker-msys.sh doctor
./tools/windows-worker/sekai-worker-msys.sh build sekaiemu Debug
./tools/windows-worker/sekai-worker-msys.sh test sekaiemu Debug
./tools/windows-worker/sekai-worker-msys.sh build client
./tools/windows-worker/sekai-worker-msys.sh package client
./tools/windows-worker/sekai-worker-msys.sh lab sekaiemu-preview
```

If Linux has to trigger a command remotely, do it as one coarse operation:

```sh
ssh sekailink-windows 'C:/msys64/usr/bin/bash.exe -lc "cd /d/SekaiLink/repos/sekailink-canonical && ./tools/windows-worker/sekai-worker-msys.sh test sekaiemu Debug"'
```

## Bootloader State

The native C++ bootloader is currently considered stable by user validation.
It performs updates correctly. Do not replace it with the Electron/PowerShell
bootloader.

Client Core must not self-update directly. It may show update notifications,
but installation should route through the bootloader.

## Client Core State

The Windows lab Client Core is expected at:

```txt
D:\SekaiLink\repos\sekailink-canonical\apps\client-core\release\win-unpacked
```

The desktop lab shortcut should launch the packaged Client Core from there.

Known client work from the recent session includes:

- redesigned BETA-3 UI work
- chat page and IRC-like channel UI
- notification fixes
- update notification modal flow
- light theme work
- multilingual UI work
- game carousel assets
- settings panel expansion
- Sekaiemu Runtime Lab page

Do not confuse this with the old BETA-2 UI or legacy Electron launcher flow.

## Sekaiemu ImGui State

Sekaiemu is actively migrating from SDL overlay UI toward Dear ImGui.

Relevant source files:

```txt
apps/sekaiemu/src/imgui_runtime.hpp
apps/sekaiemu/src/imgui_runtime.cpp
apps/sekaiemu/src/imgui_tracker_renderer.hpp
apps/sekaiemu/src/imgui_tracker_renderer.cpp
apps/sekaiemu/src/layout_preview.hpp
apps/sekaiemu/src/layout_preview.cpp
apps/sekaiemu/src/runtime_menu_imgui.cpp
```

Current validated features:

- Dear ImGui SDL2/OpenGL3 runtime wrapper
- SekaiLink-styled ImGui runtime menu
- `--layout-preview` mode that opens without ROM/core
- offline no-cartridge layout preview
- ALTTP pack-driven tracker preview rendered into an ImGui texture
- split/PIP/toggle layout preview shortcuts

Validated on Linux:

```txt
cmake -S apps/sekaiemu -B apps/sekaiemu/build-codex-imgui -DCMAKE_BUILD_TYPE=Debug
cmake --build apps/sekaiemu/build-codex-imgui -j4
ctest --test-dir apps/sekaiemu/build-codex-imgui --output-on-failure
```

Result: `13/13` tests passed.

Validated on Windows/MSYS2:

```sh
export PATH=/ucrt64/bin:/usr/bin:$PATH
cd /d/SekaiLink/repos/sekailink-canonical
cmake -S apps/sekaiemu -B apps/sekaiemu/build-win-codex-imgui -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build apps/sekaiemu/build-win-codex-imgui -j4
ctest --test-dir apps/sekaiemu/build-win-codex-imgui --output-on-failure
```

Result: `11/11` tests passed.

Latest validated Windows binary was copied to:

```txt
D:\SekaiLink\repos\sekailink-canonical\runtime\platforms\win32-x64\bin\sekaiemu_libretro_spike.exe
D:\SekaiLink\repos\sekailink-canonical\apps\client-core\release\win-unpacked\resources\runtime\platforms\win32-x64\bin\sekaiemu_libretro_spike.exe
```

Visual test shortcut:

```txt
C:\Users\sekailink\Desktop\Sekaiemu-Layout-Preview-LAB.cmd
```

It launches:

```txt
sekaiemu_libretro_spike.exe --layout-preview --windowed --tracker-pack ...\runtime\tracker-bundles\alttp-linkedworld-default --tracker-variant split-screen
```

## Sekaiemu Next Work

Next logical Sekaiemu tasks:

1. Visually review the ImGui layout preview on Windows.
2. Adjust the ImGui tracker/menu layout based on what appears on screen.
3. Continue replacing SDL overlay tracker/menu surfaces with native ImGui.
4. Keep `apps/sekaiemu/CHANGELOG.md` updated for every fix.

## Do Not Forget

- Do not push to GitHub or CDN unless explicitly requested.
- Do not break SNI logic while working on generic BizHawk/Lua/SKLMI paths.
- SKLMI changes require extra care and explanation before deeper edits.
- Windows users are the main release target.
- Portable-console layouts matter: SNES, NES, GB/GBC, GBA, N64 dimensions must
  eventually drive layout profiles rather than assuming ALTTP/SNES size.
