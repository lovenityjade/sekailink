# SNES BETA-3 Runtime Freeze

Date: 2026-06-26

Status: frozen validated.

SekaiLink BETA-3 has established the SNES/SNI runtime lane. The generic target
is no longer "prove SNES works"; it is now "avoid regressing the proven SNES
bridge while continuing other systems."

Confirmed bilateral fixtures:

- A Link to the Past
- Donkey Kong Country
- Donkey Kong Country 2
- EarthBound
- Kirby's Dream Land 3
- Lufia II Ancient Cave
- Mega Man X2
- Secret of Mana
- SMZ3
- Super Mario World
- Super Metroid

Bilateral means checks travel from game to server, and received items travel
from server back into the game.

Frozen runtime path:

```text
SNES libretro core
  -> Sekaiemu memory socket
  -> local SNI websocket bridge
  -> upstream Archipelago SNI/web client
  -> room server
```

Core notes:

- Most validated SNES modules remain on the existing default SNES path.
- Mega Man X2 validates the Snes9x/Cx4-style lane.
- Secret of Mana uses the Evermizer browser AP client path and is confirmed
  bilateral-functional.
- Secret of Mana keeps a non-blocking graphics debt on the naming/save screen.
  Do not mark the runtime incompatible for that visual bug.

Rules after freeze:

- Do not change the generic SNES/SNI bridge without a reproducible regression
  log.
- Treat future SNES failures as module-level adapter debt, ROM requirement
  cleanup, tracker/UI work, or game-specific APWorld behavior first.
- Do not use ALttP alone as proof of a regression because ALttP has its own
  established path.
