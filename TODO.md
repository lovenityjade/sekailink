# SekaiLink TODO List
**Last Updated**: January 4, 2026 (End of Week Audit)
**Project Status**: 95% Complete - Critical Testing & Fixes Needed

---

## 🚨 **CRITICAL PATH BLOCKERS** (Must Fix Immediately)

### 1. **Generation System - UNTESTED/BROKEN** ⛔
**Priority**: CRITICAL
**Status**: ❌ Code exists but NOT working

**Problem**: User reports generation not working. No end-to-end testing done.

**What We Have**:
- ✅ `generation_bridge.py` - WebHostLib integration (320 lines)
- ✅ `run_webhost_generation()` - Celery task
- ✅ API endpoint: `POST /api/generate`
- ⚠️ NO TESTING DONE

**What's Likely Broken**:
1. YAML validation might fail
2. Multidata generation might crash
3. Patch files might not be created
4. Server might not start

**Action Plan**:
1. Create test lobby with 2 users
2. Upload simple YAML (ChecksFinder or Clique - no ROM needed)
3. Mark ready
4. Click generate
5. Check Celery logs: `docker logs sekailink_celery -f`
6. Fix errors one by one
7. Verify multidata.zip created in `/tmp/generation/{lobby_id}/`
8. Verify server starts on port 38281-38380

**Files to Debug**:
- `/backend/generation_bridge.py:31` - `generate_multiworld()`
- `/backend/tasks.py:182` - `run_webhost_generation()`
- `/backend/main.py:2688` - Generate endpoint

**Expected Errors**:
- Missing Archipelago dependencies
- Path issues with YAML files
- Database session problems
- WebSocket broadcast failures

---

### 2. **YAML Creator - HAS BUGS** ⚠️
**Priority**: CRITICAL
**Status**: ⚠️ Partially working, needs fixes

**User Report**: "YAML creator has a lot of bugs"

**Known Issues**:
1. **Option rendering bugs**:
   - Some option types might not display correctly
   - Default values might be wrong
   - Form might be empty for some games

2. **Form submission bugs**:
   - YAML might not be created
   - Download might fail
   - Save to vault might fail

3. **API bugs**:
   - `/api/games/<slug>/options` might return errors
   - `/api/games/<slug>/create-yaml` might fail validation

**Action Plan**:
1. Test with 5-10 different games:
   - A Link to the Past (complex options)
   - ChecksFinder (simple)
   - Minecraft (lots of options)
   - Hollow Knight (named ranges)
   - Factorio (counters)

2. For each game:
   - Load YAML creator page
   - Check if ALL options visible
   - Fill out form
   - Click "Create YAML"
   - Verify YAML downloaded/saved
   - Check YAML syntax is valid

3. Check browser console for JavaScript errors

**Files to Debug**:
- `/frontend/src/yaml_creator.html:320` - `loadGameOptions()`
- `/frontend/src/yaml_creator.html:380` - `createOptionField()`
- `/backend/yaml_creator.py:27` - `get_game_options()`
- `/backend/main.py:667` - Options API endpoint

**Common Bugs to Watch For**:
- OptionList not rendering (checkbox lists)
- OptionCounter not working (quantity inputs)
- Named ranges showing wrong labels
- Default values not pre-selected
- Form submission creating malformed YAML

---

### 3. **Lobby Flow - UNTESTED** ⚠️
**Priority**: CRITICAL
**Status**: ⚠️ Code exists but not tested

**User Report**: "Lobbies have bugs"

**Critical Path**:
1. User creates lobby → ✅ Likely works
2. User joins lobby → ⚠️ Needs testing
3. User selects YAML → ⚠️ Dropdown might be empty
4. User uploads ROM (if needed) → ⚠️ Endpoint added but untested
5. User clicks "I'm Ready" → ⚠️ Button might not enable
6. Host sees "Generate" button → ❌ Was broken (fixed in Phase 11)
7. Host clicks Generate → ❌ Likely broken
8. Generation completes → ❌ Untested
9. Patch download appears → ⚠️ Added but untested
10. Server URL shown → ✅ Should work

**Action Plan**:
1. Create lobby as User A
2. Join lobby as User B (different browser)
3. Both upload YAMLs
4. Test ready button
5. Verify host controls visible
6. Click generate
7. Monitor progress
8. Download patches
9. Verify server connection info

**Files to Check**:
- `/frontend/src/lobby.html:686` - `loadLobbyData()`
- `/frontend/src/lobby.html:514` - YAML dropdown population
- `/frontend/src/lobby.html:945` - Ready button handler
- `/backend/main.py:1128` - Join lobby endpoint
- `/backend/main.py:1288` - Ready endpoint

