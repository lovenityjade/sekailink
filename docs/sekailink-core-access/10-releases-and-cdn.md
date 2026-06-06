# 10 - Releases et CDN

## Releases

Commandes:

```text
release current
release list
release verify [latest|version|date|manifest] [--fast]
release verify-cdn [channel] [platform|all] [--execute]
release compare <a> <b>
release publish <manifest|version|date> --confirm release:<version>:publish
release rollback <version> --confirm release:<version>:rollback
release schedule <manifest|version|date> <datetime> --confirm release:<version>:schedule
release notes [version|date|manifest]
release audit [version|date|manifest]
```

Etat Core Access actuel:

- `release current` lit le dernier manifeste local sous
  `apps/client-core/release/update-bundles/YYYYMMDD`;
- `release list` deduplique les manifestes `client_release_manifest.fragment.json`
  et `sekailink-client-release-YYYYMMDD.json`;
- `release verify` verifie existence, taille et SHA-256 des bundles locaux;
- `release verify --fast` saute le SHA-256 pour une verification rapide;
- `release verify-cdn` affiche les probes publics `release-latest`; `--execute`
  appelle seulement l'API publique, sans token;
- `release compare` compare version, channel, build, base URL, artefacts,
  tailles et hash;
- `release publish`, `release rollback` et `release schedule` creent des drafts
  locaux audites, avec confirmation exacte. Ils ne modifient pas le CDN, ne
  remplacent pas `/opt/sekailink/link/chat-api/config/client_release_latest.json`
  et ne redemarrent pas `sekailink-chat-api.service`.

## Validation

Une release valide montre:

- version;
- platform;
- update bundle URL;
- fallback URL;
- SHA;
- taille;
- date upload;
- manifest backup disponible.

## Trois bannieres Client Core

Trois slots fixes en haut du dashboard Client Core:

```text
client-banner list
client-banner edit <1|2|3>
client-banner preview <1|2|3>
client-banner publish <1|2|3>
client-banner rollback <1|2|3>
client-banner disable <1|2|3>
```

Champs: title, body, type, action label, action URL/deeplink, color/accent,
icon, enabled, starts_at, ends_at, priority.

## Packs CDN

Pipeline obligatoire:

1. check;
2. validate;
3. stage;
4. publish.

Commandes:

```text
pack repo list
pack repo add <id> <url> <game> [notes] --confirm pack-repo:<id>:add
pack repo edit <id> key=value [key=value...] --confirm pack-repo:<id>:edit
pack repo disable <id> [reason] --confirm pack-repo:<id>:disable
pack repo delete <id> [reason] --confirm pack-repo:<id>:delete
pack check <id> [notes]
pack validate <id> [notes]
pack stage <id> [notes] --confirm pack:<id>:stage
pack publish <id> [notes] --confirm pack:<id>:publish
pack rollback <id> <version> --confirm pack:<id>:rollback:<version>
pack schedule-check <id> <cron|interval> --confirm pack:<id>:schedule-check
```

Etat actuel:

- les commandes `pack repo add/edit/disable/delete` creent des drafts locaux
  audites;
- `pack check` et `pack validate` creent des drafts Service sans fetch reseau;
- `pack stage`, `pack publish`, `pack rollback` et `pack schedule-check`
  creent des drafts Admin avec confirmation exacte;
- aucun pack n'est telecharge, aucun artifact CDN n'est stage/publie, aucune
  logique Lua n'est executee, et aucun comportement SKLMI/Sekaiemu n'est
  modifie par cette tranche.
