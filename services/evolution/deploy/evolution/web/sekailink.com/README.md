# SekaiLink Website Package

This package is the first uploadable public website for `sekailink.com`.

Contents:

- `public/index.php`
- `app/config.php`
- `apache/sekailink.com.conf`
- copied static assets under `public/assets/`
- copied landing CSS/JS under `public/css/` and `public/js/`

## Upload target

Suggested layout on `evolution`:

```text
/var/www/sekailink.com/
  app/
  public/
```

Suggested Apache vhost:

- `apache/sekailink.com.conf`

## PHP config

The site reads defaults from `app/config.php`.

You can override the key links with environment variables:

- `SEKAILINK_BASE_URL`
- `SEKAILINK_DISCORD_URL`
- `SEKAILINK_DOWNLOAD_URL`
- `SEKAILINK_ROOM_BROWSER_URL`
- `SEKAILINK_LOGIN_URL`
- `SEKAILINK_REGISTER_URL`
- `SEKAILINK_SUPPORT_EMAIL`

## Notes

- This package is intentionally simple and upload-first.
- It does not depend on `WebHost`.
- The provided vhost is HTTP-first and minimum viable.
- Room/API reverse-proxy paths should be added only when the corresponding native services are publicly routed and validated.
