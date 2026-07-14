# Dolphin Memory Notes for Archipelago-Style GameCube Support

Date: 2026-06-10

This note summarizes how Archipelago-style GameCube clients talk to Dolphin,
with The Wind Waker from upstream Archipelago, Paper Mario: The Thousand-Year
Door from the local MultiworldGG/SekaiLink world copy, and Twilight Princess
from the local MultiworldGG world copy as the main examples.

## Sources Read

- Upstream Archipelago:
  - `<local-home>/Projects/reference-repos/Archipelago/worlds/tww/TWWClient.py`
  - `<local-home>/Projects/reference-repos/Archipelago/worlds/tww/docs/setup_en.md`
  - `<local-home>/Projects/reference-repos/Archipelago/worlds/tww/requirements.txt`
- Local TTYD world copy:
  - `<local-home>/Games/SoH-SekaiLink-Edition-PRE-BETA3/worlds/ttyd/TTYDClient.py`
  - `<local-home>/Games/SoH-SekaiLink-Edition-PRE-BETA3/worlds/ttyd/Rom.py`
  - `<local-home>/Games/SoH-SekaiLink-Edition-PRE-BETA3/worlds/ttyd/docs/setup_en.md`
- Dolphin Memory Engine reference:
  - `/tmp/dolphin-memory-engine-code/Source/DolphinProcess/DolphinAccessor.cpp`
  - `/tmp/dolphin-memory-engine-code/Source/DolphinProcess/Linux/LinuxDolphinProcess.cpp`
  - `/tmp/dolphin-memory-engine-code/Source/DolphinProcess/Windows/WindowsDolphinProcess.cpp`
  - `/tmp/dolphin-memory-engine-code/Source/Common/MemoryCommon.h`
  - `/tmp/dolphin-memory-engine-code/Source/Common/CommonUtils.h`
- Local Twilight Princess world copy:
  - `<local-home>/DevSSD/_sekailink_quarantine/20260601-pre-e2e/DevSSD/SekaiLinkDev/reference/upstream/multiworldgg/worlds/tp/TPClient.py`
  - `<local-home>/DevSSD/_sekailink_quarantine/20260601-pre-e2e/DevSSD/SekaiLinkDev/reference/upstream/multiworldgg/worlds/tp/ClientUtils.py`
  - `<local-home>/DevSSD/_sekailink_quarantine/20260601-pre-e2e/DevSSD/SekaiLinkDev/reference/upstream/multiworldgg/worlds/tp/__init__.py`
  - `<local-home>/.local/share/Archipelago/custom_worlds/tp/docs/setup_en.md`
  - `<local-home>/.local/share/Archipelago/custom_worlds/tp/docs/CurrentState.md`
- Twilight Princess randomizer source:
  - `<local-home>/Projects/zsrtp-Randomizer/GameCube/source/main.cpp`
  - `<local-home>/Projects/zsrtp-Randomizer/GameCube/source/rando/randomizer.cpp`

## What Archipelago Uses

The Wind Waker imports `dolphin_memory_engine` and declares
`dolphin-memory-engine>=1.3.0` in its world requirements. TTYD also imports the
same Python module as `dolphin`.

The model is not SNI and not a Dolphin plugin. It is an external process hook:

1. Find a running Dolphin process.
2. Locate the emulated RAM mapping inside that process.
3. Read and write GameCube/Wii console addresses such as `0x80000000`.
4. Poll game-specific memory addresses on a timer.
5. Send Archipelago `LocationChecks` and write inbound items into game memory.

## Dolphin Memory Address Model

Dolphin Memory Engine treats these as the important console regions:

- `MEM1`: starts at `0x80000000`.
- `MEM2`: starts at `0x90000000`, Wii-only extra RAM.
- `ARAM`: starts at `0x7E000000`, mostly relevant when MEM2 is absent.

The DME code converts console addresses to process offsets:

- MEM1 offset: `address - 0x80000000`.
- MEM2 offset: `(address - 0x90000000) + (0x90000000 - 0x80000000)`.
- ARAM has special fake-size mapping when accessible.

GameCube games generally use MEM1. Both TWW and TTYD use `0x80xxxxxx` addresses.

GameCube/Wii memory values are big-endian at the game level. The Python clients
often handle this explicitly:

- TWW reads shorts with `int.from_bytes(..., byteorder="big")`.
- TTYD packs received item IDs with `struct.pack(">H", ...)`.

