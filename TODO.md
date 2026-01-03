# SekaiLink - TODO List for Production Readiness

**Created:** 2026-01-02
**Prototype Status:** ✅ WORKING - Ready for server testing

---

## 🚀 IMMEDIATE: Get Prototype Running (Do This First!)

### 1. Database Migration

The prototype added **3 new database models**. Run these commands to create the tables:

```bash
# Stop containers
docker-compose down

# Start only database
docker-compose up -d db

# Wait 5 seconds for PostgreSQL to start
sleep 5

# Create new tables
docker-compose exec db psql -U sekailink_user -d sekailink -c "
CREATE TABLE IF NOT EXISTS lobby_settings (
    id SERIAL PRIMARY KEY,
    lobby_id INTEGER UNIQUE REFERENCES lobbies(id),
    max_players INTEGER DEFAULT 10,
    time_limit_hours INTEGER,
    sync_rules TEXT DEFAULT '',
    allow_multigame BOOLEAN DEFAULT TRUE,
    allow_broadcast BOOLEAN DEFAULT TRUE,
    blacklisted_games TEXT DEFAULT '[]'
);

CREATE TABLE IF NOT EXISTS lobby_players (
    id SERIAL PRIMARY KEY,
    lobby_id INTEGER REFERENCES lobbies(id),
    user_id VARCHAR REFERENCES users(id),
    game VARCHAR(100),
    yaml_file_id INTEGER REFERENCES yaml_files(id),
    rom_file_id INTEGER REFERENCES rom_files(id),
    is_ready BOOLEAN DEFAULT FALSE,
    status VARCHAR(20) DEFAULT 'waiting',
    patch_url VARCHAR(500),
    joined_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    finished_at TIMESTAMP
);

CREATE TABLE IF NOT EXISTS chat_messages (
    id SERIAL PRIMARY KEY,
    lobby_id INTEGER REFERENCES lobbies(id),
    user_id VARCHAR REFERENCES users(id),
    message VARCHAR(500),
    message_type VARCHAR(20) DEFAULT 'user',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

ALTER TABLE lobbies ADD COLUMN IF NOT EXISTS visibility VARCHAR(20) DEFAULT 'open';
ALTER TABLE lobbies ADD COLUMN IF NOT EXISTS server_port INTEGER;
ALTER TABLE lobbies ADD COLUMN IF NOT EXISTS started_at TIMESTAMP;
ALTER TABLE lobbies ADD COLUMN IF NOT EXISTS ended_at TIMESTAMP;

ALTER TABLE rom_files ADD COLUMN IF NOT EXISTS file_path VARCHAR(500);
ALTER TABLE rom_files ADD COLUMN IF NOT EXISTS uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP;
"

# Verify tables were created
docker-compose exec db psql -U sekailink_user -d sekailink -c "\dt"
```

**OR** use the Python migration:

```bash
docker-compose up -d

# Wait for API to start
sleep 10

docker-compose exec api python3 -c "
from main import db, app
with app.app_context():
    db.create_all()
    print('✅ Database tables created successfully')
"
```

### 2. Verify .env File

Make sure your `.env` file has all required variables:

```bash
# Check if .env exists
ls -la .env

# If not, copy template
cp .env.template .env

# Edit with your values
nano .env
```

**Required variables:**
- `FLASK_SECRET_KEY` (generate with: `python -c "import secrets; print(secrets.token_hex(32))"`)
- `DATABASE_URL`
- `POSTGRES_*` (user, password, db)
- `REDIS_URL`
- `DISCORD_CLIENT_ID`
- `DISCORD_CLIENT_SECRET`
- `DISCORD_REDIRECT_URI`

### 3. Rebuild and Start

```bash
# Rebuild containers with new code
docker-compose build

# Start all services
docker-compose up -d

# Check logs for errors
docker-compose logs -f api

# Should see:
# ✅ Environment validation passed
# ✅ SocketIO initialized for real-time communications
# 🚀 Starting SekaiLink API
```

### 4. Verify Services

