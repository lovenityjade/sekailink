# Host Runtime Surface

La surface active restante dans `host/runtime/` doit tendre vers un runtime
propre `Sekaiemu`, sans wrappers `BizHawk` historiques.

Point de lecture recommande:

1. `../../docs/ACTIVE_SURFACES.md`
2. `native/sekailink_runtime/`
3. `modules/README.md`

Les wrappers Python et ponts `BizHawk/SNI` evidents ont ete retires vers:

- `source-snapshots/transitional-runtime/`

Exception active BETA-3: `patcher_wrapper.py` est conserve comme adaptateur
generique de patch `.ap*` vers le runtime AP/MWGG prive embarque par
SekaiLink. Ce n'est pas un retour au client Python legacy: l'utilisateur
n'installe pas Python, les updates runtime sont desactivees pendant le patch,
et l'adaptateur importe seulement le world module declare par le manifest avant
de deleguer a `Patch.create_rom_file`.

Contrainte Windows: le runtime joueur doit etre autonome. `tools/python/portable-win`
embarque CPython, et `tools/python/wheelhouse/win-amd64-cp312` embarque les
wheels necessaires au patcher. En build Windows package, Core doit utiliser
`pip --no-index --find-links` et refuser de continuer si cette wheelhouse manque,
plutot que de demander Python ou Internet au joueur.

Le bundle AP/MWGG actif est stage dans `ap/` par
`client-app/scripts/stage-ap-runtime.mjs`. Il contient les fichiers AP/MWGG
necessaires au patching (`Patch.py`, `BaseClasses.py`, `worlds/Files.py`,
`rule_builder/`, `data/`, et les worlds valides Pre-BETA3). Core resout ce
bundle depuis `runtime/ap`; `SEKAILINK_AP_ROOT` est un override de developpement
ou de diagnostic explicite, pas une dependance joueur.

Le reste de cette surface n'est pas considere comme validation finale du design;
c'est la surface minimale gardee active apres premier tri.

Regle de navigation:

- si un besoin renvoie vers un wrapper Python ou un module `*_bizhawk`, partir
  d'abord du principe qu'il faut le traiter comme reference archivee
