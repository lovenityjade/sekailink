# SekaiLink Porting Research

This document classifies games by the runtime bridge they need, based on local
APWorld setup guides, local client code, and public Archipelago setup pages. It
does not authorize SKLMI changes by itself. Any SKLMI runtime change still needs
explicit approval with risks and benefits.

## Runtime Families

| Family | Meaning | SekaiLink impact |
|---|---|---|
| `sekailink_certified_sni` | Already proven through the current ALTTP Sekaiemu/SKLMI path. | Lowest risk. Use as the reference implementation. |
| `sni_client` | APWorld client subclasses `SNIClient` and setup uses SNI plus `SNI/lua/Connector.lua` for Lua-capable emulators. | Best next SNES path. Requires per-game memory/check/item mapping but matches the current SNES architecture. |
| `sni_client_rom_access` | SNI path, but setup requires ROM access or specific emulator cores. | Medium-high risk. Core choice matters; RetroArch can be blocked for some games. |
| `bizhawk_generic_lua` | APWorld uses Archipelago BizHawk Client plus `data/lua/connector_bizhawk_generic.lua`. | Medium-high risk. SekaiLink already has partial BizHawk/SKLMI support, so the remaining work is bridge completion, certification, and per-game memory-domain parity rather than starting from zero. |
| `custom_apworld_or_patcher` | External APWorld, custom patcher, mod, or Discord-only resource needed. | High risk until the install source and patch flow are normalized. |
| `tracker_optional` | Tracker absence is not a first certification blocker. | Runtime proof can focus on boot, room sync, item receive, and check send. |
| `web_tracker_only` | A web tracker exists, but no installable PopTracker pack is confirmed. | Not a runtime blocker, but dashboard/trailer automation is weaker. |

## Priority Reading

The fastest path to more games is not "one manifest per title by hand". The
fastest path is to implement connector-family adapters:

1. Extend the existing SNES/SNI-style path for `sni_client` games.
2. Finish and certify the existing partial BizHawk generic Lua support for
   `bizhawk_generic_lua` games.
3. Treat custom APWorlds, Discord-only resources, and game-specific patchers as
   manual intake until the source is pinned and repeatable.

## Candidate Matrix

