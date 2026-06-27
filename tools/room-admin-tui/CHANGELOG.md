# Room Admin TUI Changelog

## 2026-06-20

- Added the first local SekaiLink room/admin TUI.
- Added lobby listing, lobby selection, generation lookup, room status resolution, room server admin commands, event watch, client report listing, AP check/message debug helpers, and local JSONL export.
- Installed local command aliases: `sekailink-room-admin` and `skl-room`.
- Added Nexus login from the TUI, player listing, AP datapackage item listing, `give <slotname> <item name...>`, fuzzy item-name suggestions, command history, and Tab completion for commands, selected-room players, lobbies, and AP item names.
- Updated the temporary AP client used by `give`/`ap-say`/`ap-check` to report
  protocol version `0.6.7` instead of `0.6.0`, and made `give` warn that AP
  join/part messages can come from the short-lived TUI connection.
- Added a persistent Archipelago monitor connection after `select`, using
  tracker/admin mode with `items_handling=0` so the TUI can observe packets and
  send admin chat commands without consuming the real player's received items.
- Added `ap-log`, `ap-connect`, and `ap-disconnect`, and changed `give` to use
  the persistent monitor connection instead of a short-lived AP connection.
- Corrected `give` to use the Archipelago server admin command
  `/send <slotname> <item>` instead of the player/self command `!getitem`.
- Corrected the AP admin path again: server commands sent from an AP client must
  go through `!admin login <password>` and then `!admin /send ...`; plain
  `/send` inside a `Say` packet is only chat. Added `SEKAILINK_AP_ADMIN_PASSWORD`
  and `--ap-admin-password`.
- Added room admin secret retrieval support: the TUI can now load the generated
  AP remote-admin password through `/api/room_admin_secrets/<room_id>` when
  `SEKAILINK_ROOM_ADMIN_TOOL_TOKEN` is configured, then use it for `give`.
