# Nexus Seed Config Imports

SekaiLink seed configuration is owned by Nexus.

APWorlds are used as an import source for option schemas, not as the runtime source
of truth for user seed settings. User choices live in the Nexus database. YAML is
only an export format generated from the canonical Nexus values when Archipelago
needs a player file.

## Flow

1. Read installed APWorld option classes from `runtime/ap`.
2. Convert Archipelago option definitions into Nexus `seed-configs` JSON:
   - `Choice` -> `enum`
   - `Toggle` -> `boolean`
   - `Range` / `NamedRange` -> `integer`
   - `FreeText` / `TextChoice` -> `string`
   - `OptionSet` / `OptionList` -> `array`
   - `OptionDict` / `OptionCounter` -> `object`
3. Import those schemas into Nexus with the admin endpoint.
4. Client Core asks Nexus for `/seed-configs/games/{game_key}/options`.
5. Client Core saves user values back to Nexus.
6. Nexus validates and exports YAML from stored canonical values.

## Export Schemas

```bash
tools/resource-import/export_apworld_seed_configs.py \
  --installed-only \
  --output-dir runtime/generated/nexus-seed-config-imports
```

The output directory is generated data and is ignored by Git. Regenerate it when
`runtime/ap` or the supported game registry changes.

To export only one game while debugging:

```bash
tools/resource-import/export_apworld_seed_configs.py \
  --game "A Link to the Past" \
  --output-dir /tmp/sekailink-seed-config-test
```

## Import Into Nexus

Dry run:

```bash
tools/resource-import/import_seed_configs_to_nexus.py \
  --input-dir runtime/generated/nexus-seed-config-imports \
  --base-url http://127.0.0.1:19106 \
  --admin-token "$NEXUS_SEED_CONFIG_ADMIN_TOKEN" \
  --dry-run
```

Real import:

```bash
tools/resource-import/import_seed_configs_to_nexus.py \
  --input-dir runtime/generated/nexus-seed-config-imports \
  --base-url http://127.0.0.1:19106 \
  --admin-token "$NEXUS_SEED_CONFIG_ADMIN_TOKEN"
```

To import one game:

```bash
tools/resource-import/import_seed_configs_to_nexus.py \
  --input-dir runtime/generated/nexus-seed-config-imports \
  --base-url http://127.0.0.1:19106 \
  --admin-token "$NEXUS_SEED_CONFIG_ADMIN_TOKEN" \
  --game-key a_link_to_the_past
```

## Current Coverage

The exporter currently uses the installed Archipelago runtime plus
`runtime/game-registry/archipelago-clients.json` for SekaiLink game keys and
platform labels.

On the current AP runtime snapshot it exports 47 schemas, including NES, SNES,
GB/GBC, GBA, N64, GameCube, and Wii worlds present in `runtime/ap`.

## Known Limits

Some APWorld option metadata contains Python objects that cannot be represented
directly in JSON. The exporter sanitizes those values into strings so Nexus can
store diagnostics without executing AP code.

`TextChoice` and `NamedRange` are richer than the current Nexus primitive types.
The exporter preserves their AP metadata in `validation_rules`; the Client UI can
be improved later to render those hints more elegantly.
