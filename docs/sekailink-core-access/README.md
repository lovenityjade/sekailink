# SekaiLink Core Access

SekaiLink Core Access est le cockpit d'administration de SekaiLink. Il est pense
comme un outil TUI Rust lance depuis le bastion Nexus apres une connexion SSH,
puis un login SekaiLink/Nexus.

Le nom Core Access evite la confusion avec Client Core et les libretro cores de
Sekaiemu.

## Objectifs

- Donner aux Admins et au role Service un environnement de travail unique.
- Centraliser logs, lobbies, rooms, releases, packs CDN, bots, scheduler,
  maintenance, notes et audit.
- Garder Nexus comme source d'identite, de droits, d'audit et de documentation
  operationnelle.
- Former les nouveaux moderateurs avec des guides imprimables et une reference
  de commandes complete.

## Table des matieres

- [Concepts](01-concepts.md)
- [Premier login](02-first-login.md)
- [Navigation TUI](03-tui-navigation.md)
- [Roles et permissions](04-roles-and-permissions.md)
- [Logs et debugging](05-logs-and-debugging.md)
- [Users et configs](06-users-and-configs.md)
- [Lobbies et rooms](07-lobbies-and-rooms.md)
- [Signaux clients](08-client-signals.md)
- [Maintenance mode](09-maintenance-mode.md)
- [Releases et CDN](10-releases-and-cdn.md)
- [Scheduler](11-scheduler.md)
- [Bots Discord/Twitch](12-bots-discord-twitch.md)
- [Cleanup et backups](13-cleanup-and-backups.md)
- [Playbooks incidents](14-incident-playbooks.md)
- [Reference commandes](15-command-reference.md)
- [Formation moderateurs](16-training-moderators.md)
- [Runbook Admin](17-admin-runbook.md)
- [Glossaire](glossary.md)
- [Change control](change-control/README.md)

## Regles fortes

- Aucun secret reel dans les docs, PDF ou exemples.
- Toute modification future doit etre documentee dans le meme commit que le code.
- Aucune modification SKLMI sans accord explicite ecrit de Jade.
- Les changements de connectivite Client Core, Sekaiemu ou SKLMI exigent un
  contrat documente.
- Les actions dangereuses demandent une cible tapee, une raison et un audit.

## Generation PDF

Les PDF imprimables sont produits dans `dist/pdf/`:

- `sekailink-core-access-full.pdf`
- `sekailink-core-access-service-training.pdf`
- `sekailink-core-access-command-reference.pdf`
- `sekailink-core-access-incident-playbooks.pdf`
- `sekailink-core-access-quick-reference.pdf`

Commande prevue:

```sh
bash docs/sekailink-core-access/scripts/build-pdf.sh
```

La pipeline utilise Pandoc avec XeLaTeX. Le script signale clairement les
dependances manquantes.

## Executable

Le binaire local vit dans `tools/core-access/`. Il lance par defaut un cockpit
terminal plein ecran avec panels, hotkeys, ligne de commande, autocompletion,
historique, notes, approvals locales, audit JSONL et dashboard local.

Le shell ligne-par-ligne historique reste disponible avec `--shell`.

Commandes de base:

```sh
cargo run --manifest-path tools/core-access/Cargo.toml
cargo run --manifest-path tools/core-access/Cargo.toml -- --shell
cargo run --manifest-path tools/core-access/Cargo.toml -- --command "server status all"
```
