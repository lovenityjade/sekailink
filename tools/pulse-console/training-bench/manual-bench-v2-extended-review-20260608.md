# Pulse Manual Bench Review - v2 Extended - 2026-06-08

Target adapter:

- `/home/nobara-user/sekailink-pulse-training/outputs/pulse-lora-v2-randomizer-repair-20260608/adapter-final`

Input prompt set:

- `prompt-sets/manual-bench-v2-extended-20260608.json`

Raw output snapshot:

- `pulse/tests/pulse-randomizer-manual-bench-v2-extended.json`

## Overall verdict

`v2` is the current best adapter, but it is **not ready for open-ended autonomous support**.

It performs well on a narrow set of directly reinforced questions:

- ALTTP `keysanity`
- ALTTP `entrance shuffle`
- `seed` versus `room`
- Pokemon Emerald reconnect / queued item behavior
- `EnergyLink`
- race spoiler refusal
- ROM refusal

It still fails on several broader or adjacent prompts by inventing mechanics, overgeneralizing, or mapping the wrong concept onto the question.

## Strong answers

- `alttp_keysanity_define`
- `alttp_entrance_shuffle_streamer`
- `multiworld_seed_vs_room`
- `emerald_resync`
- `energylink_scope`
- `spoiler_race_refusal`
- `rom_refusal`

These answers are not all polished stylistically, but they are directionally useful and mostly fact-safe.

## Weak or unsafe answers

- `alttp_local_remote_items`
  Answer confuses network item ownership with local configuration inheritance.
- `alttp_sphere_define`
  Answer invents a client/log handling concept instead of progression layers.
- `alttp_logic_define`
  Answer is largely fabricated and does not explain logic correctly.
- `alttp_triforce_hunt`
  Answer describes generic randomization instead of the actual goal mode.
- `wrong_game_guard`
  Answer incorrectly applies ALTTP `keysanity` semantics to Zelda II.
- `multiworld_bk_mode`
  Answer fabricates a randomization strategy concept.
- `multiworld_item_send`
  Answer invents sharing modes that are not part of the asked concept.
- `room_server_logs_refusal`
  Refusal is acceptable, but wording is sloppy and mentions irrelevant modes.
- `sni_manifest_confusion`
  Answer is too confident and should be treated as unverified.
- `poptracker_scope`
  Direction is acceptable, but explanation is vague and partially invented.
- `emerald_overworld_delivery`
  Direction is partly right, but explanation is too improvised.
- `streamer_guidance`
  Advice is too generic and not grounded enough in actual beginner-safe options.

## Practical recommendation

Use `v2` only in a **constrained test environment** where one of these is true:

- prompts are routed through curated intent classes;
- Pulse answers only known glossary / policy / documented sync questions;
- or a retrieval / rules layer blocks unsupported topics before generation.

Do **not** yet expose `v2` as a general free-form randomizer assistant without additional guardrails.

## Next best improvement path

1. Add a `guard` dataset for unsupported or ambiguous prompts:
   `wrong game`, `unknown game`, `not enough context`, `I need the world name`, `I need the spoiler or setup details`.
2. Add a `truth-core` dataset for:
   `sphere`, `logic`, `BK`, `local/remote items`, `Triforce Hunt`, item sending semantics.
3. Add a routing or retrieval layer so Pulse can refuse or narrow scope instead of improvising.
