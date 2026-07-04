# Patch Status

Le patching canonique `ALTTP` doit etre redeclare ici.

Source active de connaissance:

- `sekaiemu/worlds/alttp/Rom.py`
- `sekaiemu/worlds/alttp/Options.py`
- `sekaiemu/worlds/alttp/__init__.py`

Canonique attendu ici:

- `patch.manifest.json`
- futurs schemas ou notes de validation si necessaire

Etat reel:

- le contrat ALTTP declare maintenant `patch.schema_version=sekailink-patch-contract-v1`
- `patch.mode=artifact` indique que Worlds doit produire un artifact par slot
- l'artifact ALTTP attendu reste un `.aplttp` compatible `APDeltaPatch`
- les validations ROM sont explicites: base ROM Japan 1.0, MD5
  `03a63945398191337e896e5771f77173`
- le contrat declare `server_dispatch.enabled=false`: ALTTP n'est pas un mode
  server-port/no-patch
- le contrat declare le futur host d'application `sekaiemu`, avec `--patch`,
  `--base-rom`, sortie `.sfc`, et naming `patched/{patch_stem}.sfc`
- l'acceptance native `.aplttp` est detaillee dans
  `clean-room/repos/sekailink-worlds/docs/ALTTP_NATIVE_PATCHER_ACCEPTANCE.md`
- le vecteur final seed package -> materialisation `.aplttp` -> lancement
  `Sekaiemu` est detaille dans
  `clean-room/repos/sekailink-worlds/docs/ALTTP_PATCH_RUNTIME_TEST_READINESS.md`
- un chemin `patch_artifact` peut exister comme placeholder de package, mais les
  bytes `.aplttp` doivent exister avant materialisation runtime
- la procedure historique reste conservee dans `patch_contract.procedure` pour
  alignement avec les sources amont

Relation avec les jeux sans patch:

- ALTTP n'est pas un modele special dans Worlds; c'est seulement un
  `patch.mode=artifact`
- les server-ports/no-patch comme Twilight Princess ou Ship of Harkinian doivent
  declarer leur propre contrat avec `patch.mode=none`, `server_dispatch`, ou
  `contract_only`
- le package seed reste unique pour toute la room, avec un contrat patch par
  slot, meme quand aucun fichier n'est produit pour ce slot

Travail suivant:

- relier les options de patch aux presets quand ils existeront
- ajouter un schema partage `sekailink-patch-contract-v1` quand le repo de
  contrats communs sera pret
- ajouter une fixture server-port/no-patch cote Worlds pour tester que l'absence
  de fichier sous `patches/` n'est pas une erreur si le mode le declare