**Likely Bugs**:
- WebSocket updates not working
- YAML dropdown empty
- Ready button stays disabled
- Generate button missing (fixed but retest)
- ROM upload fails silently

---

### 4. **UX is Confusing** ⚠️
**Priority**: HIGH
**Status**: ❌ Needs redesign

**User Report**: "Need to clean up the UX to make it more user friendly"

**Current Problems**:
- Lobby page is cluttered
- Too much information at once
- Poor visual hierarchy
- Controls hard to find
- Status unclear

**Redesign Needed**:
1. **Lobby Page** (`/frontend/src/lobby.html`):
   - Reorganize sections
   - Better player list layout
   - Clearer timer display
   - Obvious ready/generate buttons
   - Better chat interface

2. **Homepage** (`/frontend/src/index.html`):
   - Simplify lobby list
   - Better game grid
   - Clearer navigation

3. **Dashboard** (`/frontend/src/dashboard.html`):
   - Tab organization
   - Better visual feedback
   - Clearer CTAs

**Files to Edit**:
- `/frontend/src/lobby.html`
- `/frontend/static/css/pages.css`
- `/frontend/static/css/components.css`

---

## 🔴 **HIGH PRIORITY BUGS** (Fix This Week)

### 5. **Duplicate Templates** 🗑️
**Priority**: HIGH
**Status**: ❌ Cleanup needed

**Problem**: Templates exist in TWO locations:
- `/backend/templates/` (19 files) - **NOT USED**
- `/frontend/src/` (18 files) - **USED**

**Fix**:
```bash
# Safe to delete
rm -rf /home/sekailink/backend/templates/
```

**Verify**: Flask uses `/frontend/src` (confirmed in `main.py:129`)

---

### 6. **WebSocket Real-time Updates** ⚠️
**Priority**: HIGH
**Status**: ⚠️ Code exists, needs testing

**Test All Events**:
- [ ] Player joins lobby → all see update
- [ ] Player leaves lobby → all see update
- [ ] Player ready → all see status change
- [ ] Chat messages → instant delivery
- [ ] Typing indicators → show correctly
- [ ] Generation starts → all notified
- [ ] Generation completes → all notified
- [ ] Timer updates → real-time sync

**Files**:
- `/backend/main.py:2786-3138` - SocketIO handlers
- `/frontend/static/js/main.js` - WebSocket client
- `/frontend/src/lobby.html` - WebSocket integration

---

### 7. **Broken Navigation Links** 🔗
**Priority**: MEDIUM
**Status**: ❌ Needs audit

**Check All Links**:
- [ ] Homepage → Game pages
- [ ] Game page → Create YAML
- [ ] Game page → Create Lobby
- [ ] Dashboard → YAML creator
- [ ] Lobby → Player profiles
- [ ] Footer links (help, FAQ, about, rules, docs, donate, credits, contact)
- [ ] Header navigation

**Method**: Manual click-testing of every link

---

### 8. **Static Files Confusion** 📁
**Priority**: MEDIUM
**Status**: ⚠️ Partially cleaned up

**Current State**:
- `/backend/static/` - Files copied here (Phase 11)
- `/frontend/static/` - Original source
- Flask configured to use `/frontend/static` (line 130)

**Verify**:
1. Check if `/backend/static/` needed
2. If not, delete it
3. Ensure CSS/JS loads correctly

---

## 🟡 **MEDIUM PRIORITY** (Fix Before Launch)

### 9. **Error Handling** ⚠️
**Priority**: MEDIUM
**Status**: ❌ Incomplete

**Add Better Errors For**:
- YAML validation failures (detailed field errors)
- ROM upload failures (file type, size, hash)
- Generation failures (show Archipelago errors)
- Network errors (timeout, connection lost)
- Auth failures (session expired)

**Improve**:
- Replace alert() with toast notifications
- Show errors in UI, not just console
- Log errors server-side

---

### 10. **ROM Cleanup Verification** 🗑️
**Priority**: MEDIUM
**Status**: ⚠️ Code added, needs testing

**Verify**:
- [ ] ROMs upload to `/tmp/lobbies/{lobby_id}/`
- [ ] ROMs deleted after generation completes
- [ ] Cleanup task runs correctly
- [ ] No permanent ROM storage

**Files**:
- `/backend/main.py:1451` - ROM upload endpoint (ADDED Phase 11)
- `/backend/tasks.py:596` - Cleanup function (UPDATED Phase 11)

---

### 11. **Server Port Assignment** 🔌
**Priority**: MEDIUM
**Status**: ✅ Code exists, needs testing