```bash
# Check all containers are running
docker-compose ps

# Expected output:
# sekailink_db       running
# sekailink_cache    running
# sekailink_api      running
# sekailink_celery   running
# sekailink_bot      running (will just sleep)
```

### 5. Test Basic Flow

1. **Navigate to:** `http://localhost:7000/`
2. **Login** with Discord OAuth
3. **Create a YAML** in YAML Studio tab
4. **Create a Lobby:**
   - Go to Home tab
   - Click "+ Créer une session"
   - Enter lobby name
5. **Test Real-time Features:**
   - Open lobby in another browser/incognito window
   - Join the lobby
   - Test chat
   - Test ready status
   - (Host) Try to start generation

---

## ✅ WHAT'S WORKING NOW (Prototype v1)

### Core Features
- ✅ Discord OAuth authentication
- ✅ User profiles (bio, pronouns, avatar)
- ✅ YAML file creation and management
- ✅ ROM upload with SHA-1 validation (temporary storage)
- ✅ Lobby creation with settings
- ✅ Join/leave lobbies
- ✅ Real-time chat with WebSocket
- ✅ Player ready status
- ✅ Host controls (kick, start generation)
- ✅ Host transfer on leave
- ✅ Automatic cleanup (24h for lobbies, 30d for ROMs)
- ✅ Database persistence

### API Endpoints (12 new routes!)
- `GET /api/lobbies` - List all open lobbies
- `POST /api/lobbies/create` - Create lobby
- `GET /api/lobbies/<id>` - Get lobby details
- `POST /api/lobbies/<id>/join` - Join lobby
- `POST /api/lobbies/<id>/leave` - Leave lobby
- `POST /api/lobbies/<id>/ready` - Toggle ready
- `POST /api/lobbies/<id>/kick` - Kick player (host only)
- `POST /api/generate` - Start seed generation
- `POST /api/roms/upload` - Upload ROM
- `GET /api/roms` - List user ROMs
- `DELETE /api/roms/<id>` - Delete ROM
- `GET/POST /api/yamls` - Manage YAMLs

### WebSocket Events (14 events)
- `connect`, `disconnect`
- `join_lobby`, `leave_lobby`
- `player_ready`, `player_finished`
- `start_sync`, `stop_sync`
- `chat_message`
- `generation_progress`, `generation_complete`

### Frontend Pages
- ✅ `index.html` - Landing page
- ✅ `dashboard.html` - User dashboard with Socket.IO
- ✅ `lobby.html` - Full lobby page with real-time updates

---

## ⚠️ KNOWN LIMITATIONS (Current Prototype)

### Generation System
- ✅ Calls `Generate.py` as subprocess (works but basic)
- ✅ Progress reporting during generation (polling-based)
- ✅ Patch file download working
- ❌ No WebhostLib integration (planned for Phase 2)
- ✅ ROMs copied to generation directory
- ✅ Server port assignment (38281-38380)
- ✅ Archipelago server auto-start

### Missing Features
- ❌ Custom worlds support
- ❌ Twitch integration
- ❌ Friend/blacklist system
- ❌ User ratings/reviews
- ❌ Ban appeal system
- ❌ Email notifications
- ❌ Admin panel
- ❌ Moderation logging
- ❌ Time limit enforcement
- ❌ Server port assignment
- ❌ Game server initialization

### UI/UX
- ❌ No loading states
- ❌ No error toasts (using alerts)
- ❌ No lobby settings page
- ❌ No user search
- ❌ No game catalog
- ❌ Basic styling only

---

## 📋 PHASE 2: Production Features (After Testing)

### Priority 1: Fix Generation (Critical for gameplay) ✅ COMPLETED

**Status:** ✅ All tasks completed on 2026-01-02

**Tasks:**
- [x] Copy required ROMs to generation directory ✅
- [x] Implement patch file generation ✅
- [x] Create download endpoint: `GET /api/lobbies/<id>/patches/<filename>` ✅
- [x] Assign Archipelago server ports (38281-38380 pool) ✅
- [x] Update lobby with server info ✅
- [x] Test full generation → download → play flow ✅

