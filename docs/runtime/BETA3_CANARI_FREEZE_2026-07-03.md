# BETA-3 Canari Freeze - 2026-07-03

This freezes the Linux-confirmed Canari runtime state after the July 2-3 debug pass.

## Newly Confirmed On Linux

- Link's Awakening DX: bilateral send/reception confirmed through Sekaiemu/Gambatte and the RetroArch UDP bridge.
- Oracle of Ages: bilateral send/reception confirmed through Sekaiemu/Gambatte and the BizHawk-compatible bridge.
- Oracle of Seasons: bilateral send/reception confirmed after aligning the local apworld to the active Worlds generator version 13.7.4.
- Pokemon Emerald: bilateral send/reception confirmed through Sekaiemu/mGBA and the BizHawk-compatible bridge.
- Pokemon FireRed and LeafGreen: bilateral send/reception confirmed through Sekaiemu/mGBA and the BizHawk-compatible bridge.
- Pokemon Red and Blue: bilateral send/reception confirmed through Sekaiemu/Gambatte and the BizHawk-compatible bridge.
- Secret of Evermore: send/reception confirmed through the upstream Evermizer browser AP client and SekaiLink web_ap_client wrapper.

## Still Blocked

- Pokemon Crystal remains unavailable. It reaches gameplay in forced GBC mode, then freezes or graphically corrupts. Oracle of Ages and Pokemon Red/Blue confirm the broader GB/GBC runtime path works, so this remains Crystal-specific.
- Mega Man Battle Network 3 remains unavailable pending a dedicated MMBN3 tester.
- Final Fantasy Tactics Advance remains unavailable pending a dedicated tester because checks depend on mission reward flow.

## Windows Status

The above confirmations are Linux confirmations only. Windows validation still needs to be performed before any of these Canari runtime changes are promoted to Canonical.

## Freeze Rule

Do not promote this Canari set to Canonical until Windows confirms launch, AP connection, outgoing LocationChecks, incoming item injection/reception, clean shutdown, and tracker/runtime coexistence for the newly confirmed games.
