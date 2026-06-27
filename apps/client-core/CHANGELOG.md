# Changelog

## 2026-06-25

- Added a failed-generation recovery action in lobby rooms. When Worlds leaves a
  lobby in a failed generation state, Client Core now shows a `Reset room`
  button that clears only the failed generation record so the same lobby can
  generate again.
- Temporarily marked Donkey Kong Country unavailable in the game catalog because
  the local/runtime ROM manifest accepted a Rev 2 ROM while the APWorld
  generator requires a different USA hash. DKC can be re-enabled once the
  correct base ROM is installed and the manifest hash is aligned.
- Fixed NES/TLoZ autolaunch failing during the window-layout step with
  `autolaunch_failed: sleep is not defined`; the Electron windowing runtime now
  provides its own delay helper before polling `wmctrl`.
- Started the structural Electron main-process refactor. Runtime path helpers,
  process/port helpers, bootstrapper control, diagnostics/bug-report
  collection, Python runtime preparation, download helpers, and BizHawk runtime
  staging/launching now live in focused `electron/lib/*.cjs` modules instead of
  the monolithic `main.cjs`.
- Hardened the generic Archipelago wrapper diagnostics for core/system
  validation. `InvalidSlot` and `InvalidGame` refusals are now emitted as
  structured `connection_refused` events, mirrored into Sekaiemu's readable
  debug/chat output, and stop the wrapper instead of leaving a watcher running
  against an unauthenticated session.
- Added generic Archipelago patch metadata diagnostics to wrapper launches.
  When a patch exposes `seed_name`/`player_name`, the runtime now mirrors the
  room seed and reports a readable room/patch mismatch before core-memory
  debugging goes down the wrong path.

## 2026-06-21

- Hardened the Sekaiemu/SKLMI tracker boundary. Client Core no longer falls
  back to installed raw PopTracker packs when preparing `--tracker-pack` for
  the SKLMI companion; only adapted SekaiLink tracker bundles or explicit
  SKLMI tracker paths are passed into Sekaiemu. This prevents one game's raw
  PopTracker install from breaking its memory bridge startup.
- Fixed the SNES/SKLMI regression introduced while disabling Sekaiemu's
  internal tracker. Client Core now still passes SKLMI-compatible tracker
  metadata and the chosen variant into Sekaiemu for the companion process, while
  Sekaiemu keeps the internal tracker UI disabled. This restores ALttP/SNES
  memory bridge startup instead of failing with `tracker_pack_missing`.
- Corrected the ALttP/SNES companion pack path so SKLMI receives the adapted
  SekaiLink tracker bundle (`alttp-linkedworld-default.zip`) instead of the raw
  PopTracker pack. Sekaiemu now ignores tracker packs for its own internal
  display unless `--tracker-required` is explicitly set.
- Lobby seed selection now prompts for a PopTracker layout variant when the
  selected game's tracker pack exposes variants. The chosen variant is saved
  locally, sent with the lobby selection payload, displayed on the selected
  config badge, and passed through session launch so external PopTracker starts
  connected with the intended layout.
- Disabled Sekaiemu's internal tracker path for normal Client Core launches.
  Client Core now relies on the external PopTracker runtime instead of passing
  tracker packs or legacy tracker bundles into Sekaiemu.
- Enabled Secret of Mana in the redesigned game catalog, staged its SNI runtime
  module with `som.apworld`, and imported its PopTracker pack into runtime
  resources.
- Staged the SMZ3 PopTracker pack and linked the SMZ3 runtime manifest to the
  `dessyreqt/smz3-ap-tracker` package with the `Standard` variant as default.
- Updated the redesigned game-selection catalog to match current runtime
  validation: NES, SNES, GB/GBC except Link's Awakening DX, and GBA entries
  are selectable, while N64 and GameCube/Wii entries remain forced to Coming
  Soon even when Nexus exposes seed schemas for them.
- Marked Final Fantasy V Career Day unavailable/unstable, and documented
  Final Fantasy Tactics Advance, Lufia II, Mega Man 3, and Wario Land as
  supported trackerless games for the current compatibility tier.
- Fixed Link's Awakening DX beta wrapper gating after the generator/APWorld
  update. The client now recognizes the patched Tarin's Gift start-check bit,
  allowing queued `ReceivedItems` to proceed to memory injection instead of
  stalling after the AP server delivers them.

## 2026-06-20

- Attached Archipelago wrapper clients to their `moduleId + slot` runtime
  session. Client Core now stops stale wrapper clients before relaunching the
  same session and stops the matching wrapper when Sekaiemu exits, preventing
  orphan AP clients from receiving items after the emulator window is gone.
