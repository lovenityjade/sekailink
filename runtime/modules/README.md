# Active Runtime Modules

La surface active de `host/runtime/modules/` ne doit plus contenir que les
modules runtime encore consideres comme hotes a evaluer pour `Sekaiemu`.

Etat actuel:

- `oot_soh`
- `sm64ex`

Tout le reste de l'ancienne surface `*_bizhawk` a ete deplace vers:

- `../../source-snapshots/transitional-runtime/modules-bizhawk/`

Regle:

- aucun module `*_bizhawk` ne doit revenir ici
- la reprise `libretro` et les hotes retenus doivent se faire depuis une
  surface explicite et propre
- si un futur module doit etre restaure, documenter d'abord pourquoi il doit
  vivre dans `host/runtime/modules/` plutot que rester en snapshot