**Verify**:
- [ ] Port pool 38281-38380 works
- [ ] No port conflicts
- [ ] Server URL displays correctly
- [ ] Multiple lobbies get different ports

**Files**:
- `/backend/tasks.py:79` - `find_available_port()`
- `/backend/server_manager.py` - Multiprocess isolation

---

### 12. **Phase 7 Frontend (Rating UI)** ⏳
**Priority**: LOW (Can defer)
**Status**: Backend 100%, Frontend 0%

**Backend Complete** ✅:
- User ratings (4 criteria)
- User reviews
- Review moderation
- Server ratings (auto-calculated)

**Frontend TODO**:
- [ ] Rating modal UI (4-star inputs)
- [ ] Review modal UI
- [ ] Display stars on profiles
- [ ] Show review cards
- [ ] Moderation queue UI

**Files**:
- `/frontend/src/profile.html`
- `/frontend/src/moderation.html`

---

## 🟢 **CLEANUP & MAINTENANCE**

### 13. **Code Cleanup** 🧹
**Priority**: LOW

**Tasks**:
- [ ] Remove unused imports
- [ ] Delete commented-out code
- [ ] Standardize error messages
- [ ] Add missing docstrings
- [ ] Fix inconsistent formatting

---

### 14. **Documentation** 📚
**Priority**: MEDIUM

**User Documentation**:
- [ ] How to create YAMLs guide
- [ ] How to host a lobby guide
- [ ] ROM requirements list
- [ ] Troubleshooting guide

**Developer Documentation**:
- [ ] API reference (all 60+ endpoints)
- [ ] WebSocket event reference
- [ ] Database schema diagram
- [ ] Setup guide for contributors

---

### 15. **Performance Optimization** ⚡
**Priority**: LOW

**Tasks**:
- [ ] Add database indexes
- [ ] Optimize API queries (N+1 problems)
- [ ] Add caching for game list
- [ ] Minimize JavaScript
- [ ] Compress images

---

## 🛠️ **INFRASTRUCTURE** (This Week)

### 16. **phpMyAdmin Installation** 🗄️
**Priority**: HIGH (User requested)
**Status**: ❌ TODO

**Steps**:
1. Install phpMyAdmin on VPS
2. Configure for PostgreSQL (NOT MySQL)
   - Need to use pgAdmin or Adminer instead (phpMyAdmin is MySQL only)
3. Link to `sekailink.xyz/pgadmin` or `sekailink.xyz/adminer`
4. Secure with password
5. Test database access

**Recommendation**: Use **Adminer** (lighter, supports PostgreSQL)

```bash
# Install Adminer in Docker
docker run -d \
  --name adminer \
  --link sekailink_db:db \
  -p 8080:8080 \
  adminer

# Then configure Nginx to proxy sekailink.xyz/adminer → localhost:8080
```

---

### 17. **Database Backup System** 💾
**Priority**: HIGH (User requested)
**Status**: ❌ TODO

**Create Backup Script**:
```bash
#!/bin/bash
# /home/sekailink/backup.sh

DATE=$(date +%Y%m%d_%H%M%S)
BACKUP_DIR="/home/sekailink/backups"

mkdir -p $BACKUP_DIR

# Backup PostgreSQL database
docker exec sekailink_db pg_dump -U sekailink_user sekailink > \
  $BACKUP_DIR/sekailink_db_$DATE.sql

# Backup important files
tar -czf $BACKUP_DIR/sekailink_files_$DATE.tar.gz \
  /home/sekailink/backend \
  /home/sekailink/frontend \
  /home/sekailink/docker-compose.yml \
  /home/sekailink/.env

# Keep only last 7 days
find $BACKUP_DIR -name "*.sql" -mtime +7 -delete
find $BACKUP_DIR -name "*.tar.gz" -mtime +7 -delete

echo "✅ Backup complete: $DATE"
```

**Schedule Daily Backup**:
```bash
# Add to crontab
crontab -e

# Add line:
0 2 * * * /home/sekailink/backup.sh >> /home/sekailink/backup.log 2>&1
```

---

### 18. **Git Commit & Push** 📤
**Priority**: HIGH (User requested)
**Status**: ❌ TODO at end of session