- Lowered the default generated Sekaiemu frontend volume to 15% for lab
  launches while still respecting an explicit user-configured volume.
- Fixed Archipelago wrapper compatibility with `websockets 16.0` by adding
  `.closed` and `.open` compatibility properties to the real asyncio connection
  classes. This keeps BizHawk/Sekaiemu APWorld watchers alive for clients such
  as Metroid Zero Mission instead of silently stopping before location checks
  can be reported.

## 2026-06-17

- Extended the Archipelago client wrapper registry to 54 entries by staging the
  downloaded APWorlds for Luigi's Mansion, Paper Mario, Star Fox 64, Tetris
  Attack, Majora's Mask Recompiled, and Secret of Mana into the bundled
  runtime. Client-capable worlds are enabled through their upstream wrapper
  family; generator-only worlds stay marked as generator-only.
- Hardened Archipelago wrapper diagnostics: Client Core now keeps a short
  structured Archipelago client trace and appends it to bug-report logs, while
  wrapper crashes emit a traceback in the normalized JSON `fatal` event.
- Tightened the Windows zero-friction Python contract for wrappers: packaged
  builds must use a prepared bundled Python runtime, and the wheelhouse now
  includes custom-world extras such as `docutils`,
  `dolphin-memory-engine`, and `pyevermizer`.
- Fixed wrapper support for upstream clients that expose `run()` instead of
  `main()`/`launch()`, and pass the slot name through `--name` for module-style
  clients.

## 2026-06-12

- Fixed Windows client-bundle updates failing after download with
  `zip_extract_failed` by moving bundle staging to a short temp path and
  logging the real extraction error instead of hiding PowerShell/.NET failures.
- Disabled Client Core startup self-updates; it now checks for new releases,
  announces that an update is available, and continues the normal boot/login
  flow without downloading or applying the update by itself.
- Moved available-update alerts into the redesigned notification bell with the
  normal toaster feedback and a `Restart & Install` / `Later` confirmation
  modal that launches the bootloader instead of applying from Client Core.
- Added a search field to the redesigned game selection modal so typing filters
  the available and coming-soon game list before choosing a seed/settings game.
- Rebuilt and published the Windows `20260613.2` client update bundle with the
  packaged image assets included in `app.asar` (`public/assets/img` logos,
  banners, and game carousel art) so non-dev installs no longer miss those UI
  images.
- Temporarily disabled A Link to the Past selection in the redesigned game
  carousel so the Windows Client Core can be tested independently while the
  Sekaiemu runtime work remains paused.
- Wired the hidden Sekaiemu Runtime Lab to launch the new no-ROM layout preview
  mode, with windowed/fullscreen startup, tracker display mode selection, and
  optional tracker pack loading for visual debugging without Nexus/SKLMI.

## 2026-06-11

- Polished the native SekaiLink Bootloader UI with a calmer branded SDL2
  update window, friendly progress copy, animated progress fill, clearer error
  surface, and no Windows console window behind the launcher.
- Simplified the native Bootloader preview window into a centered borderless
  rounded-rectangle surface with a native Windows rounded region, so Windows
  chrome and background decoration no longer distract from update progress.
- Removed the animated white progress shimmer from the native Bootloader so the
  update bar reads as one clear progress indicator.
- Removed the always-visible log path from the native Bootloader window and
  reserved error-state action space for the upcoming Report Bug flow.
- Added the first shared SekaiLink bug-report payload contract and wired Client
  Core bug reports with reporter identity, component source, screenshot, logs,
  runtime versions, and system context.
- Wired the native Bootloader `Report Bug` action to submit update failures,
  bootloader logs, platform, channel, build, install path, and error context to
  the same `/api/client/bug-report` endpoint used by Client Core.
- Removed the bootloader log path from visible error text; diagnostic paths are
  now included in the submitted report payload instead.
- Fixed direct-message unread badges so counts are per friend instead of showing
  the global unread total beside every profile.
- Opening a direct-message thread now calls the real social read endpoint and
  clears the unread badge so notifications do not immediately reappear after
  refresh.
- Added a social unread-by-user endpoint for Nexus-backed friend chat badges.
- Fixed Async lobbies appearing in the Active column by recognizing
  `asynchronous`, `is_async`, `room_type`, `mode`, metadata, and async runtime
  state when classifying lobbies.
- Fixed Pulse/Easy seed classification so Pulse-created configs no longer show
  as Advanced after creation or relist, and seed dates no longer fall back to a
  fake "today" value when Nexus does not provide timestamps.
