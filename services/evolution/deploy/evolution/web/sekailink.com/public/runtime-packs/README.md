# SekaiLink Runtime Packs

This directory is the Evolution-owned public repository for desktop runtime packs.

The client reads `index.json`, filters by `module_id` and `game_id`, then applies the
referenced incremental manifests into its local runtime overlay. When a manifest hash
matches the local cache, no archive is downloaded.

ALTTP is currently a showcase entry only. Pack loading remains data-driven so new
games can publish their own tracker pack manifests without changing client logic.
