# Securite, vie privee, et logs (streamer-first)

But: documenter les risques et les regles de base pour ne pas exposer de secrets (passwords, tokens) et pour garder des logs utiles sans compromettre la vie privee, en particulier pour les streamers.

## Menaces et surface d'exposition

Menaces concretes:
- les arguments de ligne de commande (argv) peuvent etre lus localement (process list)
- les logs stdout/stderr peuvent etre captures ou affiches en overlay
- les fichiers de config peuvent etre partages par erreur (support, screenshots)
- OBS window capture peut capturer des infos sensibles (mot de passe, token)

Surfaces principales dans SekaiLink:
- CommonClient wrapper (connect AP)
- PopTracker (autoconnect AP)
- logs de room via Socket.IO
- fichier `~/.sekailink/config.json`

## Regles de base

Secrets:
- ne jamais ecrire le password AP sur disque
- ne jamais passer le password AP en argv en mode streamer
- preferer stdin, env var, ou fichier temporaire chmod 600

Logs:
- redacter les secrets avant d'afficher en UI
- redacter les URLs contenant des tokens
- limiter la retention par defaut (ring buffer)

UI streamer mode:
- masquer les champs password par defaut
- ne pas afficher les paths sensibles (keys, bios) sauf en mode debug

## Etat actuel (risque)

Dans l'implementation actuelle (apres ajustements):
- CommonClient wrapper:
- le process python est demarre sans password dans argv
- le password est envoye via stdin JSON (commande `connect`) uniquement
- PopTracker:
- le password n'est plus passe via `--ap-pass` (argv)
- SekaiLink exporte le secret via env var et passe seulement `--ap-pass-env SEKAILINK_AP_PASS`

Risque residuel:
- le password existe toujours en memoire (Electron main + process child) et transite sur stdin JSON
- la variable d'environnement existe dans l'environnement du process PopTracker uniquement (pas globale), mais peut apparaitre dans certains crash dumps/outils de debug avancés

## Recommandation (a implementer plus tard)

CommonClient wrapper:
- deja fait: password via stdin (IPC JSON) uniquement
- a renforcer: redaction systematique dans les logs UI (si jamais un message l'inclut)

PopTracker:
- fait: `--ap-pass-env <ENVVAR>` + fallback env `SEKAILINK_AP_PASS` / `SKL_AP_PASS`
- a considerer plus tard: `--ap-pass-stdin` (encore mieux que env)

Electron:
- streamer mode active:
- interdire `--ap-pass` en argv (deja fait cote launcher SekaiLink)
- activer redaction logs

## Logs: quoi conserver et ou

Sources:
- logs serveur room: Socket.IO `terminal_output` (voir `sekailink-client-plan/10-server-apis-and-logs.md`)
- logs client bridge: events `commonclient:event`
- logs runtimes (emulateurs): best-effort (souvent non capture)

Stockage propose:
- `userData/logs/sessions/<session_id>.log`
- rotation (taille max)
- export manuel via UI

Format:
- JSON lines (facile a parser)
- champs:
- `ts`
- `session_id`
- `source`: `server_terminal`, `commonclient`, `tracker`, `runtime`
- `level`
- `message` (redacted)

## Confidentialite: paths et dumps

Ne pas leak:
- chemins BIOS/keys/firmware
- fichiers de ROM patchees
- contenu de ROM

Support:
- ajouter un bouton "Create Support Bundle" qui:
- inclut logs redacted
- inclut versions runtimes
- n'inclut pas de ROM/BIOS/keys
