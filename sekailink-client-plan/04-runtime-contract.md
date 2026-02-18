# Runtime contract (schema)

But: une description stable et validee par jeu, utilisee par l'orchestrator.

Etat:
- Schema existe: `client/schema/game_runtime.schema.json`
- Manifests existants: `client/runtime/modules/*/manifest.json`
- Le mapping UI (patch ext -> module) est encore hardcode dans `client/app/src/pages/Lobby.tsx`

## 2 niveaux de contrat

### A) `manifest.json` (par module)
Source de verite runtime, versionnee dans le repo.

Proposition (superset) en pseudo-JSON:

```json
{
  "module_id": "pokemon_red_bizhawk",
  "runtime_version": 1,
  "game_id": "pokemon_red",
  "display_name": "Pokemon Red",

  "patch": {
    "modes": ["ap_patch"],
    "exts": [".apred"],
    "accepts": {
      "content_types": ["application/octet-stream"],
      "filename_regex": "\\.apred$"
    }
  },

  "required_assets": {
    "rom": {
      "extensions": [".gb"],
      "md5": ["3d45c1ee9abd5738df46d2bdda8b57dc"],
      "sha1": []
    },
    "bios": null,
    "keys": null,
    "firmware": null
  },

  "driver": {
    "family": "bizhawk",
    "preferred": "bundled",
    "args_template": ["--lua={lua}", "{rom}"]
  },

  "connector": {
    "type": "bizhawk_lua",
    "path": "lua/connector.lua",
    "protocol_version": 1
  },

  "tracking": {
    "poptracker": {
      "pack_source": "github_release",
      "pack_repo": "palex00/rb_tracker",
      "variants": ["default"],
      "default_variant": "default"
    },
    "universal_tracker": {
      "supported": false,
      "flags": []
    }
  },

  "features": {
    "autotracking": true,
    "save_required": true,
    "multi_patch_per_player": false
  }
}
```

Notes:
- Les manifests actuels sont un sous-ensemble (ex: `rom_requirements`, `lua_connector`, `tracker_pack_uid`).
- Le but est d'avoir un contrat stable pour resoudre "quoi faire" sans code hardcode dans l'UI.

### B) Registry runtime (genere)
- Etat: `client/registry/games.generated.json` existe deja (listing de jeux pour l'UI)
- Besoin: trouver moduleId par extension patch
- Besoin: savoir quel driver/family utiliser
- Besoin: savoir quels assets demander

Suggestion:
- `client/registry/runtime.generated.json`

## Validation
- Validation CI: JSON schema + sanity checks (exts uniques, manifests coherents)
- Validation runtime: check prereqs avant d'activer le bouton "Play"

## Resolution patch -> module (PatchResolver)
But: enlever le hardcode de `resolveModuleId()`.

Contract propose:
1. Input: `downloadUrl` (du webhost)
2. Determiner si c'est un patch `.ap*` ou un slot file
3. Resoudre l'extension et trouver le module correspondant
4. Retourner `{kind, moduleId, downloadUrl, filename, contentType}`

Regle importante:
- `WebHostLib/api/room.py` renvoie parfois `download_slot_file` (pas un patch `.ap*`).
- on doit supporter les 2 pour atteindre "tout ce qui est supporte".

