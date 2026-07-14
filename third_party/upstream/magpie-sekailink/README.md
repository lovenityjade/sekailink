# Magpie integration for SekaiLink

SekaiLink uses the official local Magpie 1.1.2 builds as the specialized
external tracker for Link's Awakening DX.

- Upstream source: https://github.com/kbranch/Magpie
- Pinned source revision: `7db45747b22b8fe182ecab4e232f23549ce185cc`
- Upstream project and local builds are licensed under the MIT License.
- SekaiLink downloads the platform build from the upstream project, verifies
  its SHA-256 digest, and runs it as a local service.

The SekaiLink launcher opens Magpie with autotracking enabled. The existing
Archipelago LADX client receives the room server (including its port) and slot
from Client Core, then supplies that identity and live tracker state to Magpie
through the local websocket bridge on port 17026. Archipelago passwords are
not placed in Magpie's process arguments or URL.

No upstream Magpie source files are modified in place. SekaiLink-specific
process lifecycle, verified installation, and window management live in
`apps/client-core/electron/lib/poptracker-runtime.cjs`.