## How Hooking Works Internally

DME has a platform-specific `IDolphinProcess` implementation.

On Linux:

- It scans `/proc` for Dolphin process names such as `dolphin-emu`.
- It parses `/proc/<pid>/maps`.
- It looks for mappings named `/dev/shm/dolphin-emu` or `/dev/shm/dolphinmem`.
- Reads use `process_vm_readv`.
- Writes use `process_vm_writev`.
- It may require `cap_sys_ptrace` or root depending on system hardening.

On Windows:

- It enumerates processes and matches names such as `Dolphin.exe` or
  `DolphinQt2.exe`.
- It opens Dolphin with process query/read/write permissions.
- It walks memory with `VirtualQueryEx`.
- Reads use `ReadProcessMemory`.
- Writes use `WriteProcessMemory`.

On both platforms, it probes structures near the start of the emulated RAM to
detect whether a GameCube, Wii, or WAD-style title is running, then derives MEM1
and optional MEM2/ARAM sizes.

## The Wind Waker Client Pattern

TWW is the clean upstream example.

Connection:

- Calls `dolphin_memory_engine.hook()`.
- Requires `is_hooked()`.
- Verifies the game by reading `0x80000000..0x80000005` and expecting
  `GZLE99`.
- If wrong, unhooks and retries after 5 seconds.

Authentication:

- Reads the slot name from `0x803FE8A0`.
- Uses that to authenticate to the AP server.

Inbound items:

- Reads expected item index at `0x803C528C`.
- Writes item IDs into a 16-byte give-item array at `0x803FE87C`.
- Empty item slots are `0xFF`.
- The game/mod consumes this array every frame.

Checks:

- Reads several saved/current stage bitfields:
  - charts at `0x803C4CFC`
  - saved chests/switches/pickups at `0x803C4F88+`
  - current stage chests/switches/pickups at `0x803C5380+`
- Interprets per-location type and bit.
- Sends AP `LocationChecks` for newly checked locations.

Tracker/map context:

- Reads current stage name at `0x803C9D3C`.
- Sends AP `Bounce` with `tww_stage_name`.

DeathLink:

- Reads current health at `0x803C4C0A`.
- Writes zero health to apply incoming death.

Loop rate:

- Normal connected loop sleeps around `0.1s`.
- Failed hook retries sleep around `5s`.

## TTYD Client Pattern

TTYD uses the same Dolphin memory engine but relies more heavily on its patch/mod.

Important addresses:

- `RECEIVED_INDEX = 0x803DB860`
- `RECEIVED_ITEM_ARRAY = 0x80001000`
- `RECEIVED_LENGTH = 0x80000FFC`
- `SEED = 0x80003210`
- `GP_BASE = 0x803DAC18`
- `GSWF_BASE = 0x178`
- `GSW0 = 0x174`
- `GSW_BASE = 0x578`
- `ROOM = 0x803DF728`

Connection:

- Calls `dolphin.hook()`.
- Tracks a local `dolphin_connected` boolean.
- Does not verify `Game ID` directly in the current client code.
- Verifies ROM/room match by reading the seed string at `0x80003210` and
  checking it against the server seed name.

Inbound items:

- If `RECEIVED_LENGTH` is nonzero, the client waits.
- Reads `RECEIVED_INDEX`.
- Packs up to 255 pending item ROM IDs as big-endian 16-bit values.
- Writes them into `RECEIVED_ITEM_ARRAY`.
- Writes item count to `RECEIVED_LENGTH`.
- Writes the next expected index to `RECEIVED_INDEX`.
- The patched game/mod consumes this queue.

Checks:

- Uses GSW and GSWF tables from `Data.py`.
- `GSW` values are read from `GP_BASE + GSW0` or `GP_BASE + index + GSW_BASE`.
- `GSWF` bits are dynamically mapped:
  - word index: `bit_number >> 5`
  - bit position: `bit_number & 0x1F`
  - word address: `GP_BASE + (word_index * 4) + GSWF_BASE`
  - byte within word: `3 - (bit_position >> 3)`
  - final byte address: `word_address + byte_within_word`
  - bit: `bit_position & 0x7`
- Location checks iterate `location_gsw_info`.
- Some unit/shop-style locations adjust offset dynamically.

Room/tracker context:

- Reads current room string at `0x803DF728`.
- Writes AP data storage key `ttyd_room_<team>_<slot>` when room changes.