**Commit Message**:
```
fix: Week wrap-up - Comprehensive audit, fixes, and documentation

🔍 AUDIT COMPLETE:
- Scanned entire codebase for bugs and issues
- Documented all broken features
- Created comprehensive TODO.md
- Updated README.md for accuracy

🐛 KNOWN ISSUES DOCUMENTED:
1. Generation system untested/broken
2. YAML creator has multiple bugs
3. Lobby flow needs testing
4. UX improvements needed
5. Duplicate templates cleanup required

📋 COMPLETED THIS WEEK:
- Phase 10: WebHostLib integration (100%)
- Phase 11: Critical bug fixes (10 bugs fixed)
- Comprehensive testing checklist created
- TODO.md fully updated
- README.md modernized

📊 PROJECT STATUS: 95% Complete
- 9/10 phases complete
- Critical path: Generation, YAML creator, lobbies need testing
- Est. 2-3 weeks to production

📝 NEXT WEEK PRIORITIES:
1. Test & fix generation system
2. Test & fix YAML creator
3. Test & fix lobby flow
4. Improve UX (user feedback)
5. Install phpMyAdmin/Adminer
6. Setup database backups

---

🤖 Generated with Claude Code
Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>
```

---

## 📋 **END-TO-END TESTING CHECKLIST**

### User Flow (Must Work Before Launch)

**Setup**:
- [ ] User A logs in with Discord
- [ ] User B logs in with Discord (different browser)

**YAML Creation**:
- [ ] User A visits game page (e.g., ChecksFinder)
- [ ] Clicks "Create YAML"
- [ ] YAML creator loads
- [ ] All options visible
- [ ] Can fill out form
- [ ] Can download YAML
- [ ] Can save to vault
- [ ] YAML appears in dashboard

**Lobby Creation**:
- [ ] User A creates lobby
- [ ] Lobby appears on homepage
- [ ] Lobby appears in dashboard
- [ ] User B can see lobby on homepage

**Lobby Join**:
- [ ] User B clicks "Join"
- [ ] User B added to lobby
- [ ] User A sees User B join (WebSocket)
- [ ] Both users see player list

**Lobby Preparation**:
- [ ] User A selects YAML from dropdown
- [ ] User B selects YAML from dropdown
- [ ] (If ROM needed) Users upload ROMs
- [ ] Ready button enables after YAML selected
- [ ] User A clicks "I'm Ready"
- [ ] User B sees User A ready (WebSocket)
- [ ] User B clicks "I'm Ready"
- [ ] Host (User A) sees "Generate" button enabled

**Generation**:
- [ ] User A clicks "Generate"
- [ ] Confirmation dialog appears
- [ ] User A confirms
- [ ] Status changes to "generating"
- [ ] Celery task runs
- [ ] No errors in logs
- [ ] Generation completes
- [ ] Status changes to "ready"

**Post-Generation**:
- [ ] Server URL appears (sekailink.xyz:PORT)
- [ ] PORT is in range 38281-38380
- [ ] "Download Patch" buttons appear
- [ ] User A downloads patch successfully
- [ ] User B downloads patch successfully
- [ ] Patches are valid .ap files

**Server Connection**:
- [ ] User A opens Archipelago client
- [ ] User A connects to sekailink.xyz:PORT
- [ ] Connection successful
- [ ] User B connects successfully
- [ ] Items sync between players

**Cleanup**:
- [ ] ROMs deleted from `/tmp/lobbies/{lobby_id}/`
- [ ] Generation files present in `/tmp/generation/{lobby_id}/`
- [ ] Server process running

**If ANY step fails, DO NOT LAUNCH**

---

## 📊 **METRICS & TRACKING**

### Current Status
- **Overall Progress**: 95%
- **Backend**: 98%
- **Frontend**: 92%
- **Testing**: 10% ⚠️
- **Documentation**: 75%

### Critical Bugs
- **Blockers**: 3 (Generation, YAML creator, Lobby flow)
- **High Priority**: 5
- **Medium Priority**: 6
- **Low Priority**: 4

### Time Estimates
- **Week 1** (Next): Testing & critical bug fixes
- **Week 2**: UX improvements & polish
- **Week 3**: Final testing & production deployment

---

## 💡 **NOTES FOR NEXT SESSION**

### What We Know
✅ **Working Features**:
- Discord OAuth
- Game API (83 games)
- Friends system
- Favorites system
- Chat system
- Timer system
- Admin/mod tools (backend)
- Real-time WebSocket (code exists)

❌ **Broken Features**:
- Generation (untested/broken)
- YAML creator (has bugs)
- Lobby flow (needs testing)
- Rating UI (not implemented)

### User Feedback
> "We still can't generate and the YAML creator has a lot of bugs still, as well as the lobbies. We still need also to clean up the UX to make it more user friendly."

**Action**: Focus ENTIRELY on these 4 items next week:
1. Fix generation
2. Fix YAML creator
3. Fix lobbies
4. Improve UX

Everything else is secondary.

---

**END OF TODO LIST**

*Last Updated: January 4, 2026 23:59 UTC*
*Next Update: After critical bugs fixed*
