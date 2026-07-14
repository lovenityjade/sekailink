# Email-shaped username exposure - 2026-07-13

## Impact

One account had an email address stored as its canonical username. The value was then eligible to appear as the public owner name of a lobby and in Link presence/profile projections.

## Containment

- Closed the affected public test lobby and the related lobby requested by the administrator.
- Stopped both associated Archipelago room-server processes.
- Replaced the affected canonical username with the account's existing public display name.
- Revoked the account's active sessions so cached identity payloads cannot persist.
- Removed the email-shaped username from Link lobby, presence, profile, and player-state projections.

## Correction

- Nexus rejects registration and administrative account creation when the username contains `@` or equals the account email.
- Link public identity helpers never fall back to an email address for usernames, display names, chat authors, or lobby owners.
- The website registration form rejects email-shaped public usernames before submission.

## Verification

- Nexus identity smoke test passes with an explicit rejected `username=email` case.
- Link Chat API smoke test passes after the public identity fallback changes.
- The live Nexus registration endpoint returns HTTP 400 with `username_must_not_be_email` for the regression case.
- No remaining email-shaped usernames were found in Nexus users or the checked Link public identity projections.
- Both targeted lobbies are closed and neither associated room-server process remains active.

## Follow-up

- Keep authentication by username or email. Authentication input must never be reused as a public identity field.
- Add an administrative privacy audit that flags email-shaped values in every public identity projection.
- The website no longer performs a redundant immediate `/me` request after a successful registration. That second request could surface a transient `Session unauthorized` message even though the account and session had been created correctly.

## Mail delivery correction

- Verification tokens are issued for 3600 seconds. Nexus now enforces one hour as the minimum even if a future configuration specifies less.
- Nexus now passes `noreply@sekailink.com` to Sendmail as the SMTP envelope sender instead of relying only on the visible `From` header.
- Link rewrites the former Nexus service sender to `noreply@sekailink.com` for compatibility with messages already entering the relay.
- Nexus is trusted by OpenDKIM on Link so relayed SekaiLink messages are signed.
- Link outbound SMTP is restricted to IPv4, which is covered by the published SekaiLink SPF record.
- A live verification resend to Gmail was accepted with SMTP status `250 2.0.0` after the correction.
