# ALTTP External Test Checklist

Ce document sert de checklist d'exploitation pour une session de test temps reel
ALTTP menee par une personne qui n'est pas dans l'equipe de dev.

## Cible minimale

La session de test est acceptable si les points suivants sont tous vrais:

- `Sekaiemu` ouvre bien le patch `.aplttp`
- la ROM source `Japan 1.0` est acceptee
- le tracker s'affiche en side-by-side
- `SKLMI` se connecte a la room
- au moins une metadata seed/room/slot visible atteint le tracker
- au moins un evenement gameplay visible (`location_checked` ou `item_received`)
  est observe sur la session
- les logs sont trouvables sans fouille manuelle profonde

## Verifications avant envoi a un testeur

1. Patch
   - un `.aplttp` valide existe
   - la ROM source correspond bien au MD5 attendu

2. Bundle tracker
   - `tracker/default.zip` existe au moment de la distribution test
   - `config/default.yaml` existe au moment de la distribution test
   - la surface cible checks/items/settings est lisible dans
     `docs/TRACKER_RUN_COMPLETE_SURFACE.md`

3. Room metadata
   - `room.state` ou, a defaut, `trace.jsonl` expose au moins:
     - `seed_id`
     - `seed_hash`
     - `slot_id` ou `slot_name`
     - `tracker_pack`
     - `tracker_variant`

4. Logs
   - un log `Sekaiemu`
   - un log ou etat `SKLMI`
   - un flux room ou journal `Link Room`

## Ordre de diagnostic recommande

1. verifier `room.state`
2. verifier `trace.jsonl`
3. verifier le `tracker_state`
4. verifier les events room
5. verifier les injections/acks

## Cas acceptable meme si `room.state` est pauvre

Si `room.state` ne contient encore que:

- `meta|connected|1`

la session peut quand meme rester exploitable pour le tracker si `Sekaiemu`
recupere l'identite de la session depuis `trace.jsonl`:

- `room_client_ready`
- `room_metadata_ready`
- `slot_connected`

Ce n'est pas l'etat final ideal, mais c'est un chemin de test acceptable tant
que les metadata critiques deviennent visibles cote tracker.

## Ce qui invalide la session test

- tracker absent ou non visible
- image jeu etiree en fullscreen
- aucune metadata seed/room/slot visible
- aucun log exploitable
- aucun evenement gameplay visible
- bundle tracker ou YAML manquants

## Verdict a remonter

Une session test doit etre classee dans une seule categorie:

- `passable pour test externe`
- `encore reservee dev`

Si la session repose encore sur des traces partielles, elle reste
`passable pour test externe` seulement si le tracker montre quand meme une
identite seed/room/slot lisible et qu'au moins un evenement gameplay remonte.
