# SekaiLink AI Writer (VPS) — 2026-02-18

## Objectif
- Ajouter une IA légère sur le VPS pour:
  - brouillons d'annonces Discord
  - brouillons de changelog
  - brouillons FAQ
- Ajouter une mémoire sémantique persistante.
- Lier l'IA au bot Discord existant.

## Stack installée
- `llama.cpp` (`llama-server`) local sur le VPS
- Modèle: `Qwen2.5-3B-Instruct-Q4_K_M.gguf`
- API inference: `http://127.0.0.1:8096`
- Service bots: `http://127.0.0.1:8095`
- Mémoire sémantique:
  - moteur embeddings: `sentence-transformers/paraphrase-multilingual-MiniLM-L12-v2` (via `fastembed`)
  - DB: SQLite persistante
  - chemin: `/opt/sekailink-social-bots/data/sekailink-ai-memory.sqlite3`

## Services systemd
- `sekailink-llama.service`
- `sekailink-social-bots.service`

## Sécurité
- ports IA et bots en écoute locale seulement (`127.0.0.1`)
- clé API LLM dédiée (`/opt/sekailink-ai/run/api_key.txt`)
- clé contrôle bot (`SEKAILINK_BOT_CONTROL_API_KEY`) requise pour routes critiques
- ingestion docs exclut explicitement:
  - `XX-server-infos.md`
  - `XX-temp-tokens.md`

## Endpoints AI ajoutés
- `GET /ai/status`
- `POST /ai/memory/ingest`
- `POST /ai/draft/announcement`
- `POST /ai/draft/changelog`
- `POST /ai/draft/faq`

## Lien Discord
- Commande Discord admin rapide: `!sekai-ai <instruction>`
- Génération via API possible puis post Discord facultatif (`post_to_discord=true`).

## Ingestion mémoire effectuée
- source: `/opt/sekailink-ai/knowledge/sekailink-client-plan`
- source: `/opt/sekailink-ai/knowledge/sekailink-docs`
- état après ingestion: ~`815` chunks

## Vérification
- `systemctl is-active sekailink-llama sekailink-social-bots` => `active`
- `ss -ltnp` => `127.0.0.1:8096` et `127.0.0.1:8095`
