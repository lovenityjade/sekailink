# IDEAS.md

## Wayland: "Single Window" via Portal Capture (1-click UX)
- Goal: make SekaiLink feel integrated on KDE Plasma + Wayland without re-writing emulators.
- Approach: launch emulator normally (BizHawk/snes9x-rr), then capture the emulator window via
  `xdg-desktop-portal` + PipeWire and render inside SekaiLink.
- UX target: user sees a single portal prompt, selects the emulator window, clicks Accept.
  - Force "window-only" (no screen option) to reduce steps.
  - Keep capture session open for the whole play session (no reprompt).
  - Store/attempt restore token if supported by portal (may remove prompt on next run).
- Limits:
  - Wayland requires the portal choice; cannot auto-target a specific window.
  - Latency typically +2–4 frames (varies by hardware/driver).
- Outcome:
  - Single-window presentation + SekaiLink overlays (chat/toasts/tracker).
  - Cross-platform fallback: Windows embedding (no prompt), Linux Wayland capture (1-click).

## Background Emulator + Stream (Windows + Linux Wayland)
- Goal: keep emulator running in the background (native input + audio), while SekaiLink shows
  the video stream and overlays in a single app window.
- Core idea:
  - Emulator runs normally and handles input/audio directly.
  - SekaiLink captures the emulator window output and renders it in the Electron UI.
  - SekaiLink overlays chat/toasts/tracker on top of the stream.
- Windows path (no macOS):
  - Capture with DXGI Desktop Duplication (or OS window capture API).
  - Render in Electron via canvas/WebGL.
  - Expect +1–3 frames latency depending on capture/compose pipeline.
  - No portal prompt needed.
- Linux Wayland path:
  - Capture via `xdg-desktop-portal` + PipeWire (KDE Plasma target).
  - User must select the emulator window once per session (portal prompt).
  - Keep session alive to avoid reprompt; attempt restore token when supported.
  - Expect +2–4 frames latency in typical setups.
- Input model:
  - Since emulator owns input, SekaiLink does not inject keys/buttons.
  - Provide hotkeys in SekaiLink to focus/raise emulator if needed.
- Audio model:
  - Emulator audio stays native (no re-routing).
  - Optional: capture audio for in-app meters/visualization.
- Pros:
  - Single-window experience on both Windows and Wayland.
  - Overlays are fully controlled by SekaiLink.
  - No changes to emulator or SNI/Lua scripts required.
- Cons:
  - Latency from capture + render.
  - Wayland portal prompt is unavoidable.
  - Fullscreen/VRR may behave differently vs native emulator window.

## Libretro Cores Inside SekaiLink (Long-term R&D)
- Idea: embed libretro cores directly in SekaiLink for a true single-window experience
  without external emulator windows.
- Motivation: full UI control (overlays, tracker, chat, toasts) and consistent UX.
- Major challenge: Archipelago/SNI expects emulator-side Lua hooks; libretro cores do not
  expose a standard Lua runtime or unified memory API.
- Implication: would require a native SNI/Archipelago bridge per core (or per console),
  plus a custom frontend to load cores, ROMs, BIOS, input, audio, and savestates.
- Likely scope: start with one console + one core + one world as a proof-of-concept.

## Brainstorm: Alternatives to Wayland Capture Prompts
### 1) Shared Memory Video Bridge (BizHawk External Tool)
- Concept: BizHawk writes the framebuffer into shared memory; SekaiLink reads it and renders
  in a canvas.
- Benefits: no Wayland portal prompt, near-zero latency, full overlay control in SekaiLink.
- Feasibility: strong on Windows (BizHawk External Tools are stable), not cross-platform
  unless BizHawk itself is available on Linux.
- Cost: requires a BizHawk C# tool/plugin + a native shared memory reader in Electron.

### 2) Window Docking + Overlays (No Capture)
- Concept: keep emulator as a normal window, but auto-position it next to or under SekaiLink.
  SekaiLink renders chat/toasts/tracker in its own UI or in always-on-top overlay windows.
- Benefits: no capture latency, no portal prompts.
- Risks: Wayland restricts programmatic window control and click-through overlays, behavior
  varies by compositor.
- Result: good for Windows, fragile on Wayland unless a KWin script/extension is allowed.

### 3) Gamescope Wrapper (Linux/Deck)
- Concept: launch SekaiLink and the emulator under Gamescope, using it as a micro-compositor
  to manage presentation and overlays.
- Benefits: Steam Deck-friendly, consistent composition on Linux.
- Risks: Linux-only, requires Gamescope installed, adds another dependency.
