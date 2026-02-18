# SekaiLink Social Bots (Discord + Twitch)

Service autonome (hors client/serveur) pour hébergement VPS.

## Fonctions incluses (base fonctionnelle)

### Discord
- Annonces via API (`/discord/announce`)
- Synchronisation FAQ via API (`/discord/faq/sync`)
- Canaux vocaux temporaires:
  - auto-création quand un user rejoint le salon lobby configuré
  - nettoyage auto quand le salon temporaire devient vide
  - création manuelle via API (`/discord/temp-voice/create`)

### Twitch
- Base bot connectée au chat Twitch
- Commande `!sekai`
- Envoi de message via API (`/twitch/message`)
- Structure prête pour EventSub/fonctions avancées (à déterminer)

### Sekai AI Writer
- Génération de drafts:
  - annonces (`/ai/draft/announcement`)
  - changelog (`/ai/draft/changelog`)
  - FAQ (`/ai/draft/faq`)
- Mémoire sémantique persistante SQLite (embeddings) pour contexte projet
- Ingestion de docs/fichiers (`/ai/memory/ingest`)
- Commande Discord admin rapide: `!sekai-ai <instruction>`

## Installation

```bash
cd services/social-bots
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
```

Renseigner `.env` avec les secrets (tu peux partir de `sekailink-client-plan/XX-temp-tokens.md` sans committer les tokens).

## Lancer

```bash
cd services/social-bots
source .venv/bin/activate
./run.sh
```

Healthcheck:

```bash
curl http://127.0.0.1:8095/health
```

Status AI:

```bash
curl http://127.0.0.1:8095/ai/status \
  -H 'X-SekaiLink-Bot-Key: CHANGE_ME'
```

## API (auth)

Toutes les routes critiques demandent l'en-tête:
- `X-SekaiLink-Bot-Key: <SEKAILINK_BOT_CONTROL_API_KEY>`

### Annonce Discord

```bash
curl -X POST http://127.0.0.1:8095/discord/announce \
  -H 'Content-Type: application/json' \
  -H 'X-SekaiLink-Bot-Key: CHANGE_ME' \
  -d '{"title":"SekaiLink","message":"Nouveau room live."}'
```

### Sync FAQ Discord

```bash
curl -X POST http://127.0.0.1:8095/discord/faq/sync \
  -H 'Content-Type: application/json' \
  -H 'X-SekaiLink-Bot-Key: CHANGE_ME' \
  -d '{"entries":[{"question":"Comment joindre une room?","answer":"Utilise Join dans Room List."}]}'
```

### Message Twitch

```bash
curl -X POST http://127.0.0.1:8095/twitch/message \
  -H 'Content-Type: application/json' \
  -H 'X-SekaiLink-Bot-Key: CHANGE_ME' \
  -d '{"message":"SekaiLink est en ligne."}'
```

### Draft annonce IA

```bash
curl -X POST http://127.0.0.1:8095/ai/draft/announcement \
  -H 'Content-Type: application/json' \
  -H 'X-SekaiLink-Bot-Key: CHANGE_ME' \
  -d '{"request":"Annonce la maintenance de ce soir et les bénéfices joueurs.","language":"fr"}'
```

### Ingestion mémoire IA

```bash
curl -X POST http://127.0.0.1:8095/ai/memory/ingest \
  -H 'Content-Type: application/json' \
  -H 'X-SekaiLink-Bot-Key: CHANGE_ME' \
  -d '{"directory":"/opt/multiworldgg/sekailink-client-plan"}'
```

## Déploiement VPS (recommandé)
- exécuter en service `systemd`
- reverse proxy + firewall (accès API en privé)
- rotation de secrets et logs
