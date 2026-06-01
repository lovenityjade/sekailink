# Evolution Native Identity Service

This directory contains the first deployment artifacts for the native `evolution` identity service.

Files:

- `identity_service.json.example`
- `sekailink-identity.service`

The service is intended to stay loopback-only initially on `evolution`.

Password recovery is implemented as a one-time reset token flow:

- `POST /password-recovery/request`
- `POST /password-recovery/complete`

Passwords are never emailed. The service sends a reset link through the configured `sendmail` transport, and the final password change happens through the API.

Patreon account linking is also supported through the native identity API:

- `GET /me/linked-accounts/patreon`
- `POST /me/linked-accounts/patreon/begin`
- `POST /me/linked-accounts/patreon/complete`
- `POST /me/linked-accounts/patreon/unlink`

The identity database stores Patreon linkage metadata and the current Patreon tier on the user record.