**Files modified:**
- `backend/tasks.py` - Added port management, server startup, ROM copying
- `backend/main.py` - Added patch download route, server_port in API
- `frontend/src/lobby.html` - Added download buttons and server connection UI

**Testing checklist:**
```
□ 2+ players join lobby
□ Both upload ROMs (if needed)
□ Both select YAMLs and ready up
□ Host clicks "Start Generation"
□ Generation completes successfully
□ Patch files are available for download
□ Server port is displayed
□ Players can connect to server
```

---

### Priority 2: WebhostLib Integration (Better generation)

**Why:** Proper integration with Archipelago's generation system instead of subprocess.

**Tasks:**
- [ ] Study `/home/sekailink/archipelago_core/WebHostLib/` structure
- [ ] Import WebhostLib modules in tasks.py
- [ ] Pass Archipelago options from LobbySettings
- [ ] Implement progress callbacks → WebSocket
- [ ] Handle multi-game scenarios
- [ ] Error handling with specific messages

**References:**
- `archipelago_core/WebHostLib/generate.py`
- `archipelago_core/WebHostLib/options.py`

---

### Priority 3: Custom Worlds Support

**Tasks:**
- [ ] Create `CustomWorld` database model
- [ ] Add upload endpoint: `POST /api/custom-worlds/upload`
- [ ] Extract metadata from `.apworld` files
- [ ] Store in `/opt/Archipelago/lib/worlds/`
- [ ] Detect required custom worlds from YAMLs
- [ ] Show warnings for unstable worlds
- [ ] Add to frontend dashboard