- Pulse Easy Config now returns a generated settings summary before confirm and
  lets the player rename the seed before it is saved.
- Added Pulse character art to the dedicated Pulse chat page.
- Wired the redesigned Settings page to load and save visible Client,
  Sekaiemu, Audio, Social, SKLMI, language, theme, and diagnostics values
  through the Electron runtime config instead of leaving them as local toggles.
- Dashboard lobby cards now keep title, status, host/activity, join button, and
  progress bar aligned with long lobby names.
- Dashboard banners now use the uploaded banner assets with a softer transition
  between slides.
- Fixed Dashboard banner rotation so the outgoing and incoming uploaded banner
  images now crossfade instead of swapping abruptly.
- Smoothed Dashboard banner crossfade by keeping all uploaded banner images
  mounted and transitioning only opacity, removing the one-frame flash when a
  new banner image appears.
- Pulse Easy Config now builds a human-readable seed settings paragraph from
  the generated values, so players can understand the run before saving even
  when the full Pulse response module is unavailable.
- Added a local Pulse chat fallback endpoint for randomizer, Archipelago,
  tracker, YAML, spoiler, and sync questions so the Pulse page no longer reports
  an unconnected response module for basic support prompts.
- Reconnected the redesigned Pulse chat page to the trained Pulse module on
  `pulse.sekailink.com` through the limited public ask endpoint, so client
  questions no longer rely on the stale local module path.
- Login sessions now preserve their server `expires_at` locally and the
  identity service enforces a minimum 30-day session TTL after login.
- Fixed Advanced seed configs disappearing after save by tagging Advanced
  configs in the saved values/source payload, accepting all current seed-list
  payload shapes, forcing a seed refresh after save, and keeping the saved
  config visible if the server list lags.
- Fixed Advanced seed saves rejecting the internal SekaiLink metadata by keeping
  client-only tags out of APWorld option values before Nexus schema validation.
- Hardened Windows Client Core startup diagnostics by forcing the main window to
  maximize before showing, disabling GPU compositing by default on Windows, and
  replacing silent black-screen load failures with a visible diagnostic page.
- Added a redesigned Chat page under Library with IRC-style channels, message
  history, presence/name list, and right-click member actions for profile view
  and local mute.
- Extended the Chat page with `#welcome` MOTD and `#event`, automatic scroll to
  latest messages, self/other message alignment, member actions for add friend,
  block, and remove friend, plus OP/VIP badges from admin/moderator and Patreon
  support metadata.
- Fixed the Chat user list so it reflects only the active channel presence
  list instead of mixing in offline friends; busy and away users remain visible
  when the chat gateway reports them as present in the channel.
- Replaced raw `channel_forbidden` chat failures with a disabled composer notice
  that says "You are not authorized to talk in that channel."
- Added standard text emoticon rendering and an emote picker to redesigned chat;
  classic tokens such as `:)`, `:(`, `:D`, and `<3` now render graphically in
  channel chat, lobby chat, and friend direct messages.
- Added lobby text links for redesigned chat: typing `[` now inserts `[]` with
  the cursor inside and opens a waiting lobby picker; selected active or async
  waiting lobbies are sent as `[Lobby Name]` and render as clickable links that
  open the lobby.
- Imported the future SekaiLink game artwork set into Client Core and rebuilt
  the seed game carousel with uniform uncropped cards; only A Link to the Past
  remains selectable while the other planned games render dimmed as Coming Soon.
- Added maintained French, English, and Japanese translations for the redesigned
  Client Core shell and wired the Settings language selector to the active i18n
  provider, with first-boot system language detection preserved.
- Added a real SekaiLink Light theme, wired the Settings theme selector to the
  renderer theme runtime, and hydrate the saved theme on startup before the
  redesigned UI renders.
- Fixed Light theme readability by converting hardcoded white redesign text to
  dark ink on light surfaces while preserving white text on accent buttons,
  avatars, badges, and sent chat bubbles.
- Polished the redesigned Chat page in Light theme with light side panels,
  readable channel rows, incoming/system message bubbles, member list surfaces,
  and degraded-mode notices.
- Hid the Solo Mode entry from the BETA-3 login screen while keeping the solo
  runtime path available behind the existing internal flag/URL flow for BETA-4.
- Cleaned up the login screen footer by removing the internal API note and
  spacing the forgot-password/register actions into separate pill buttons.

## 2026-06-08

- Replaced the public Electron/PowerShell bootstrapper path with a native C++
  SekaiLink Bootloader using SDL2 for status UI, libcurl for release downloads,
  OpenSSL for SHA-256/HMAC launch tokens, and embedded `miniz` ZIP extraction.
