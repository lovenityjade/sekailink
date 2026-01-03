# Environment Setup Guide

## Quick Start

1. **Copy the template:**
   ```bash
   cp .env.template .env
   ```

2. **Edit `.env` with your actual values** (never commit this file!)

3. **Verify `.env` is in `.gitignore`** (already done!)

## Required Variables Setup

### 1. Flask Secret Key
Generate a secure random key:
```bash
python -c "import secrets; print(secrets.token_hex(32))"
```
Copy the output to `FLASK_SECRET_KEY` in your `.env` file.

### 2. Database Passwords
Create strong passwords for:
- `POSTGRES_PASSWORD` - Your PostgreSQL database password
- `REDIS_PASSWORD` - Your Redis password

**Tip:** Use a password generator or:
```bash
python -c "import secrets; print(secrets.token_urlsafe(32))"
```

### 3. Discord OAuth2

1. Go to [Discord Developer Portal](https://discord.com/developers/applications)
2. Click "New Application"
3. Name it "SekaiLink" (or your preferred name)
4. Go to "OAuth2" → "General"
5. Copy the **Client ID** → `DISCORD_CLIENT_ID`
6. Click "Reset Secret" and copy → `DISCORD_CLIENT_SECRET`
7. Add redirect URL: `http://localhost:5000/api/auth/callback` (update for production!)

### 4. Discord Bot Token

1. In the same Discord application
2. Go to "Bot" section
3. Click "Add Bot"
4. Click "Reset Token" and copy → `DISCORD_BOT_TOKEN`
5. Enable required intents:
   - Presence Intent
   - Server Members Intent
   - Message Content Intent

### 5. Discord Server ID

1. Enable Developer Mode in Discord (User Settings → Advanced → Developer Mode)
2. Right-click your Discord server → "Copy ID"
3. Paste into `DISCORD_GUILD_ID`

### 6. Twitch OAuth (Optional - for broadcasting)

1. Go to [Twitch Developer Console](https://dev.twitch.tv/console/apps)
2. Click "Register Your Application"
3. Name: "SekaiLink"
4. OAuth Redirect URL: `http://localhost:5000/api/twitch/callback`
5. Category: "Website Integration"
6. Copy **Client ID** → `TWITCH_CLIENT_ID`
7. Generate **Client Secret** → `TWITCH_CLIENT_SECRET`

### 7. Email/SMTP (Optional - for notifications)

For Gmail:
1. Enable 2-Factor Authentication
2. Generate an [App Password](https://myaccount.google.com/apppasswords)
3. Use the app password in `SMTP_PASSWORD`

## Production Deployment

When deploying to production, update these variables:

```bash
FLASK_ENV=production
FLASK_DEBUG=False
BASE_URL=https://sekailink.xyz
COOKIE_DOMAIN=.sekailink.xyz
DISCORD_REDIRECT_URI=https://sekailink.xyz/api/auth/callback
TWITCH_REDIRECT_URI=https://sekailink.xyz/api/twitch/callback
ALLOWED_ORIGINS=https://sekailink.xyz
```

## Security Checklist

- [ ] `.env` is listed in `.gitignore`
- [ ] All passwords are strong and unique
- [ ] `FLASK_SECRET_KEY` is randomly generated
- [ ] `FLASK_DEBUG=False` in production
- [ ] Discord/Twitch redirect URIs match your domain
- [ ] SMTP credentials are valid
- [ ] `.env` file permissions are restricted: `chmod 600 .env`

## Docker Compose

The `docker-compose.yml` will automatically use your `.env` file. Just run:

```bash
docker-compose up -d
```

## Troubleshooting

**Problem:** "Database connection failed"
- Check `POSTGRES_PASSWORD` matches in `.env` and `docker-compose.yml`
- Verify `POSTGRES_HOST` is `sekailink_db` (Docker service name)

**Problem:** "Discord OAuth error"
- Verify redirect URI matches exactly in Discord Developer Portal
- Check `DISCORD_CLIENT_ID` and `DISCORD_CLIENT_SECRET` are correct

**Problem:** "Redis connection refused"
- Check `REDIS_HOST` is `sekailink_redis` (Docker service name)
- Verify Redis container is running: `docker ps`

## Need Help?

- Check logs: `docker-compose logs -f`
- Discord: https://discord.gg/XvvcBxrRsk
- GitHub Issues: https://github.com/lovenityjade/sekailink/issues
