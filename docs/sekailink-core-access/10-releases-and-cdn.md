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
pack repo add
pack repo edit <id>
pack repo disable <id>
pack repo delete <id>
pack check <id>
pack validate <id>
pack stage <id>
pack publish <id>
pack rollback <id> <version>
pack schedule-check <id> <cron|interval>
```
