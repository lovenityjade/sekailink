# 01 - Concepts

## Core Access

Core Access est le cockpit d'exploitation de SekaiLink. Il ne remplace pas les
services; il les orchestre avec des droits, des confirmations et un audit
centralise.

## Bastion Nexus

L'outil est lance sur Nexus apres SSH. Le modele d'acces est volontairement en
deux etapes:

1. utilisateur Linux autorise sur le bastion;
2. utilisateur SekaiLink/Nexus avec role Admin ou Service.

Un mapping explicite associe le compte Linux au `user_id` SekaiLink attendu.

## Services

- Nexus: identite, utilisateurs, configs, audit, scheduler, approvals.
- Link: API publique, lobbies, chat, room admin passthrough, client signals.
- Worlds: generation et logs de generation.
- Evolution: distribution, CDN, packs, releases.
- Pulse: assistant/config helper et health.

## Ops commit

Chaque action mutante produit un ops commit dans Nexus. Un ops commit est une
unite atomique d'audit: commande, acteur, cible, raison, diff, logs lies,
resultat, erreur et rollback.

## Approval queue

Le role Service peut preparer certaines actions sensibles. L'action reste en
attente jusqu'a approbation Admin. L'approbation ou le refus est lui-meme audite.

## Maintenance mode

Maintenance mode est un etat central qui informe le client et bloque certaines
actions selon le scope: full, generation_only, cdn_only, read_only, lobby_only
ou client_update_only.

## Realtime client signals

Core Access peut envoyer des signaux aux clients SekaiLink: broadcast,
maintenance banner, force update prompt, refresh lobby/room, relogin, reconnect
SKLMI, diagnostics request.

Les diagnostics demandent le consentement de l'utilisateur.

