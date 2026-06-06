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

Fonctions requises:

- autocomplete contextuelle;
- selection de suggestion avec Tab;
- fleches gauche/droite;
- correction au milieu de ligne;
- historique avec fleche haut/bas;
- Ctrl+A/E;
- Alt+B/F;
- Ctrl+W;
- reverse search;
- edition multi-line JSON.

## Tabs, splits et workspaces

- F11 change ou sauvegarde le layout.
- Un workspace debug capture les panels ouverts, filtres, pins et notes.
- Les workspaces sont lies a un incident, lobby, room, user ou release.

## Selection de logs

1. Placer le curseur sur le panel log.
2. Selectionner une plage.
3. F4 pour creer une note.
4. F7 pour exporter.

