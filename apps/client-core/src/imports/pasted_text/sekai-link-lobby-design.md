
  Design the SekaiLink Client Core Lobby screen.

  SekaiLink is a desktop client for randomizer and multiworld sessions. The Lobby is the waiting
  room where players gather before generating and launching a shared Sync. A Sync is the final
  generated multiworld session. The Lobby is not the game itself; it is the coordination screen
  before and during setup.

  The Lobby must be practical, compact, and stable. It should help users understand:
  - who is in the lobby
  - who owns/hosts the lobby
  - which game/seed config each player selected
  - who is ready
  - whether the lobby is ready to generate
  - current chat/system events
  - generation/launch status

  Main Lobby Areas:

  1. Lobby Header
  Purpose:
  Shows the identity and status of the current lobby.

  Content:
  - Lobby name
  - Short lobby description
  - Owner/host username
  - Created date/time
  - Lobby status: Waiting, Ready, Generating, Generated, Failed
  - Primary action button: Generate Sync / Launch / Ready depending on state

  Design notes:
  The header should be compact. Do not make it a huge hero section. It should feel like a control
  bar or room summary.

  2. Player / Users Panel
  Purpose:
  Shows everyone currently in the lobby.

  Each player row should show:
  - Avatar or user icon
  - Username
  - Host crown/icon if they are the lobby owner
  - Ready state
  - Selected game
  - Selected seed config
  - Connection/presence state if useful

  Important behavior:
  A user should only appear once. If the user is host, show the crown on that same row instead of
  duplicating them.

  Ready state:
  - Not Ready
  - Ready
  - Missing Seed Config
  - Generating
  - Generated
  Use icons and labels, not only color.

  3. Seed Config Selection Per Player
  Purpose:
  Each player must select one or more Seed Configs before generation.

  Context:
  A Seed Config is a saved game configuration created from the Library:
  - Easy mode via Pulse assistant
  - Advanced mode via APWorld-derived settings form

  For the current Pre-BETA3 showcase, most users will select one A Link to the Past config. Later,
  a single user may select multiple configs, including multiple configs for the same game.

  Display per player:
  - Game name, e.g. “A Link to the Past”
  - Seed Config name, e.g. “Test Pulse”
  - Config source/status: Easy, Advanced, Valid, Draft, Error
  - Duplicate game instance number if needed:
    “A Link to the Past {1}”
    “A Link to the Past {2}”

  Design requirement:
  Make selected seeds easy to scan. Use a compact stacked list or chips inside each player row/
  card. Do not hide this in a deep menu.

  4. Active Seed List / Local Selection
  Purpose:
  Shows the current user’s selected Seed Configs before they are attached to the lobby.

  Actions:
  - Add Seed Config
  - Select Seed Config
  - Remove from active list
  - Edit config
  - Mark Ready when valid

  Behavior:
  When the user selects a seed config, it becomes part of their lobby setup.
  The lobby should clearly show whether the local user has a valid selected config.

  5. Lobby Dashboard / Status Panel
  Purpose:
  Shows room readiness and generation state.

  Content:
  - Total players
  - Ready players
  - Players missing configs
  - Selected configs count
  - Generation status
  - Last error if any
  - Sync ID once generated
  - Room/server connection state

  Design notes:
  This should be a concise status panel, not a large statistics dashboard.

  6. Chatroom
  Purpose:
  Allows players to communicate without voice chat and shows system events.

  Chat messages:
  - Human-readable usernames, never raw user IDs.
  - Compact chat rows.
  - Optional subtle timestamp.
  - Distinguish player messages from system messages.

  System/item format:
  Use this readable format:
  “Jade (A Link to the Past {1}) sends Boomerang to Raifu (Twilight Princess)”

  Other system messages:
  - Player joined
  - Player left
  - Player changed seed config
  - Player is ready
  - Generation started
  - Generation completed
  - Generation failed

  Design notes:
  Chat should be readable but not dominate the lobby. It should be stable and should not resize/
  flicker when messages arrive.

  7. Controls / Actions
  Purpose:
  Provides lobby actions appropriate to the user’s role and room state.

  Possible actions:
  For all players:
  - Select Seed Config
  - Ready / Unready
  - Leave Lobby

  For host:
  - Generate Sync
  - Cancel Generation
  - Kick player if supported later
  - Close Lobby if supported later

  Do not include old/removed controls:
  - Tracker controls
  - Hint controls
  - Local Sessions Runtime
  - Plando/custom YAML controls
  - Placeholder names like “Proto.create_room”

  8. Generation Flow
  Purpose:
  Explains what happens when the host clicks Generate Sync.

  Expected backend flow:
  - Each player has selected one or more Seed Configs.
  - Client Core sends the lobby setup to SekaiLink backend.
  - Backend converts database-backed Seed Config values into generator-compatible YAML.
  - All player YAMLs are bundled together.
  - The bundle is sent to the generator service.
  - The generator returns a generated Sync package.
  - The lobby receives the result and becomes ready to launch.
  - Sekaiemu/SKLMI use the generated package for gameplay, tracker, chat, and runtime integration.

  UI states:
  - Waiting for players
  - Waiting for seed configs
  - Ready to generate
  - Generating
  - Generated / Ready to launch
  - Error with human-readable details
