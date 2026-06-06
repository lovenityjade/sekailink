# 10 - Releases et CDN

## Releases

Commandes:

```text
release current
release list
release verify
release verify-cdn
release compare <a> <b>
release publish <manifest>
release rollback <version>
release schedule <version> <datetime>
release notes <version>
release audit <version>
```

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

