# PRIVATE SERVER INFO (REDACTED FOR GIT)

This file is intentionally redacted and can be committed safely.

Use a local private copy (not committed) for:
- SSH credentials
- Email/SMTP credentials
- API secrets
- OAuth secrets

## Public / non-secret operational facts
- Primary domain: `sekailink.com`
- Admin domain: `admin.sekailink.com`
- Web stack: Apache2 reverse proxy + Gunicorn WebHost
- Host OS target: Ubuntu 25.04 (planned migration to LTS recommended)
- Main app path: `/opt/multiworldgg`

## Runtime services (expected)
- `multiworldgg-webhost.service`
- `multiworldgg-workers.service`
- `sekailink-social-bots.service`
- `sekailink-llama.service`
- `webmin.service`

## Notes
- Keep real credentials only in secure vault / local encrypted notes.
- Never commit tokens/passwords to Git.
