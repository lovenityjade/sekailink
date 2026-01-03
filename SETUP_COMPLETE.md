# ✅ Setup Complete: Archipelago Server Port Assignment

**Date:** 2026-01-02
**Status:** Ready for testing

---

## 🎯 What Was Implemented

### 1. Database Migration System ✅

**Files Created:**
- `migrate_db.py` - Python migration script with verification
- `migrate.sh` - Convenient wrapper for Docker

**What it does:**
- Creates 3 new tables: `lobby_settings`, `lobby_players`, `chat_messages`
- Adds new columns to `lobbies` table: `visibility`, `server_port`, `started_at`, `ended_at`
- Adds new columns to `rom_files` table: `file_path`, `uploaded_at`
- Verifies all tables are created successfully

**How to run:**
```bash
# Option 1: Using the wrapper script
./migrate.sh

# Option 2: Direct Python (if .env is configured)
python3 migrate_db.py

# Option 3: Inside Docker container
docker-compose exec api python3 /app/migrate_db.py
```

---

### 2. Archipelago Server Port Assignment ✅

**Files Modified:**
- `backend/tasks.py` - Added port management and server startup
- `backend/main.py` - Added server_port to API response
- `frontend/src/lobby.html` - Added server connection UI

**Features Implemented:**

#### Port Pool Management
- **Port Range:** 38281-38380 (100 concurrent lobbies)
- **Smart Allocation:** Checks database for ports in use
- **Automatic Assignment:** Finds first available port

#### Server Lifecycle
- **Start:** Launches Archipelago MultiServer after generation completes
- **Monitor:** Stores PID for process tracking
- **Logs:** Saves server output to `/tmp/generation/{lobby_id}/logs/server.log`
- **Stop:** Gracefully shuts down server on lobby cleanup

#### What Happens During Generation

```
1. Players join lobby and ready up
2. Host starts generation
3. ✅ Archipelago generates seed and patches
4. ✅ ROMs are copied to lobby directory
5. ✅ Patches are assigned to players
6. 🆕 Available port is found (e.g., 38281)
7. 🆕 Archipelago server starts on that port
8. 🆕 Lobby is updated with port number
9. ✅ Players can download patches
10. 🆕 Players can connect to server
```

---

## 📁 File Changes Summary

### `backend/tasks.py`
**Lines 79-178:** Added port management functions
```python
def find_available_port()
    # Scans database for ports 38281-38380
    # Returns first available port

def start_archipelago_server(lobby_id, seed_file, port)
    # Launches MultiServer.py with seed file
    # Logs to /tmp/generation/{lobby_id}/logs/server.log
    # Stores PID for cleanup
```

**Lines 313-341:** Modified `run_generator()` to start server
- Finds available port after generation
- Starts Archipelago server
- Updates `lobby.server_port` and `lobby.started_at`

**Lines 401-441:** Added `stop_archipelago_server()` task
- Reads PID from file
- Sends SIGTERM for graceful shutdown
- Force kills if needed

**Lines 444-452:** Modified `cleanup_lobby_files()`
- Now stops server before cleaning files

---

### `backend/main.py`
**Lines 544, 574-575:** Updated API response
```python
players.append({
    # ... existing fields ...
    "patch_url": lp.patch_url,  # NEW
})

return jsonify({
    # ... existing fields ...
    "server_port": lobby.server_port,  # NEW
    "seed_url": lobby.seed_url,        # NEW
})
```

---

### `frontend/src/lobby.html`
**Lines 470-487:** Added server connection UI
```html
<div id="server-info">
    <h3>Server Connection</h3>
    <p>Server Address: <span id="server-address">...</span></p>
    <p>Port: <span id="server-port">...</span></p>
</div>
```

**Lines 674, 681, 707-736:** Updated status handling
- Shows server info when lobby status is 'ready' or 'active'
- Displays server address (localhost or sekailink.xyz)
- Shows assigned port in large, readable font

---

## 🧪 Testing Checklist

### Prerequisites
```bash
# 1. Ensure .env file is configured
ls -la .env

# 2. Stop all containers
docker-compose down

# 3. Run database migration
./migrate.sh

# 4. Start services
docker-compose up -d

# 5. Check logs for errors
docker-compose logs -f api
docker-compose logs -f celery_worker
```

### End-to-End Flow Test

#### Step 1: Create Lobby
1. Navigate to http://localhost:7000
2. Login with Discord
3. Create a YAML file in YAML Studio
4. Go to Home → Create Lobby
5. Enter lobby name → Lobby page opens

#### Step 2: Prepare Players
1. Open lobby in second browser/incognito
2. Second player joins lobby
3. Both players select their YAMLs
4. Both players click "I'm Ready!"

