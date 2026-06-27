# SekaiLink Pulse Console

Small internal web console for `pulse.sekailink.com`.

Purpose:

- challenge Pulse with product/config/randomizer questions;
- ask APWorld/RAG questions through the existing Pulse RAG service;
- keep server-side JSONL audit logs;
- avoid exposing llama.cpp API keys to browsers.

Runtime on Pulse:

- app root: `/opt/sekailink/pulse-console`
- service: `sekailink-pulse-console.service`
- loopback app: `127.0.0.1:18183`
- public proxy: nginx `https://pulse.sekailink.com`
- audit logs: `/opt/sekailink/pulse-console/logs/pulse-console-YYYY-MM-DD.jsonl`
- auth file: `/opt/sekailink/pulse-console/etc/auth.json`

The console is intentionally narrow. It refuses secrets, SSH, deployment, and
server administration prompts. For APWorld mode, it forwards questions to
`sekailink-pulse-rag-api.service`; for general challenge/debug mode, it calls
the local llama.cpp server from the backend only.

Web verification:

- automatic web assist is enabled when `PULSE_AUTO_WEB_SEARCH` is not false;
- Serper API key is read from `PULSE_SERPER_API_KEY` or
  `PULSE_SERPER_API_KEY_FILE`;
- default key path: `/opt/sekailink/pulse-console/etc/serper_api_key`;
- web context is injected server-side only, and responses include consulted
  source URLs when search was used.

Deployment notes:

- nginx is installed on Pulse and redirects HTTP to HTTPS.
- Let's Encrypt is active for `pulse.sekailink.com`; renewal is handled by
  `certbot-renew.timer`.
- The backend never exposes the llama.cpp API key to the browser.
