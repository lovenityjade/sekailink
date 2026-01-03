# Environment Validation System

## Overview

SekaiLink now implements **fail-fast environment validation** that checks all required configuration variables at startup. This prevents runtime errors and ensures the application is properly configured before accepting any requests.

## How It Works

### Startup Sequence

1. **Environment Validation** (FIRST - before anything else)
   - Checks all required environment variables are set
   - Checks that values are not empty/blank
   - If ANY required variable is missing → **Application exits immediately**
   - If validation passes → Continue to step 2

2. **Flask Initialization**
   - Only runs if validation passed
   - Safe to use environment variables without defaults
   - No risk of using hardcoded credentials

3. **Application Start**
   - All services start with validated configuration
   - Clear logs indicate successful startup

## What Gets Validated

### Required Variables (MUST be set)

These variables **must** be present in your `.env` file or the application will not start:

#### Flask API (`main.py`)
- `FLASK_SECRET_KEY` - Session encryption key
- `DATABASE_URL` - PostgreSQL connection string
- `POSTGRES_USER` - Database username
- `POSTGRES_PASSWORD` - Database password
- `POSTGRES_DB` - Database name
- `REDIS_URL` - Redis connection string
- `DISCORD_CLIENT_ID` - Discord OAuth2 client ID
- `DISCORD_CLIENT_SECRET` - Discord OAuth2 secret
- `DISCORD_REDIRECT_URI` - Discord callback URL

#### Celery Worker (`tasks.py`)
- `REDIS_URL` - Celery broker connection
- `DATABASE_URL` - Database access for tasks
- `ARCHIPELAGO_PATH` - Path to Archipelago installation

### Optional Variables (Warnings only)

These variables are optional but recommended:
- `TWITCH_CLIENT_ID` - Enables Twitch broadcasting feature
- `SMTP_HOST` - Enables email notifications
- `LOG_LEVEL` - Controls logging verbosity

If optional variables are missing, you'll see a **warning** in the logs, but the application will still start.

## Error Messages

### Example: Missing Required Variables

```
======================================================================
❌ CONFIGURATION ERROR: Missing required environment variables
======================================================================

Missing variables (not set in .env):
  - FLASK_SECRET_KEY: Flask secret key for session encryption
  - DISCORD_CLIENT_ID: Discord OAuth2 client ID
  - DISCORD_CLIENT_SECRET: Discord OAuth2 client secret

======================================================================
How to fix:
  1. Copy .env.template to .env:
     cp .env.template .env

  2. Edit .env and fill in all required values

  3. See ENV_SETUP.md for detailed setup instructions
======================================================================
```

### Example: Empty Variables

```
======================================================================
❌ CONFIGURATION ERROR: Missing required environment variables
======================================================================

Empty variables (set but blank in .env):
  - POSTGRES_PASSWORD: PostgreSQL password
  - REDIS_URL: Redis connection URL

======================================================================
How to fix:
  1. Copy .env.template to .env:
     cp .env.template .env

  2. Edit .env and fill in all required values

  3. See ENV_SETUP.md for detailed setup instructions
======================================================================
```

## Success Messages

When validation passes, you'll see logs like:

```
2025-01-15 10:30:45 - SekaiLink - INFO - ✅ Environment validation passed
2025-01-15 10:30:45 - SekaiLink - INFO - 🚀 Starting SekaiLink API (Environment: production)
```

For Celery workers:

```
2025-01-15 10:30:46 - SekaiLink.Celery - INFO - ✅ Celery environment validation passed
2025-01-15 10:30:46 - SekaiLink.Celery - INFO - 🚀 Celery worker initialized successfully
```

## Benefits

### 1. **Security**
- No hardcoded credentials in code
- Forces proper .env configuration
- Prevents accidental use of default passwords

### 2. **Developer Experience**
- Clear error messages tell you exactly what's wrong
- Fails immediately instead of mysterious runtime errors
- Easy to debug configuration issues

### 3. **Production Safety**
- Ensures all secrets are properly configured
- Prevents partial deployments with missing config
- Clear audit trail in logs

### 4. **Maintenance**
- Easy to add new required variables
- Self-documenting (error messages explain each variable)
- Consistent validation across all services

## Testing the Validation

### Test 1: Missing .env file

```bash
# Remove .env file
rm .env

# Try to start the application
docker-compose up api

# Expected: Application exits with clear error message
```

### Test 2: Empty variables

```bash
# Create .env with empty values
cat > .env << EOF
FLASK_SECRET_KEY=
DISCORD_CLIENT_ID=
EOF

# Try to start
docker-compose up api

# Expected: Error listing empty variables
```

### Test 3: Successful start

```bash
# Copy template and fill in values
cp .env.template .env
# Edit .env and add real values

# Start application
docker-compose up -d

# Check logs
docker-compose logs api

# Expected: ✅ Environment validation passed
```

## Troubleshooting

### "ConfigurationError" when starting

**Cause:** Required environment variables are missing or empty

**Solution:**
1. Check `.env` file exists: `ls -la .env`
2. Verify variables are set: `cat .env | grep FLASK_SECRET_KEY`
3. Ensure no empty values: `VARIABLE=` is invalid
4. Follow `ENV_SETUP.md` for proper configuration

### Docker container keeps restarting

**Cause:** Environment validation failing on startup

**Solution:**
```bash
# Check container logs
docker-compose logs api

# Look for the validation error message
# Fix the missing variables in .env
# Restart containers
docker-compose restart
```

### "Variable not set" inside container

**Cause:** `.env` file not being loaded by Docker Compose

**Solution:**
1. Ensure `.env` is in the same directory as `docker-compose.yml`
2. Verify `docker-compose.yml` is properly formatted
3. Restart Docker Compose: `docker-compose down && docker-compose up -d`

## Adding New Required Variables

When adding new features that require environment variables:

1. **Add to `.env.template`** with description
2. **Add to validation function** in `main.py` or `tasks.py`:
   ```python
   required_vars = {
       'NEW_VARIABLE': 'Description of what this does',
   }
   ```
3. **Update `ENV_SETUP.md`** with setup instructions
4. **Remove any default values** from the code
5. **Test** that validation fails without the variable

## Migration from Old Code

The old code had dangerous defaults like:

```python
# ❌ OLD (INSECURE)
app.config['SECRET_KEY'] = os.getenv('SECRET_KEY', 'SkyNode@2BuildOn#')
```

New code validates first, then uses values safely:

```python
# ✅ NEW (SECURE)
validate_environment()  # Fails if FLASK_SECRET_KEY missing
app.config['SECRET_KEY'] = os.getenv('FLASK_SECRET_KEY')  # Safe - already validated
```

## Files Modified

- `backend/main.py` - Added `validate_environment()` function
- `backend/tasks.py` - Added `validate_celery_environment()` function
- `.env.template` - Comprehensive template with all variables
- `docker-compose.yml` - Uses environment variables, no hardcoded values
- `.gitignore` - Protects `.env` from being committed

## Summary

✅ **No more hardcoded credentials**
✅ **Fail fast with clear error messages**
✅ **Self-documenting error messages**
✅ **Consistent validation across all services**
✅ **Production-ready security**

If you see a configuration error, **it's intentional** - fix your `.env` file and the application will start successfully!
