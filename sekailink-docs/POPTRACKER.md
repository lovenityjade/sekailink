# PopTracker Integration (SekaiLink Client)

PopTracker is vendored into this repo as source-only and compiled as part of the client build.

Location
- Source: `third_party/PopTracker`

CLI Additions (SekaiLink patch)
PopTracker now accepts Archipelago connection info on the command line and will auto-connect by default
when any AP parameter is provided.

Supported flags:
- `--ap-host <host:port>`
- `--ap-slot <slot>`
- `--ap-pass <password>`
- `--ap-autoconnect` (optional; auto-connect is enabled by default when any AP flag is present)
- `--pack-variant <variant>` (select pack variant on load)
- `--list-pack-variants <uid|path>` (list variants for a pack)

Notes:
- The AP password is kept in memory only (not written to config).
- Pack loading is unchanged and still uses PopTracker CLI:
  - `PopTracker <path/to/pack> --pack-variant <variant>`
  - `PopTracker --load-pack <uid> --pack-variant <variant>`
- SekaiLink installe les packs dans `userData/poptracker/packs/<gameId>` et passe le chemin local.
- PopTracker will need a CLI path to auto-connect to SNI when a game requires it.

Where the patch lives
- `third_party/PopTracker/src/main.cpp` (CLI parsing + args wiring)
- `third_party/PopTracker/src/poptracker.cpp` (AP args + auto-connect on pack load)
- `third_party/PopTracker/src/poptracker.h`
- `third_party/PopTracker/src/core/autotracker.h` (backend index helpers)
- `third_party/PopTracker/doc/commandline.txt` (help text)

Update policy
- PopTracker is updated when we update Archipelago and the SekaiLink client.
- Recompile PopTracker after updates; re-apply the above patch if upstream changes conflict.
