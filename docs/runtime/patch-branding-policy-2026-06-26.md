# Sekailink Patch Branding Policy

Date: 2026-06-26

## Prime rule

Patch branding must be done game by game, inside the world module that creates the generated patch.

Do not run global binary replacements over `.ap*` patch payloads. Patch files can contain delta streams,
fixed-address token data, metadata, signatures, and protocol identifiers. A global replacement can corrupt
ROM patches even when the replacement is length-preserving.

## Safe pattern

Only change explicit ROM text writes when all of these are true:

- The target address or patch token is already writing user-visible text.
- The encoding and terminator are understood.
- The replacement keeps the exact expected field length.
- The change is local to one game module.
- The patch still compiles after the edit.

## Current explicit Sekailink branding

- `worlds/smz3/TotalSMZ3/Patch.py`
  - Changes the synthetic server/player 0 display name from `Archipelago` to `Sekailink`.
  - Keeps the existing 16-byte centered player-name field.

- `worlds/sm/__init__.py`
  - Changes the Super Metroid ROM player-name table entry for player id `0` from `Archipelago` to `Sekailink`.
  - Does not rename internal `ArchipelagoItem` item types.

- `worlds/dkc3/Rom.py`
  - Changes the DKC3 credits text at `0x32A5DF` from `ARCHIPELAGO MOD` to `SEKAILINK MOD`.
  - Keeps the same 15-byte field length and high-bit text terminator.

## Do not rename without a dedicated adapter

- `ArchipelagoLufia`
  - Used by Lufia II Ancient Cave as a signature/protocol marker.
  - Renaming it would likely break client or patch compatibility.

- `ArchipelagoItem`
  - Used by Super Metroid/VARIA as an internal item category/type.
  - Renaming it would break logic and item lookup.

- Documentation strings
  - Setup docs and external project names can remain as Archipelago references.
  - Credits to upstream projects should not be erased.

## Future work

For each game, inspect its `Rom.py`, generated token format, and any assembled basepatch source before changing text.
When a game needs branding but stores strings in assembled data, create a dedicated game adapter and verify with a
fresh generated patch plus a boot smoke test.
