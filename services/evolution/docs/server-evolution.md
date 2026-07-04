# Evolution Server

Date: 2026-03-26

## Role

`evolution` is the web and mail host.

It owns:
- Apache2 + PHP website
- `sekailink.com`
- public web pages and portal surface
- mail transport for account flows
- reverse-proxy cooperation with `link`

## Logical layout

```text
/opt/sekailink/evolution/
  web/
  mail/
  logs/
```

## Core responsibilities

- website and account portal
- PHP web surface for `sekailink.com`
- mail transport for password reset / email verification
- no longer the long-term target for central DB/API

## Important rule

`evolution` is no longer the target database host in the updated topology.

## Uploadable website package

An uploadable Apache2 + PHP website package now exists in:

- `deploy/evolution/web/sekailink.com/`

This is the first public-site package intended to be copied directly onto `evolution`.

## Migration note

Native identity and room-query slices were first brought up on `evolution` during the transition.

The updated target topology moves those DB/API roles onto `Nexus`.

Until cutover is complete, `evolution` can still temporarily host those services, but they are no longer the destination architecture.

Cutover has now been executed:

- public `/api/identity/*` and `/api/room/*` now terminate on `Nexus` behind `link`
- the old transition services on `evolution` have been disabled:
  - `sekailink-identity.service`
  - `sekailink-evolution-room-query.service`
- the old native runtime trees have now also been archived locally under:
  - `/opt/sekailink/evolution/_retired-api/`
  to preserve rollback/reference data without keeping those slices in the active live tree

## Current public path

Public `https://sekailink.com` traffic still lands on `link` first.

Current arrangement:

- `link` serves as the stable public entrypoint
- `evolution` serves the website
- backend API hosting is moving to `Nexus`

## Mail transport note

`evolution` keeps the mail role:

- Postfix/sendmail
- password reset delivery
- email verification delivery

That mail role stays here for the current infrastructure split.

That mail slice has now advanced again:

- `evolution` now uses the OVH `link` host as its internal SMTP relay
- current outbound path is:
  - native identity service -> `/usr/sbin/sendmail` on `evolution`
  - local Postfix on `evolution`
  - relay to `link` over SMTP port `25`
  - outbound Internet delivery from `link`
- current key settings on `evolution` include:
  - `relayhost = [167.114.3.84]:25`
  - `smtp_sasl_auth_enable = no`
  - `smtp_tls_wrappermode = no`
  - `smtp_tls_security_level = may`
  - `smtp_generic_maps = hash:/etc/postfix/generic`
- current key settings on `link` include:
  - `myhostname = link.sekailink.com`
  - `myorigin = sekailink.com`
  - `mynetworks` explicitly allows `66.102.139.114/32`
  - generic sender rewriting for local service users

Operational note:

- this path has been validated with real acceptance from `evolution` to `link`
- `link` has also been validated with successful upstream `status=sent` delivery
- one older queue entry is still present on `evolution` and should be cleaned up deliberately
- longer-term deliverability still depends on DNS reputation and SPF/DKIM/DMARC tuning

## Native admin-agent deployment

The native admin agent is now also deployed on `evolution` as a private loopback service.

Current runtime path:

- binary: `/opt/sekailink/evolution/admin-agent/bin/sekailink_admin_agent_service`
- config: `/opt/sekailink/evolution/admin-agent/config/admin_agent.json`
- systemd: `sekailink-evolution-admin-agent.service`

Current role:

- operational visibility into the native `evolution` services
- private `/health`
- private `/services`
- private `/services/{name}`

That private ops surface now also includes:

- native identity `state_file` visibility
- recent postfix activity through a lightweight tail snapshot
- native room-query `state_file` visibility
- structured postfix queue visibility through `postfix-queue`

The native `evolution` room-query service now also writes a structured runtime state file.

Current state payload includes:

- `http_port`
- `projection_backend`
- `projection_target`
- `mysql` summary when MySQL-backed
- `total_requests`
- `total_errors`
- `updated_at`

Current runtime path for this state file:

- `/opt/sekailink/evolution/api-gateway/data/evolution_room_query_state.json`

The mail ops slice now also includes a structured queue snapshot:

- snapshot path:
  - `/opt/sekailink/evolution/admin-agent/data/postfix_queue_state.json`
- update mechanism:
  - `sekailink-postfix-queue-state.service`
  - `sekailink-postfix-queue-state.timer`
