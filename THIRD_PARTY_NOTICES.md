# Third-Party Notices

This repository includes or depends on third-party software components.
Each component remains licensed under its own terms.

## Core Upstream Projects

| Component | Usage in SekaiLink | License / Notice Source |
|---|---|---|
| Archipelago Multiworld | Core protocol/server/client base used and extended by SekaiLink | `LICENSE` (repo root) |
| PopTracker | Tracker runtime and pack integration | `third_party/PopTracker/LICENSE` |
| BizHawk | Emulator runtime integration (Lua connector / bridge workflows) | `third_party/emulators/BizHawk/LICENSE` |

## Additional Embedded Third-Party Code

SekaiLink vendors additional third-party code under:

- `third_party/`
- `client/app/node_modules/` (build/runtime JS dependencies)
- `client/runtime/_bundled_libs/`

For these dependencies, the applicable license is provided in each dependency's own source tree and must be respected when distributing binaries or source.

## Distribution Notes

- This file is a notice index, not a replacement for upstream licenses.
- When creating official release artifacts, include this file together with all relevant upstream license files and attribution notices from bundled dependencies.
