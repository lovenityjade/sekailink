# Source Of Truth

For `sekailink-nexus`, authoritative code and config are:

1. `server/native/sekailink_server_core` for Nexus runtime/server logic.
2. `deploy/nexus` for Nexus service deployment units and configs.
3. `third_party/sekailink_runtime/third_party` for embedded build headers.

`docs/nexus-configs.md` defines the Nexus Configs tier contract:

- `deploy/nexus/**/*.json.example` files are the committed config templates.
- live configs derived from those templates stay outside git under `/opt/sekailink/nexus/*/config/`.
- CMake only gains new config sources or smoke executables after the matching files exist.

Planning/history documents in `docs` are reference context only.