- Native bootloader installs now validate `resources/app.asar`,
  `resources.pak`, and the platform client executable before replacing an
  install, preventing the previous Windows extraction failures from producing a
  broken client folder.
- Native bootloader packaging now builds both Linux and Windows launchers and
  stages the Windows MSYS2 DLL dependency closure beside
  `SekaiLink Bootloader.exe`.
- Windows installer shortcuts now launch
  `SekaiLink Bootloader/SekaiLink Bootloader.exe` instead of the legacy
  `SekaiLink-bootstrapper.cmd`, and the installer removes old duplicate
  desktop/start-menu shortcut names during install/uninstall.
- Client Core now resolves the native bootloader folder for updater relaunches,
  so installed clients can repair/update through the same native entrypoint used
  by the shortcuts.
- Fixed social presence in Client Core so absent, stale, or unknown friend
  presence no longer renders as online; the social service now reports friends
  online only while their profile heartbeat is recent and their visible status is
  not offline.

## 2026-06-06

- Fixed the lobby Launch action so it is disabled while the "Launching Game"
  modal is active, preventing duplicate Windows/Linux launch attempts from
  double-clicks or repeated presses.
- Fixed notification read handling so read notifications are cleared instead
  of staying unread in the notification center.
- Added a direct-message notification action that opens the correct social chat
  conversation and refreshes unread counts after messages are marked read.
- Replaced placeholder Sekaiemu runtime controls with real Client Core settings
  for menu mode, tracker display, chat overlay, notifications, bridge terminal,
  volume, tracker visibility, auto-follow/autotabbing, and per-core preferences.
- Client Core now materializes Sekaiemu runtime settings into
  `frontend-config/sekaiemu.cfg` and `frontend-config/cores.cfg` before launching
  the emulator so Windows and Linux launches receive the same settings.
- Pulse Easy Settings now shows a generated settings summary when a guided
  configuration is complete, making the created seed easier to review before
  confirmation.
- Lobby profile modals now include a profile avatar/fallback row so player
  identity is visible in the client profile surface.
- Added the clean Electron SekaiLink Bootloader with release checking, download
  progress, extraction progress, scrollable changelog, manual launch, and
  auto-launch controls.
- Bootstrapper packaging now ships the Electron bootloader app instead of the
  legacy cmd/PowerShell/sh script bundle as the primary update surface.
- Windows packaging now stages Sekaiemu, SKLMI, LuaJIT, MSYS2 UCRT64 DLLs, SKLMI
  manifests, and a Windows runtime manifest into the client bundle so release
  builds keep the same dependency contract as the validated MSYS2 build.
- Fixed Windows installed shortcuts so they route through the bootstrapper again
  instead of launching `SekaiLink Client.exe` directly, preserving updater state
  and bootstrap launch tokens while avoiding the legacy verbose console flow.
- Fixed client self-update relaunch so Windows and Linux bundle updates restart
  through the bootstrapper when it is available instead of directly launching
  the client executable after files are swapped.
- Fixed the Electron bootloader install step on Windows so a locked previous
  `resources` directory no longer aborts the update; it now tries an atomic
  backup swap and falls back to a fresh side-by-side install with diagnostics.
- Fixed the Electron bootloader extraction cache on Windows so a stale
  `downloads/extract/resources` directory no longer aborts after download; each
  update now uses a unique extraction folder and cleans old folders best-effort.
- Fixed Electron bootloader launch validation so stale side-by-side installs are
  ignored unless they contain a complete Electron bundle, and the fallback
  Windows install path now uses the short `sekailink-client-next` directory.
- Fixed Windows bootloader extraction of large Electron bundles by shipping and
  using `extract-zip` as the primary extractor, with native OS extractors kept
  only as fallbacks.
- Changed the Electron bootloader on Windows to delegate the update/install
  phase to the proven legacy PowerShell bootstrapper while keeping the Electron
  UI, logging, diagnostics, and launch flow around it.
- The Electron bootloader now ships a bundled Windows `7za.exe` extractor and
  passes it to the PowerShell bootstrapper, so large client bundles extract
  consistently without requiring user-installed tools.
- Fixed Windows bootloader installs launched from the install directory by
  preserving `SekaiLink Bootloader.exe` while replacing client files.
- Fixed Windows PowerShell parameter binding in the Electron bootloader by
  passing bootstrap settings through environment variables and invoking the
  script with only the minimal launch flags.
- Added a visible Start Menu uninstaller shortcut.