DeathLink:

- Local death flag read at `0x80003240`.
- Incoming death writes `1` to `0x8000323F`.

Loop rate:

- Normal in-game loop sleeps around `0.5s`.
- Failed hook retry sleeps around `3s`.

## Dolphin Configuration Requirements Seen in Docs

TWW:

- Use latest Dolphin.
- Disable emulated memory size override.
- Do not run Launcher or Dolphin as Administrator on Windows.
- Disable cheats/codes when troubleshooting.
- Fallback region should be `NTSC-U`.
- Dolphin on external drive can reportedly cause connection issues.

TTYD:

- US ISO only.
- Not `.ciso`, `.nkit.iso`, or `.nkit.rvz`.
- Disable Dual Core in per-game config.
- Prefer OpenGL or Vulkan; Direct3D can cause crashes.
- Disable CPU clock override.
- Disable memory size override.
- macOS requires code-signing steps for Dolphin memory access.

## SekaiLink Design Implications

For SKLMI/Sekaiemu, Dolphin should be treated as a separate memory provider
family, not as a libretro core:

```text
MemoryProvider
  sekaiemu_runtime_socket
  sni
  dolphin_process
```

A Dolphin provider should expose at least:

- `gc_mem1`, mapped to console addresses `0x80000000..MEM1_END`.
- `wii_mem2`, mapped to `0x90000000..MEM2_END` when present.
- Optional `aram`.

The manifest layer must support dynamic address logic for games like TTYD:

- Static direct reads for simple checks.
- Dynamic bitfield helpers for GSWF-style flags.
- Queue-style item delivery with “busy length” semantics.

This is similar to the FireRed dynamic SaveBlock1 work, but for GameCube:

- FireRed resolves a dynamic save block pointer.
- TTYD resolves dynamic GSWF byte/bit from a logical flag number.
- TWW uses mostly fixed bitfield bases plus current-stage overlays.

For TTYD specifically, a minimal SKLMI manifest cannot be purely static. It
needs either:

1. a first-class `ttyd_gsw` / `ttyd_gswf` dynamic check source; or
2. a generated manifest that expands every GSW/GSWF location to direct byte/bit
   rules using the formulas above; plus special handling for dynamic unit/shop
   offsets.

The safer first target would be a dedicated Dolphin memory provider plus a
small TTYD adapter, not a generic static manifest.

## Twilight Princess Client Pattern

Twilight Princess is the odd one compared to TWW and TTYD. It still uses
`dolphin_memory_engine`, but the randomizer expects a REL loader plus a custom
seed/save setup in Dolphin. The AP client mostly talks to a live save-file
memory area that the injected TP randomizer code also watches.

Setup requirements from the local docs:

- Dolphin plus a North American Twilight Princess ISO are expected.
- The player must download the REL loader from `tprandomizer.com`.
- The player must place the custom seed file, the RELoader save, and
  `RandomizerAP.US.gci` in Dolphin save data.
- Dolphin's `Enable Emulated Memory Size Override` must be disabled.
- The client supports US, EU, and JP memory base selection in code, but the
  docs say non-US support is still under development and non-US Ganon kill
  completion does not work the same way.
- Player names with most special characters are not supported, and the `/name`
  command truncates to 16 characters.

Connection:

- Calls `dolphin_memory_engine.hook()`.
- Verifies the running title by reading `0x80000000..0x80000002` and expecting
  `GZ2`.
- Reads the region byte at `0x80000003`, accepting `E`, `P`, or `J`.
- Chooses all main addresses from a region-specific save-file base:
  - US/default: `SAVE_FILE_ADDR = 0x804061C0`
  - EU/P: `SAVE_FILE_ADDR = 0x80408160`
  - JP/J: `SAVE_FILE_ADDR = 0x80400300`
- String encoding is ASCII for US/EU and Shift-JIS for JP.

Derived addresses:

- `CURR_HEALTH_ADDR = SAVE_FILE_ADDR + 0x2`
- `SLOT_NAME_ADDR = SAVE_FILE_ADDR + 0x1B4`
- `NODES_START_ADDR = SAVE_FILE_ADDR + 0x1F0`
- `ACTIVE_NODE_ADDR = SAVE_FILE_ADDR + 0x958`
- `CURR_NODE_ADDR = SAVE_FILE_ADDR + 0x978`
- `ITEM_WRITE_ADDR = SAVE_FILE_ADDR + 0x8F0`
- `EXPECTED_INDEX_ADDR = SAVE_FILE_ADDR + 0x900`

