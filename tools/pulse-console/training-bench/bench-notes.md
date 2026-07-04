# Pulse Bench Notes

## Current Location

`/home/nobara-user/sekailink/pulse-training-bench-20260608`

## Important Source Material

- Train dataset: `pulse/rag/datasets/pulse-train-v0.jsonl`
- Eval dataset: `pulse/rag/datasets/pulse-eval-v0.jsonl`
- APWorld option index: `pulse/rag/indexes/apworld-options.jsonl`
- Randomizer reference doc:
  `docs/pulse-training/randomizer-multiworld-reference.md`
- Randomizer hardening dataset:
  `pulse/rag/datasets/randomizer-multiworld-reference-v0.jsonl`
- Spoiler practice dataset:
  `pulse/rag/datasets/pulse-sync-practice-20260608-worlds-spoiler.jsonl`
- Existing LoRA adapter: `pulse/models/pulse-lora-v0-adapter/`
- Base GGUF: `pulse/models/qwen2.5-coder-3b-instruct-q4_k_m.gguf`
- Current candidate GGUF:
  `pulse/models/candidates/pulse-qwen25-coder-3b-sekailink-v0-q4_k_m.gguf`

## Next Data Pass Focus

Create a canonical randomizer glossary before retraining:

- keysanity
- entrance shuffle
- dungeon item shuffle
- small key / big key / map / compass
- sphere
- logic
- progression item
- filler / junk
- local item / remote item
- goal
- Triforce Hunt
- pedestal
- accessibility
- beginner / casual / challenge / expert presets

For every term, add:

- short definition;
- beginner-friendly explanation;
- concrete ALTTP example;
- when to recommend it;
- when not to recommend it;
- one challenge answer;
- one eval question that fails if Pulse answers using the wrong game.

## Hard Eval Examples To Add

- Q: C'est quoi keysanity dans ALTTP?
  Must mention keys/maps/compasses/big keys can enter the item pool; must not
  answer Zelda II.
- Q: Est-ce que keysanity est bon pour un debutant?
  Must usually discourage it for first seeds.
- Q: Explique entrance shuffle a un streamer qui veut une seed simple.
  Must warn about navigation complexity.

## Training Run 2026-06-08

Started on `gaming-pc`:

- Workspace: `/home/nobara-user/sekailink-pulse-training`
- Train dataset: `datasets/pulse-train-v1-randomizer-20260608.jsonl`
- Eval dataset: `datasets/pulse-eval-v1-randomizer-20260608.jsonl`
- Output dir: `outputs/pulse-lora-v1-randomizer-20260608`
- Log: `logs/pulse-lora-v1-randomizer-20260608.log`
- PID file: `logs/pulse-lora-v1-randomizer-20260608.pid`
- Base model: `Qwen/Qwen2.5-Coder-3B-Instruct`
- Max steps: `1000`
- Save/eval interval: `100`

Dataset composition:

- Existing Pulse APWorld training rows
- `randomizer-multiworld-reference-v0.jsonl` upweighted for glossary and safety
- Deterministic subset of `pulse-sync-practice-20260608-worlds-spoiler.jsonl`

## Repair Run 2026-06-08

Second pass started after direct adapter eval showed targeted regressions on:

- `entrance shuffle` beginner explanation
- `seed` versus `room`
- Pokemon Emerald reconnect / queued items behavior
- ROM refusal phrasing

Repair artifacts:

- Repair dataset:
  `pulse/rag/datasets/randomizer-repair-v1-20260608.jsonl`
- Train dataset:
  `/home/nobara-user/sekailink-pulse-training/datasets/pulse-train-v2-randomizer-repair-20260608.jsonl`
- Eval dataset:
  `/home/nobara-user/sekailink-pulse-training/datasets/pulse-eval-v2-randomizer-repair-20260608.jsonl`
- Output dir:
  `/home/nobara-user/sekailink-pulse-training/outputs/pulse-lora-v2-randomizer-repair-20260608`
- Eval snapshots:
  `pulse/tests/pulse-randomizer-eval-v1.json`
  `pulse/tests/pulse-randomizer-eval-v2.json`

Observed outcome:

- Strong improvement on `entrance shuffle`
- Strong improvement on `seed` versus `room`
- Strong improvement on Pokemon Emerald resync explanation
- Cleaner ROM refusal
- Small wording regression on `keysanity` beginner recommendation, but still directionally discouraging for first-time players

## Polish Run 2026-06-08

Third pass was used as a style and beginner-guidance polish attempt:

- Train dataset:
  `/home/nobara-user/sekailink-pulse-training/datasets/pulse-train-v3-randomizer-polish-20260608.jsonl`
- Eval dataset:
  `/home/nobara-user/sekailink-pulse-training/datasets/pulse-eval-v3-randomizer-polish-20260608.jsonl`
- Output dir:
  `/home/nobara-user/sekailink-pulse-training/outputs/pulse-lora-v3-randomizer-polish-20260608`
- Eval snapshot:
  `pulse/tests/pulse-randomizer-eval-v3.json`

Observed outcome:

- Style changed, but factual quality regressed
- `keysanity` definition degraded
- `seed` versus `room` became weaker
- `EnergyLink` answer regressed badly
- `Pokemon Emerald` reconnect answer became less reliable

Current best adapter after direct evaluation remains:

- `/home/nobara-user/sekailink-pulse-training/outputs/pulse-lora-v2-randomizer-repair-20260608/adapter-final`
