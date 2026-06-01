# ALTTP Reference Patch Validation

Date: 2026-05-10

Purpose:

- Validate the expected ALTTP seed artefact shape before the native C++ Worlds
  generator is allowed to claim full ALTTP support.
- Use the existing Archipelago-compatible generator only as an oracle.
- Keep Worlds generic: this validation does not permit hardcoded ALTTP behavior
  in `generic_generation.cpp`.
- Keep the final target as one multiworld package assembled from several Nexus
  config versions. This single-slot ALTTP probe validates patch shape only.

## Reference Inputs

Generator command:

```bash
python3 /home/thelovenityjade/DevSSD/sekailink-beta-3-final/sekaiemu/Generate.py \
  --player_files_path /tmp/sekailink-alttp-generate/Players \
  --outputpath /tmp/sekailink-alttp-generate/output \
  --seed 20260510 \
  --multi 1 \
  --spoiler 1 \
  --log_level info
```

Seed archive:

- Path: `/tmp/sekailink-alttp-generate/output/AP_66054182889765821095.zip`
- SHA-256: `d0f0d9262916826ffb140feaccf48522ce30c1e87214f061e27bc3e0238239a4`

Patch:

- Path: `/tmp/sekailink-alttp-generate/extracted/AP_66054182889765821095_P1_Jade-ALTTP.aplttp`
- SHA-256: `89e417e06c0365fcd47b583cebd98eb05ae49752a014998cd8260b4d2ff283dc`
- Internal procedure: `apply_bsdiff4(delta.bsdiff4)`
- Required base checksum: `03a63945398191337e896e5771f77173`
- Result extension: `.sfc`

Base ROM used for validation:

- Path: `/home/thelovenityjade/.local/share/MultiworldGG/Zelda no Densetsu - Kamigami no Triforce (Japan).sfc`
- MD5: `03a63945398191337e896e5771f77173`

## Applied ROM

Manual application produced:

- Path: `/tmp/sekailink-alttp-generate/applied/AP_66054182889765821095_P1_Jade-ALTTP.sfc`
- Size: `4194304`
- MD5: `afff6eca79c06918336234d6313e34d5`
- SHA-256: `8fe2d4ee10309ab57714f27d7cd4217ac8c4f8c65f438ca765b911e92c5bfb50`
- LoROM title bytes: `AP072_1_   20260510`

Sekaiemu patch materialization produced the same ROM hash:

- Path: `/tmp/sekaiemu-alttp-generated-patch-probe/patched/AP_66054182889765821095_P1_Jade-ALTTP.sfc`
- MD5: `afff6eca79c06918336234d6313e34d5`
- SHA-256: `8fe2d4ee10309ab57714f27d7cd4217ac8c4f8c65f438ca765b911e92c5bfb50`

## Headless Sekaiemu Probe

Direct patched ROM probe:

```bash
/home/thelovenityjade/Sekaiemu-Libretro-Spike-Codex/workspace/sekaiemu-libretro-spike/build/sekaiemu_libretro_spike \
  --probe-only \
  --profile /home/thelovenityjade/Sekaiemu-Libretro-Spike-Codex/workspace/sekaiemu-libretro-spike/profiles/alttp-starter.profile \
  --core /usr/lib64/libretro/bsnes_mercury_performance_libretro.so \
  --game /tmp/sekailink-alttp-generate/applied/AP_66054182889765821095_P1_Jade-ALTTP.sfc \
  --save-dir /tmp/sekaiemu-alttp-generated-probe-escalated \
  --memory-socket /tmp/sekaiemu-alttp-generated-probe-escalated/memory.sock
```

Result:

- Exit code: `0`
- Core loaded: `bsnes-mercury v094 (Performance)`
- ROM loaded as `program.rom size=0x400000`
- Geometry: `256x224`
- FPS: `60.098812`
- Memory domains exposed: `system_ram`, `save_ram`, `video_ram`
- Bridge active: `A Link to the Past watch=system_ram 0xf000..0xf23f checks=8 receive_slots=3`

Patch-first Sekaiemu probe:

```bash
/home/thelovenityjade/Sekaiemu-Libretro-Spike-Codex/workspace/sekaiemu-libretro-spike/build/sekaiemu_libretro_spike \
  --probe-only \
  --profile /home/thelovenityjade/Sekaiemu-Libretro-Spike-Codex/workspace/sekaiemu-libretro-spike/profiles/alttp-starter.profile \
  --core /usr/lib64/libretro/bsnes_mercury_performance_libretro.so \
  --patch /tmp/sekailink-alttp-generate/extracted/AP_66054182889765821095_P1_Jade-ALTTP.aplttp \
  --base-rom /home/thelovenityjade/.local/share/MultiworldGG/Zelda\ no\ Densetsu\ -\ Kamigami\ no\ Triforce\ \(Japan\).sfc \
  --save-dir /tmp/sekaiemu-alttp-generated-patch-probe \
  --memory-socket /tmp/sekaiemu-alttp-generated-patch-probe/memory.sock
```

Result:

- Exit code: `0`
- Sekaiemu wrote `/tmp/sekaiemu-alttp-generated-patch-probe/patched/AP_66054182889765821095_P1_Jade-ALTTP.sfc`
- The materialized ROM hash matched the manually applied ROM.
- The materialized ROM loaded headless through libretro and exposed memory.

## Contract Impact

This proves:

- The reference ALTTP seed archive can produce a valid `.aplttp`.
- The `.aplttp` can be applied to the expected base ROM.
- Sekaiemu can apply the `.aplttp` itself and boot the result headlessly.
- The resulting runtime exposes the memory surfaces needed by SKLMI/tracker work.

This does not yet prove:

- Native C++ ALTTP solving.
- Native C++ ALTTP item placement.
- A complete generated Worlds package from the generic C++ generator.
- Multi-slot generation from several Nexus config versions.
- Mixed LinkedWorld generation in one room.
- Full end-to-end room distribution for a complete run.

The ALTTP LinkedWorld generation gates therefore remain closed until the native
solver, placer, and patch emitter are implemented and covered by tests.
