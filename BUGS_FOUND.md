# Bugs Found During Phase 9 Audit

## Critical Issues

### 1. **Lobby URL Routing Mismatch** ✅ FIXED
- **Location**: `frontend/src/index.html:245`
- **Issue**: JavaScript linked to `/lobby/${lobby.slug || lobby.id}` but route doesn't exist
- **Fix**: Changed to `/lobby.html?id=${lobby.id}`
- **Status**: ✅ FIXED

### 2. **Game Card Click URL** ✅ VERIFIED
- **Location**: `frontend/src/index.html:360`
- **Issue**: Links to `/game/${slug}`
- **Status**: ✅ Route exists at main.py:2606

### 3. **Missing Twitch Disconnect Endpoint** ✅ FIXED
- **Location**: `frontend/src/dashboard.html:822`
- **Issue**: Called `/api/twitch/disconnect` but endpoint didn't exist
- **Fix**: Added POST /api/twitch/disconnect at main.py:388
- **Status**: ✅ FIXED

### 4. **Lobby List API Response Mismatch** ✅ FIXED
- **Location**: `backend/main.py:754` (GET /api/lobbies)
- **Issue**: Returned `players: "X/Y"` but frontend expected `player_count` and `max_players`
- **Issue**: Missing `host_name` field in response
- **Fix**: Changed response to include `player_count`, `max_players`, `host_name`, and `slug` fields
- **Status**: ✅ FIXED

## Testing Queue

### Frontend Pages to Test:
- [ ] index.html - Homepage
- [ ] dashboard.html - User dashboard
- [ ] game.html - Game pages
- [ ] create_room.html - Lobby creation
- [ ] lobby.html - Lobby page
- [ ] profile.html - User profiles
- [ ] moderation.html - Mod dashboard
- [ ] admin.html - Admin dashboard

### API Endpoints to Test:
- [ ] GET /api/lobbies
- [ ] GET /api/games
- [ ] POST /api/lobbies/create
- [ ] Authentication flow
- [ ] WebSocket connections

### Features to Test:
- [ ] Lobby creation flow
- [ ] Lobby joining
- [ ] YAML upload
- [ ] ROM upload
- [ ] Generation process
- [ ] Server creation
- [ ] Friend system
- [ ] Rating system