#### Step 3: Upload ROMs (if needed)
1. Go to ROM Vault tab
2. Upload required ROM files
3. Wait for SHA-1 verification
4. Verify status shows "verified"

#### Step 4: Start Generation
1. Host clicks "Start Generation"
2. Status changes to "generating"
3. Progress bar appears
4. Wait 1-3 minutes for completion

#### Step 5: Verify Server Assignment ✅
1. Status changes to "ready"
2. ✅ Download buttons appear for all players
3. ✅ **Server Connection** panel appears
4. ✅ Server address shows (localhost or sekailink.xyz)
5. ✅ Port shows (should be 38281-38380)

#### Step 6: Download and Connect
1. Click "Download Patch"
2. Save patch file
3. Open Archipelago client
4. Connect to `<address>:<port>` shown in UI
5. Load your patch file
6. ✅ Successfully connect to multiworld server

---

## 🐛 Troubleshooting

### "No available ports in pool"
**Cause:** 100 lobbies are currently active
**Fix:** Wait for old lobbies to expire (24h auto-cleanup) or manually stop servers

```bash
# Check active lobbies
docker-compose exec db psql -U sekailink_user -d sekailink -c \
  "SELECT id, name, server_port, status FROM lobbies WHERE server_port IS NOT NULL;"

# Force cleanup old lobby
docker-compose exec api python3 -c "
from tasks import stop_archipelago_server, cleanup_lobby_files
stop_archipelago_server(LOBBY_ID)
cleanup_lobby_files(LOBBY_ID)
"
```

### "Failed to start game server"
**Cause:** Archipelago server script not found
**Check:**
```bash
# Verify Archipelago is installed
ls /opt/Archipelago/MultiServer.py
# OR
ls /opt/Archipelago/ArchipelagoServer.py

# Check ARCHIPELAGO_PATH in .env
grep ARCHIPELAGO_PATH .env
```

### Server not responding
**Debug steps:**
```bash
# 1. Check server logs
cat /tmp/generation/LOBBY_ID/logs/server.log

# 2. Check if process is running
cat /tmp/generation/LOBBY_ID/server.pid
ps aux | grep PROCESS_ID

# 3. Check port is listening
netstat -tuln | grep PORT_NUMBER
```

### Database migration fails
**Common issues:**
```bash
# Issue: "relation already exists"
# Solution: Normal, means tables already created

# Issue: "permission denied"
# Solution: Check database credentials in .env

# Issue: "database not running"
# Solution: Start database first
docker-compose up -d db
sleep 5
./migrate.sh
```

---

## 📊 How to Verify It's Working

### Check 1: Database Schema
```sql
-- Connect to database
docker-compose exec db psql -U sekailink_user -d sekailink

-- Verify new columns exist
\d lobbies

-- Should show:
-- server_port    | integer
-- started_at     | timestamp
-- ended_at       | timestamp
-- visibility     | character varying(20)
```

### Check 2: Port Assignment
```sql
-- Check active lobbies with ports
SELECT id, name, server_port, status, started_at
FROM lobbies
WHERE server_port IS NOT NULL;
```

### Check 3: Server Process
```bash
# List all Archipelago server processes
ps aux | grep MultiServer

# Check listening ports
netstat -tuln | grep -E "3828[0-9]"
```

### Check 4: API Response
```bash
# Get lobby details (replace LOBBY_ID)
curl http://localhost:7000/api/lobbies/LOBBY_ID \
  -H "Cookie: session=YOUR_SESSION_COOKIE"

# Should include:
# "server_port": 38281,
# "seed_url": "/api/lobbies/1/patches/...",
```

---

## 🚀 What's Next?

### Completed ✅
- [x] Database migration system
- [x] Port pool management (38281-38380)
- [x] Automatic server startup after generation
- [x] Server connection info in UI
- [x] Graceful server shutdown on cleanup
- [x] PID tracking for process management
- [x] Server logs saved to disk

### Still TODO (Phase 2)
- [ ] WebhostLib integration (better generation)
- [ ] Custom worlds support (.apworld uploads)
- [ ] Server health monitoring
- [ ] Port exhaustion alerts
- [ ] Multi-game support in same lobby
- [ ] Time limit enforcement
- [ ] Auto-release items on timeout
- [ ] Better error handling (toasts instead of alerts)
- [ ] Loading states throughout UI
- [ ] Admin panel for manual server management

---

## 📞 Support

**Issues?** Open a GitHub issue with:
1. What you were trying to do
2. Expected behavior
3. Actual behavior
4. Relevant logs from `docker-compose logs`

**Discord:** https://discord.gg/XvvcBxrRsk
**GitHub:** https://github.com/lovenityjade/sekailink

---

**Happy Testing!** 🎮🎉
