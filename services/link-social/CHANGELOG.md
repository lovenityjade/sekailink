# Changelog

## 2026-06-26

- Deployed the live `sekailink-chat-api` reset-generation route for Client Core.
- The route is `POST /api/lobbies/:lobby_id/generation/reset`.
- Reset only clears failed generation states and refuses running or successful generations.
- Live backup before deploy:
  `/opt/sekailink/link/chat-api/backups/sekailink_chat_api_service.before-reset-route-20260626T024350Z`.
- Verified the live service restarted successfully and the deployed binary contains the `generation_reset_refused` reset-route guard.
