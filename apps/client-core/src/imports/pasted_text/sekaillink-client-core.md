  Design a complete desktop application UI for “SekaiLink Client Core”.

  SekaiLink Client Core is the main desktop client for SekaiLink, a launcher and sync hub for randomizer and multiworld games. It is not a landing page. It is a functional app used by players to log in, choose
  games, configure their game seed, join or create a live lobby, chat with other players, launch the emulator, and track game/session status.

  The app should feel like a polished desktop control center for retro game randomizer sessions: readable, fast, technical but friendly, streamer-friendly, and comfortable for long sessions.

  Primary audience:
  - Casual players who want randomizers without installing complex tools.
  - Experienced multiworld players who need fast access to lobbies, configs, players, chat, and launch status.
  - Streamers who need readable UI, minimal visual noise, and clear session state.

  Core concept:
  SekaiLink Client Core connects the user to their account, game library, saved seed configurations, active lobbies, player readiness, chat, notifications, and launch flow. It does not emulate games itself; it
  coordinates with Sekaiemu and backend services.

  Use the word “Sync” for multiworld/session context. A Sync is the live shared generated session. A Seed Config is the user’s saved configuration for a game. A Lobby is the waiting room before generation/
  launch.

  Information architecture / main sections:

  1. Main Shell
  - Persistent sidebar or compact navigation.
  - Sections: Home, Library, Lobbies, Account, Settings.
  - Notification button always visible.
  - User profile/avatar visible but not oversized.
  - Avoid marketing hero layouts. This is a tool.

  2. Home / Dashboard
  Purpose: quick status overview.
  Must include:
  - Current user identity.
  - Active Sync or “No active Sync”.
  - Quick Select game list.
  - Recent lobbies.
  - Recent seed configs.
  - Notifications summary.
  - Clear primary actions: Create Lobby, Join Lobby, Select Game.

  3. Library
  Purpose: manage games and their seed configs.
  For now showcase only “A Link to the Past”.
  Game card should include:
  - Game title.
  - Game description from APWorld.
  - Box art.
  - Status tag: Showcase.
  Actions:
  - Add a Seed Config.
  - Manage Seed Configs.
  - Select Seed Config.
  Do not use old buttons like Play, Continue, Options, Online.
  Do not show technical tags like SNES, Tracker, SNI as primary UI.

  4. Add Seed Config Modal
  Purpose: create a saved configuration for the selected game.
  Two tabs:
  - Easy
  - Advanced

  Easy tab:
  - Friendly guided questions powered by Pulse, the SekaiLink configuration assistant.
  - The user should not see raw YAML.
  - Questions should feel simple, e.g. desired difficulty, run length, shuffled keys/maps, beginner-friendly mode, starting comfort options.
  - Result creates a saved Seed Config.

  Advanced tab:
  - Full APWorld-derived form.
  - Group options exactly like the APWorld groups.
  - Use proper controls: dropdowns for choices, toggles for booleans, sliders/numeric fields for ranges, text areas for lists.
  - Tooltips should be tooltips, not large descriptions inside every card.
  - Remove plando options from normal UI.
  - Forms must be compact, aligned, searchable/filterable, and not overwhelming.

  5. Manage Seed Configs Modal
  Purpose: view/edit/delete saved configs for a game.
  Must include:
  - List of saved configs.
  - Edit opens Advanced form prefilled.
  - Delete requires confirmation.
  - Show source/status: valid, draft, Pulse-created, Advanced-created.
  - Show updated date.

  6. Select Seed Config Modal
  Purpose: choose which saved seed config is active for the next lobby/sync.
  Must include:
  - Search/list of configs.
  - Clear selected state.
  - Add to active seed list.
  - Active seed list should be visually obvious and easy for beginners.

  7. Lobby / Sync Room
  Purpose: pre-generation live room.
  Layout should be compact and practical.
  Must include:
  - Lobby title and short description, not a huge hero.
  - Owner/host indicator.
  - Player list with readiness.
  - Each player’s selected game/config.
  - Chatroom.
  - Launch/generate controls.
  - Room status: waiting, ready, generating, generated, failed.
  - Dashboard panel with users/info.
  Remove outdated controls:
  - Tracker controls.
  - Hint controls.
  - Local Sessions Runtime.
  - Placeholder action names like “Proto.create_room”.

  8. Chat
  Purpose: room communication and system feed.
  Must include:
  - Lobby chat.
  - Human-readable names, not IDs.
  - Item send/receive messages in readable format:
    “Jade (A Link to the Past {1}) sends Boomerang to Raifu (Twilight Princess)”
  - Compact message rows.
  - Timestamp optional/subtle.
  - System messages visually distinct but not loud.

  9. Notifications
  Purpose: user alerts.
  Must include:
  - Notification tray.
  - Badge count.
  - Mark all as read.
  - Success/error/info states.
  - Messages should be human-readable.
  - Avoid raw technical validation messages unless in an expandable detail area.
  Example: instead of “entrance_shuffle_seed: value does not match option type enum”, show “Seed config could not be saved. Entrance Shuffle Seed has an invalid value.” with details expandable.

  10. Settings
  Purpose: configure client behavior.
  Tabs or sections:
  - Easy / Advanced settings mode.
  - Account.
  - Audio/notifications.
  - Chat overlay.
  - Client behavior.
  - Emulator launch integration.
  - Bridge/status advanced section.
  - Sync infos.
  Do not make it decorative; it should be clear and practical.

  Visual direction:
  - Dark interface, teal/cyan accents, subtle sci-fi grid/circuit aesthetic.
  - Avoid excessive glow and clutter.
  - Use high contrast, accessible colors, readable typography.
  - UI should be colorblind-friendly: don’t rely only on green/yellow/red.
  - Use panels with thin borders, compact spacing, and clear hierarchy.
  - Cards should be functional, not decorative.
  - No giant marketing hero sections.
  - No huge empty top banners.
  - Keep important actions visible without overwhelming the user.

  Design constraints:
  - Desktop-first Electron app.
  - Should work around 1280x720 and larger.
  - Avoid layouts that flicker or resize when lists update.
  - Keep chat, player lists, and lobby panels stable.
  - Prioritize performance perception: no heavy animated clutter.
  - All modals should be usable without scrolling forever.
  - Tooltips must not replace layout.
  - Forms should be grouped, searchable, and scannable.

  Deliver screens for:
  - Login/Auth gate.
  - Home dashboard.
  - Library game detail for A Link to the Past.
  - Add Seed Config modal: Easy tab.
  - Add Seed Config modal: Advanced tab.
  - Manage Seed Configs modal.
  - Select Seed Config modal.
  - Lobby room with chat and player list.
  - Notification center.
  - Settings page.

  The final design should look like a real production desktop app for organizing and launching live randomizer syncs, not a website mockup.