| Game | Family | Difficulty | Setup requirement | Notes |
|---|---|---|---|---|
| A Link to the Past | `sekailink_certified_sni` | Low | SNI/SNES connector path already proven. | Reference implementation for ALTTP release. |
| EarthBound | `sni_client` | Medium | SNI-capable SNES emulator, `.apeb` patch, `SNI/lua/Connector.lua` when Lua is required. | Strong next SNES candidate. Optional PopTracker exists. |
| Super Mario World | `sni_client` | Medium | SNI, `.apsmw` patch, `SNI/lua/Connector.lua`; RetroArch 1.10.3+ can work with network commands. | Platformer, tracker useful but not core runtime blocker. |
| Super Metroid | `sni_client` | Medium | SNI client and SNES device bridge. | Strong candidate, but Super Metroid-specific state is denser than platformers. |
| Donkey Kong Country 2 | `sni_client` | Medium | SNI included; snes9x-nwa or snes9x-rr recommended; BizHawk reported but less endorsed. | Tracker or Universal Tracker can be secondary. |
| Kirby's Dream Land 3 | `sni_client_rom_access` | Medium-high | SNI with ROM access, `.apkdl3` patch, BSNES core for BizHawk; RetroArch is incompatible in the official guide. | Core/emulator constraints make packaging more delicate. |
| Final Fantasy IV Free Enterprise | `custom_apworld_or_patcher` | High | SNI path plus FF4FE-specific patcher/mod workflow. | Needs source and patch flow review before automation. |
| Chrono Trigger | `custom_apworld_or_patcher` | High | External APWorld/Jets of Time flow and web patcher/YAML flow. | Promising, but source normalization comes first. |
| Secret of Evermore | `custom_apworld_or_patcher` | Unknown | Core status from research, but local APWorld path is unresolved in this registry. | Needs a focused source pass. |
| Mega Man 2 | `bizhawk_generic_lua` | Medium-high | BizHawk 2.7+, `.apmm2` patch, BizHawk Client, `connector_bizhawk_generic.lua`. | NES support can build on partial SKLMI/client BizHawk support. |
| Mega Man 3 | `bizhawk_generic_lua` | Medium-high | BizHawk 2.7+, `.apmm3` patch, BizHawk Client, `connector_bizhawk_generic.lua`. | Tracker is non-blocking; remaining work is bridge certification and game coverage. |
| Super Mario Land 2 | `bizhawk_generic_lua` | Medium-high | BizHawk 2.9.1 recommended, `.apsml2` patch, BizHawk Client, `connector_bizhawk_generic.lua`. | Platformer, but the client bridge is still BizHawk. |
| Pokemon Emerald | `bizhawk_generic_lua` | Medium-high | BizHawk 2.7+, `.apemerald` patch, BizHawk Client, `connector_bizhawk_generic.lua`. | Good target once the existing generic bridge is proven beyond ALTTP/SNES-style flows. |
| Pokemon Red and Blue | `bizhawk_generic_lua` | Medium-high | BizHawk Client and `connector_bizhawk_generic.lua`. | PopTracker exists; runtime challenge is bridge parity. |
| Pokemon Crystal | `bizhawk_generic_lua` | Medium-high | BizHawk Client path from local APWorld evidence. | Needs public setup URL confirmation. |
| Pokemon FireRed and LeafGreen | `bizhawk_generic_lua` | Medium-high | BizHawk Client and generic Lua connector from local setup. | Similar bridge class as Emerald. |
| Metroid: Zero Mission | `bizhawk_generic_lua` | Medium-high | BizHawk Client and generic Lua connector from local setup. | GBA bridge certification required. |
| The Legend of Zelda | `bizhawk_generic_lua` | Medium-high | BizHawk Client from local APWorld evidence. | NES bridge certification required. |
| Oracle of Ages / Oracle of Seasons | `bizhawk_generic_lua` | Medium-high | BizHawk 2.10 style path, `.apoos` for Seasons, BizHawk Client. | Verify Ages/Seasons split before automation. |
| Wario Land / Wario Land 4 | `bizhawk_generic_lua` | Medium-high | BizHawk Client and generic Lua connector from local setup. | Platformer category, but still BizHawk bridge. |
| Link's Awakening DX | `web_tracker_only` | Unknown-high | Web tracker confirmed by research, runtime setup still unresolved locally. | Tracker is not blocker; APWorld/client source must be pinned. |
| ActRaiser | `tracker_optional` | Unknown-high | Research indicates Discord-only APWorld/setup resources. | Tracker optional, but install source is not automation-safe yet. |
| Yoshi's Island | `tracker_optional` | Unknown-high | Research indicates Discord-hosted PopTracker and web tracker alternative. | Tracker optional; APWorld/source pinning still needed. |

## Porting Implications

- `sni_client` games are the best short-term expansion lane for SekaiLink because
  they resemble the existing ALTTP/SNES architecture.
- `bizhawk_generic_lua` games should be batched behind one bridge-completion and
  certification project. SKLMI/client support already exists in partial form, so
  many NES/GB/GBC/GBA games become cheaper together once that path is proven.
- Tracker status should not control runtime priority alone. Platformers can be
  trailer-ready with AP sync, received item messages, and check reporting.
- Custom patcher/mod games need pinned URLs, repeatable install steps, and a
  packaging policy before they enter the normal CDN/update path.

## Evidence Sources

- Local APWorld client and setup docs under `runtime/ap/worlds`.
- Existing partial SekaiLink BizHawk/SKLMI runtime support in
  `apps/client-core/electron/main.cjs`, `runtime/ap/data/lua/connector_bizhawk_generic.lua`,
  and `services/sklmi/src/api_socket_memory.cpp`.
- Deep Research report at `/home/thelovenityjade/deep-research-report.md`.
- Public Archipelago setup pages for EarthBound, Super Mario World, Kirby's
  Dream Land 3, Mega Man 2, Mega Man 3, Pokemon Emerald, Pokemon Red and Blue,
  and Super Mario Land 2.
