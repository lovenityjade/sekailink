# 03 - Navigation TUI

## Layout par defaut

```text
┌ SekaiLink Core Access ─────────────────────────────────────────────┐
│ Nexus OK | Link WARN | Worlds OK | Evolution OK | Pulse DOWN       │
├ Matrix ──────────────┬ Live filtered logs ──────┬ Alerts/Jobs ─────┤
│ SERVER CPU RAM ERR   │ 22:41 room 38293 joined  │ approvals: 2     │
│ Link   38  64  12    │ 22:42 SKLMI reconnect    │ pack check: 14m  │
│ ...                  │ ...                      │ ...             │
├──────────────────────┴──────────────────────────┴─────────────────┤
│ skl> room logs pas-pour-certo --follow                             │
└────────────────────────────────────────────────────────────────────┘
```

## Command line

La ligne de commande est disponible partout.

Fonctions presentes dans le cockpit MVP:

- autocomplete par prefixe avec Tab;
- fleches gauche/droite;
- correction au milieu de ligne;
- historique avec fleche haut/bas;
- Ctrl+A/E;
- Ctrl+W;

Fonctions planifiees:

- selection de suggestion avancee;
- selection visuelle de plage dans les panels logs;
- Alt+B/F;
- reverse search;
- edition multi-line JSON;
- execution non bloquante de flux logs live.

## Hotkeys MVP

```text
F1  Help
F2  Server status
F3  Command registry
F4  Prepare note
F5  Refresh view
F6  Log catalog
F7  Audit search
F8  Prepare log tail
F9  Drop to shell mode
F10 Clear output
F11 Cycle workspace
F12 Panic view
```

`Ctrl+Q` ou `Ctrl+C` quitte le cockpit. Le mode `--shell` garde l'ancien prompt
ligne-par-ligne pour les sessions de logs longues.

## Tabs, splits et workspaces

- F11 change ou sauvegarde le layout.
- Un workspace debug capture les panels ouverts, filtres, pins et notes.
- Les workspaces sont lies a un incident, lobby, room, user ou release.

## Selection de logs

MVP actuel:

1. Chercher avec `logs search <query> [source|all]`.
2. Isoler avec `logs filter user:<id> lobby:<id> source:<source|all>`.
3. Epingler avec `logs pin <source> <text>`.
4. Ajouter une note avec `logs note <source> <text>`.
5. Exporter avec `logs export [query] --format md`.

Selection visuelle planifiee:

1. Placer le curseur sur le panel log.
2. Selectionner une plage.
3. F4 pour creer une note depuis la selection.
4. F7 pour exporter la selection.