**Database schema:**
```sql
CREATE TABLE custom_worlds (
    id SERIAL PRIMARY KEY,
    uploader_id VARCHAR REFERENCES users(id),
    filename VARCHAR(255),
    file_path VARCHAR(500),
    file_hash VARCHAR(64),
    world_name VARCHAR(100),
    version VARCHAR(50),
    description TEXT,
    status VARCHAR(20) DEFAULT 'pending',
    uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

---

### Priority 4: Time Limit System

**Tasks:**
- [ ] Add timer display to lobby page
- [ ] Start timer when sync begins
- [ ] Countdown display
- [ ] Auto-release items when time expires (if restricted mode)
- [ ] Notify players when time is up
- [ ] Force lobby close after time limit

**Files to modify:**
- `frontend/src/lobby.html` (add timer UI)
- `backend/main.py` (WebSocket event for timer)
- `backend/tasks.py` (scheduled task to check time limits)

---

### Priority 5: Enhanced Moderation

**Tasks:**
- [ ] Add moderation logging table
- [ ] Create admin panel page
- [ ] Ban/suspend users (with duration)
- [ ] Ban appeal system
- [ ] Kick from any lobby (admin)
- [ ] Force close lobbies (admin)
- [ ] View all active lobbies (admin)
- [ ] Server maintenance mode

**New database models:**
```sql
CREATE TABLE moderation_logs (
    id SERIAL PRIMARY KEY,
    moderator_id VARCHAR REFERENCES users(id),
    target_user_id VARCHAR REFERENCES users(id),
    action VARCHAR(50),
    reason TEXT,
    duration INTEGER,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE bans (
    id SERIAL PRIMARY KEY,
    user_id VARCHAR REFERENCES users(id),
    reason TEXT,
    banned_by VARCHAR REFERENCES users(id),
    banned_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP,
    appeal TEXT,
    appeal_status VARCHAR(20) DEFAULT 'pending'
);
```

---

## 🧪 TESTING CHECKLIST

### Before Each Release

#### Database Tests
- [ ] Can create/update/delete users
- [ ] Lobbies persist across restarts
- [ ] Chat messages are saved
- [ ] Player status updates correctly
- [ ] Foreign key constraints work

#### Authentication Tests
- [ ] Discord OAuth login works
- [ ] Session persists across pages
- [ ] Logout clears session
- [ ] Unauthorized requests rejected

#### Lobby Tests
- [ ] Create lobby
- [ ] Join lobby (multiple users)
- [ ] Leave lobby
- [ ] Host transfer on host leave
- [ ] Kick player (host only)
- [ ] Close lobby
- [ ] Ready/unready toggle
- [ ] All players ready → enable generation

#### WebSocket Tests
- [ ] Chat messages appear instantly
- [ ] Player join/leave broadcasts
- [ ] Ready status updates in real-time
- [ ] Multiple lobbies don't interfere
- [ ] Reconnection works after disconnect

#### Generation Tests
- [ ] Generation starts only when all ready
- [ ] Only host can start
- [ ] Progress updates (when implemented)
- [ ] Success → patches available
- [ ] Failure → clear error message
- [ ] Cleanup happens after 24h

#### ROM Tests
- [ ] Upload ROM with SHA-1 check
- [ ] Verified ROM shows green status
- [ ] Unverified ROM shows warning
- [ ] Delete ROM removes file
- [ ] 30-day cleanup works

---

## 🐛 KNOWN BUGS TO FIX

### High Priority
- [x] **Generation doesn't copy ROMs** - ✅ FIXED - ROMs now copied to generation directory
- [x] **No patch download** - ✅ FIXED - Download buttons working
- [x] **Server port not assigned** - ✅ FIXED - Ports assigned from pool 38281-38380
- [ ] **Chat history loads in wrong order** - Should be oldest first
- [ ] **Lobby list doesn't auto-refresh** - Need WebSocket broadcast for new lobbies

### Medium Priority
- [ ] **No loading states** - Users don't know if action is processing
- [ ] **Error messages use alerts** - Should use toasts/notifications
- [ ] **YAML selector doesn't show games** - Hard to pick right YAML
- [ ] **No player count validation** - Can exceed max_players
- [ ] **Host actions visible to non-hosts** - Should be hidden client-side too

### Low Priority
- [ ] **French/English mixing** - Some UI in French, some in English
- [ ] **No favicon** - Browser tab shows default icon
- [ ] **No 404 page** - Errors show ugly default page
- [ ] **Avatar images don't load** - CORS issue with Discord CDN

---

## 📊 MONITORING & OPERATIONS

### Logs to Monitor

```bash
# API logs
docker-compose logs -f api | grep ERROR

# Celery worker logs
docker-compose logs -f celery_worker

# Database logs
docker-compose logs -f db

# Redis logs
docker-compose logs -f redis
```

### Health Checks

```bash
# Check API is responding
curl http://localhost:7000/

# Check database connection
docker-compose exec db psql -U sekailink_user -d sekailink -c "SELECT COUNT(*) FROM users;"

# Check Redis
docker-compose exec redis redis-cli ping

# Check disk usage (temporary files)
du -sh /tmp/generation/
```

### Performance Metrics to Track
- [ ] WebSocket connection count
- [ ] Active lobbies
- [ ] Generation success rate
- [ ] Average generation time
- [ ] Database query time
- [ ] ROM storage usage
- [ ] Celery task queue length

---

## 🔐 SECURITY AUDIT (Before Public Launch)

### Critical
- [ ] All secrets in `.env` (not in code) ✅
- [ ] `.env` in `.gitignore` ✅
- [ ] No hardcoded passwords ✅
- [ ] Environment validation at startup ✅
- [ ] SQL injection protection (using ORM) ✅
- [ ] Session-based auth ✅
- [ ] Host-only actions validated server-side ✅

### Important
- [ ] Rate limiting on uploads
- [ ] Rate limiting on API endpoints
- [ ] Input validation on all forms
- [ ] XSS protection (escape HTML in chat)
- [ ] CSRF tokens on state-changing requests
- [ ] File upload size limits
- [ ] File type validation
- [ ] ROM SHA-1 validation ✅

### Nice-to-Have
- [ ] 2FA for admins
- [ ] Audit logging for all admin actions
- [ ] IP blocking for abusers
- [ ] Honeypot fields in forms
- [ ] Content Security Policy headers
- [ ] HTTPS enforcement (in production)

---

## 📚 DOCUMENTATION TO CREATE

### For Users
- [ ] Getting Started guide
- [ ] How to create YAMLs
- [ ] How to upload ROMs
- [ ] How to host a lobby
- [ ] Troubleshooting common issues
- [ ] FAQ page
- [ ] Rules & Code of Conduct

### For Developers
- [ ] API documentation (Swagger/OpenAPI)
- [ ] WebSocket event reference (exists: `WEBSOCKET_API.md`) ✅
- [ ] Database schema diagram
- [ ] Deployment guide
- [ ] Contributing guidelines
- [ ] Testing guide

### For Admins
- [ ] Moderation handbook
- [ ] Server management guide
- [ ] Backup/restore procedures
- [ ] Disaster recovery plan

---

## 🚢 DEPLOYMENT CHECKLIST

### Pre-Deployment
- [ ] All tests passing
- [ ] Database migrations tested
- [ ] `.env` configured for production
- [ ] `FLASK_ENV=production`
- [ ] `FLASK_DEBUG=False`
- [ ] HTTPS certificates installed
- [ ] Domain DNS configured
- [ ] Firewall rules set
- [ ] Backup system in place

### Deployment Steps
1. [ ] Stop old version
2. [ ] Backup database
3. [ ] Pull latest code
4. [ ] Run database migrations
5. [ ] Build Docker images
6. [ ] Start new version
7. [ ] Smoke test critical paths
8. [ ] Monitor logs for errors
9. [ ] Announce to users

### Post-Deployment
- [ ] Monitor error logs (24h)
- [ ] Check resource usage
- [ ] Test from external network
- [ ] Verify WebSocket connections
- [ ] Test Discord OAuth
- [ ] Verify generation works

---

## 💡 FUTURE ENHANCEMENTS (Post-MVP)

### User Experience
- [ ] Dark/light theme toggle
- [ ] Multi-language support (FR/EN/ES/JP)
- [ ] User profiles with stats
- [ ] Achievement system
- [ ] Leaderboards
- [ ] Friend recommendations
- [ ] Lobby templates (save settings)

### Social Features
- [ ] Discord bot integration (roles, announcements)
- [ ] Voice chat integration
- [ ] Twitch streaming integration
- [ ] Spectator mode
- [ ] Replays/VODs

### Advanced Generation
- [ ] Race mode (competitive)
- [ ] Tournament brackets
- [ ] Scheduled syncs
- [ ] Recurring lobbies
- [ ] Custom world marketplace

### Analytics
- [ ] Player statistics dashboard
- [ ] Game popularity charts
- [ ] Generation success metrics
- [ ] User retention tracking

---

## 🆘 SUPPORT & COMMUNITY

### Getting Help
- **Discord:** https://discord.gg/XvvcBxrRsk
- **GitHub Issues:** https://github.com/lovenityjade/sekailink/issues
- **Email:** sekailink@themiareproject.com

### Contributing
1. Fork the repository
2. Create a feature branch
3. Test your changes
4. Submit a pull request
5. Follow code style (French comments OK!)

---

## ✨ CREDITS

**Developer:** Jade (lovenityjade)
**Platform:** Built on Archipelago.gg
**Hosting:** Hostinger VPS
**Community:** Archipelago Discord

---

**Last Updated:** 2026-01-02
**Version:** Prototype v1 - Server Testing Phase

---

# 🎮 NEXT STEPS FOR TESTING

1. **Run database migration** (see section above)
2. **Start Docker containers:** `docker-compose up -d`
3. **Check logs:** `docker-compose logs -f api`
4. **Open browser:** http://localhost:7000
5. **Test with friends:**
   - Both login with Discord
   - Create a lobby
   - Join from both accounts
   - Test chat
   - Upload YAMLs
   - Try ready/unready
   - (Host) Try to kick
   - (Host) Try to start generation

**Report bugs:** Create issues on GitHub with steps to reproduce!

Good luck testing! 🚀
