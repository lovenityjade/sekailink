# Archipelago Live Probe - 2026-05-21

This probe validates SKLMI against a real local MultiworldGG/Archipelago server,
not only against an in-process fake transport.

## Runtime

- Server binary:
  `<local-home>/Games/The Legend of Jade/clients/squashfs-root/opt/MultiworldGG/MultiworldGGServer`
- Seed archive:
  `<local-home>/Downloads/AP_78856210104802680998.zip`
- Local endpoint:
  `127.0.0.1:38283`
- SKLMI probe:
  `build/sekailink_sklmi_archipelago_live_probe`

## Results

- `Jade-ALTTP` connected as Archipelago slot `2`.
- `Jade-TWW` connected as Archipelago slot `11`.
- `GetDataPackage` was accepted by the server and SKLMI resolved received item
  IDs into human-readable item names.
- Reporting ALTTP location `60025` (`Sanctuary`) produced a real server-side
  item send:
  `Jade-ALTTP sent Progressive Magic Meter to Jade-TWW (Sanctuary)`.
- Connecting as `Jade-TWW` returned the item through `ReceivedItems`:
  `ap_item_id=2322490 name=Progressive Magic Meter location=60025 player=2`.
- Reporting ALTTP location `59836` (`Link's House`) produced a real server-side
  item send:
  `Jade-ALTTP sent Recovery Heart to Jade-SoH (Link's House)`.
- The full runtime path was also validated with
  `sekailink_sklmi_archipelago_runtime_live_probe`:
  - a fake Sekaiemu-compatible Unix memory socket exposed `system_ram`
  - `sekailink_sklmi_runtime --mode archipelago` loaded
    `manifests/alttp.phase1.json`
  - the runtime read the checked bit at `61476`
  - SKLMI emitted AP `LocationChecks` for `60025`
  - the real local MultiworldGG server confirmed:
    `Jade-ALTTP sent Progressive Magic Meter to Jade-TWW (Sanctuary)`

## Known Follow-Up

- The server warns that the native client does not support compressed WebSocket
  connections. The current plain WebSocket path works for this local probe, but
  production SKLMI should add permessage-deflate support or explicitly negotiate
  no compression if the target server policy permits it.
- This does not prove full ALTTP memory coverage. It proves the runtime packet
  loop that SKLMI needs once Sekaiemu exposes real memory-derived location
  events.