Authentication:

- If not already authenticated, reads the slot name from `SLOT_NAME_ADDR`.
- The client provides `/name <name>` to write the in-game save name before
  connecting.
- This is different from TWW's fixed slot-name address and TTYD's seed-string
  check. TP's player identity is essentially the save file name.

Inbound items:

- The client receives AP `ReceivedItems` and keeps a local ordered list.
- It reads `EXPECTED_INDEX_ADDR` as a big-endian short.
- It computes pending items from `items_received[expected_index:]`.
- It only writes if the 8-byte item queue at `ITEM_WRITE_ADDR..+7` is fully
  empty, where empty means byte `0x00`.
- It writes up to 8 one-byte TP item IDs into that queue.
- It then writes `idx + 1` back to `EXPECTED_INDEX_ADDR`.
- The injected TP randomizer code reads `dComIfG_gameInfo.save.save_file.reserve.unk`
  as the event item queue, starts a get-item event when Link is in a safe
  movement/wait proc, and clears the queued byte after the get-item actor is
  created.

This is the key architectural difference: for TWW/TTYD, the AP client writes
to explicit mod-owned addresses. For TP, the mailbox is the save-file reserved
bytes used by the REL randomizer. SekaiLink should not treat this as a pure
memory manifest without understanding the injected REL/save contract.

Checks:

- Reads current node from `CURR_NODE_ADDR`; `0xFF` means menu.
- Region-scoped locations use either:
  - active node memory: `ACTIVE_NODE_ADDR + offset` when the location's region
    equals the current node; or
  - saved node memory: `NODES_START_ADDR + (region * 32) + offset`.
- Flag-style locations use `SAVE_FILE_ADDR + offset`.
- Each location is checked with `byte & bit`.
- `Hyrule Castle Ganondorf` is special: if its save flag is set, the client
  sends `StatusUpdate` with `CLIENT_GOAL`.
- To avoid reading while the stage/node changes, it sleeps briefly and skips
  that poll if `CURR_NODE_ADDR` changed mid-pass.

Server data side channel:

- TP builds AP DataStorage keys named `TP_<team>_<slot>_<key>`.
- It initializes the keys with `Set` messages using defaults from
  `ClientUtils.server_data`.
- During location checks it also tracks selected save/region flags and the
  current node, then writes changes to DataStorage.
- This is useful tracker state, but it is not the same as `LocationChecks`.

DeathLink:

- Local death reads health at `CURR_HEALTH_ADDR`.
- Incoming death writes a big-endian zero short to `CURR_HEALTH_ADDR`.

Loop rate:

- Normal connected loop sleeps around `0.1s`.
- Failed hook retry sleeps around `5s`.

## Twilight Princess Design Implications

TP should be modeled as a Dolphin process provider plus a game-specific
adapter, not as a static manifest-only game.

The adapter needs:

- region detection from the `GZ2?` game ID;
- region-specific save-file base selection;
- ASCII/Shift-JIS name handling;
- an 8-byte zero-empty inbound item queue;
- a big-endian 16-bit received index;
- active-node versus saved-node flag reads;
- a DataStorage/tracker side channel if SekaiLink wants parity with the AP
  client;
- awareness that the REL loader and `RandomizerAP.US.gci` are part of the
  runtime contract.

For SekaiLink, a practical phase 1 for TP would be a diagnostic-only adapter:

1. hook Dolphin;
2. verify `GZ2E/GZ2P/GZ2J`;
3. compute `SAVE_FILE_ADDR`;
4. read current node, slot name, expected index, and item queue occupancy;
5. read a small curated set of known flags;
6. report whether the REL/save mailbox appears alive.

Only after that should SKLMI attempt item injection or full location tracking.

## Recommended Next Step

Before implementing GameCube support, build a tiny Dolphin diagnostic tool:

1. Hook Dolphin.
2. Expose domains:
   - `gc_mem1`
   - `wii_mem2` if present
3. Read:
   - `0x80000000` game ID or magic bytes.
   - TTYD seed at `0x80003210` when TTYD is loaded.
   - TTYD room at `0x803DF728` when a save is loaded.
4. Emit a SKLMI-style domain descriptor snapshot.

Once that works, TTYD can be ported using its queue addresses and GSW/GSWF
rules.
